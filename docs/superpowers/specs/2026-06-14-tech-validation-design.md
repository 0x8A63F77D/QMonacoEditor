# QMonacoEditor — Tech Validation Design

## Goal

Minimally validate the technical feasibility of embedding Monaco Editor in a Qt C++ Widget, covering three core risk areas:

1. QWebEngineView can embed and render Monaco Editor
2. Web Workers function correctly in the Qt environment (syntax highlighting depends on Workers)
3. Bidirectional C++/JS communication via QWebChannel

## Scope

Only 3 APIs: `setText`, `getText`, `textChanged` signal, plus the `editorReady` lifecycle signal. Language switching, theme switching, and cursor operations are out of scope.

## Tech Stack

- **Rendering backend:** QWebEngineView (Qt 6 only)
- **Communication layer:** QWebChannel, direct connection, no abstraction layer
- **Frontend bundling:** Vite + vite-plugin-monaco-editor
- **Build system:** CMake, auto-triggers npm/vite frontend build
- **Minimum Qt version:** Qt 6.2 (development uses Qt 6.8 LTS)
- **License:** MIT

## Project Structure

```
QMonacoEditor/
├── CMakeLists.txt              # Top-level CMake, coordinates C++ and frontend builds
├── LICENSE
├── .gitignore
│
├── src/                        # C++ library source
│   ├── CMakeLists.txt
│   ├── QMonacoEditor.h         # Public header (QWidget subclass)
│   ├── QMonacoEditor.cpp
│   ├── MonacoBridge.h          # QObject, bridge exposed to JS
│   └── MonacoBridge.cpp
│
├── web/                        # Frontend resources (Vite project)
│   ├── package.json
│   ├── vite.config.js
│   ├── src/
│   │   └── index.ts            # JS entry: Monaco init + QWebChannel bridge
│   └── index.html              # Host page
│
├── resources/                  # Vite build output directory
│   └── (vite build outputs here, CMake packages into qrc)
│
└── example/                    # Example application
    ├── CMakeLists.txt
    └── main.cpp                # Test window with buttons
```

## C++ API

### QMonacoEditor

```cpp
class QMonacoEditor : public QWidget {
    Q_OBJECT
public:
    explicit QMonacoEditor(QWidget *parent = nullptr);
    ~QMonacoEditor();

    void getText(std::function<void(const QString &)> callback);
    void setText(const QString &text);

signals:
    void editorReady();
    void textChanged(const QString &newText);

private:
    QWebEngineView *m_webView;
    MonacoBridge *m_bridge;
    QWebChannel *m_channel;
    bool m_ready = false;
};
```

- `getText` uses a callback rather than a return value because C++ → JS → C++ is asynchronous IPC
- `setText` is fire-and-forget. Calls made before `editorReady` are queued and replayed once ready
- `getText` callbacks will not fire if called before `editorReady`

### MonacoBridge

```cpp
class MonacoBridge : public QObject {
    Q_OBJECT
public:
    explicit MonacoBridge(QObject *parent = nullptr);

public slots:
    // JS → C++
    void onEditorReady();
    void onTextChanged(const QString &text);
    void onGetTextResult(const QString &text);

signals:
    // C++ → JS
    void requestSetText(const QString &text);
    void requestGetText();
};
```

## Communication Flow

### setText

```
C++ emit requestSetText("hello")
  → QWebChannel transport
    → JS receives, calls editor.setValue("hello")
```

### getText

```
C++ emit requestGetText()
  → QWebChannel transport
    → JS receives, calls editor.getValue()
      → JS calls bridge.onGetTextResult(value)
        → QWebChannel transport
          → C++ slot fires, executes callback
```

### textChanged (user input)

```
JS: editor.onDidChangeModelContent
  → JS calls bridge.onTextChanged(newText)
    → QWebChannel transport
      → C++ slot fires
        → emit QMonacoEditor::textChanged(newText)
```

## JS Responsibilities (index.ts)

1. Initialize QWebChannel, obtain bridge object reference
2. Create Monaco Editor instance
3. Connect bridge signals to Monaco API (`requestSetText` → `editor.setValue()`)
4. Connect Monaco events to bridge slots (`onDidChangeModelContent` → `bridge.onTextChanged()`)
5. Call `bridge.onEditorReady()` after initialization completes

## Monaco Resource Loading

### Loading Flow

```
Application starts
  → QMonacoEditor constructor
    → Extract qrc frontend resources to system temp directory
    → QWebEngineView::setUrl(file:///temp_dir/index.html)
      → Monaco loads normally, Workers created from file:// paths
```

### Temp Directory

- Path: `{QStandardPaths::TempLocation}/qmonacoeditor/{version_hash}/`
- Version hash is based on resource content at build time, ensuring updated files are used automatically
- Old versions are not actively cleaned up — managed by the OS
- Multiple instances share the same version directory since file contents are identical

### Why Extraction to Temp Directory Is Required

Qt Resource System (qrc://) is a virtual filesystem — browsers cannot create Web Workers from qrc:// paths. Workers function correctly under the file:// protocol.

### Vite Build Output

```
resources/
├── index.html
├── assets/
│   ├── index-xxxx.js
│   └── index-xxxx.css
└── monacoeditorwork/
    ├── editor.worker.js
    ├── json.worker.js
    ├── css.worker.js
    ├── html.worker.js
    └── ts.worker.js
```

## Build Flow

CMake `add_custom_command` automatically executes:

1. `npm install` (install frontend dependencies)
2. `npm run build` (Vite bundles Monaco into resources/)
3. `qt_add_resources` (embed resources/ into qrc)
4. Compile C++ source + link Qt WebEngine/WebChannel

Developers only need `cmake --build` — frontend build is triggered automatically. Build environment requires Node.js.

## Example Application

```
┌──────────────────────────────────┐
│  [Set Text]  [Get Text]          │  ← Button bar
├──────────────────────────────────┤
│                                  │
│       QMonacoEditor              │  ← Editor fills remaining space
│                                  │
├──────────────────────────────────┤
│  Status bar: editorReady state   │  ← QLabel
└──────────────────────────────────┘
```

- **Set Text:** calls `editor->setText("// Hello from C++\nint main() {}")`
- **Get Text:** calls `editor->getText(callback)`, displays content in QMessageBox
- **textChanged:** connected to `qDebug()` for console output
- **editorReady:** connected to status bar QLabel, displays "Editor Ready"

Validation coverage: C++ → JS (setText), JS → C++ (getText callback), JS-initiated notification to C++ (textChanged).

## Success Criteria

1. Monaco Editor renders normally in QWebEngineView, typing and editing work
2. Syntax highlighting functions correctly (proves Workers loaded successfully)
3. Clicking Set Text updates editor content
4. Clicking Get Text shows a dialog with the correct current content
5. Typing in the editor prints textChanged output to the console