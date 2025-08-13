import { linter } from "@codemirror/lint";
import { WasmInterface } from "./RiscV";

import { lineHighlightEffect } from "./LineHighlight";
import { latestAsm } from "./EmulatorState";
import { view } from "./App";
import { createSignal } from "solid-js";
const [compileErrors, setCompileErrors] = createSignal("");
export { compileErrors };

export const createAsmLinter = (wasmInterface: WasmInterface) => {
  let delay: number = 100;
  return linter(
    async (ev) => {
      const code = ev.state.doc.toString();
      if (latestAsm["text"] == code) return [];
      latestAsm["text"] = code;
      let err = await wasmInterface.build(code);
      view.dispatch({
        effects: lineHighlightEffect.of(0), // disable the line highlight, as line numbering starts from 1
      });
      if (err !== null) {
        setCompileErrors(`Error on line ${err.line}: ${err.message}`);
        return [
          {
            from: ev.state.doc.line(err.line).from,
            to: ev.state.doc.line(err.line).to,
            message: err.message,
            severity: "error",
          },
        ];
      } else {
        setCompileErrors("");
        return [];
      }
    },
    {
      delay: delay,
    },
  );
};
