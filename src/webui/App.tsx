import {
  createSignal,
  onMount,
  Show,
  type Component,
} from "solid-js";
import { basicSetup, EditorView } from "codemirror";
import { keymap } from "@codemirror/view";
import { Compartment, EditorState, Extension } from "@codemirror/state";

import { lineHighlightEffect, lineHighlightState } from "./LineHighlight";
import { breakpointGutter, breakpointState } from "./Breakpoint";
import { createAsmLinter } from "./AssemblerErrors";
import { forceLinting } from "@codemirror/lint";
import { defaultKeymap, indentWithTab } from "@codemirror/commands"

import { WasmInterface } from "./RiscV";

import { parser } from "./riscv.grammar";
import { highlighting } from "./GrammarHighlight";
import { HighlightStyle, LRLanguage, LanguageSupport, indentService, syntaxHighlighting } from "@codemirror/language"
import { RegisterTable } from "./RegisterTable";
import { MemoryView } from "./MemoryView";
import { PaneResize } from "./PaneResize";
import { PaneResizeComp } from "./PaneResizeComp";

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


import { tags as t } from "@lezer/highlight"



const colors = {
  "base0": "#0d1117",
  "base1": "#161b22",
  "base2": "#21262d",
  "base3": "#89929b",
  "base4": "#c6cdd5",
  "base5": "#ecf2f8",
  "red": "#fa7970",
  "orange": "#faa356",
  "green": "#7ce38b",
  "lightblue": "#a2d2fb",
  "blue": "#77bdfb",
  "purp": "#cea5fb",
};

export const githubDarkTheme = EditorView.theme({
  "&": {
    color: colors.base5,
    backgroundColor: colors.base0
  },

  ".cm-content": {
    caretColor: colors.blue
  },

  ".cm-cursor, .cm-dropCursor": { borderLeftColor: colors.blue },
  "&.cm-focused > .cm-scroller > .cm-selectionLayer .cm-selectionBackground, .cm-selectionBackground, .cm-content ::selection": { backgroundColor: colors.base2 },

  ".cm-panels": { backgroundColor: colors.base0, color: colors.base5 },
  ".cm-panels.cm-panels-top": { borderBottom: "2px solid black" },
  ".cm-panels.cm-panels-bottom": { borderTop: "2px solid black" },

  ".cm-searchMatch": {
    backgroundColor: "#72a1ff59",
    outline: "1px solid #457dff"
  },
  ".cm-searchMatch.cm-searchMatch-selected": {
    backgroundColor: "#6199ff2f"
  },

  ".cm-activeLine": { backgroundColor: "#6699ff0b" },
  ".cm-selectionMatch": { backgroundColor: "#aafe661a" },

  "&.cm-focused .cm-matchingBracket, &.cm-focused .cm-nonmatchingBracket": {
    backgroundColor: "#bad0f847"
  },

  ".cm-gutters": {
    backgroundColor: colors.base0,
    color: colors.base4,
    border: "none"
  },

  ".cm-activeLineGutter": {
    backgroundColor: colors.base1
  },

  ".cm-foldPlaceholder": {
    backgroundColor: "transparent",
    border: "none",
    color: "#ddd"
  },
  ".cm-textfield": {
    backgroundColor: colors.base2,
    backgroundImage: "none",
    border: "none",
  },
  ".cm-button": {
    backgroundColor: colors.base2,
    backgroundImage: "none",
    border: "none",
  },

  ".cm-search > label": {
    "display": "flex",
    "align-items": "center"
  },
  ".cm-search > br": {
    "display": "none",
  },
  ".cm-panel.cm-search input[type=checkbox]": {
    "-webkit-appearance": "none",
    "-moz-appearance": "none",
    "appearance": "none",
    "width": "20px",
    "margin": "5px",
    "height": "20px",
    "border": "none",
    "background-color": colors.base2,
    "cursor": "pointer",
  },

  ".cm-panel.cm-search input[type=checkbox]:hover": {
    "background-color": colors.base3,
  },

  ".cm-panel.cm-search input[type=checkbox]:checked": {
    "background-color": colors.base5,
  },

  ".cm-panel.cm-search input[type=checkbox]:checked:hover": {
    "background-color": colors.base4,
  },

  ".cm-search > button:hover": {
    "background-color": colors.base3,
    "background-image": "none",
  },

  ".cm-search > button:active": {
    "background-color": colors.base5,
    "color": colors.base0,
    "background-image": "none",
  },

  ".cm-search > button:active:hover": {
    "background-color": colors.base4,
    "color": colors.base0,
    "background-image": "none",
  },

  ".cm-tooltip": {
    border: "none",
    backgroundColor: colors.base3
  },
  ".cm-tooltip .cm-tooltip-arrow:before": {
    borderTopColor: "transparent",
    borderBottomColor: "transparent"
  },
  ".cm-tooltip .cm-tooltip-arrow:after": {
    borderTopColor: colors.base3,
    borderBottomColor: colors.base3
  },

}, { dark: true })

export const githubDarkHighlightStyle = HighlightStyle.define([
  {
    tag: t.keyword,
    color: colors.purp
  },
  {
    tag: [t.name, t.deleted, t.character, t.propertyName, t.macroName],
    color: colors.red
  },
  {
    tag: [t.function(t.variableName), t.labelName],
    color: colors.blue
  },
  {
    tag: [t.color, t.constant(t.name), t.standard(t.name)],
    color: colors.orange
  },
  {
    tag: [t.definition(t.name), t.separator],
    color: colors.base4
  },
  {
    tag: [t.typeName, t.className, t.number, t.changed, t.annotation, t.modifier, t.self, t.namespace],
    color: colors.orange
  },
  {
    tag: [t.operator, t.operatorKeyword, t.url, t.escape, t.regexp, t.link, t.special(t.string)],
    color: colors.lightblue
  },
  {
    tag: [t.meta, t.comment],
    color: colors.base3
  },
  {
    tag: t.strong,
    fontWeight: "bold"
  },
  {
    tag: t.emphasis,
    fontStyle: "italic"
  },
  {
    tag: t.strikethrough,
    textDecoration: "line-through"
  },
  {
    tag: t.link,
    color: colors.base3,
    textDecoration: "underline"
  },
  {
    tag: t.heading,
    fontWeight: "bold",
    color: colors.red
  },
  {
    tag: [t.atom, t.bool, t.special(t.variableName)],
    color: colors.orange
  },
  {
    tag: [t.processingInstruction, t.string, t.inserted],
    color: colors.green
  },
  {
    tag: t.invalid,
    color: colors.base5
  },
])

/// Extension to enable the One Dark theme (both the editor theme and
/// the highlight style).
export const githubDark: Extension = [githubDarkTheme, syntaxHighlighting(githubDarkHighlightStyle)]






let breakpointSet: Set<number> = new Set();
let view: EditorView;
let cmTheme: Compartment = new Compartment();
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
  background-color: ${colors.base0};
}
.cm-debugging {
  background-color: ${true ? "#603000" : "#f8c080"};
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
  background-color: ${interpolate(
    colors.base0,
    colors.base5,
    0.2)};
}
.theme-bg-active:active {
  background-color: ${interpolate(
      colors.base0,
      colors.base5,
      0.2)};
}
.theme-gutter {
  background-color: ${colors.base0};
}
.theme-separator {
  background-color: ${interpolate(colors.base0, colors.base5, 0.1)};
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
  border-color: ${interpolate(colors.base2, colors.base3, 0.5)};
}
.frame-highlight {
  background-color: ${colors.green};
}
@keyframes fadeHighlight {
  from {
    background-color: ${colors.orange};
  }
  to {
  }
}
.animate-fade-highlight {
  animation: fadeHighlight 1s forwards;
}
`;
}

const [shadowStack, setShadowStack] = createSignal<{ name: string, args: number[], sp: number }[]>(new Array());
function stackPush() {
  let pc = wasmInterface.pc[0];
  let name = wasmInterface.getStringFromPc(pc);
  let sp = wasmInterface.regsArr[2 - 1];
  let args = Array.from(wasmInterface.regsArr.slice(10 - 1, 18 - 1));
  setShadowStack([...shadowStack(), { name: name, args: args, sp: sp }]);
  console.log(shadowStack());
}

function stackPop() {
  setShadowStack(shadowStack().slice(0, -1));
  console.log(shadowStack());
}

window.addEventListener("DOMContentLoaded", () => {
  themeStyle = document.createElement("style");
  document.head.appendChild(themeStyle);
  updateCss();
});

function doChangeTheme(): void {
  //  if (true) {
  //    cssTheme = defaultSettingsGruvboxLight;
  //    view.dispatch({ effects: cmTheme.reconfigure(gruvboxLight) });
  //  } else {
  //    cssTheme = defaultSettingsGruvboxDark;
  //    view.dispatch({ effects: cmTheme.reconfigure(gruvboxDark) });
  //  }
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
    wasmInterface.run(null, null, setConsoleText);
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
    wasmInterface.run(stackPush, stackPop, setConsoleText);
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
    wasmInterface.run(stackPush, stackPop, setConsoleText);
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
    {shadowStack().toReversed().map(ent => <BacktraceCall name={ent.name} args={ent.args} sp={ent.sp} />)}
  </div>;
};

const Editor: Component = () => {
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
        cmTheme.of(githubDark),
        [lineHighlightState],
        keymap.of([...defaultKeymap, indentWithTab]),
      ],
    });
    view = new EditorView({ state, parent: editor });
  });

  return <main
            class="w-full h-full overflow-hidden theme-scrollbar"
            ref={editor} />;
}


const App: Component = () => {
  return (
    <div class="h-dvh max-h-dvh w-dvw max-w-dvw flex flex-col justify-between overflow-hidden">
      <Navbar />
      <div class="flex w-full h-full overflow-hidden">
        {PaneResize(
          0.5,
          "horizontal",
          false,
          <PaneResizeComp firstSize={0.75} direction="vertical" disableSecond={!debugMode()} a={<Editor />} b={<BacktraceView />} />,
          /* Reactivity is broken for both */
          PaneResize(
            0.75,
            "vertical",
            false,
            PaneResize(0.55, "horizontal", false,
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
