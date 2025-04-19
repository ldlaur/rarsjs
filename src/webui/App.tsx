import {
  createSignal,
  onMount,
  Show,
  type Component,
} from "solid-js";
import { basicSetup, EditorView } from "codemirror";
import { keymap } from "@codemirror/view";
import { Compartment, EditorState } from "@codemirror/state";

import { lineHighlightEffect, lineHighlightState } from "./LineHighlight";
import { breakpointGutter, breakpointState } from "./Breakpoint";
import { createAsmLinter } from "./AssemblerErrors";
import { forceLinting } from "@codemirror/lint";
import { defaultKeymap, indentWithTab } from "@codemirror/commands"

import { WasmInterface } from "./RiscV";

import { parser } from "./riscv.grammar";
import { highlighting } from "./GrammarHighlight";
import { LRLanguage, LanguageSupport, indentService, indentUnit } from "@codemirror/language"
import { RegisterTable } from "./RegisterTable";
import { MemoryView } from "./MemoryView";
import { PaneResize } from "./PaneResize";
import { githubLight, githubDark, Theme, Colors } from './GithubTheme'

let parserWithMetadata = parser.configure({
  props: [highlighting]
})
export const riscvLanguage = LRLanguage.define({
  parser: parserWithMetadata,
})


let currentTheme: Theme = getDefaultTheme();

export const wasmInterface = new WasmInterface();

export const [dummy, setDummy] = createSignal<number>(0);
export const [wasmPc, setWasmPc] = createSignal<number>(0);
export const [wasmRegs, setWasmRegs] = createSignal<number[]>(new Array(31).fill(0));
export const [debugMode, setDebugMode] = createSignal<boolean>(false);
export const [hasError, setHasError] = createSignal<boolean>(false);
export const [consoleText, setConsoleText] = createSignal<string>("");

let breakpointSet: Set<number> = new Set();
let view: EditorView;
let cmTheme: Compartment = new Compartment();


const TEXT_BASE = 0x00400000;
const TEXT_END = 0x10000000;
const DATA_BASE = 0x10000000;
const STACK_TOP = 0x7FFFF000;
const STACK_LEN = 4096;
const DATA_END = 0x70000000;

export function toUnsigned(x: number): number {
    return x >>> 0;
}

export function convertNumber(x: number, decimal: boolean): string {
    let ptr = false;
    if (decimal) {
        if (x >= TEXT_BASE && x < TEXT_END) ptr = true;
        else if (x >= STACK_TOP-STACK_LEN && x < STACK_TOP) ptr = true;
        else if (x >= DATA_BASE && x < DATA_END) ptr = true;
        if (ptr) return "0x"+(toUnsigned(x).toString(16).padStart(8, "0"));
        else return toUnsigned(x).toString();
    } else {
        return toUnsigned(x).toString(16).padStart(8, "0");
    }
}


function updateCss(colors: Colors): void {
  document.getElementById('themestyle').innerHTML = `
.theme-bg {
  background-color: ${colors.base0};
}
.cm-debugging {
  background-color: ${colors.bgorange};
}
.cm-tooltip-lint {
  color: ${colors.base5};
  background-color: ${colors.base0};
  font-family: monospace;
}
.cm-breakpoint-marker {
  background-color: ${colors.red};
}
.theme-bg-hover:hover {
  background-color: ${colors.base1};
}
.theme-bg-active:active {
  background-color: ${colors.base1};
}
.theme-gutter {
  background-color: ${colors.base0};
}
.theme-separator {
  background-color: ${colors.base1};
}
.theme-fg {
  color: ${colors.base4};
}
.theme-fg2 {
  color: ${colors.base3};
}
.theme-scrollbar-slim {
  scrollbar-width: thin;
  scrollbar-color: ${colors.base3} ${colors.base0};
}
.theme-scrollbar {
  scrollbar-color: ${colors.base3} ${colors.base0};
}
.theme-border {
  border-color: ${colors.base2};
}
.frame-highlight {
  background-color: ${colors.bggreen};
}
@keyframes fadeHighlight {
  from {
    background-color: ${colors.bgorange};
  }
  to {
  }
}
.animate-fade-highlight {
  animation: fadeHighlight 1s forwards;
}
`;
}

export const [shadowStack, setShadowStack] = createSignal<{ name: string, args: number[], sp: number }[]>([]);

window.addEventListener("DOMContentLoaded", () => {
  updateCss(currentTheme.colors);
});

function changeTheme(theme: Theme): void {
  view.dispatch({ effects: cmTheme.reconfigure(theme.cmTheme) });
  updateCss(theme.colors);
}

function getDefaultTheme() {
  const savedTheme = localStorage.getItem("theme");
  if (savedTheme && savedTheme == "GithubDark") return githubDark;
  else if (savedTheme && savedTheme == "GithubLight") return githubLight;

  const prefersDark = window.matchMedia("(prefers-color-scheme: dark)").matches;
  if (prefersDark) return githubDark;
  else return githubLight;
}

function doChangeTheme(): void {
  if (currentTheme == githubDark) {
    currentTheme = githubLight;
    changeTheme(githubLight);
    localStorage.setItem("theme", "GithubLight");
  }
  else if (currentTheme == githubLight) {
    currentTheme = githubDark;
    changeTheme(githubDark);
    localStorage.setItem("theme", "GithubDark");
  }
}


window.addEventListener('keydown', (event) => {
  if (debugMode() && event.altKey && event.key.toUpperCase() == 'S') {
    event.preventDefault();
    singleStepRiscV();
  }
  else if (debugMode() && event.altKey && event.key.toUpperCase() == 'N') {
    event.preventDefault();
    nextStepRiscV();
  }
  else if (debugMode() && event.altKey && event.key.toUpperCase() == 'F') {
    event.preventDefault();
    finishStepRiscV();
  }
  else if (debugMode() && event.altKey && event.key.toUpperCase() == 'C') {
    event.preventDefault();
    continueStepRiscV();
  }
  else if (debugMode() && event.altKey && event.key.toUpperCase() == 'Q') {
    event.preventDefault();
    quitRiscV();
  }
  else if (event.altKey && event.key.toUpperCase() == 'R') {
    event.preventDefault();
    runRiscV();
  }
  else if (event.altKey && event.key.toUpperCase() == 'D') {
    event.preventDefault();
    startStepRiscV();
  }
});


const Navbar: Component = () => {
  return (
    <nav class="flex-none theme-gutter">
      <div class="mx-auto px-2">
        <div class="flex items-center h-10">
          <div class="flex-shrink-0">
            <h1 class="text-xl font-bold theme-fg">rars.js</h1>
          </div>
          <div class="flex-shrink-0 mx-auto"></div>
          <Show when={debugMode()}>
            <button
              on:click={nextStepRiscV}
              class="cursor-pointer flex-0-shrink flex material-symbols-outlined theme-fg theme-bg-hover theme-bg-active"
              title="Step over/Next (Alt-N)"
            >
              step_over
            </button>
            <button
              on:click={singleStepRiscV}
              class="cursor-pointer flex-0-shrink flex material-symbols-outlined theme-fg theme-bg-hover theme-bg-active"
              title="Step into (Alt-S)"
            >
              step_into
            </button>
            <button
              on:click={singleStepRiscV}
              class="cursor-pointer flex-0-shrink flex material-symbols-outlined theme-fg theme-bg-hover theme-bg-active"
              title="Step out/Finish (Alt-F)"
            >
              step_out
            </button>
            <button
              on:click={continueStepRiscV}
              class="cursor-pointer flex-0-shrink flex material-symbols-outlined theme-fg theme-bg-hover theme-bg-active"
              title="Continue (Alt-C)"
            >
              resume
            </button>
            <button
              on:click={quitRiscV}
              class="cursor-pointer flex-0-shrink flex material-symbols-outlined theme-fg theme-bg-hover theme-bg-active"
              title="Quit debugging (Alt-Q)"
            >
              stop
            </button>
            <div class="cursor-pointer flex-shrink-0 mx-auto"></div>
          </Show>
          <button
            on:click={doChangeTheme}
            class="cursor-pointer flex-0-shrink flex material-symbols-outlined theme-fg theme-bg-hover theme-bg-active"
            title="Change theme"
          >
            dark_mode
          </button>
          <button
            on:click={runRiscV}
            class="cursor-pointer flex-0-shrink flex material-symbols-outlined theme-fg theme-bg-hover theme-bg-active"
            title="Run (Alt-R)"
          >
            play_circle
          </button>
          <button
            on:click={startStepRiscV}
            class="cursor-pointer flex-0-shrink flex material-symbols-outlined theme-fg theme-bg-hover theme-bg-active"
            title="Debug (Alt-D)"
          >
            arrow_forward
          </button>
        </div>
      </div>
    </nav>
  );
};

function setBreakpoints(): void {
  breakpointSet = new Set();
  const breakpoints = view.state.field(breakpointState);
  breakpoints.between(0, view.state.doc.length, (from) => {
    const line = view.state.doc.lineAt(from);
    const lineNum = line.number;
    for (let i = 0; i < 65536; i++) {
      if (wasmInterface.textByLinenum[i] >= lineNum) {
        breakpointSet.add(0x00400000 + i * 4);
      }
    }
  });
}

function updateLineNumber() {
  let linenoIdx = (wasmInterface.pc[0] - 0x00400000) / 4;
  if (linenoIdx < wasmInterface.textByLinenumLen[0] && (debugMode() || hasError())) {
    let lineno = wasmInterface.textByLinenum[linenoIdx];
    view.dispatch({
      effects: lineHighlightEffect.of(lineno),
    });
  } else {
    view.dispatch({
      effects: lineHighlightEffect.of(0), // disable the line highlight, as line numbering starts from 1 
    });
  }
}

function updateShadowStack() {
  let len = wasmInterface.shadowStackLen[0];
  let st: { name: string, args: number[], sp: number }[] = new Array(len);
  let shadowStack = wasmInterface.getShadowStack();
  for (let i = 0; i < wasmInterface.shadowStackLen[0]; i++) {
    let pc = shadowStack[i * (96/4) + 0];
    let sp = shadowStack[i * (96/4) + 1];
    let args = shadowStack.slice(i * (96/4) + 2).slice(0, 8);
    st[i] = { name: wasmInterface.getStringFromPc(pc), args: [...args], sp: sp };
  }
  setShadowStack(st);
}

async function runRiscV(): Promise<void> {
  let err = await wasmInterface.build(
    view.state.doc.toString(),
  );
  if (err !== null) {
    forceLinting(view);
    return;
  }

  setDebugMode(false);
  setHasError(false);
  setConsoleText("");

  while (1) {
    wasmInterface.run(setConsoleText);
    if (wasmInterface.successfulExecution || wasmInterface.hasError) break;
  }
  if (wasmInterface.successfulExecution || wasmInterface.hasError) setDebugMode(false);
  setHasError(wasmInterface.hasError);
  setWasmPc(wasmInterface.pc[0]);
  setWasmRegs([...wasmInterface.regsArr.slice(0, 31)]);
  setDummy(dummy() + 1);
  updateLineNumber();
  if (wasmInterface.hasError) updateShadowStack();
  if (wasmInterface.successfulExecution) setShadowStack([]);
}

async function startStepRiscV(): Promise<void> {
  let err = await wasmInterface.build(
    view.state.doc.toString(),
  );
  if (err !== null) return;
  else forceLinting(view);

  setDebugMode(true);
  setConsoleText("");
  setBreakpoints();
  setHasError(false);
  setWasmPc(wasmInterface.pc[0]);
  setWasmRegs([...wasmInterface.regsArr.slice(0, 31)]);
  setDummy(dummy() + 1);
  updateLineNumber();
  setShadowStack([]);
}

function singleStepRiscV(): void {
  wasmInterface.run(setConsoleText);
  if (wasmInterface.successfulExecution || wasmInterface.hasError) setDebugMode(false);
  setWasmPc(wasmInterface.pc[0]);
  setWasmRegs([...wasmInterface.regsArr.slice(0, 31)]);

  setDummy(dummy() + 1);
  updateLineNumber();
  updateShadowStack();
}

let temporaryBreakpoint: number | null = null;

function continueStepRiscV(): void {
  setBreakpoints();
  while (1) {
    wasmInterface.run(setConsoleText);
    if (temporaryBreakpoint == wasmInterface.pc[0]) { temporaryBreakpoint = null; break; }
    if (breakpointSet.has(wasmInterface.pc[0])) break;
    if (wasmInterface.successfulExecution || wasmInterface.hasError) break;
  }

  if (wasmInterface.successfulExecution || wasmInterface.hasError) setDebugMode(false);
  setHasError(wasmInterface.hasError);
  setWasmPc(wasmInterface.pc[0]);
  setWasmRegs([...wasmInterface.regsArr.slice(0, 31)]);
  setDummy(dummy() + 1);
  updateLineNumber();
  updateShadowStack();
}

function quitRiscV(): void {
  setHasError(false);
  setDebugMode(false);
  setDummy(dummy() + 1);
  updateLineNumber();
  setShadowStack([]);
}


function nextStepRiscV(): void {
  temporaryBreakpoint = wasmInterface.pc[0] + 4; // TODO: fix for 8byte pseudoinsns
  continueStepRiscV();
}

function finishStepRiscV(): void {
  temporaryBreakpoint = wasmInterface.regsArr[1 - 1]; // ra
  continueStepRiscV();
}

const dummyIndent = indentService.of((context, pos) => {
  if (pos < 0 || pos > context.state.doc.length) return null;
  let line = context.lineAt(pos);
  if (line.from === 0) return 0;
  let prevLine = context.lineAt(line.from - 1);
  let match = /^\s*/.exec(prevLine.text);
  if (!match) return 0;
  let cnt = 0;
  for (let i = 0; i < match[0].length; i++) {
    if (match[0][i] == '\t') cnt = cnt + 4 - cnt % 4;
    else cnt += 1; 
  }
  return cnt;
});

const tabKeymap = keymap.of([{
  key: "Tab",
  run(view) {
    const { state, dispatch } = view;
    const { from, to } = state.selection.main;
    // insert tab instead of indenting if it's a single line selection
    if (from == to || state.doc.lineAt(from).number == state.doc.lineAt(to).number) {
      dispatch(state.update(state.replaceSelection("\t"), {
        scrollIntoView: true,
        userEvent: "input"
      }));
      return true;
    }
    return false;
  }
}]);

function toHex(arg: number): string {
  return "0x" + arg.toString(16).padStart(8, "0");
}
const BacktraceCall: Component<{ name: string, args: number[], sp: number }> = (props) => {
  return <div class="flex">
    <div class="font-bold pr-1">{props.name}</div>
    <div class="flex flex-row flex-wrap">
      <div class="theme-fg2">args=</div>
      <div class="pr-1">{toHex(props.args[0])}</div>
      <div class="pr-1">{toHex(props.args[1])}</div>
      <div class="pr-1">{toHex(props.args[2])}</div>
      <div class="pr-1">{toHex(props.args[3])}</div>
      <div class="pr-1">{toHex(props.args[4])}</div>
      <div class="pr-1">{toHex(props.args[5])}</div>
      <div class="pr-1">{toHex(props.args[6])}</div>
      <div class="pr-1">{toHex(props.args[7])}</div>
      <div class="theme-fg2">sp=</div>
      <div class="pr-1">{toHex(props.sp)}</div>
    </div>
  </div>
};


const BacktraceView: Component = () => {
  return <div class="w-full h-full font-mono text-sm overflow-auto theme-scrollbar-slim flex flex-col">
    {[...shadowStack()].reverse().map(ent => <BacktraceCall name={ent.name} args={ent.args} sp={ent.sp} />)}
  </div>;
};

const Editor: Component = () => {
  let editor: HTMLDivElement | undefined;

  onMount(() => {
    const theme = EditorView.theme({
      "&.cm-editor": { height: "100%" },
      ".cm-scroller": { overflow: "auto" },
    });
    const savedText = localStorage.getItem("savedtext") || "";
    const state = EditorState.create({
      doc: savedText,
      extensions: [
        tabKeymap,
        new LanguageSupport(riscvLanguage, [dummyIndent]),
        createAsmLinter(wasmInterface),
        breakpointGutter, // must be first so it's the first gutter
        basicSetup,
        theme,
        EditorView.editorAttributes.of({ style: "font-size: 1.4em" }),
        cmTheme.of(currentTheme.cmTheme),
        [lineHighlightState],
        indentUnit.of("    "),
        keymap.of([...defaultKeymap, indentWithTab]),
      ],
    });
    view = new EditorView({ state, parent: editor });

    setInterval(() => {
      localStorage.setItem("savedtext", view.state.doc.toString());
    }, 1000);
  });

  return <main
    class="w-full h-full overflow-hidden theme-scrollbar" style={{ contain: "strict" }}
    ref={editor} />;
}


const App: Component = () => {
  return (
    <div class="fullsize flex flex-col justify-between overflow-hidden">
      <Navbar />
      <div class="grow flex overflow-hidden">
        <PaneResize firstSize={0.5} direction="horizontal" disableSecond={false}>
          {() => <PaneResize firstSize={0.85} direction="vertical" disableSecond={!debugMode()}>
            {() => <Editor />}
            {() => <BacktraceView />}
          </PaneResize>}
          {() => <PaneResize firstSize={0.75} direction="vertical" disableSecond={false}>
            {() => <PaneResize firstSize={0.55} direction="horizontal" disableSecond={false}>
              {() => <RegisterTable pc={wasmPc()}
                regs={wasmRegs()}
                regWritten={wasmInterface.regWritten ? wasmInterface.regWritten[0] : 0} />}
              {() => <MemoryView dummy={dummy}
                writeAddr={wasmInterface.memWrittenAddr ? wasmInterface.memWrittenAddr[0] : 0}
                writeLen={wasmInterface.memWrittenLen ? wasmInterface.memWrittenLen[0] : 0}
                sp={wasmInterface.regsArr ? wasmInterface.regsArr[2 - 1] : 0}
                load={wasmInterface.emu_load}
              />}
            </PaneResize>}
            {() => <div
              innerText={consoleText() ? consoleText() : "Console output will go here..."}
              class={"w-full h-full font-mono text-md overflow-auto theme-scrollbar theme-bg " + (consoleText() ? "theme-fg" : "theme-fg2")}
            ></div>}
          </PaneResize>}
        </PaneResize>
      </div>
    </div>
  );
};

export default App;
