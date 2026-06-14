import { defineConfig } from "vite";
import monacoEditorPlugin from "vite-plugin-monaco-editor";
import path from "path";

export default defineConfig({
  plugins: [
    monacoEditorPlugin({
      languageWorkers: ["editorWorkerService", "json", "css", "html", "typescript"],
      // outDir below is an absolute path; the plugin's default distPath
      // computation uses path.join(root, outDir, ...), which mis-handles
      // absolute outDir values on Windows. Resolve it ourselves instead.
      customDistPath: (root, buildOutDir, base) =>
        path.resolve(root, buildOutDir, base, "monacoeditorwork"),
    }),
  ],
  build: {
    outDir: path.resolve(__dirname, "../resources"),
    emptyOutDir: true,
  },
  base: "./",
});
