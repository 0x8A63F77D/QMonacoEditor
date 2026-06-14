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
