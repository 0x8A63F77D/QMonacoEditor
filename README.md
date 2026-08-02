# QMonacoEditor

A Qt widget that embeds the [Monaco Editor](https://github.com/microsoft/monaco-editor)
(the code editor component used by VS Code) via Qt WebEngine, exposing a C++ API
for common editor operations.

> **Status:** early development. The API is small and may change. Developed and
> tested with Qt 6.8.3 (MSVC 2022) on Windows; other platforms are untested so far.

## Features

- `QMonacoEditor` is a plain `QWidget` — drop it into any layout
- Text get/set with a `textChanged` signal
- Syntax highlighting language switching (`setLanguage` / `language`)
- Built-in themes: `vs`, `vs-dark`, `hc-black`
- Read-only mode
- Cursor position get/set with a `cursorPositionChanged` signal
- Monaco assets are bundled into the library at build time — no files to ship
  separately, no network access at runtime

## How it works

The Monaco frontend is bundled with Vite at build time and embedded into the
static library as Qt resources. At runtime the assets are extracted to a
per-content-hash temp directory and loaded in a `QWebEngineView` (Monaco's Web
Workers cannot load from `qrc:` URLs). C++ and JavaScript communicate over
`QWebChannel`: state-changing calls go through bridge signals; reads are
synchronous evaluations against the live editor, so the editor itself is the
single source of truth.

## Requirements

- Qt 6 with the Widgets, WebEngineWidgets, and WebChannel modules
- CMake 3.19+
- Node.js / npm (used at build time to bundle the Monaco frontend)
- A C++17 compiler

## Building

```bash
cmake -B build -S . -G Ninja -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/msvc2022_64
cmake --build build
```

Run the demo app at `build/example/qmonacoeditor_example`, or the test suite
with `ctest` from the build directory.

## Usage

Add the repository as a subdirectory and link against the `qmonacoeditor`
target:

```cmake
add_subdirectory(QMonacoEditor)
target_link_libraries(my_app PRIVATE qmonacoeditor)
```

Or let CMake fetch it:

```cmake
include(FetchContent)
FetchContent_Declare(
    QMonacoEditor
    GIT_REPOSITORY https://github.com/0x8A63F77D/QMonacoEditor.git
    GIT_TAG main
)
FetchContent_MakeAvailable(QMonacoEditor)

target_link_libraries(my_app PRIVATE qmonacoeditor)
```

Either way, this library's own tests and example app are not built — they
default to off unless QMonacoEditor is the top-level project.

Two things to keep in mind when consuming the library:

- Node.js / npm must be on `PATH` when the *consuming* project is configured.
  The Monaco frontend is bundled during the consumer's build, so a missing
  `npm` aborts configuration.
- The Ninja and Visual Studio (MSBuild) generators are both exercised on
  Windows; Ninja is additionally covered by CI.

Then use the widget like any other `QWidget`:

```cpp
#include <QApplication>
#include "QMonacoEditor.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QMonacoEditor editor;
    // Setters are no-ops until the editor has loaded; set initial state
    // in the editorReady handler.
    QObject::connect(&editor, &QMonacoEditor::editorReady, [&editor] {
        editor.setLanguage("cpp");
        editor.setText("int main() { return 0; }\n");
    });

    editor.resize(800, 600);
    editor.show();
    return app.exec();
}
```

## License

This project is licensed under the [MIT License](LICENSE).

Note that it depends on Qt WebEngine, which is available under LGPL v3 (among
other Qt licenses). If you distribute applications built with this library,
link Qt dynamically or otherwise ensure your Qt licensing obligations are met.
Monaco Editor is MIT-licensed by Microsoft.
