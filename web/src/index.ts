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
