import {
  createEffect,
  createMemo,
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
  StateEffect,
  StateField,
} from "@codemirror/state";
import {
  defaultSettingsGruvboxDark,
  defaultSettingsGruvboxLight,
  gruvboxDark,
  gruvboxLight,
} from "@uiw/codemirror-theme-gruvbox-dark";
import { ViewPlugin, DecorationSet, ViewUpdate, lineNumberMarkers } from "@codemirror/view";

import { Extension } from "@codemirror/state";
import { Facet } from "@codemirror/state";

import { Decoration } from "@codemirror/view";
import { RangeSetBuilder } from "@codemirror/state";
// Define an effect to update the highlighted line number.
const setHighlightedLine = StateEffect.define<number | null>();

// Create a state field that produces a decoration for the highlighted line.
const highlightedLineField = StateField.define<DecorationSet>({
  create() {
    return Decoration.none;
  },
  update(highlights, tr) {
    // Look for our external effect that sets a new highlighted line.
    let newLine: number | null = null;
    for (let effect of tr.effects) {
      if (effect.is(setHighlightedLine)) {
        newLine = effect.value;
      }
    }
    if (newLine !== null) {
      // Assume line numbers are 1-indexed. Validate the number if necessary.
      if (newLine < 1 || newLine > tr.state.doc.lines) return Decoration.none;
      // Get the line's document position.
      let line = tr.state.doc.line(newLine);
      // Create a decoration that highlights the entire line.
      console.log("here")
      return Decoration.set([
        Decoration.line({class: "cm-debugging"}).range(line.from, line.from)
      ]);
    }
    // If no new effect, remap the existing decoration to account for document changes.
    return highlights.map(tr.changes);
  },
  provide: f => EditorView.decorations.from(f)
});

// Export an extension that includes our field.
const debuggerLineHighlightExtension = [
  highlightedLineField
];


let view: EditorView;
let cmTheme = new Compartment();
let cssTheme = defaultSettingsGruvboxLight;

function interpolate(color1, color2, percent) {
  // Convert the hex colors to RGB values
  const r1 = parseInt(color1.substring(1, 3), 16);
  const g1 = parseInt(color1.substring(3, 5), 16);
  const b1 = parseInt(color1.substring(5, 7), 16);

  const r2 = parseInt(color2.substring(1, 3), 16);
  const g2 = parseInt(color2.substring(3, 5), 16);
  const b2 = parseInt(color2.substring(5, 7), 16);

  // Interpolate the RGB values
  const r = Math.max(0, Math.min(255, Math.round(r1 + (r2 - r1) * percent)));
  const g = Math.max(0, Math.min(255, Math.round(g1 + (g2 - g1) * percent)));
  const b = Math.max(0, Math.min(255, Math.round(b1 + (b2 - b1) * percent)));

  // Convert the interpolated RGB values back to a hex color
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
      background-color: ${
        cssTheme.gutterBackground != cssTheme.background
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

interface NavbarProps {
  onRun: () => void;
  changeTheme: () => void;
}

const Navbar: Component<NavbarProps> = (props: NavbarProps) => {
  return (
    <nav class="sticky theme-gutter">
      <div class="mx-auto px-2">
        <div class="flex items-center h-8">
          <div class="flex-shrink-0">
            <h1 class="text-l font-bold theme-fg">rars.js</h1>
          </div>
          <div class="flex-shrink-0 mx-auto"></div>
          <button
            on:click={props.changeTheme}
            style={{ "font-size": "large" }}
            class="flex-0-shrink flex material-symbols-outlined theme-fg theme-bg-hover theme-bg-active"
          >
            dark_mode
          </button>
          <button
            on:click={props.onRun}
            class="flex-0-shrink flex material-symbols-outlined theme-fg theme-bg-hover theme-bg-active"
          >
            play_arrow
          </button>
          <button class="flex-0-shrink flex material-symbols-outlined theme-fg theme-bg-hover theme-bg-active">
            arrow_right_alt
          </button>
        </div>
      </div>
    </nav>
  );
};

let wasmInstance = null;
let memory = new WebAssembly.Memory({ initial: 16 });
let loadedPromise = null;
let stop = false;
let textbuffer;
let globalSetText;
let originalMemory;
let count;

function loadWasmModule() {
  if (loadedPromise) return loadedPromise;
  loadedPromise = fetch("http://localhost:3000/main.wasm")
    .then((res) => res.arrayBuffer())
    .then((buffer) =>
      WebAssembly.instantiate(buffer, {
        env: {
          memory: memory,
          putchar: (n) => {
            textbuffer += String.fromCharCode(n);
            globalSetText(textbuffer);
          },
          emu_exit: () => {
            console.log("exit");
            stop = true;
          },
          panic: () => {
            alert("wasm panic");
          },
          gettime64: () => BigInt(new Date().getTime() * 10 * 1000),
        },
      })
    )
    .then((result) => {
      wasmInstance = result.instance;
      originalMemory = new Uint8Array(memory.buffer.slice(0));
      console.log("Wasm module loaded");
    })
    .catch((err) => {
      console.error("Failed to load wasm module", err);
    });
  return loadedPromise;
}

async function buildWasm(str) {
  await loadWasmModule();
  stop = false;
  textbuffer = "";
  count = 0;

  const memBuffer = new Uint8Array(memory.buffer);
  memBuffer.set(originalMemory);

  const encoder = new TextEncoder();
  const strBytes = encoder.encode(str);
  const requiredBytes = strBytes.length;
  const pageSize = 64 * 1024;

  let offset = originalMemory.length;
  if (offset + requiredBytes > memory.buffer.byteLength) {
    const pagesNeeded = Math.ceil(requiredBytes / pageSize);
    memory.grow(pagesNeeded);
  }

  const updatedMemoryView = new Uint8Array(
    memory.buffer,
    offset,
    requiredBytes
  );
  updatedMemoryView.set(strBytes);

  wasmInstance.exports.assemble(offset, requiredBytes);
  view.dispatch({
    effects: setHighlightedLine.of(1)
  });

}

async function runWasm(str) {
  wasmInstance.exports.emulate();
  const pc = new Uint32Array(
    memory.buffer,
    wasmInstance.exports.ip,
    4
  );
  const lines = new Uint32Array(
    memory.buffer,
    wasmInstance.exports.ram_by_linenum,
    65536
  );
  count++;
  let pc_word = pc[0] / 4;
  let lineno = lines[pc_word];
  view.dispatch({
    effects: setHighlightedLine.of(lineno+1)
  });
}

function doRiscV(str: string, setText) {
  if (count == undefined) buildWasm(str);
  else runWasm(str);
}

import { For } from "solid-js";
import { VirtualList } from "@solid-primitives/virtual";
import { createVirtualizer } from "@tanstack/solid-virtual";
const RegisterTable = () => {
  // Generate 31 dummy registers
  const registers = Array.from({ length: 31 }, (_, i) => "0xdeadbeef");
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
          <div class="self-center pr-[1ch]">{"0xdeadbeef"}</div>
        </div>

        <For each={registers}>
          {(reg, idx) => (
            <div class="justify-between flex flex-row box-content theme-border border-l border-b py-[0.5ch] ">
              <div class="self-center pl-[1ch] font-bold">
                {regnames[idx()]}/x{idx() + 1}
              </div>
              <div class="self-center pr-[1ch]">{reg}</div>
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

/*

function HexEditor(props) {
  let container;      // ref to the container div
  let dummyChunk;     // hidden dummy element used to measure chunk width

  // Signals to store the container width, measured chunk width, and computed chunks per line
  const [containerWidth, setContainerWidth] = createSignal(0);
  const [chunkWidth, setChunkWidth] = createSignal(0);
  const [chunksPerLine, setChunksPerLine] = createSignal(1);

  // Use provided data or generate some dummy data (256 bytes)
  const data = props.data || (() => {
    const arr = new Uint8Array(256);
    for (let i = 0; i < 256; i++) { arr[i] = i; }
    return arr;
  })();

  // Break the data into 4-byte chunks (each chunk will be rendered as 4 bytes in hex)
  const chunksArray = [];
  for (let i = 0; i < data.length; i += 4) {
    const chunk = Array.from(data.slice(i, i + 4));
    chunksArray.push(chunk);
  }

  // Signal for the lines (each line is an array of chunks)
  const [lines, setLines] = createSignal([]);

  // Function to re-calculate the lines based on the number of chunks that fit per line
  const updateLines = () => {
    const perLine = chunksPerLine();
    const newLines = [];
    for (let i = 0; i < chunksArray.length; i += perLine) {
      newLines.push(chunksArray.slice(i, i + perLine));
    }
    setLines(newLines);
  };

  onMount(() => {
    // Measure the dummy element’s width once it is rendered.
    if (dummyChunk) {
      setChunkWidth(dummyChunk.getBoundingClientRect().width);
    }
    // Use ResizeObserver to monitor the container’s width.
    const ro = new ResizeObserver(entries => {
      for (const entry of entries) {
        setContainerWidth(entry.contentRect.width);
      }
    });
    if (container) ro.observe(container);
    return () => ro.disconnect();
  });

  // Recompute how many chunks fit per line whenever container or chunk widths change.
  createEffect(() => {
    const cw = chunkWidth();
    const cWidth = containerWidth();
    if (cw > 0 && cWidth > 0) {
      // Add some extra spacing (8px here, adjust as needed) to account for margins/padding.
      const count = Math.floor(cWidth / (cw + 8));
      setChunksPerLine(count || 1);
      updateLines();
    }
  });

  return (
    <div
      ref={container}
      class="p-4 border border-gray-300 resize overflow-auto"
    >
      <div
        ref={dummyChunk}
        class="invisible absolute font-mono p-1 bg-gray-800 text-green-400 rounded"
      >
        {"00 00 00 00"}
      </div>

      <For each={lines()}>
        {line => (
          <div class="flex space-x-2 mb-2">
            <For each={line}>
              {chunk => (
                <div class="bg-gray-800 text-green-400 font-mono p-1 rounded">
                  {chunk
                    .map((byte) => byte.toString(16).padStart(2, "0"))
                    .join(" ")}
                </div>
              )}
            </For>
          </div>
        )}
      </For>
    </div>
  );
}
*/
function MemoryView() {
  let parentRef;
  const data = (() => {
    const arr = new Uint8Array(131072);
    for (let i = 0; i < 131072; i++) {
      arr[i] = i;
    }
    return arr;
  })();
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
      setLineCount(count == 0 ? 131072 / 4 : 131072 / 4 / count);
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
                  let str = "";
                  let chunks = chunksPerLine() - 1;
                  if (chunksPerLine() < 2) chunks = 1;

                  for (let i = 0; i < chunks; i++) {
                    for (let j = 0; j < 4; j++) {
                      str += data[(virtualItem.index * chunks + i) * 4 + j]
                        .toString(16)
                        .padStart(2, "0");
                    }
                    str += " ";
                  }
                  return str;
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

  const [size, setSize] = createSignal(200);

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

  onMount(() => {
    document.addEventListener("mousemove", resizeMove);
    document.addEventListener("touchmove", resizeMove);
    document.addEventListener("mouseup", resizeUp);
    document.addEventListener("touchend", resizeUp);
    onCleanup(() => {
      view.destroy();
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

const App: Component = () => {
  let editor;
  let handle;

  const [text, setText] = createSignal("");
  globalSetText = setText;
  onMount(() => {
    const theme = EditorView.theme({
      "&.cm-editor": { height: "100%" },
      ".cm-scroller": { overflow: "auto" },
      // cm-debugging defined elsewhere
    });
    const state = EditorState.create({
      doc: "",
      extensions: [
        basicSetup,
        theme,
        cmTheme.of(gruvboxLight),
        debuggerLineHighlightExtension,
      ],
    });
    view = new EditorView({ state, parent: editor });
  });

  let [clicked, setClicked] = createSignal("data");

  const doRun = () => {
    doRiscV(view.state.doc.toString(), setText);
  };
  let [regsArr, setRegsArr] = createSignal(Array(31));
  return (
    <div class="h-dvh max-h-dvh w-dvw max-w-dvw flex flex-col justify-between overflow-hidden">
      <Navbar onRun={doRun} changeTheme={doChangeTheme}></Navbar>

      <div class="flex w-full h-full overflow-hidden">
        {PaneResize(
          "horizontal",
          <main class="w-full h-full overflow-hidden" ref={editor} />,

          PaneResize(
            "vertical",
            PaneResize("horizontal", <RegisterTable />, <MemoryView />),
            <div
              innerText={text()}
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
