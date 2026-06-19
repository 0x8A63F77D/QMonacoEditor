#include "QMonacoEditor.h"

#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QDebug>
#include <QComboBox>
#include <QCheckBox>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle("QMonacoEditor - MVP Demo");
    window.resize(800, 600);

    auto *central = new QWidget;
    auto *mainLayout = new QVBoxLayout(central);

    auto *buttonBar = new QHBoxLayout;

    auto *setTextBtn = new QPushButton("Set Text");
    auto *getTextBtn = new QPushButton("Get Text");
    buttonBar->addWidget(setTextBtn);
    buttonBar->addWidget(getTextBtn);

    buttonBar->addSpacing(12);

    auto *langCombo = new QComboBox;
    langCombo->addItems({"cpp", "javascript", "python", "json", "html", "css"});
    buttonBar->addWidget(new QLabel("Language:"));
    buttonBar->addWidget(langCombo);

    auto *themeCombo = new QComboBox;
    themeCombo->addItems({"vs-dark", "vs", "hc-black"});
    buttonBar->addWidget(new QLabel("Theme:"));
    buttonBar->addWidget(themeCombo);

    auto *readOnlyCheck = new QCheckBox("Read Only");
    buttonBar->addWidget(readOnlyCheck);

    buttonBar->addStretch();
    mainLayout->addLayout(buttonBar);

    auto *editor = new QMonacoEditor;
    mainLayout->addWidget(editor, 1);

    auto *statusLabel = new QLabel("Waiting for editor...");
    mainLayout->addWidget(statusLabel);

    QObject::connect(editor, &QMonacoEditor::editorReady, [statusLabel]() {
        statusLabel->setText("Editor Ready");
    });

    QObject::connect(editor, &QMonacoEditor::cursorPositionChanged, [statusLabel](int line, int column) {
        statusLabel->setText(QString("Ln %1, Col %2").arg(line).arg(column));
    });

    QObject::connect(editor, &QMonacoEditor::textChanged, [](const QString &text) {
        qDebug() << "textChanged:" << text.left(100);
    });

    QObject::connect(setTextBtn, &QPushButton::clicked, [editor, langCombo]() {
        QString lang = langCombo->currentText();
        QString sample;
        if (lang == "python") {
            sample = "# Hello from C++\ndef main():\n    print(\"Hello, World!\")\n\nif __name__ == \"__main__\":\n    main()\n";
        } else if (lang == "javascript") {
            sample = "// Hello from C++\nfunction main() {\n    console.log(\"Hello, World!\");\n}\n\nmain();\n";
        } else if (lang == "json") {
            sample = "{\n    \"message\": \"Hello from C++\",\n    \"version\": 1\n}\n";
        } else {
            sample = "// Hello from C++\nint main() {\n    return 0;\n}\n";
        }
        editor->setText(sample);
    });

    QObject::connect(getTextBtn, &QPushButton::clicked, [editor]() {
        editor->getText([](const QString &text) {
            QMessageBox::information(nullptr, "Editor Content", text);
        });
    });

    QObject::connect(langCombo, &QComboBox::currentTextChanged, [editor](const QString &lang) {
        editor->setLanguage(lang);
    });

    QObject::connect(themeCombo, &QComboBox::currentTextChanged, [editor](const QString &theme) {
        editor->setTheme(theme);
    });

    QObject::connect(readOnlyCheck, &QCheckBox::toggled, [editor](bool checked) {
        editor->setReadOnly(checked);
    });

    window.setCentralWidget(central);
    window.show();

    return app.exec();
}
