import { linter } from "@codemirror/lint";

import { lineHighlightEffect } from "./LineHighlight";
import { AsmErrState, buildAsm, buildWithTestcase, IdleState, latestAsm, RuntimeState, setWasmRuntime, StoppedState, testData, wasmRuntime } from "./EmulatorState";

export const createAsmLinter = () => {
  let delay: number = 300;
  return linter(
    async (ev) => {
      if (wasmRuntime.status != "idle" && wasmRuntime.status != "stopped" && wasmRuntime.status != "asmerr") return [];
      if (latestAsm["text"] != ev.state.doc.toString()) {
        if (testData == null) await buildAsm(wasmRuntime, setWasmRuntime);
        else {
          let testcases = testData.testcases;
	        let testPrefix = testData.testPrefix;
          await buildWithTestcase(wasmRuntime, setWasmRuntime, testPrefix + testcases[0].input);
        }
      }
        
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
