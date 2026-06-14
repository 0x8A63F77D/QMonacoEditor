#include "QMonacoEditor.h"

#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QDebug>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle("QMonacoEditor - Tech Validation");
    window.resize(800, 600);

    auto *central = new QWidget;
    auto *mainLayout = new QVBoxLayout(central);

    auto *buttonBar = new QHBoxLayout;
    auto *setTextBtn = new QPushButton("Set Text");
    auto *getTextBtn = new QPushButton("Get Text");
    buttonBar->addWidget(setTextBtn);
    buttonBar->addWidget(getTextBtn);
    buttonBar->addStretch();
    mainLayout->addLayout(buttonBar);

    auto *editor = new QMonacoEditor;
    mainLayout->addWidget(editor, 1);

    auto *statusLabel = new QLabel("Waiting for editor...");
    mainLayout->addWidget(statusLabel);

    QObject::connect(editor, &QMonacoEditor::editorReady, [statusLabel]() {
        statusLabel->setText("Editor Ready");
    });

    QObject::connect(editor, &QMonacoEditor::textChanged, [](const QString &text) {
        qDebug() << "textChanged:" << text.left(100);
    });

    QObject::connect(setTextBtn, &QPushButton::clicked, [editor]() {
        editor->setText("// Hello from C++\nint main() {\n    return 0;\n}\n");
    });

    QObject::connect(getTextBtn, &QPushButton::clicked, [editor]() {
        editor->getText([](const QString &text) {
            QMessageBox::information(nullptr, "Editor Content", text);
        });
    });

    window.setCentralWidget(central);
    window.show();

    return app.exec();
}
