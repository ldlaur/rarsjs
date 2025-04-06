import { linter } from "@codemirror/lint";
import { WasmInterface } from "./RiscV";
import { setDummy, setWasmPc, dummy, debugMode } from "./App";

export const createAsmLinter = (wasmInterface: WasmInterface) => {
  let delay: number = 750;
  return linter(
    async (ev) => {
      if (!debugMode()) {
        const code = ev.state.doc.toString();
        let err = await wasmInterface.build(code);
        setDummy(dummy() + 1);
        setWasmPc(wasmInterface.pc[0]);
        if (err !== null)
          return [
            {
              from: ev.state.doc.line(err.line).from,
              to: ev.state.doc.line(err.line).to,
              message: err.message,
              severity: "error",
            },
          ];
        else {
          return [];
        }
      }
    },
    {
      delay: delay,
    },
  );
};
