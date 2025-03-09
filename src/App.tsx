import {
  createEffect,
  createSignal,
  onCleanup,
  onMount,
  Show,
  type Component,
} from "solid-js";
import { basicSetup, EditorView } from "codemirror";
import {
  Compartment,
  EditorState,
} from "@codemirror/state";
import {
  defaultSettingsGruvboxDark,
  defaultSettingsGruvboxLight,
  gruvboxDark,
  gruvboxLight,
} from "@uiw/codemirror-theme-gruvbox-dark";
import { lineHighlightEffect, lineHighlightState } from "./LineHighlight";

import { WasmInterface } from "./RiscV";

const wasmInterface = new WasmInterface();

const [dummy, setDummy] = createSignal(0);
const [regsArray, setRegsArray] = createSignal(new Array(31).fill(0));
const [wasmPc, setWasmPc] = createSignal("0x00000000");


let view: EditorView;
let cmTheme = new Compartment();
let cssTheme = defaultSettingsGruvboxLight;

function interpolate(color1, color2, percent) {
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

let themeStyle = null;

function updateCss() {
  themeStyle.innerHTML = `
    .theme-bg {
      background-color: ${cssTheme.background};
    }
    .cm-debugging {
      background-color: ${cssTheme == defaultSettingsGruvboxDark ? "#f08020" : "#f8c080"};
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
    0.1
  )};
    }
    .theme-bg-active:active {
      background-color: ${interpolate(
    cssTheme.background,
    cssTheme.foreground,
    0.2
  )};
    }
    .theme-gutter {
      background-color: ${cssTheme.gutterBackground};
    }
    .theme-separator {
      background-color: ${cssTheme.gutterBackground != cssTheme.background
      ? cssTheme.gutterBackground
      : interpolate(cssTheme.background, cssTheme.foreground, 0.1)
    };
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
      0.5
    )} ${cssTheme.background};
    }
      .theme-scrollbar {

      scrollbar-color: ${interpolate(
      cssTheme.background,
      cssTheme.foreground,
      0.5
    )} ${cssTheme.background};
    }
      
    .theme-border {
      border-color: ${interpolate(
      cssTheme.foreground,
      cssTheme.background,
      0.8
    )};
    }

  @keyframes fadeHighlight {
  from {
    background-color: ${cssTheme == defaultSettingsGruvboxDark ? "#f08020" : "#f8c080"
    };
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

function doChangeTheme() {
  if (cssTheme == defaultSettingsGruvboxDark) {
    cssTheme = defaultSettingsGruvboxLight;
    view.dispatch({ effects: cmTheme.reconfigure(gruvboxLight) });
  } else {
    cssTheme = defaultSettingsGruvboxDark;
    view.dispatch({ effects: cmTheme.reconfigure(gruvboxDark) });
  }
  updateCss();
}

const Navbar: Component = () => {
  const [debugMode, setDebugMode] = createSignal(false);

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
              on:click={singleStepRiscV}
              class="flex-0-shrink flex material-symbols-outlined theme-fg theme-bg-hover theme-bg-active"
            >arrow_downward_alt</button>
            <button
              on:click={continueStepRiscV}
              class="flex-0-shrink flex material-symbols-outlined theme-fg theme-bg-hover theme-bg-active"
            >resume</button>
            <div class="flex-shrink-0 mx-auto"></div>
          </Show>
          <button
            on:click={doChangeTheme}
            class="flex-0-shrink flex material-symbols-outlined theme-fg theme-bg-hover theme-bg-active"
          >
            dark_mode
          </button>
          <button
            on:click={runRiscV}
            class="flex-0-shrink flex material-symbols-outlined theme-fg theme-bg-hover theme-bg-active"
          >
            play_circle
          </button>
          <button
            on:click={() => { startStepRiscV(); setDebugMode(true); }}
            class="flex-0-shrink flex material-symbols-outlined theme-fg theme-bg-hover theme-bg-active">
            arrow_forward
          </button>
        </div>
      </div>
    </nav>
  );
};


import { For } from "solid-js";
import { VirtualList } from "@solid-primitives/virtual";
import { createVirtualizer } from "@tanstack/solid-virtual";
import { breakpointGutter, breakpointState } from "./Breakpoint";
import { createAsmLinter } from "./AssemblerErrors";
const RegisterTable = () => {
  // Generate 31 dummy registers
  const regnames = [
    "ra",
    "sp",
    "gp",
    "tp",
    "t0",
    "t1",
    "t2",
    "fp",
    "s1",
    "a0",
    "a1",
    "a2",
    "a3",
    "a4",
    "a5",
    "a6",
    "a7",
    "s2",
    "s3",
    "s4",
    "s5",
    "s6",
    "s7",
    "s8",
    "s9",
    "s10",
    "s11",
    "t3",
    "t4",
    "t5",
    "t6",
  ];
  // all units being ch makes so that the precise sum is 1ch (left pad) + 7ch (x27/a10) + 10ch (0xdeadbeef) + 1ch (right pad)
  // round to 20ch so it has some padding between regname and hex
  // now i have the precise size in a font-independent format, as long as it's monospace
  return (
    <div class="overflow-auto flex-grow h-full self-start flex-shrink text-sm font-mono  theme-scrollbar-slim theme-border">
      <div class="grid-cols-[repeat(auto-fit,minmax(20ch,1fr))] grid">
        <div class="justify-between flex flex-row box-content theme-border border-l border-b py-[0.5ch] ">
          <div class="self-center pl-[1ch] font-bold">pc</div>
          <div class="self-center pr-[1ch]">
            {wasmPc()}
          </div>
        </div>
        <For each={regsArray()}>
          {(reg, idx) => (
            <div class="justify-between flex flex-row box-content theme-border border-l border-b py-[0.5ch] ">
              <div class="self-center pl-[1ch] font-bold">
                {regnames[idx()]}/x{idx() + 1}
              </div>

              <div
                class={
                  "self-center mr-[1ch] " +
                  ((wasmInterface.regWritten && idx() + 1 == wasmInterface.regWritten[0]) ? "animate-fade-highlight" : "")
                }
              >
                {"0x" + reg.toString(16).padStart(8, "0")}
              </div>
            </div>
          )}
        </For>
        {/* Dummy for the left border of the last element */}
        <div class="theme-border border-l"></div>
      </div>
    </div>
  );
};

const ROW_HEIGHT = 25;

function MemoryView() {
  let parentRef;

  let dummyChunk;
  const [containerWidth, setContainerWidth] = createSignal(0);
  const [chunkWidth, setChunkWidth] = createSignal(0);
  const [chunksPerLine, setChunksPerLine] = createSignal(1);
  const [lineCount, setLineCount] = createSignal(10);

  onMount(() => {
    if (dummyChunk) {
      setChunkWidth(dummyChunk.getBoundingClientRect().width);
    }
    const ro = new ResizeObserver((entries) => {
      for (const entry of entries) {
        setContainerWidth(entry.contentRect.width);
      }
    });
    if (parentRef) ro.observe(parentRef);
    return () => ro.disconnect();
  });

  // Recompute how many chunks fit per line whenever container or chunk widths change.
  createEffect(() => {
    const cw = chunkWidth();
    const cWidth = containerWidth();
    if (cw > 0 && cWidth > 0) {
      const count = Math.floor(cWidth / cw);
      setChunksPerLine(count);
      setLineCount(count == 0 ? 65536 / 4 : 65536 / 4 / count);
    }
  });

  const rowVirtualizer = createVirtualizer({
    get count() {
      return lineCount();
    },
    getScrollElement: () => parentRef ?? null,
    estimateSize: () => ROW_HEIGHT,
    overscan: 5,
  });

  return (
    <div ref={parentRef} class="h-full overflow-auto theme-scrollbar px-2">
      <div ref={dummyChunk} class="invisible absolute font-mono">
        {"000000000"}
      </div>

      <div
        style={{
          height: `${rowVirtualizer.getTotalSize()}px`,
          width: "100%",
          position: "relative",
        }}
      >
        <For each={rowVirtualizer.getVirtualItems()}>
          {(virtualItem) => (
            <div
              style={{
                position: "absolute",
                top: `${virtualItem.start}px`,
                width: "100%",
              }}
            >
              <div class="font-mono">
                <Show when={chunksPerLine() > 1}>
                  <a class="theme-fg2 pr-2">
                    {(virtualItem.index * (chunksPerLine() - 1) * 4)
                      .toString(16)
                      .padStart(8, "0")}
                  </a>
                </Show>
                {(() => {
                  dummy();
                  if (!wasmInterface.riscvRam) return "";
                  let components = [];
                  let chunks = chunksPerLine() - 1;
                  if (chunksPerLine() < 2) chunks = 1;
                  for (let i = 0; i < chunks; i++) {
                    for (let j = 0; j < 4; j++) {
                      let ptr = (virtualItem.index * chunks + i) * 4 + j;
                      let is_animated =
                        ptr >= wasmInterface.memWrittenAddr[0] &&
                        ptr < wasmInterface.memWrittenAddr[0] + wasmInterface.memWrittenLen[0];
                      let style = "";
                      if (is_animated) style += "animate-fade-highlight";
                      components.push(
                        <a class={style}>
                          {wasmInterface.riscvRam[ptr].toString(16).padStart(2, "0")}
                        </a>
                      );
                      if (j == 3) components.push(<a class="pr-1" />);
                    }
                  }
                  return components;
                })()}
              </div>
            </div>
          )}
        </For>
      </div>
    </div>
  );
}

function PaneResize(direction, a, b) {
  let handle;
  let container;

  const [size, setSize] = createSignal(0);
  const [containerSize, setContainerSize] = createSignal(0);

  const [resizeState, setResizeState] = createSignal(null);

  const resizeUp = (e) => {
    setResizeState(null);
    document.body.style.pointerEvents = "";
    document.body.style.userSelect = "";
    handle.style.pointerEvents = "";
  };

  const resizeDown = (e) => {
    e.preventDefault();
    document.body.style.pointerEvents = "none";
    document.body.style.userSelect = "none";
    handle.style.pointerEvents = "auto";
    const client =
      direction == "vertical"
        ? e.clientY ?? e.touches[0]?.clientY
        : e.clientX ?? e.touches[0]?.clientX;
    setResizeState({ origSize: size(), orig: client });
  };

  const resizeMove = (e) => {
    if (resizeState() === null) return;
    const client =
      direction == "vertical"
        ? e.clientY ?? e.touches[0]?.clientY
        : e.clientX ?? e.touches[0]?.clientX;
    const calcSize = resizeState().origSize + (client - resizeState().orig);
    setSize(
      Math.min(
        calcSize,
        direction == "vertical" ? container.clientHeight : container.clientWidth
      )
    );
  };

  const updateSize = () => {
    const newSize = direction == "vertical" ? container.clientHeight : container.clientWidth;
    setSize((size() / containerSize()) * newSize);
    setContainerSize(newSize);
  };

  onMount(() => {
    const initialSize = direction == "vertical" ? container.clientHeight : container.clientWidth;
    setSize(initialSize / 2);
    setContainerSize(initialSize);

    const ro = new ResizeObserver(() => updateSize());
    ro.observe(container);

    document.addEventListener("mousemove", resizeMove);
    document.addEventListener("touchmove", resizeMove);
    document.addEventListener("mouseup", resizeUp);
    document.addEventListener("touchend", resizeUp);
    onCleanup(() => {
      view.destroy();
      ro.disconnect();
      document.removeEventListener("mousemove", resizeMove);
      document.removeEventListener("touchmove", resizeMove);
      document.removeEventListener("mouseup", resizeUp);
      document.removeEventListener("touchend", resizeUp);
    });
  });

  return (
    <div
      class="flex w-full h-full max-h-full max-w-full theme-fg theme-bg"
      ref={container}
      classList={{
        "flex-col": direction == "vertical",
        "flex-row": direction == "horizontal",
      }}
    >
      <div
        class="theme-bg theme-fg flex-shrink overflow-hidden"
        style={{
          height: direction == "vertical" ? `${size()}px` : "auto",
          "min-height": direction == "vertical" ? `${size()}px` : "auto",
          width: direction == "horizontal" ? `${size()}px` : "auto",
          "min-width": direction == "horizontal" ? `${size()}px` : "auto",
        }}
      >
        {a}
      </div>
      <div
        on:mousedown={resizeDown}
        on:touchstart={resizeDown}
        ref={handle}
        style={{ "flex-shrink": 0 }}
        class={
          direction == "vertical"
            ? "w-full h-1 theme-separator cursor-row-resize"
            : "h-full w-1 theme-separator cursor-col-resize"
        }
      ></div>
      <div class="theme-bg theme-fg flex-grow flex-shrink overflow-hidden">
        {b}
      </div>{" "}
    </div>
  );
}
const [consoleText, setConsoleText] = createSignal("");

let breakpointSet = new Set();
function setBreakpoints() {
  const breakpoints = view.state.field(breakpointState);
  breakpoints.between(0, view.state.doc.length, (from) => {
    const line = view.state.doc.lineAt(from);
    const lineNum = line.number;
    for (let i = 0; i < 65536; i++) {
      if (wasmInterface.ramByLinenum[i] == lineNum) {
        breakpointSet.add(i * 4);
      }
    }
  });

  // 
}

async function runRiscV() {
  await wasmInterface.build(setConsoleText, view.state.doc.toString());
  while (!wasmInterface.stopExecution) {
    wasmInterface.run();
  }

  setDummy(dummy() + 1);
  setRegsArray([...wasmInterface.regArr]);
  setWasmPc("0x" + wasmInterface.pc[0].toString(16).padStart(8, "0"));
  let lineno = wasmInterface.ramByLinenum[wasmInterface.pc[0] / 4];
  view.dispatch({
    effects: lineHighlightEffect.of(lineno),
  });
}

async function startStepRiscV() {
  breakpointSet = new Set();
  await wasmInterface.build(setConsoleText, view.state.doc.toString());
  setBreakpoints();
  setDummy(dummy() + 1);

  let lineno = wasmInterface.ramByLinenum[0];
  view.dispatch({
    effects: lineHighlightEffect.of(lineno),
  });
}

function singleStepRiscV() {
  if (!wasmInterface.stopExecution) {
    console.log(wasmInterface.pc[0], wasmInterface.stopExecution);
    wasmInterface.run();
    setDummy(dummy() + 1);
    setRegsArray([...wasmInterface.regArr]);
    setWasmPc("0x" + wasmInterface.pc[0].toString(16).padStart(8, "0"));
    let lineno = wasmInterface.ramByLinenum[wasmInterface.pc[0] / 4];
    view.dispatch({
      effects: lineHighlightEffect.of(lineno),
    });
  }
}

function continueStepRiscV() {
  while (1) {
    console.log(wasmInterface.pc[0], wasmInterface.stopExecution);
    wasmInterface.run();
    setDummy(dummy() + 1);
    setRegsArray([...wasmInterface.regArr]);
    setWasmPc("0x" + wasmInterface.pc[0].toString(16).padStart(8, "0"));
    let lineno = wasmInterface.ramByLinenum[wasmInterface.pc[0] / 4];
    view.dispatch({
      effects: lineHighlightEffect.of(lineno),
    });

    if (breakpointSet.has(wasmInterface.pc[0])) break;
    if (wasmInterface.stopExecution) break;
  }
}


const App: Component = () => {
  let editor;

  onMount(() => {
    const theme = EditorView.theme({
      "&.cm-editor": { height: "100%" },
      ".cm-scroller": { overflow: "auto" },
    });
    const state = EditorState.create({
      doc: "",
      extensions: [
        createAsmLinter(wasmInterface),
        breakpointGutter, // must be first so it's the first gutter
        basicSetup,
        theme,
        cmTheme.of(gruvboxLight),
        [lineHighlightState],
      ],
    });
    view = new EditorView({ state, parent: editor });
  });

  return (
    <div class="h-dvh max-h-dvh w-dvw max-w-dvw flex flex-col justify-between overflow-hidden">
      <Navbar />

      <div class="flex w-full h-full overflow-hidden">
        {PaneResize(
          "horizontal",
          <main class="w-full h-full overflow-hidden theme-scrollbar" ref={editor} />,
          PaneResize(
            "vertical",
            PaneResize("horizontal", <RegisterTable />, <MemoryView />),
            <div
              innerText={consoleText()}
              class="w-full h-full overflow-auto theme-scrollbar theme-fg theme-bg"
              style={{ "font-family": "monospace" }}
            ></div>
          )
        )}
      </div>
    </div>
  );
};

export default App;