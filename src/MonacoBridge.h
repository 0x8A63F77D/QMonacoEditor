#ifndef MONACOBRIDGE_H
#define MONACOBRIDGE_H

#include <QObject>
#include <QString>

/**
 * @brief Internal QWebChannel bridge object shared between C++ and the Monaco JS layer.
 *
 * Registered on the channel as "bridge". Naming is from the JS side's perspective:
 * - `requestSet*` signals are emitted by C++ and connected to in JS (C++ → JS commands).
 * - `on*` slots are called from JS and re-emitted as the matching plain signal, which
 *   QMonacoEditor forwards to its own public signals (JS → C++ events).
 *
 * This type is an implementation detail and not part of the public API.
 */
class MonacoBridge : public QObject {
    Q_OBJECT
public:
    explicit MonacoBridge(QObject *parent = nullptr);

public slots:
    /// Called from JS once Monaco has loaded; re-emitted as editorReady().
    void onEditorReady();
    /// Called from JS on every content change; re-emitted as textChanged().
    /// @param text The full editor content after the change.
    void onTextChanged(const QString &text);
    /// Called from JS whenever the cursor moves; re-emitted as cursorPositionChanged().
    /// @param line   1-based line number.
    /// @param column 1-based column number.
    void onCursorPositionChanged(int line, int column);

signals:
    // --- C++ → JS commands. Emitted by QMonacoEditor, connected to on the JS side. ---

    /// Requests JS to replace the entire editor content.
    void requestSetText(const QString &text);
    /// Requests JS to set the current model's language (e.g. "cpp", "python").
    void requestSetLanguage(const QString &languageId);
    /// Requests JS to set the global editor theme ("vs", "vs-dark", "hc-black").
    void requestSetTheme(const QString &themeId);
    /// Requests JS to toggle read-only mode.
    void requestSetReadOnly(bool readOnly);
    /// Requests JS to move the cursor to (line, column), 1-based, and focus the editor.
    void requestSetCursorPosition(int line, int column);

    // --- JS → C++ events. Re-emitted from the on* slots; forwarded by QMonacoEditor. ---

    /// Fired once when the editor finishes loading.
    void editorReady();
    /// Fired on every content change, carrying the full text.
    void textChanged(const QString &text);
    /// Fired whenever the cursor moves, carrying 1-based line/column.
    void cursorPositionChanged(int line, int column);
};

#endif // MONACOBRIDGE_H
