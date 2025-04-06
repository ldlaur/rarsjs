import {
  createSignal,
  onMount,
  Show,
  type Component,
} from "solid-js";
import { basicSetup, EditorView } from "codemirror";
import { keymap } from "@codemirror/view";
import { Compartment, EditorState } from "@codemirror/state";
import {
  defaultSettingsGruvboxDark,
  defaultSettingsGruvboxLight,
  gruvboxDark,
  gruvboxLight,
} from "@uiw/codemirror-theme-gruvbox-dark";
import { lineHighlightEffect, lineHighlightState } from "./LineHighlight";
import { breakpointGutter, breakpointState } from "./Breakpoint";
import { createAsmLinter } from "./AssemblerErrors";
import { forceLinting } from "@codemirror/lint";
import { defaultKeymap, indentWithTab, insertNewlineAndIndent, insertNewlineKeepIndent } from "@codemirror/commands"

import { WasmInterface } from "./RiscV";
import { Settings } from "@uiw/codemirror-themes";

import { parser } from "./riscv.grammar";
import { highlighting } from "./GrammarHighlight";
import { LRLanguage, LanguageSupport, indentService } from "@codemirror/language"
import { RegisterTable } from "./RegisterTable";
import { MemoryView } from "./MemoryView";
import { PaneResize } from "./PaneResize";

let parserWithMetadata = parser.configure({
  props: [highlighting]
})
export const riscvLanguage = LRLanguage.define({
  parser: parserWithMetadata,
})

export const wasmInterface = new WasmInterface();

export const [dummy, setDummy] = createSignal<number>(0);
export const [wasmPc, setWasmPc] = createSignal<number>(0);
export const [wasmRegs, setWasmRegs] = createSignal<number[]>(new Array(31).fill(0));
export const [debugMode, setDebugMode] = createSignal<boolean>(false);
const [consoleText, setConsoleText] = createSignal<string>("");


let breakpointSet: Set<number> = new Set();
let view: EditorView;
let cmTheme: Compartment = new Compartment();
let cssTheme: Settings = defaultSettingsGruvboxLight;
let themeStyle: HTMLStyleElement | null = null;

function interpolate(color1: string, color2: string, percent: number): string {
  const r1 = parseInt(color1.substring(1, 3), 16);
  const g1 = parseInt(color1.substring(3, 5), 16);
  const b1 = parseInt(color1.substring(5, 7), 16);

  const r2 = parseInt(color2.substring(1, 3), 16);
  const g2 = parseInt(color2.substring(3, 5), 16);
  const b2 = parseInt(color2.substring(5, 7), 16);

  const r = Math.max(0, Math.min(255, Math.round(r1 + (r2 - r1) * percent)));
  const g = Math.max(0, Math.min(255, Math.round(g1 + (g2 - g1) * percent)));
  const b = Math.max(0, Math.min(255, Math.round(b1 + (b2 - b1) * percent)));

  return "#" + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
}

function updateCss(): void {
  themeStyle.innerHTML = `
.theme-bg {
  background-color: ${cssTheme.background};
}
.cm-debugging {
  background-color: ${cssTheme == defaultSettingsGruvboxDark ? "#603000" : "#f8c080"};
}
.cm-tooltip-lint {
  color: ${cssTheme.foreground};
  background-color: ${cssTheme.background};
  font-family: monospace;
}
.cm-breakpoint-marker {
  background-color: ${cssTheme == defaultSettingsGruvboxDark ? "#e04010" : "#ff5030"};
}
.theme-bg-hover:hover {
  background-color: ${interpolate(
    cssTheme.background,
    cssTheme.foreground,
    0.1)};
}
.theme-bg-active:active {
  background-color: ${interpolate(
      cssTheme.background,
      cssTheme.foreground,
      0.2)};
}
.theme-gutter {
  background-color: ${cssTheme.gutterBackground};
}
.theme-separator {
  background-color: ${cssTheme.gutterBackground != cssTheme.background
      ? cssTheme.gutterBackground
      : interpolate(cssTheme.background, cssTheme.foreground, 0.1)};
}
.theme-fg {
  color: ${cssTheme.foreground};
}
.theme-fg2 {
  color: ${interpolate(cssTheme.background, cssTheme.foreground, 0.5)};
}
.theme-scrollbar-slim {
  scrollbar-width: thin;
  scrollbar-color: ${interpolate(
        cssTheme.background,
        cssTheme.foreground,
        0.5)} ${cssTheme.background};
}
.theme-scrollbar {
  scrollbar-color: ${interpolate(
          cssTheme.background,
          cssTheme.foreground,
          0.5)} ${cssTheme.background};
}
  
.theme-border {
  border-color: ${interpolate(
            cssTheme.foreground,
            cssTheme.background,
            0.8)};
}
.frame-highlight {
  background-color: ${cssTheme == defaultSettingsGruvboxDark ? "#307010" : "#90f080"};
}
@keyframes fadeHighlight {
  from {
    background-color: ${cssTheme == defaultSettingsGruvboxDark ? "#f08020" : "#f8c080"};
  }
  to {
  }
}
.animate-fade-highlight {
  animation: fadeHighlight 1s forwards;
}
`;
}

window.addEventListener("DOMContentLoaded", () => {
  themeStyle = document.createElement("style");
  document.head.appendChild(themeStyle);
  updateCss();
});

function doChangeTheme(): void {
  if (cssTheme == defaultSettingsGruvboxDark) {
    cssTheme = defaultSettingsGruvboxLight;
    view.dispatch({ effects: cmTheme.reconfigure(gruvboxLight) });
  } else {
    cssTheme = defaultSettingsGruvboxDark;
    view.dispatch({ effects: cmTheme.reconfigure(gruvboxDark) });
  }
  updateCss();
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
    <nav class="sticky theme-gutter">
      <div class="mx-auto px-2">
        <div class="flex items-center h-8">
          <div class="flex-shrink-0">
            <h1 class="text-l font-bold theme-fg">rars.js</h1>
          </div>
          <div class="flex-shrink-0 mx-auto"></div>
          <Show when={debugMode()}>
            <button
              on:click={nextStepRiscV}
              class="flex-0-shrink flex material-symbols-outlined theme-fg theme-bg-hover theme-bg-active"
              title="Step over/Next (Alt-N)"
            >
              step_over
            </button>
            <button
              on:click={singleStepRiscV}
              class="flex-0-shrink flex material-symbols-outlined theme-fg theme-bg-hover theme-bg-active"
              title="Step into (Alt-S)"
            >
              step_into
            </button>
            <button
              on:click={singleStepRiscV}
              class="flex-0-shrink flex material-symbols-outlined theme-fg theme-bg-hover theme-bg-active"
              title="Step out/Finish (Alt-F)"
            >
              step_out
            </button>
            <button
              on:click={continueStepRiscV}
              class="flex-0-shrink flex material-symbols-outlined theme-fg theme-bg-hover theme-bg-active"
              title="Continue (Alt-C)"
            >
              resume
            </button>
            <div class="flex-shrink-0 mx-auto"></div>
          </Show>
          <button
            on:click={doChangeTheme}
            class="flex-0-shrink flex material-symbols-outlined theme-fg theme-bg-hover theme-bg-active"
            title="Change theme"
          >
            dark_mode
          </button>
          <button
            on:click={runRiscV}
            class="flex-0-shrink flex material-symbols-outlined theme-fg theme-bg-hover theme-bg-active"
            title="Run (Alt-R)"
          >
            play_circle
          </button>
          <button
            on:click={startStepRiscV}
            class="flex-0-shrink flex material-symbols-outlined theme-fg theme-bg-hover theme-bg-active"
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
      if (wasmInterface.textByLinenum[i] == lineNum) {
        breakpointSet.add(0x00400000 + i * 4);
      }
    }
  });
}

function updateLineNumber(error: boolean = false) {
  let linenoIdx = (wasmInterface.pc[0] - 0x00400000) / 4;
  if (linenoIdx < wasmInterface.textByLinenumLen[0] && (debugMode() || error)) {
    let lineno = wasmInterface.textByLinenum[linenoIdx];
    view.dispatch({
      effects: lineHighlightEffect.of(lineno),
    });
  } else {
    view.dispatch({
      effects: lineHighlightEffect.of(0), // disable the line highlight, as line numbering starts from1 
    });
  }
}

// TODO: inhibit rebuild while running
async function runRiscV(): Promise<void> {
  let err = await wasmInterface.build(
    view.state.doc.toString(),
  );
  if (err !== null) {
    forceLinting(view);
    return;
  }
  
  setDebugMode(false);
  setConsoleText("");

  while (!wasmInterface.stopExecution) {
    wasmInterface.run(setConsoleText);
  }

  setWasmPc(wasmInterface.pc[0]);
  setWasmRegs([...wasmInterface.regsArr.slice(0, 31)]);
  setDummy(dummy() + 1);
  updateLineNumber(wasmInterface.hasError);
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

  setWasmPc(wasmInterface.pc[0]);
  setWasmRegs([...wasmInterface.regsArr.slice(0, 31)]);
  setDummy(dummy() + 1);
  updateLineNumber();
}

function singleStepRiscV(): void {
  if (!wasmInterface.stopExecution) {
    wasmInterface.run(setConsoleText);
    setDebugMode(!wasmInterface.stopExecution);
    setWasmPc(wasmInterface.pc[0]);
    setWasmRegs([...wasmInterface.regsArr.slice(0, 31)]);

    setDummy(dummy() + 1);
    updateLineNumber();
  }
}

let temporaryBreakpoint: number | null = null;

function continueStepRiscV(): void {
  while (1) {
    setBreakpoints();
    wasmInterface.run(setConsoleText);
    setDebugMode(!wasmInterface.stopExecution);
    setWasmPc(wasmInterface.pc[0]);
    setWasmRegs([...wasmInterface.regsArr.slice(0, 31)]);

    setDummy(dummy() + 1);
    updateLineNumber();
    if (temporaryBreakpoint == wasmInterface.pc[0]) { temporaryBreakpoint = null; break; }
    if (breakpointSet.has(wasmInterface.pc[0])) break;
    if (wasmInterface.stopExecution) {
      break;
    }
  }
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
  let line = context.lineAt(pos);
  let prevLine = context.lineAt(line.from - 1);
  let match = /^\s*/.exec(prevLine.text);
  return match ? match[0].length : 0;
});

const App: Component = () => {
  let editor: HTMLDivElement | undefined;

  onMount(() => {
    const theme = EditorView.theme({
      "&.cm-editor": { height: "100%" },
      ".cm-scroller": { overflow: "auto" },
    });
    const state = EditorState.create({
      doc: "",
      extensions: [
        new LanguageSupport(riscvLanguage, [dummyIndent]),
        createAsmLinter(wasmInterface),
        breakpointGutter, // must be first so it's the first gutter
        basicSetup,
        theme,
        cmTheme.of(gruvboxLight),
        [lineHighlightState],
        keymap.of([...defaultKeymap, indentWithTab]),
      ],
    });
    view = new EditorView({ state, parent: editor });
  });

  return (
    <div class="h-dvh max-h-dvh w-dvw max-w-dvw flex flex-col justify-between overflow-hidden">
      <Navbar />
      <div class="flex w-full h-full overflow-hidden">
        {PaneResize(
          0.5,
          "horizontal",
          <main
            class="w-full h-full overflow-hidden theme-scrollbar"
            ref={editor}
          />,
          /* Reactivity is broken for both */
          PaneResize(
            0.75,
            "vertical",
            PaneResize(0.55, "horizontal",
              <RegisterTable pc={wasmPc()}
                regs={wasmRegs()}
                regWritten={wasmInterface.regWritten ? wasmInterface.regWritten[0] : 0} />,
              <MemoryView dummy={dummy}
                writeAddr={wasmInterface.memWrittenAddr ? wasmInterface.memWrittenAddr[0] : 0}
                writeLen={wasmInterface.memWrittenLen ? wasmInterface.memWrittenLen[0] : 0}
                sp={wasmInterface.regsArr ? wasmInterface.regsArr[2 - 1] : 0}
                load={wasmInterface.LOAD}
              />),
            <div
              innerText={consoleText() ? consoleText() : "Console output will go here..."}
              class={"w-full h-full font-mono overflow-auto theme-scrollbar theme-bg " + (consoleText() ? "theme-fg" : "theme-fg2")}
            ></div>
          ),
        )}
      </div>
    </div>
  );
};

export default App;
