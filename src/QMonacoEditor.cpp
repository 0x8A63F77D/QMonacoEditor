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
        if (m_hasPendingText) {
            emit m_bridge->requestSetText(m_pendingText);
            m_hasPendingText = false;
            m_pendingText.clear();
        }
        emit m_bridge->requestSetLanguage(m_language);
        emit m_bridge->requestSetTheme(m_theme);
        emit m_bridge->requestSetReadOnly(m_readOnly);
        emit editorReady();
    });

    connect(m_bridge, &MonacoBridge::textChanged, this, [this](const QString &text) {
        emit textChanged(text);
    });

    connect(m_bridge, &MonacoBridge::getTextResult, this, [this](const QString &text) {
        if (m_getTextCallback) {
            auto cb = std::move(m_getTextCallback);
            m_getTextCallback = nullptr;
            cb(text);
        }
    });

    extractResources();
    qCDebug(lcMonaco) << "Loading from:" << resourceDir();
    qCDebug(lcMonaco) << "index.html exists:" << QFile::exists(resourceDir() + "/index.html");
    m_webView->setUrl(QUrl::fromLocalFile(resourceDir() + "/index.html"));
}

QMonacoEditor::~QMonacoEditor() = default;

void QMonacoEditor::setText(const QString &text) {
    if (!m_ready) {
        m_pendingText = text;
        m_hasPendingText = true;
        return;
    }
    emit m_bridge->requestSetText(text);
}

void QMonacoEditor::getText(std::function<void(const QString &)> callback) {
    if (!m_ready) {
        return;
    }
    m_getTextCallback = std::move(callback);
    emit m_bridge->requestGetText();
}

void QMonacoEditor::setLanguage(const QString &languageId) {
    m_language = languageId;
    if (m_ready) {
        emit m_bridge->requestSetLanguage(languageId);
    }
}

void QMonacoEditor::setTheme(const QString &themeId) {
    m_theme = themeId;
    if (m_ready) {
        emit m_bridge->requestSetTheme(themeId);
    }
}

void QMonacoEditor::setReadOnly(bool readOnly) {
    m_readOnly = readOnly;
    if (m_ready) {
        emit m_bridge->requestSetReadOnly(readOnly);
    }
}

bool QMonacoEditor::isReadOnly() const {
    return m_readOnly;
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
