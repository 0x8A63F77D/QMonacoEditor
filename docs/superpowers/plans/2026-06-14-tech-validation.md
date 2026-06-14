# QMonacoEditor Tech Validation — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Validate that Monaco Editor can be embedded in a Qt C++ Widget with bidirectional C++/JS communication via QWebChannel, and that Web Workers function correctly.

**Architecture:** QMonacoEditor (QWidget) embeds QWebEngineView. MonacoBridge (QObject) is exposed to JS via QWebChannel. JS side initializes Monaco Editor and wires bridge signals/slots to Monaco API calls. Monaco resources are bundled via Vite, embedded as Qt resources, and extracted to a temp directory at runtime for Worker compatibility.

**Tech Stack:** Qt 6.2+ (WebEngine, WebChannel, Widgets), Vite, monaco-editor npm package, TypeScript, CMake

**Spec:** `docs/superpowers/specs/2026-06-14-tech-validation-design.md`

---

## File Map

| File | Action | Responsibility |
|------|--------|---------------|
| `CMakeLists.txt` | Create | Top-level CMake: find Qt, add subdirectories, trigger frontend build |
| `src/CMakeLists.txt` | Create | Library target: sources, Qt resource embedding, link Qt modules |
| `src/MonacoBridge.h` | Create | QObject bridge: slots for JS→C++, signals for C++→JS |
| `src/MonacoBridge.cpp` | Create | MonacoBridge implementation |
| `src/QMonacoEditor.h` | Create | Public API: QWidget subclass with setText/getText/signals |
| `src/QMonacoEditor.cpp` | Create | Widget implementation: WebView setup, resource extraction, bridge wiring |
| `web/package.json` | Create | npm project: monaco-editor, vite, vite-plugin-monaco-editor deps |
| `web/vite.config.js` | Create | Vite config: Monaco worker plugin, output to ../resources/ |
| `web/index.html` | Create | Host page: container div, script entry |
| `web/src/index.ts` | Create | JS entry: QWebChannel init, Monaco init, bridge wiring |
| `example/CMakeLists.txt` | Create | Example executable target |
| `example/main.cpp` | Create | Test window with Set Text / Get Text buttons and status bar |
| `.gitignore` | Modify | Add node_modules/, resources/, build/ |

---

### Task 1: Vite Frontend Project

Set up the Vite project that bundles Monaco Editor into static files.

**Files:**
- Create: `web/package.json`
- Create: `web/vite.config.js`
- Create: `web/index.html`
- Create: `web/src/index.ts`

- [ ] **Step 1: Create package.json**

```json
{
  "name": "qmonacoeditor-web",
  "private": true,
  "scripts": {
    "build": "vite build",
    "dev": "vite"
  },
  "dependencies": {
    "monaco-editor": "^0.52.0"
  },
  "devDependencies": {
    "vite": "^6.0.0",
    "vite-plugin-monaco-editor": "^1.1.0",
    "typescript": "^5.7.0"
  }
}
```

- [ ] **Step 2: Create vite.config.js**

```js
import { defineConfig } from "vite";
import monacoEditorPlugin from "vite-plugin-monaco-editor";
import path from "path";

export default defineConfig({
  plugins: [
    monacoEditorPlugin({
      languageWorkers: ["editorWorkerService", "json", "css", "html", "typescript"],
    }),
  ],
  build: {
    outDir: path.resolve(__dirname, "../resources"),
    emptyOutDir: true,
  },
  base: "./",
});
```

Key: `base: "./"` ensures all asset URLs are relative, which is required for `file://` loading. `outDir` points to `../resources/` so CMake can find the build output.

- [ ] **Step 3: Create index.html**

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>QMonacoEditor</title>
  <style>
    html, body {
      margin: 0;
      padding: 0;
      width: 100%;
      height: 100%;
      overflow: hidden;
    }
    #editor-container {
      width: 100%;
      height: 100%;
    }
  </style>
</head>
<body>
  <div id="editor-container"></div>
  <script type="module" src="/src/index.ts"></script>
</body>
</html>
```

- [ ] **Step 4: Create index.ts with Monaco init (no bridge yet)**

Create a minimal `web/src/index.ts` that just creates a Monaco Editor instance to verify Vite bundling works. The QWebChannel bridge wiring will be added in Task 4.

```ts
import * as monaco from "monaco-editor";

const editor = monaco.editor.create(
  document.getElementById("editor-container")!,
  {
    value: "// QMonacoEditor\n",
    language: "javascript",
    theme: "vs-dark",
    automaticLayout: true,
  }
);

console.log("Monaco editor created:", editor.getId());
```

- [ ] **Step 5: Install dependencies and build**

Run from the `web/` directory:

```bash
cd web && npm install && npm run build
```

Expected: Build succeeds. `resources/` directory contains `index.html`, `assets/` with JS/CSS bundles, and `monacoeditorwork/` with worker JS files.

- [ ] **Step 6: Verify build output structure**

```bash
find ../resources -type f | head -20
```

Expected output should show:
- `resources/index.html`
- `resources/assets/index-*.js`
- `resources/assets/index-*.css`
- `resources/monacoeditorwork/editor.worker.js`
- (other worker files)

- [ ] **Step 7: Commit**

```bash
git add web/ resources/
git commit -m "feat: add Vite project for Monaco Editor bundling"
```

Note: we commit `resources/` so the C++ build works without requiring Node.js at CMake time during initial development. This can be revisited later.

---

### Task 2: MonacoBridge C++ Class

The QObject that acts as the communication bridge between C++ and JS via QWebChannel.

**Files:**
- Create: `src/MonacoBridge.h`
- Create: `src/MonacoBridge.cpp`

- [ ] **Step 1: Create MonacoBridge.h**

```cpp
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
```

Note: MonacoBridge re-emits internal signals (`editorReady`, `textChanged`, `getTextResult`) so QMonacoEditor can connect to them without reaching into slots directly.

- [ ] **Step 2: Create MonacoBridge.cpp**

```cpp
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
```

- [ ] **Step 3: Commit**

```bash
git add src/MonacoBridge.h src/MonacoBridge.cpp
git commit -m "feat: add MonacoBridge QObject for C++/JS communication"
```

---

### Task 3: QMonacoEditor Widget

The public-facing QWidget that users interact with.

**Files:**
- Create: `src/QMonacoEditor.h`
- Create: `src/QMonacoEditor.cpp`

- [ ] **Step 1: Create QMonacoEditor.h**

```cpp
#ifndef QMONACOEDITOR_H
#define QMONACOEDITOR_H

#include <QWidget>
#include <QString>
#include <functional>

class QWebEngineView;
class QWebChannel;
class MonacoBridge;

class QMonacoEditor : public QWidget {
    Q_OBJECT
public:
    explicit QMonacoEditor(QWidget *parent = nullptr);
    ~QMonacoEditor();

    void setText(const QString &text);
    void getText(std::function<void(const QString &)> callback);

signals:
    void editorReady();
    void textChanged(const QString &newText);

private:
    void extractResources();
    QString resourceDir() const;

    QWebEngineView *m_webView = nullptr;
    QWebChannel *m_channel = nullptr;
    MonacoBridge *m_bridge = nullptr;
    bool m_ready = false;
    QString m_pendingText;
    bool m_hasPendingText = false;
    std::function<void(const QString &)> m_getTextCallback;
};

#endif // QMONACOEDITOR_H
```

- [ ] **Step 2: Create QMonacoEditor.cpp**

```cpp
#include "QMonacoEditor.h"
#include "MonacoBridge.h"

#include <QVBoxLayout>
#include <QWebEngineView>
#include <QWebChannel>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QUrl>

QMonacoEditor::QMonacoEditor(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_webView = new QWebEngineView(this);
    layout->addWidget(m_webView);

    m_bridge = new MonacoBridge(this);
    m_channel = new QWebChannel(this);
    m_channel->registerObject(QStringLiteral("bridge"), m_bridge);
    m_webView->page()->setWebChannel(m_channel);

    connect(m_bridge, &MonacoBridge::editorReady, this, [this]() {
        m_ready = true;
        if (m_hasPendingText) {
            emit m_bridge->requestSetText(m_pendingText);
            m_hasPendingText = false;
            m_pendingText.clear();
        }
        emit editorReady();
    });

    connect(m_bridge, &MonacoBridge::textChanged, this, [this](const QString &text) {
        emit textChanged(text);
    });

    connect(m_bridge, &MonacoBridge::getTextResult, this, [this](const QString &text) {
        if (m_getTextCallback) {
            auto cb = std::move(m_getTextCallback);
            m_getTextCallback = nullptr;
            cb(text);
        }
    });

    extractResources();
    m_webView->setUrl(QUrl::fromLocalFile(resourceDir() + "/index.html"));
}

QMonacoEditor::~QMonacoEditor() = default;

void QMonacoEditor::setText(const QString &text) {
    if (!m_ready) {
        m_pendingText = text;
        m_hasPendingText = true;
        return;
    }
    emit m_bridge->requestSetText(text);
}

void QMonacoEditor::getText(std::function<void(const QString &)> callback) {
    if (!m_ready) {
        return;
    }
    m_getTextCallback = std::move(callback);
    emit m_bridge->requestGetText();
}

QString QMonacoEditor::resourceDir() const {
    QByteArray hash;
    QDirIterator it(":/qmonacoeditor", QDirIterator::Subdirectories);
    QCryptographicHash hasher(QCryptographicHash::Md5);
    while (it.hasNext()) {
        hasher.addData(it.next().toUtf8());
    }
    hash = hasher.result().toHex().left(8);

    return QStandardPaths::writableLocation(QStandardPaths::TempLocation)
           + "/qmonacoeditor/" + QString::fromLatin1(hash);
}

void QMonacoEditor::extractResources() {
    QString destDir = resourceDir();
    if (QDir(destDir).exists()) {
        return;
    }

    QDirIterator it(":/qmonacoeditor", QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString srcPath = it.next();
        QFileInfo info(srcPath);
        if (!info.isFile()) {
            continue;
        }

        QString relativePath = srcPath.mid(QString(":/qmonacoeditor/").length());
        QString destPath = destDir + "/" + relativePath;

        QDir().mkpath(QFileInfo(destPath).absolutePath());

        QFile srcFile(srcPath);
        if (srcFile.open(QIODevice::ReadOnly)) {
            QFile destFile(destPath);
            if (destFile.open(QIODevice::WriteOnly)) {
                destFile.write(srcFile.readAll());
            }
        }
    }
}
```

- [ ] **Step 3: Commit**

```bash
git add src/QMonacoEditor.h src/QMonacoEditor.cpp
git commit -m "feat: add QMonacoEditor widget with resource extraction"
```

---

### Task 4: JS Bridge Wiring (index.ts)

Update the JS entry point to connect QWebChannel to Monaco Editor API calls.

**Files:**
- Modify: `web/src/index.ts`

- [ ] **Step 1: Replace index.ts with full bridge implementation**

Replace the entire contents of `web/src/index.ts`:

```ts
import * as monaco from "monaco-editor";

declare global {
  interface Window {
    QWebChannel: any;
    qt: { webChannelTransport: any };
  }
}

function initEditor(bridge: any): void {
  const editor = monaco.editor.create(
    document.getElementById("editor-container")!,
    {
      value: "",
      language: "javascript",
      theme: "vs-dark",
      automaticLayout: true,
    }
  );

  bridge.requestSetText.connect((text: string) => {
    editor.setValue(text);
  });

  bridge.requestGetText.connect(() => {
    const value = editor.getValue();
    bridge.onGetTextResult(value);
  });

  editor.onDidChangeModelContent(() => {
    const value = editor.getValue();
    bridge.onTextChanged(value);
  });

  bridge.onEditorReady();
}

const channel = new window.QWebChannel(
  window.qt.webChannelTransport,
  (channel: any) => {
    const bridge = channel.objects.bridge;
    initEditor(bridge);
  }
);
```

Key details:
- `window.qt.webChannelTransport` is provided by QWebEngineView when a QWebChannel is set on the page
- `bridge.requestSetText.connect()` subscribes to C++ signals
- `bridge.onGetTextResult()` and `bridge.onTextChanged()` call C++ slots
- `bridge.onEditorReady()` is called after Monaco is fully initialized

- [ ] **Step 2: Add qwebchannel.js to index.html**

Update `web/index.html` to load the QWebChannel JS library before the app script. Replace the entire file:

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>QMonacoEditor</title>
  <style>
    html, body {
      margin: 0;
      padding: 0;
      width: 100%;
      height: 100%;
      overflow: hidden;
    }
    #editor-container {
      width: 100%;
      height: 100%;
    }
  </style>
  <script src="qrc:///qtwebchannel/qwebchannel.js"></script>
</head>
<body>
  <div id="editor-container"></div>
  <script type="module" src="/src/index.ts"></script>
</body>
</html>
```

Note: `qrc:///qtwebchannel/qwebchannel.js` is a built-in resource provided by Qt WebEngine at runtime. It provides the `QWebChannel` JS class. This URL works even when the page itself is loaded from `file://` because QWebEngine resolves `qrc://` internally.

- [ ] **Step 3: Rebuild frontend**

```bash
cd web && npm run build
```

Expected: Build succeeds. `resources/` updated with new JS bundle containing the bridge code.

- [ ] **Step 4: Commit**

```bash
git add web/src/index.ts web/index.html resources/
git commit -m "feat: wire QWebChannel bridge to Monaco Editor API"
```

---

### Task 5: CMake Build System

Set up CMake to build the C++ library, auto-trigger frontend build, and embed resources.

**Files:**
- Create: `CMakeLists.txt`
- Create: `src/CMakeLists.txt`
- Create: `example/CMakeLists.txt`
- Modify: `.gitignore`

- [ ] **Step 1: Create top-level CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.19)
project(QMonacoEditor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets WebEngineWidgets WebChannel)

add_subdirectory(src)
add_subdirectory(example)
```

- [ ] **Step 2: Create src/CMakeLists.txt**

```cmake
# --- Frontend build via npm/vite ---
set(WEB_DIR "${CMAKE_SOURCE_DIR}/web")
set(RESOURCES_DIR "${CMAKE_SOURCE_DIR}/resources")
set(VITE_STAMP "${CMAKE_CURRENT_BINARY_DIR}/vite_build.stamp")

find_program(NPM_EXECUTABLE npm REQUIRED)

add_custom_command(
    OUTPUT "${VITE_STAMP}"
    COMMAND "${NPM_EXECUTABLE}" install
    COMMAND "${NPM_EXECUTABLE}" run build
    COMMAND ${CMAKE_COMMAND} -E touch "${VITE_STAMP}"
    WORKING_DIRECTORY "${WEB_DIR}"
    DEPENDS "${WEB_DIR}/package.json"
            "${WEB_DIR}/vite.config.js"
            "${WEB_DIR}/index.html"
            "${WEB_DIR}/src/index.ts"
    COMMENT "Building Monaco Editor frontend resources"
)

add_custom_target(web_build DEPENDS "${VITE_STAMP}")

# --- Collect resource files dynamically ---
file(GLOB_RECURSE RESOURCE_FILES CONFIGURE_DEPENDS "${RESOURCES_DIR}/*")

# --- Qt resource file ---
set(QRC_FILE "${CMAKE_CURRENT_BINARY_DIR}/qmonacoeditor.qrc")
file(WRITE "${QRC_FILE}" "<RCC>\n  <qresource prefix=\"/qmonacoeditor\">\n")
foreach(FILE ${RESOURCE_FILES})
    file(RELATIVE_PATH REL_PATH "${RESOURCES_DIR}" "${FILE}")
    file(APPEND "${QRC_FILE}" "    <file alias=\"${REL_PATH}\">${FILE}</file>\n")
endforeach()
file(APPEND "${QRC_FILE}" "  </qresource>\n</RCC>\n")

# --- Library target ---
add_library(qmonacoeditor STATIC
    QMonacoEditor.h
    QMonacoEditor.cpp
    MonacoBridge.h
    MonacoBridge.cpp
    "${QRC_FILE}"
)

add_dependencies(qmonacoeditor web_build)

target_include_directories(qmonacoeditor PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(qmonacoeditor PUBLIC
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::WebChannel
)
```

Key details:
- `add_custom_command` runs `npm install` and `npm run build` only when frontend source files change (tracked by stamp file)
- The `.qrc` file is generated dynamically from whatever is in `resources/`, so it automatically picks up all Vite output files
- Library is `STATIC` for simplicity during tech validation

- [ ] **Step 3: Create example/CMakeLists.txt**

```cmake
add_executable(qmonacoeditor_example main.cpp)

target_link_libraries(qmonacoeditor_example PRIVATE qmonacoeditor)
```

- [ ] **Step 4: Update .gitignore**

Append to the existing `.gitignore`:

```
# Node.js
node_modules/

# Build output
build/
cmake-build-*/

# IDE
.idea/
.vscode/
*.user

# Vite build output (regenerated by npm run build)
resources/
```

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/CMakeLists.txt example/CMakeLists.txt .gitignore
git commit -m "feat: add CMake build system with auto frontend build"
```

---

### Task 6: Example Application

The test window with buttons to exercise setText/getText/textChanged.

**Files:**
- Create: `example/main.cpp`

- [ ] **Step 1: Create main.cpp**

```cpp
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
```

- [ ] **Step 2: Commit**

```bash
git add example/main.cpp
git commit -m "feat: add example app with Set Text / Get Text buttons"
```

---

### Task 7: Build and Validate

Build everything and verify the 5 success criteria from the spec.

**Files:** None (build and manual test only)

- [ ] **Step 1: Configure CMake**

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.0/msvc2022_64
```

Expected: Configuration succeeds, finds Qt6 Widgets, WebEngineWidgets, WebChannel. npm is found.

- [ ] **Step 2: Build**

```bash
cmake --build build
```

Expected: Frontend builds (npm install + vite build), qrc is generated, C++ compiles and links successfully.

- [ ] **Step 3: Run the example**

```bash
./build/example/qmonacoeditor_example
```

Verify all 5 success criteria:

1. Monaco Editor renders in the window with syntax-highlighted code — **confirms QWebEngineView embedding works**
2. The editor shows syntax highlighting colors (not plain text) — **confirms Workers loaded successfully**
3. Click "Set Text" → editor content changes to the C++ snippet — **confirms C++ → JS communication**
4. Click "Get Text" → message box shows current editor content — **confirms JS → C++ communication**
5. Type in the editor → console prints `textChanged:` lines — **confirms JS event → C++ signal**

- [ ] **Step 4: Check status label**

The bottom label should change from "Waiting for editor..." to "Editor Ready" after Monaco finishes loading — **confirms editorReady lifecycle signal**.

- [ ] **Step 5: Commit any fixes**

If any issues were found and fixed during validation, commit them:

```bash
git add -A
git commit -m "fix: address issues found during tech validation"
```

If no fixes needed, skip this step.
