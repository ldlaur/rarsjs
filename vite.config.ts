import { defineConfig } from 'vite';
import solidPlugin from 'vite-plugin-solid';
import clangPlugin from "./vite-plugin-clang.js";
import {lezer} from "@lezer/generator/rollup";

export default defineConfig({
  plugins: [solidPlugin(), clangPlugin(), lezer()],
  server: {
    port: 3000,
  },
  optimizeDeps: {
    include: ["@lezer/generator"]
  },
  build: {
    target: 'esnext',
    outDir: "dist",
  },
});
