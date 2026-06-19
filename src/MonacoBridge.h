#ifndef MONACOBRIDGE_H
#define MONACOBRIDGE_H

#include <QObject>
#include <QString>

class MonacoBridge : public QObject {
    Q_OBJECT
public:
    explicit MonacoBridge(QObject *parent = nullptr);

public slots:
    void onEditorReady();
    void onTextChanged(const QString &text);
    void onCursorPositionChanged(int line, int column);

signals:
    void requestSetText(const QString &text);
    void requestSetLanguage(const QString &languageId);
    void requestSetTheme(const QString &themeId);
    void requestSetReadOnly(bool readOnly);
    void requestSetCursorPosition(int line, int column);

    void editorReady();
    void textChanged(const QString &text);
    void cursorPositionChanged(int line, int column);
};

#endif // MONACOBRIDGE_H
