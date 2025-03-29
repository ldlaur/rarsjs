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
      `clang --target=wasm32 -nostdlib -Wl,--export-all -Wl,--no-entry -Wl,--allow-undefined -Wl,--import-memory ${opts} -o ${outpath}/main.wasm src/exec/main.c src/exec/wasm.c`,
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
  let outputDir = "src"; // Default for dev mode

  return {
    name: "vite-plugin-clang",
    enforce: "pre",

    configResolved(config) {
      isProduction = config.command === "build";
      if (isProduction) outputDir = config.build.outDir;
    },


    async buildStart() {
      try {
        await compile(path.resolve(__dirname, outputDir), isProduction);
      } catch (err) {
        throw new Error(err);
      }
    },

    async handleHotUpdate({ file, server }) {
      if (file.endsWith(".c") || file.endsWith(".h")) {
        await compile(path.resolve(__dirname, outputDir), isProduction);
        server.ws.send({ type: "full-reload" });
      }
    },
  };
}
