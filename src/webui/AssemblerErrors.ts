import { linter } from "@codemirror/lint";

import { lineHighlightEffect } from "./LineHighlight";
import { buildAsm, latestAsm, setWasmRuntime, wasmRuntime } from "./EmulatorState";

export const createAsmLinter = () => {
  let delay: number = 100;
  return linter(
    async (ev) => {
      if (wasmRuntime.status != "idle" && wasmRuntime.status != "stopped" && wasmRuntime.status != "asmerr") return [];
      console.log(wasmRuntime.status, "starting");
      if (latestAsm["text"] != ev.state.doc.toString())
        await buildAsm(wasmRuntime, setWasmRuntime);
      ev.dispatch({
        effects: lineHighlightEffect.of(0), // disable the line highlight, as line numbering starts from 1
      });
      if (wasmRuntime.status === "asmerr") {
        return [
          {
            from: ev.state.doc.line(wasmRuntime.line).from,
            to: ev.state.doc.line(wasmRuntime.line).to,
            message: wasmRuntime.message,
            severity: "error",
          },
        ];
      } else {
        return [];
      }
    },
    {
      delay: delay,
    },
  );
};
