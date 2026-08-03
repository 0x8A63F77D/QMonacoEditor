#include "QMonacoEditor.h"
#include "MonacoBridge.h"

#include <QVBoxLayout>
#include <QChildEvent>
#include <QEvent>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebChannel>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QUrl>
#include <QLoggingCategory>
#include <QEventLoop>
#include <QPointer>
#include <QTimer>

#include <memory>

Q_LOGGING_CATEGORY(lcMonaco, "qmonacoeditor", QtWarningMsg)

QMonacoEditor::QMonacoEditor(QWidget *parent)
    : QWidget(parent)
{
    Q_INIT_RESOURCE(qmonacoeditor);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_webView = new QWebEngineView(this);
    layout->addWidget(m_webView);
    watchForShortcuts(m_webView);

    m_bridge = new MonacoBridge(this);
    m_channel = new QWebChannel(this);
    m_channel->registerObject(QStringLiteral("bridge"), m_bridge);
    m_webView->page()->setWebChannel(m_channel);

    connect(m_bridge, &MonacoBridge::editorReady, this, [this]() {
        m_ready = true;
        emit editorReady();
    });

    connect(m_bridge, &MonacoBridge::textChanged, this, [this](const QString &text) {
        emit textChanged(text);
    });

    connect(m_bridge, &MonacoBridge::cursorPositionChanged, this, [this](int line, int column) {
        emit cursorPositionChanged(line, column);
    });

    extractResources();
    qCDebug(lcMonaco) << "Loading from:" << resourceDir();
    qCDebug(lcMonaco) << "index.html exists:" << QFile::exists(resourceDir() + "/index.html");
    m_webView->setUrl(QUrl::fromLocalFile(resourceDir() + "/index.html"));
}

QMonacoEditor::~QMonacoEditor() = default;

void QMonacoEditor::setShortcutPolicy(ShortcutPolicy policy) {
    m_shortcutPolicy = policy;
}

QMonacoEditor::ShortcutPolicy QMonacoEditor::shortcutPolicy() const {
    return m_shortcutPolicy;
}

void QMonacoEditor::watchForShortcuts(QObject *target) {
    if (!target) {
        return;
    }
    target->installEventFilter(this);
    const QObjectList children = target->children();
    for (QObject *child : children) {
        watchForShortcuts(child);
    }
}

bool QMonacoEditor::isInsideWebView(const QObject *object) const {
    for (const QObject *o = object; o; o = o->parent()) {
        if (o == m_webView) {
            return true;
        }
    }
    return false;
}

bool QMonacoEditor::eventFilter(QObject *watched, QEvent *event) {
    // ShortcutOverride is delivered to the focus widget, which for a loaded page is the
    // view's focus proxy -- a QQuickWidget the render process creates *after* the
    // QWebEngineView exists, and replaces on things like a render-process crash. Rather
    // than resolve focusProxy() once and hope, the filter follows the subtree: every
    // new object below the view gets watched as it is parented, so whichever widget
    // ends up being the focus proxy is covered. ChildAdded reaches us because its
    // receiver is the new object's parent, which we are already watching.
    if (event->type() == QEvent::ChildAdded) {
        watchForShortcuts(static_cast<QChildEvent *>(event)->child());
        return false;
    }

    // Being watched is not the same as being part of the editor: a child can be
    // reparented out of the view after the filter was installed, and an installed
    // filter does not follow it back off. The policy therefore keys off where the
    // object sits *now*, so it can never govern a widget outside the editor.
    if (event->type() == QEvent::ShortcutOverride
        && m_shortcutPolicy != ShortcutPolicy::QtDefault
        && isInsideWebView(watched)) {
        if (m_shortcutPolicy == ShortcutPolicy::HostShortcutsWin) {
            // Leaving the event unaccepted is what makes the host win: QApplication
            // keeps walking the parent chain and finally triggers the QAction. Consuming
            // it here (returning true) is what denies the web view its chance to accept.
            event->ignore();
        } else {
            // EditorFirst: accepting stops the walk, so no QAction triggers and the chord
            // is re-delivered as a plain key press to the page. Chords Monaco does not
            // bind come back out through WebEngine's unhandled-key path, which hands them
            // to this widget's *parent* -- outside the subtree filtered here, so host
            // QActions on those still fire.
            event->accept();
        }
        return true;
    }

    return QWidget::eventFilter(watched, event);
}

void QMonacoEditor::setText(const QString &text) {
    if (m_ready) {
        emit m_bridge->requestSetText(text);
    }
}

QVariant QMonacoEditor::evalJsSync(const QString &expr) const {
    if (!m_ready) {
        return {};
    }
    // runJavaScript is async-only (Chromium runs in a separate process), so we spin a
    // nested event loop to turn it into a blocking read. The loop keeps the app
    // responsive while waiting.
    //
    // State lives on the heap and is shared with the callback: if we return early on
    // timeout, a late-arriving callback must not touch dead stack frames.
    struct EvalState {
        QEventLoop loop;
        QVariant result;
    };
    auto state = std::make_shared<EvalState>();
    // The nested loop can process events that delete this widget (e.g. the parent window
    // closing) before runJavaScript's callback returns. QPointer goes null in that case,
    // so we detect it and avoid touching a dangling `this`.
    QPointer<const QMonacoEditor> guard(this);
    m_webView->page()->runJavaScript(expr, [state](const QVariant &value) {
        state->result = value;
        state->loop.quit();
    });
    // The callback never arrives if the render process dies or the page navigates away
    // mid-flight; give up after 5s instead of hanging the caller forever. The loop is
    // the connection context, so a fired timer after loop teardown is a no-op.
    QTimer::singleShot(5000, &state->loop, &QEventLoop::quit);
    state->loop.exec();
    if (!guard) {
        return {};
    }
    return state->result;
}

// All getter expressions use optional chaining: if the page reloaded or crashed after
// editorReady(), the JS globals vanish and the expression must yield undefined (mapped
// to a default-constructed QVariant) instead of throwing.
QString QMonacoEditor::text() const {
    return evalJsSync(QStringLiteral("window.__monacoEditor?.getValue()")).toString();
}

void QMonacoEditor::setLanguage(const QString &languageId) {
    if (m_ready) {
        emit m_bridge->requestSetLanguage(languageId);
    }
}

void QMonacoEditor::setTheme(const QString &themeId) {
    if (m_ready) {
        emit m_bridge->requestSetTheme(themeId);
    }
}

void QMonacoEditor::setReadOnly(bool readOnly) {
    if (m_ready) {
        emit m_bridge->requestSetReadOnly(readOnly);
    }
}

QString QMonacoEditor::language() const {
    return evalJsSync(QStringLiteral(
        "window.__monacoEditor?.getModel()?.getLanguageId()")).toString();
}

QString QMonacoEditor::theme() const {
    return evalJsSync(QStringLiteral("window.__monacoTheme")).toString();
}

bool QMonacoEditor::isReadOnly() const {
    return evalJsSync(QStringLiteral(
        "!!window.__monacoEditor?.getRawOptions()?.readOnly")).toBool();
}

void QMonacoEditor::setCursorPosition(int line, int column) {
    if (m_ready) {
        emit m_bridge->requestSetCursorPosition(line, column);
    }
}

int QMonacoEditor::cursorLine() const {
    return evalJsSync(QStringLiteral(
        "window.__monacoEditor?.getPosition()?.lineNumber")).toInt();
}

int QMonacoEditor::cursorColumn() const {
    return evalJsSync(QStringLiteral(
        "window.__monacoEditor?.getPosition()?.column")).toInt();
}

QString QMonacoEditor::resourceDir() const {
    // The cache key is a hash of the resource *paths*, not contents. This is only
    // correct because Vite emits content-hashed filenames: any frontend change renames
    // a file, which changes the path list and thus the target directory.
    QByteArray hash;
    QDirIterator it(":/qmonacoeditor", QDirIterator::Subdirectories);
    QCryptographicHash hasher(QCryptographicHash::Md5);
    while (it.hasNext()) {
        hasher.addData(it.next().toUtf8());
    }
    hash = hasher.result().toHex().left(8);

    return QStandardPaths::writableLocation(QStandardPaths::TempLocation)
           + "/qmonacoeditor/" + QString::fromLatin1(hash);
}

void QMonacoEditor::extractResources() {
    const QString destDir = resourceDir();

    if (QDir(destDir).exists()) {
        qCDebug(lcMonaco) << "Resources already extracted at" << destDir;
        return;
    }

    QDirIterator it(":/qmonacoeditor", QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString srcPath = it.next();
        QFileInfo info(srcPath);
        if (!info.isFile()) {
            continue;
        }

        QString relativePath = srcPath.mid(QString(":/qmonacoeditor/").length());
        QString destPath = destDir + "/" + relativePath;

        // A failed extraction surfaces later as a blank editor, so make the root
        // cause (full disk, permissions, ...) visible in the log instead of silent.
        if (!QDir().mkpath(QFileInfo(destPath).absolutePath())) {
            qCWarning(lcMonaco) << "Failed to create directory for" << destPath;
            continue;
        }

        QFile srcFile(srcPath);
        QFile destFile(destPath);
        if (!srcFile.open(QIODevice::ReadOnly)
            || !destFile.open(QIODevice::WriteOnly)
            || destFile.write(srcFile.readAll()) < 0) {
            qCWarning(lcMonaco) << "Failed to extract" << srcPath << "->" << destPath
                                << destFile.errorString();
        }
    }
}
