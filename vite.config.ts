import { defineConfig } from 'vite';
import solidPlugin from 'vite-plugin-solid';
import clangPlugin from "./src/webui/vite-plugin-clang.js";
import {lezer} from "@lezer/generator/rollup";
import tailwindcss from 'tailwindcss'
import autoprefixer from 'autoprefixer'

export default defineConfig({
  plugins: [solidPlugin(), clangPlugin(), lezer()],
  server: {
    port: 3000,
  },
  optimizeDeps: {
    include: ["@lezer/generator"]
  },
  css: {
    postcss: {
      plugins: [
        tailwindcss(),
        autoprefixer()
      ]
    }
  },

  build: {
    target: 'es6',
    outDir: "dist",
  },
});
