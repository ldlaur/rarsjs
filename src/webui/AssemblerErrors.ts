import { linter } from "@codemirror/lint";
import { WasmInterface } from "./RiscV";
import {
  setDummy,
  setWasmPc,
  dummy,
  view,
  setConsoleText,
  setHasError,
} from "./App";
import { lineHighlightEffect } from "./LineHighlight";

export const createAsmLinter = (wasmInterface: WasmInterface) => {
  let delay: number = 100;
  return linter(
    async (ev) => {
      const code = ev.state.doc.toString();
      let err = await wasmInterface.build(code);
      setDummy(dummy() + 1);
      setWasmPc(wasmInterface.pc[0]);
      setHasError(false);
      view.dispatch({
        effects: lineHighlightEffect.of(0), // disable the line highlight, as line numbering starts from 1
      });
      if (err !== null) {
        setConsoleText(`Error on line ${err.line}: ${err.message}`);
        return [
          {
            from: ev.state.doc.line(err.line).from,
            to: ev.state.doc.line(err.line).to,
            message: err.message,
            severity: "error",
          },
        ];
      } else {
        setConsoleText("");
        return [];
      }
    },
    {
      delay: delay,
    },
  );
};
