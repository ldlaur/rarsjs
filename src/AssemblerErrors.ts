import { linter } from "@codemirror/lint";
import { WasmInterface } from "./RiscV";

export const createAsmLinter = (wasmInterface: WasmInterface) => {
  let delay: number = 750;
  return linter(
    async (ev) => {
      const code = ev.state.doc.toString();
      let err = await wasmInterface.build(null, code);
      if (err !== null)
        return [
          {
            from: ev.state.doc.line(err.line).from,
            to: ev.state.doc.line(err.line).to,
            message: err.message,
            severity: "error",
          },
        ];
      else return [];
    },
    {
      delay: delay,
    },
  );
};
