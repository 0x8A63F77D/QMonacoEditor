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

    void setCursorPosition(int line, int column);
    void getCursorPosition(std::function<void(int line, int column)> callback);

signals:
    void editorReady();
    void textChanged(const QString &newText);
    void cursorPositionChanged(int line, int column);

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
    std::function<void(int, int)> m_getCursorCallback;
};

#endif // QMONACOEDITOR_H
