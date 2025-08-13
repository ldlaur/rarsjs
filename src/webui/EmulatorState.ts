import { createStore } from "solid-js/store";
import { WasmInterface } from "./RiscV";
import { view } from "./App";
import { forceLinting } from "@codemirror/lint";
import { breakpointState } from "./Breakpoint";

export type ShadowEntry = { name: string; args: number[]; sp: number };

export let breakpoints = new Set();

let globalVersion = 1;

type IdleState = {
	status: "idle";
	version: number;
};

type RunningState = {
	status: "running";
	consoleText: string;
	pc: number;
	regs: number[];
	version: number;
};

export type DebugState = {
	status: "debug";
	consoleText: string;
	pc: number;
	regs: number[];
	shadowStack: ShadowEntry[];
	version: number;
};

export type ErrorState = {
	status: "error";
	consoleText: string;
	pc: number;
	regs: number[];
	shadowStack: ShadowEntry[];
	version: number;
};

export type StoppedState = {
	status: "stopped";
	consoleText: string;
	pc: number;
	regs: number[];
	version: number;
};

export type RuntimeState =
	| IdleState
	| RunningState
	| DebugState
	| ErrorState
	| StoppedState;

export const initialRegs = new Array(31).fill(0);
export const [wasmRuntime, setWasmRuntime] = createStore<RuntimeState>({ status: "idle", version: 0 });

export const wasmInterface = new WasmInterface();
export let latestAsm = { text: "" };

// TODO: cleanup
function setBreakpoints(): void {
  breakpoints = new Set();
  view.state.field(breakpointState).between(0, view.state.doc.length, (from) => {
    const line = view.state.doc.lineAt(from);
    const lineNum = line.number;
    for (let i = 0; i < 65536; i++) {
      if (wasmInterface.textByLinenum[i] == lineNum) {
        breakpoints.add(0x00400000 + i * 4);
      }
    }
  });
}

function buildShadowStack() {
	let len = wasmInterface.shadowStackLen[0];
	let st: { name: string, args: number[], sp: number }[] = new Array(len);
	let shadowStack = wasmInterface.getShadowStack();
	for (let i = 0; i < wasmInterface.shadowStackLen[0]; i++) {
		let pc = shadowStack[i * (96 / 4) + 0];
		let sp = shadowStack[i * (96 / 4) + 1];
		let args = shadowStack.slice(i * (96 / 4) + 2).slice(0, 8);
		st[i] = { name: wasmInterface.getStringFromPc(pc), args: [...args], sp: sp };
	}
	return st;
}

function updateReactiveState(setRuntime) {
	if (wasmInterface.hasError) {
		const st = buildShadowStack();
		setRuntime({
			status: "error",
			consoleText: wasmInterface.textBuffer,
			pc: wasmInterface.pc?.[0] ?? 0,
			regs: [...wasmInterface.regsArr?.slice(0, 31) ?? initialRegs],
			shadowStack: st,
			version: globalVersion++
		});
	} else if (wasmInterface.successfulExecution) {
		setRuntime({
			status: "stopped",
			consoleText: wasmInterface.textBuffer,
			pc: wasmInterface.pc?.[0] ?? 0,
			regs: [...wasmInterface.regsArr?.slice(0, 31) ?? initialRegs],
			version: globalVersion++
		});
	} else {
		setRuntime({
			status: "debug",
			consoleText: wasmInterface.textBuffer,
			pc: wasmInterface.pc?.[0] ?? 0,
			regs: [...wasmInterface.regsArr?.slice(0, 31) ?? initialRegs],
			shadowStack: buildShadowStack(),
			version: globalVersion++
		});
	}
}

export async function runNormal(wasmRuntime: RuntimeState, setRuntime): Promise<void> {
	const asm = view.state.doc.toString();
	const err = await wasmInterface.build(asm);
	if (err !== null) {
		forceLinting(view);
		setRuntime({ status: "idle" });
		return;
	}
	latestAsm.text = asm;
	setRuntime({
		status: "running",
		consoleText: "",
		pc: wasmInterface.pc?.[0] ?? 0x00400000,
		regs: [...wasmInterface.regsArr?.slice(0, 31) ?? initialRegs],
	});

	// run loop
	while (true) {
		wasmInterface.run();
		if (wasmInterface.successfulExecution || wasmInterface.hasError) break;
	}
	updateReactiveState(setRuntime);
}

export async function startStep(runtime: RuntimeState, setRuntime): Promise<void> {
	const asm = view.state.doc.toString();
	const err = await wasmInterface.build(asm);
	if (err !== null) {
		forceLinting(view);
		return;
	}
	latestAsm.text = asm;

	setRuntime({
		status: "debug",
		consoleText: "",
		pc: wasmInterface.pc?.[0],
		regs: initialRegs,
		shadowStack: [],
	});
}

export function singleStep(_runtime: DebugState, setRuntime): void {
	setBreakpoints();
	wasmInterface.run();
	updateReactiveState(setRuntime);
}

let temporaryBreakpoint: number | null = null;
let savedSp = 0;

export function continueStep(_runtime: DebugState, setRuntime): void {
	setBreakpoints();
	while (true) {
		wasmInterface.run();
		if (temporaryBreakpoint === wasmInterface.pc[0] && savedSp === wasmInterface.regsArr[2 - 1]) {
			temporaryBreakpoint = null;
			break;
		}
		if (breakpoints.has(wasmInterface.pc[0])) break;
		if (wasmInterface.successfulExecution || wasmInterface.hasError) break;
	}
	updateReactiveState(setRuntime);
}

export function nextStep(_runtime: DebugState, setRuntime): void {
	const inst = wasmInterface.emu_load(wasmInterface.pc[0], 4);
	const opcode = inst & 127;
	const funct3 = (inst >> 12) & 7;
	const rd = (inst >> 7) & 31;
	const isJal = opcode === 0x6f;
	const isJalr = opcode === 0x67 && funct3 === 0;
	if ((isJal || isJalr) && rd === 1) {
		temporaryBreakpoint = wasmInterface.pc[0] + 4;
		savedSp = wasmInterface.regsArr[2 - 1];
		continueStep(_runtime, setRuntime);
	} else {
		singleStep(_runtime, setRuntime);
	}
}

export function quitDebug(_runtime: DebugState, setRuntime): void {
	setRuntime({ status: "idle" });
}
