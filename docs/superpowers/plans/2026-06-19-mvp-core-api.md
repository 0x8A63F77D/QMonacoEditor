# MVP Core API Implementation Plan

> ⚠️ **SUPERSEDED — historical record.** This plan was executed, but the read/getter API
> design changed substantially during implementation. The descriptions below (callback
> getters, JS→C++ result signals/slots, and C++-side state caching) do **not** match the
> shipped code. They are kept as-is to preserve the planning history. For the final design see:
> - `src/QMonacoEditor.h` — the actual public API
> - memory `cpp-js-bridge-design.md` — the final architecture rationale
>
> **Key divergences from this plan:**
> - **Getters are synchronous, not callback-based.** `getText(cb)` / `getCursorPosition(cb)`
>   became `text()`, `cursorLine()`, `cursorColumn()` — blocking reads via `evalJsSync`
>   (`runJavaScript` + nested `QEventLoop`). `isReadOnly()` reads live too.
> - **No JS→C++ result round-trip.** `requestGetText`, `requestGetCursorPosition`,
>   `onGetCursorPositionResult`, `getCursorPositionResult` were never shipped.
> - **No C++-side state caching.** `m_language`/`m_theme`/`m_readOnly`/`m_pendingText` were
>   removed; the editor (JS) is the single source of truth, consistency over convenience.
> - **Setters are no-ops before `editorReady`** (no pending buffer); set initial state in the
>   `editorReady` slot.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend QMonacoEditor with the remaining MVP API surface — language switching, theme switching, read-only mode, and cursor position — building on the validated QWebChannel bridge.

**Architecture:** Each new API follows the established pattern: C++ public method → MonacoBridge signal → JS handler. Bidirectional APIs (cursor position) also add JS→C++ slots. C++ stores desired state locally; if set before `editorReady`, it is pushed on ready. All new properties sync to JS in the `editorReady` handler.

**Tech Stack:** Qt 6 (C++17), QWebChannel, Monaco Editor API, Vite/TypeScript

---

## File Map

| File | Role | Changes |
|------|------|---------|
| `src/MonacoBridge.h` | QObject IPC bridge (signals/slots) | Add signals for language/theme/readOnly/cursor; add slots for cursor JS→C++ |
| `src/MonacoBridge.cpp` | Bridge slot implementations | Add cursor slot implementations (re-emit as internal signals) |
| `web/src/index.ts` | JS-side Monaco wiring | Add handlers for all new bridge signals |
| `src/QMonacoEditor.h` | Public widget API | Add setLanguage, setTheme, setReadOnly/isReadOnly, cursor methods, cursorPositionChanged signal |
| `src/QMonacoEditor.cpp` | Widget implementation | Add new methods, wire bridge connections, sync state on editorReady |
| `example/main.cpp` | Demo application | Add language/theme combos, read-only checkbox, cursor position label |

No new files. All changes extend existing files.

---

## Monaco API Reference (for implementer)

These are the Monaco Editor JS APIs used in this plan:

```typescript
// Language — per-model
monaco.editor.setModelLanguage(editor.getModel()!, languageId);

// Theme — global (applies to all editor instances)
monaco.editor.setTheme(themeId);  // "vs", "vs-dark", "hc-black"

// Read-only — per-editor option
editor.updateOptions({ readOnly: value });

// Cursor — 1-based line/column
editor.setPosition({ lineNumber: line, column: column });
editor.getPosition();  // returns { lineNumber, column }
editor.onDidChangeCursorPosition(e => {
    e.position.lineNumber;  // int
    e.position.column;      // int
});
```

---

### Task 1: MonacoBridge — Add All New Signals and Slots

**Files:**
- Modify: `src/MonacoBridge.h`
- Modify: `src/MonacoBridge.cpp`

- [ ] **Step 1: Add language/theme/readOnly signals to MonacoBridge.h**

These are C++→JS only (no JS→C++ needed), so they only need signals:

```cpp
// In the signals: section of MonacoBridge, add after existing signals:

    void requestSetLanguage(const QString &languageId);
    void requestSetTheme(const QString &themeId);
    void requestSetReadOnly(bool readOnly);
```

- [ ] **Step 2: Add cursor signals and slots to MonacoBridge.h**

Cursor needs both directions — C++→JS (signals) and JS→C++ (slots + re-emit signals):

```cpp
// In public slots: section, add after existing slots:

    void onCursorPositionChanged(int line, int column);
    void onGetCursorPositionResult(int line, int column);

// In signals: section, add after the ones from Step 1:

    void requestSetCursorPosition(int line, int column);
    void requestGetCursorPosition();

    void cursorPositionChanged(int line, int column);
    void getCursorPositionResult(int line, int column);
```

- [ ] **Step 3: Implement cursor slots in MonacoBridge.cpp**

```cpp
void MonacoBridge::onCursorPositionChanged(int line, int column) {
    emit cursorPositionChanged(line, column);
}

void MonacoBridge::onGetCursorPositionResult(int line, int column) {
    emit getCursorPositionResult(line, column);
}
```

- [ ] **Step 4: Build to verify compilation**

Run: CMake build (no runtime test needed — just verify the MOC-generated code compiles).

Expected: Build succeeds with no errors.

- [ ] **Step 5: Commit**

```bash
git add src/MonacoBridge.h src/MonacoBridge.cpp
git commit -m "feat(bridge): add signals/slots for language, theme, readOnly, cursor"
```

---

### Task 2: JS Handlers — Wire All New Bridge Signals

**Files:**
- Modify: `web/src/index.ts`

- [ ] **Step 1: Add language, theme, and readOnly handlers**

Inside the `initEditor` function, after the existing `bridge.requestGetText.connect(...)` block, add:

```typescript
  bridge.requestSetLanguage.connect((languageId: string) => {
    const model = editor.getModel();
    if (model) {
      monaco.editor.setModelLanguage(model, languageId);
    }
  });

  bridge.requestSetTheme.connect((themeId: string) => {
    monaco.editor.setTheme(themeId);
  });

  bridge.requestSetReadOnly.connect((readOnly: boolean) => {
    editor.updateOptions({ readOnly });
  });
```

- [ ] **Step 2: Add cursor handlers**

After the handlers from Step 1, add:

```typescript
  bridge.requestSetCursorPosition.connect((line: number, column: number) => {
    editor.setPosition({ lineNumber: line, column: column });
    editor.focus();
  });

  bridge.requestGetCursorPosition.connect(() => {
    const pos = editor.getPosition();
    if (pos) {
      bridge.onGetCursorPositionResult(pos.lineNumber, pos.column);
    }
  });

  editor.onDidChangeCursorPosition((e: monaco.editor.ICursorPositionChangedEvent) => {
    bridge.onCursorPositionChanged(e.position.lineNumber, e.position.column);
  });
```

- [ ] **Step 3: Build frontend to verify TypeScript compiles**

Run (from `web/` directory):
```bash
npm run build
```

Expected: Build succeeds, output in `resources/` directory.

- [ ] **Step 4: Commit**

```bash
git add web/src/index.ts
git commit -m "feat(web): add JS handlers for language, theme, readOnly, cursor"
```

---

### Task 3: QMonacoEditor — Language, Theme, and ReadOnly API

**Files:**
- Modify: `src/QMonacoEditor.h`
- Modify: `src/QMonacoEditor.cpp`

- [ ] **Step 1: Add language, theme, readOnly declarations to QMonacoEditor.h**

Add public methods after `getText`:

```cpp
    void setLanguage(const QString &languageId);
    void setTheme(const QString &themeId);
    void setReadOnly(bool readOnly);
    bool isReadOnly() const;
```

Add private members after existing members:

```cpp
    QString m_language = QStringLiteral("cpp");
    QString m_theme = QStringLiteral("vs-dark");
    bool m_readOnly = false;
```

- [ ] **Step 2: Implement setLanguage, setTheme, setReadOnly in QMonacoEditor.cpp**

Add after the `getText` method:

```cpp
void QMonacoEditor::setLanguage(const QString &languageId) {
    m_language = languageId;
    if (m_ready) {
        emit m_bridge->requestSetLanguage(languageId);
    }
}

void QMonacoEditor::setTheme(const QString &themeId) {
    m_theme = themeId;
    if (m_ready) {
        emit m_bridge->requestSetTheme(themeId);
    }
}

void QMonacoEditor::setReadOnly(bool readOnly) {
    m_readOnly = readOnly;
    if (m_ready) {
        emit m_bridge->requestSetReadOnly(readOnly);
    }
}

bool QMonacoEditor::isReadOnly() const {
    return m_readOnly;
}
```

- [ ] **Step 3: Sync state on editorReady**

In the `editorReady` lambda in the constructor, add state sync calls **before** `emit editorReady()`:

```cpp
    connect(m_bridge, &MonacoBridge::editorReady, this, [this]() {
        m_ready = true;
        if (m_hasPendingText) {
            emit m_bridge->requestSetText(m_pendingText);
            m_hasPendingText = false;
            m_pendingText.clear();
        }
        emit m_bridge->requestSetLanguage(m_language);
        emit m_bridge->requestSetTheme(m_theme);
        emit m_bridge->requestSetReadOnly(m_readOnly);
        emit editorReady();
    });
```

- [ ] **Step 4: Build to verify compilation**

Run: CMake build.

Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add src/QMonacoEditor.h src/QMonacoEditor.cpp
git commit -m "feat: add setLanguage, setTheme, setReadOnly API"
```

---

### Task 4: QMonacoEditor — Cursor Position API

**Files:**
- Modify: `src/QMonacoEditor.h`
- Modify: `src/QMonacoEditor.cpp`

- [ ] **Step 1: Add cursor declarations to QMonacoEditor.h**

Add public methods after `isReadOnly`:

```cpp
    void setCursorPosition(int line, int column);
    void getCursorPosition(std::function<void(int line, int column)> callback);
```

Add signal after `textChanged`:

```cpp
    void cursorPositionChanged(int line, int column);
```

Add private member after `m_readOnly`:

```cpp
    std::function<void(int, int)> m_getCursorCallback;
```

- [ ] **Step 2: Wire cursor bridge connections in constructor**

In `QMonacoEditor.cpp` constructor, add after the existing `getTextResult` connection:

```cpp
    connect(m_bridge, &MonacoBridge::cursorPositionChanged, this, [this](int line, int column) {
        emit cursorPositionChanged(line, column);
    });

    connect(m_bridge, &MonacoBridge::getCursorPositionResult, this, [this](int line, int column) {
        if (m_getCursorCallback) {
            auto cb = std::move(m_getCursorCallback);
            m_getCursorCallback = nullptr;
            cb(line, column);
        }
    });
```

- [ ] **Step 3: Implement setCursorPosition and getCursorPosition**

Add after `isReadOnly`:

```cpp
void QMonacoEditor::setCursorPosition(int line, int column) {
    if (m_ready) {
        emit m_bridge->requestSetCursorPosition(line, column);
    }
}

void QMonacoEditor::getCursorPosition(std::function<void(int line, int column)> callback) {
    if (!m_ready) {
        return;
    }
    m_getCursorCallback = std::move(callback);
    emit m_bridge->requestGetCursorPosition();
}
```

Note: `setCursorPosition` does not store pending state — setting cursor position before the editor has content is not meaningful. If the editor isn't ready, the call is silently dropped.

- [ ] **Step 4: Build to verify compilation**

Run: CMake build.

Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add src/QMonacoEditor.h src/QMonacoEditor.cpp
git commit -m "feat: add setCursorPosition/getCursorPosition API"
```

---

### Task 5: Example App — Demo All New APIs

**Files:**
- Modify: `example/main.cpp`

- [ ] **Step 1: Add required includes**

Add at the top with existing includes:

```cpp
#include <QComboBox>
#include <QCheckBox>
```

- [ ] **Step 2: Replace the button bar with a full control bar**

Replace the entire button bar section (from `auto *buttonBar` through `mainLayout->addLayout(buttonBar)`) with:

```cpp
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
```

- [ ] **Step 3: Replace the status label with a cursor-aware status label**

The existing `statusLabel` definition and its `editorReady` connection stay the same. Add a cursor position display. After the existing `editorReady` connection:

```cpp
    QObject::connect(editor, &QMonacoEditor::cursorPositionChanged, [statusLabel](int line, int column) {
        statusLabel->setText(QString("Ln %1, Col %2").arg(line).arg(column));
    });
```

- [ ] **Step 4: Wire language, theme, and readOnly controls**

After the existing `getTextBtn` connection:

```cpp
    QObject::connect(langCombo, &QComboBox::currentTextChanged, [editor](const QString &lang) {
        editor->setLanguage(lang);
    });

    QObject::connect(themeCombo, &QComboBox::currentTextChanged, [editor](const QString &theme) {
        editor->setTheme(theme);
    });

    QObject::connect(readOnlyCheck, &QCheckBox::toggled, [editor](bool checked) {
        editor->setReadOnly(checked);
    });
```

- [ ] **Step 5: Update the Set Text sample code per language**

Replace the existing `setTextBtn` connection with language-aware sample text:

```cpp
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
```

- [ ] **Step 6: Update window title**

Change:
```cpp
    window.setWindowTitle("QMonacoEditor - Tech Validation");
```
To:
```cpp
    window.setWindowTitle("QMonacoEditor - MVP Demo");
```

- [ ] **Step 7: Build to verify compilation**

Run: CMake build.

Expected: Build succeeds.

- [ ] **Step 8: Commit**

```bash
git add example/main.cpp
git commit -m "feat(example): add language/theme/readOnly/cursor controls"
```

---

### Task 6: Build and Verify All Features

**Files:** None (verification only)

- [ ] **Step 1: Full rebuild**

Clean build to ensure all layers are in sync (Vite → qrc → rcc → compile → link).

Run: CMake clean + build.

Expected: Build succeeds with no warnings related to our code.

- [ ] **Step 2: Run example app and verify language switching**

Launch the example app in **Release mode** (Debug mode has Chromium performance issues — see tech-validation-lessons).

Test:
1. App starts, editor loads with C++ syntax highlighting (default)
2. Select "python" from language dropdown
3. Click "Set Text" — editor shows Python sample code with Python highlighting
4. Select "javascript" — highlighting changes to JavaScript
5. Click "Set Text" — shows JavaScript sample code

Expected: Syntax highlighting matches the selected language. Keywords (`def`, `function`, `int`) are highlighted correctly.

- [ ] **Step 3: Verify theme switching**

Test:
1. Select "vs" from theme dropdown — editor switches to light theme
2. Select "hc-black" — editor switches to high-contrast theme
3. Select "vs-dark" — editor returns to dark theme

Expected: Editor background, text color, and syntax colors change with each theme.

- [ ] **Step 4: Verify read-only mode**

Test:
1. Type in the editor — text appears (editor is writable)
2. Check "Read Only" checkbox
3. Try to type — editor does not accept input
4. Uncheck "Read Only"
5. Try to type — editor accepts input again

Expected: Read-only toggles correctly, no side effects on other features.

- [ ] **Step 5: Verify cursor position**

Test:
1. Click in the editor at various positions
2. Status bar shows "Ln X, Col Y" matching the cursor position
3. Use arrow keys — status bar updates in real-time
4. Click "Set Text" — cursor resets, status bar updates

Expected: Cursor coordinates are accurate and update on every cursor move.

- [ ] **Step 6: Verify Get Text still works**

Test:
1. Type some text in the editor
2. Click "Get Text" — message box shows the current editor content

Expected: getText callback returns the complete current content.

- [ ] **Step 7: Commit if any fixes were needed**

If any issues were found and fixed in earlier tasks, commit the fixes:

```bash
git add -A
git commit -m "fix: address issues found during MVP verification"
```

If everything passed, no commit needed.
