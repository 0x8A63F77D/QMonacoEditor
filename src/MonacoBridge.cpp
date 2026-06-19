#include "MonacoBridge.h"

MonacoBridge::MonacoBridge(QObject *parent)
    : QObject(parent) {}

void MonacoBridge::onEditorReady() {
    emit editorReady();
}

void MonacoBridge::onTextChanged(const QString &text) {
    emit textChanged(text);
}

void MonacoBridge::onCursorPositionChanged(int line, int column) {
    emit cursorPositionChanged(line, column);
}
