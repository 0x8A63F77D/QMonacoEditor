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

## Deployment size

Qt WebEngine dominates the size of a deployed application, so it is worth
knowing the numbers up front. Measured at commit `2c0a203` on Windows x64
(Release, Qt 6.8.3, deployed with `windeployqt`):

- **For an application that already ships Qt WebEngine**, the incremental cost
  of this library is 4.0 MiB — QMonacoEditor's own code plus the embedded
  Monaco assets, as linked into the example executable.
- **For a standalone deployment**, the full `windeployqt` output of the example
  app is 281.6 MiB across 136 files. Removing the software-OpenGL fallback, the
  DevTools resource pack, and all locales except `en`/`zh-CN` brings it to
  208.0 MiB; both deployments were verified to launch. Note that the
  software-OpenGL fallback is what renders on machines without working GPU
  drivers, so dropping it is a deployment-policy decision.
- Qt WebEngine (`Qt6WebEngineCore.dll`, `QtWebEngineProcess.exe`, the Chromium
  locale and resource data, and the software-OpenGL fallback) accounts for
  80.0% of the full deployment.

The per-file breakdown, the trimming details, and the commands used are in
[issue #6](https://github.com/0x8A63F77D/QMonacoEditor/issues/6).

## Requirements

- Qt 6 with the Widgets, WebEngineWidgets, and WebChannel modules
- CMake 3.19+
- Node.js / npm (used at build time to bundle the Monaco frontend) — not needed
  if you supply a [prebuilt bundle](#building-without-nodejs--npm)
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
  `npm` aborts configuration — unless you supply a prebuilt bundle, as below.
- The Ninja and Visual Studio (MSBuild) generators are both exercised on
  Windows; Ninja is additionally covered by CI.

### Building without Node.js / npm

Each published release carries a `qmonacoeditor-resources-<version>.zip` asset:
the Monaco frontend, already bundled. Point `QMONACO_PREBUILT_RESOURCES` at it
and the build uses those files as-is — npm is neither looked for nor invoked, so
it does not have to be installed at all:

```bash
cmake -B build -S . -DQMONACO_PREBUILT_RESOURCES=/abs/path/to/qmonacoeditor-resources-v0.1.0.zip
```

The value may be either:

- an **archive** (`.zip`, `.tar.gz`, …), extracted into the build tree during
  configuration — its files must sit at the archive root, not inside a wrapping
  directory; or
- a **directory** holding the bundle — for instance the `resources/` directory
  left behind by a normal npm build, which you can copy from one machine to
  another.

Either way the path must contain `index.html`; if it does not, configuration
stops with a message saying so rather than producing a library that fails at
runtime. A relative path on the `cmake` command line is resolved against the
directory you run `cmake` from, as with any other CMake `PATH` variable.

When consuming via `FetchContent`, set the variable before
`FetchContent_MakeAvailable`, using an **absolute** path:

```cmake
set(QMONACO_PREBUILT_RESOURCES "D:/bundles/qmonacoeditor-resources-v0.1.0.zip")
FetchContent_MakeAvailable(QMonacoEditor)
```

Because the bundle is yours and may change while a build tree already exists,
the resource file is rebuilt from it on every build rather than only when a
change is detected. Swapping a bundle in place is therefore picked up
immediately, at the cost of a few seconds per incremental build; the default
npm path is unaffected.

Note that the bundle is tied to the release it ships with. When you fetch `main`
or another commit, the frontend is not guaranteed to match the C++ side, so the
npm build remains the supported path there.

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
