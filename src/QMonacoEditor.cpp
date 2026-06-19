#include "QMonacoEditor.h"
#include "MonacoBridge.h"

#include <QVBoxLayout>
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

Q_LOGGING_CATEGORY(lcMonaco, "qmonacoeditor", QtWarningMsg)

QMonacoEditor::QMonacoEditor(QWidget *parent)
    : QWidget(parent)
{
    Q_INIT_RESOURCE(qmonacoeditor);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_webView = new QWebEngineView(this);
    layout->addWidget(m_webView);

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
    QEventLoop loop;
    QVariant result;
    // The nested loop can process events that delete this widget (e.g. the parent window
    // closing) before runJavaScript's callback returns. QPointer goes null in that case,
    // so we detect it and avoid touching a dangling `this`.
    QPointer<const QMonacoEditor> guard(this);
    m_webView->page()->runJavaScript(expr, [&loop, &result](const QVariant &value) {
        result = value;
        loop.quit();
    });
    loop.exec();
    if (!guard) {
        return {};
    }
    return result;
}

QString QMonacoEditor::text() const {
    return evalJsSync(QStringLiteral("window.__monacoEditor.getValue()")).toString();
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

bool QMonacoEditor::isReadOnly() const {
    return evalJsSync(QStringLiteral(
        "!!window.__monacoEditor.getRawOptions().readOnly")).toBool();
}

void QMonacoEditor::setCursorPosition(int line, int column) {
    if (m_ready) {
        emit m_bridge->requestSetCursorPosition(line, column);
    }
}

int QMonacoEditor::cursorLine() const {
    return evalJsSync(QStringLiteral("window.__monacoEditor.getPosition().lineNumber")).toInt();
}

int QMonacoEditor::cursorColumn() const {
    return evalJsSync(QStringLiteral("window.__monacoEditor.getPosition().column")).toInt();
}

QString QMonacoEditor::resourceDir() const {
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
    QString destDir = resourceDir();

    QDirIterator check(":/qmonacoeditor", QDirIterator::Subdirectories);
    int qrcCount = 0;
    while (check.hasNext()) { check.next(); qrcCount++; }
    qCDebug(lcMonaco) << "qrc resources:" << qrcCount << "files, target:" << destDir;

    if (QDir(destDir).exists()) {
        qCDebug(lcMonaco) << "Resources already extracted, skipping";
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

        QDir().mkpath(QFileInfo(destPath).absolutePath());

        QFile srcFile(srcPath);
        if (srcFile.open(QIODevice::ReadOnly)) {
            QFile destFile(destPath);
            if (destFile.open(QIODevice::WriteOnly)) {
                destFile.write(srcFile.readAll());
            }
        }
    }
}
