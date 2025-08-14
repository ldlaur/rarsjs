import { exec } from "child_process";
import path from "path";
import fs from "fs";

function compile(outpath, optimize) {
  let opts = optimize ? "-flto -O3" : "";
  return new Promise((resolve, reject) => {
    if (!fs.existsSync(outpath)) {
      fs.mkdirSync(outpath, { recursive: true });
    }
    exec(
      `clang --target=wasm32 -flto -nostdlib -Wl,--export-all -Wl,--no-entry -Wl,--allow-undefined -Wl,--import-memory ${opts} -o ${outpath}/main.wasm src/exec/dev.c src/exec/core.c src/exec/emulate.c src/exec/callsan.c src/exec/wasm.c`,
      (error, stdout, stderr) => {
        if (error) {
          reject(stderr);
        } else {
          resolve();
        }
      }
    );
  });
}

export default function clangPlugin() {
  let isProduction = false;

  return {
    name: "vite-plugin-clang",
    enforce: "pre",


    async buildStart() {
      try {
        await compile(path.resolve(__dirname), false);
      } catch (err) {
        throw new Error(err);
      }
    },

    async generateBundle() {
      try {
        await compile(path.resolve(__dirname), true);
      } catch (err) {
        this.error(err);
      }
    },

    async handleHotUpdate({ file, server }) {
      if (file.endsWith(".c") || file.endsWith(".h")) {
        await compile(path.resolve(__dirname), false);
        server.ws.send({ type: "full-reload" });
      }
    },
  };
}
