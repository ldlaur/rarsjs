import { defineConfig } from 'vite';
import solidPlugin from 'vite-plugin-solid';
import clangPlugin from "./vite-plugin-clang.js";

export default defineConfig({
  plugins: [solidPlugin(), clangPlugin()],
  server: {
    port: 3000,
  },
  build: {
    target: 'esnext',
    outDir: "dist",
  },
});
