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
    void onGetTextResult(const QString &text);

signals:
    void requestSetText(const QString &text);
    void requestGetText();

    void editorReady();
    void textChanged(const QString &text);
    void getTextResult(const QString &text);
};

#endif // MONACOBRIDGE_H
