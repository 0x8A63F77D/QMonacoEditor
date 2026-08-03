#ifndef QMONACOEDITOR_H
#define QMONACOEDITOR_H

#include <QWidget>
#include <QString>
#include <QVariant>

class QWebEngineView;
class QWebChannel;
class MonacoBridge;

/**
 * @brief A Qt widget that embeds the Monaco Editor (the editor behind VS Code).
 *
 * QMonacoEditor hosts Monaco inside a QWebEngineView and bridges it to C++ via
 * QWebChannel. The embedded editor (JavaScript) is the single source of truth for
 * all editor state; this class holds no mirrored copy of the text, language, theme,
 * or cursor.
 *
 * Two communication channels are used by design:
 * - **Commands and events** (set*, change notifications) travel over QWebChannel
 *   asynchronously, surfaced as Qt signals/slots.
 * - **Synchronous reads** (text(), isReadOnly(), cursorLine(), cursorColumn()) block
 *   on a nested event loop until the editor replies, so callers get a value inline.
 *
 * @note Setters are no-ops until the editor has loaded. Wait for editorReady() (or set
 *       initial state inside an editorReady() slot) before calling them; calls made
 *       earlier are silently dropped rather than buffered.
 */
class QMonacoEditor : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Who wins when a host QAction and the embedded editor claim the same chord.
     *
     * Qt asks the focused widget chain (via QEvent::ShortcutOverride) whether it wants a
     * chord before triggering the matching QAction. By default the web view claims only
     * what Qt considers a *text-editing* key (undo, redo, cut/copy/paste, select-all, the
     * move/select family, ...), so every other Monaco binding -- Ctrl+F, Ctrl+H, Ctrl+D,
     * Ctrl+/, F12 ... -- silently loses to a host QAction on the same chord. This enum
     * overrides that split.
     *
     * @note Qt::ShortcutContext has no influence on this arbitration; it only decides
     *       whether a QAction is a candidate at all.
     */
    enum class ShortcutPolicy {
        /// Qt's built-in behaviour, unchanged: the editor keeps text-editing chords,
        /// the host wins everything else. The default, so existing code is unaffected.
        QtDefault,
        /// Host QActions always win: no chord that matches one is delivered to the editor.
        HostShortcutsWin,
        /// The editor gets first refusal on every chord. Chords Monaco does not bind
        /// (Ctrl+S, for example) are still reported unhandled by the render process and
        /// forwarded back to the Qt parent chain, so host QActions on those keep firing.
        EditorFirst,
    };
    Q_ENUM(ShortcutPolicy)

    /**
     * @brief Constructs the widget and begins loading Monaco asynchronously.
     *
     * The editor is not usable immediately; the API becomes live once editorReady()
     * is emitted.
     * @param parent Parent widget, passed to QWidget.
     */
    explicit QMonacoEditor(QWidget *parent = nullptr);
    ~QMonacoEditor();

    /**
     * @brief Replaces the entire editor content.
     * @param text The new content.
     * @note No-op if called before editorReady().
     */
    void setText(const QString &text);

    /**
     * @brief Returns the current editor content.
     *
     * Synchronous: blocks on a nested event loop until the editor replies.
     * @return The full text, or an empty string if called before editorReady().
     */
    QString text() const;

    /**
     * @brief Sets the syntax-highlighting language for the current model.
     * @param languageId A Monaco language id, e.g. "cpp", "javascript", "python", "json".
     * @note No-op if called before editorReady().
     */
    void setLanguage(const QString &languageId);

    /**
     * @brief Returns the language id of the current model.
     *
     * Synchronous: blocks on a nested event loop until the editor replies.
     * @return A Monaco language id (e.g. "cpp"), or an empty string before editorReady().
     */
    QString language() const;

    /**
     * @brief Sets the editor color theme. Themes are global to all Monaco instances.
     * @param themeId One of "vs", "vs-dark", or "hc-black".
     * @note No-op if called before editorReady().
     */
    void setTheme(const QString &themeId);

    /**
     * @brief Returns the current editor theme id.
     *
     * Synchronous: blocks on a nested event loop until the editor replies.
     * @return "vs", "vs-dark", or "hc-black"; empty string before editorReady().
     */
    QString theme() const;

    /**
     * @brief Toggles read-only mode.
     * @param readOnly true to make the editor non-editable.
     * @note No-op if called before editorReady().
     */
    void setReadOnly(bool readOnly);

    /**
     * @brief Returns whether the editor is currently read-only.
     *
     * Synchronous: blocks on a nested event loop until the editor replies.
     * @return The live read-only state, or false if called before editorReady().
     */
    bool isReadOnly() const;

    /**
     * @brief Moves the cursor and gives the editor focus.
     * @param line   1-based line number.
     * @param column 1-based column number.
     * @note No-op if called before editorReady().
     */
    void setCursorPosition(int line, int column);

    /**
     * @brief Returns the cursor's current line.
     *
     * Synchronous: blocks on a nested event loop until the editor replies.
     * @return 1-based line number, or 0 if called before editorReady().
     */
    int cursorLine() const;

    /**
     * @brief Returns the cursor's current column.
     *
     * Synchronous: blocks on a nested event loop until the editor replies.
     * @return 1-based column number, or 0 if called before editorReady().
     */
    int cursorColumn() const;

    /**
     * @brief Chooses who wins chords claimed by both a host QAction and the editor.
     *
     * Takes effect immediately and applies to every subsequent key press; unlike the
     * other setters it does not need the editor to be loaded.
     *
     * @param policy The arbitration policy; see ShortcutPolicy for what each one does.
     * @note This governs the **Qt-side** arbitration only -- whether a chord is delivered
     *       to the web page at all. It does not add, remove or rebind anything inside
     *       Monaco, and it has no say over chords Chromium handles natively once the key
     *       has reached the page.
     */
    void setShortcutPolicy(ShortcutPolicy policy);

    /**
     * @brief Returns the current shortcut arbitration policy.
     * @return The active policy; ShortcutPolicy::QtDefault unless it has been changed.
     */
    ShortcutPolicy shortcutPolicy() const;

signals:
    /**
     * @brief Emitted once when Monaco has finished loading and the API is live.
     *
     * Until this fires, setters are no-ops and getters return defaults. Use this to
     * push initial state (text, language, theme, ...).
     */
    void editorReady();

    /**
     * @brief Emitted whenever the editor content changes, for any reason.
     * @param newText The full content after the change.
     */
    void textChanged(const QString &newText);

    /**
     * @brief Emitted whenever the cursor moves.
     * @param line   1-based line number.
     * @param column 1-based column number.
     */
    void cursorPositionChanged(int line, int column);

protected:
    /**
     * @brief Applies the shortcut policy to QEvent::ShortcutOverride on the web view.
     *
     * Reimplemented to arbitrate shortcuts; it does not filter anything else. Subclasses
     * that reimplement it must call this base implementation for events they do not
     * consume, or setShortcutPolicy() stops having any effect.
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /// Installs the shortcut filter on @p target and everything currently below it.
    void watchForShortcuts(QObject *target);
    /// True if @p object is the web view or currently sits below it in the object tree.
    bool isInsideWebView(const QObject *object) const;
    /// Extracts the bundled web assets from the qrc into a temp dir for loading.
    void extractResources();
    /// Returns the per-build temp directory holding the extracted web assets.
    QString resourceDir() const;
    /// Evaluates a JS expression and blocks until it returns; the backbone of all getters.
    QVariant evalJsSync(const QString &expr) const;

    QWebEngineView *m_webView = nullptr;
    QWebChannel *m_channel = nullptr;
    MonacoBridge *m_bridge = nullptr;
    bool m_ready = false;  ///< True once editorReady() has fired; gates all API calls.
    ShortcutPolicy m_shortcutPolicy = ShortcutPolicy::QtDefault;
};

#endif // QMONACOEDITOR_H
