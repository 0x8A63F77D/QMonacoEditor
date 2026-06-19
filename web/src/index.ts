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
      language: "cpp",
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
