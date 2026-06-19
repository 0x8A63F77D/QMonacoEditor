#include "MonacoBridge.h"

MonacoBridge::MonacoBridge(QObject *parent)
    : QObject(parent) {}

void MonacoBridge::onEditorReady() {
    emit editorReady();
}

void MonacoBridge::onTextChanged(const QString &text) {
    emit textChanged(text);
}

void MonacoBridge::onGetTextResult(const QString &text) {
    emit getTextResult(text);
}

void MonacoBridge::onCursorPositionChanged(int line, int column) {
    emit cursorPositionChanged(line, column);
}

void MonacoBridge::onGetCursorPositionResult(int line, int column) {
    emit getCursorPositionResult(line, column);
}
