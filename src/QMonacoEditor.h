#ifndef QMONACOEDITOR_H
#define QMONACOEDITOR_H

#include <QWidget>
#include <QString>
#include <QVariant>

class QWebEngineView;
class QWebChannel;
class MonacoBridge;

class QMonacoEditor : public QWidget {
    Q_OBJECT
public:
    explicit QMonacoEditor(QWidget *parent = nullptr);
    ~QMonacoEditor();

    void setText(const QString &text);
    QString text() const;

    void setLanguage(const QString &languageId);
    void setTheme(const QString &themeId);
    void setReadOnly(bool readOnly);
    bool isReadOnly() const;

    void setCursorPosition(int line, int column);
    int cursorLine() const;
    int cursorColumn() const;

signals:
    void editorReady();
    void textChanged(const QString &newText);
    void cursorPositionChanged(int line, int column);

private:
    void extractResources();
    QString resourceDir() const;
    QVariant evalJsSync(const QString &expr) const;

    QWebEngineView *m_webView = nullptr;
    QWebChannel *m_channel = nullptr;
    MonacoBridge *m_bridge = nullptr;
    bool m_ready = false;
};

#endif // QMONACOEDITOR_H
