import { Diagnostic, linter } from "@codemirror/lint";
import { EditorView } from "@codemirror/view";

function msgToDiagnostic(ev: EditorView, input: string): Diagnostic {
	return 
}

export const createAsmLinter = (wasmInterface) => {
	let delay = 750;
	return linter(async (ev) => {
		const code = ev.state.doc.toString()
        let err = await wasmInterface.build(null, code);
		if (err !== null) return [
            {
                from: ev.state.doc.line(err.line).from,
                to: ev.state.doc.line(err.line).to,
                message: err.message,
                severity: "error"
            }
        ];
        else return [];
	}, {
		delay: delay,
	})
}