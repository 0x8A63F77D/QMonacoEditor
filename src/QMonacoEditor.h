#ifndef QMONACOEDITOR_H
#define QMONACOEDITOR_H

#include <QWidget>
#include <QString>
#include <functional>

class QWebEngineView;
class QWebChannel;
class MonacoBridge;

class QMonacoEditor : public QWidget {
    Q_OBJECT
public:
    explicit QMonacoEditor(QWidget *parent = nullptr);
    ~QMonacoEditor();

    void setText(const QString &text);
    void getText(std::function<void(const QString &)> callback);

    void setLanguage(const QString &languageId);
    void setTheme(const QString &themeId);
    void setReadOnly(bool readOnly);
    bool isReadOnly() const;

signals:
    void editorReady();
    void textChanged(const QString &newText);

private:
    void extractResources();
    QString resourceDir() const;

    QWebEngineView *m_webView = nullptr;
    QWebChannel *m_channel = nullptr;
    MonacoBridge *m_bridge = nullptr;
    bool m_ready = false;
    QString m_pendingText;
    bool m_hasPendingText = false;
    std::function<void(const QString &)> m_getTextCallback;
    QString m_language = QStringLiteral("cpp");
    QString m_theme = QStringLiteral("vs-dark");
    bool m_readOnly = false;
};

#endif // QMONACOEDITOR_H
