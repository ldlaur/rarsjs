import {
  createEffect,
  createSignal,
  onCleanup,
  onMount,
  Show,
  type Component,
} from "solid-js";
import { basicSetup, EditorView } from "codemirror";
import { Compartment, EditorState } from "@codemirror/state";
import {
  defaultSettingsGruvboxDark,
  defaultSettingsGruvboxLight,
  gruvboxDark,
  gruvboxLight,
} from "@uiw/codemirror-theme-gruvbox-dark";
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

async function loadWasm(str, setText) {
  let blob = await (
    await fetch("http://localhost:3000/main.wasm")
  ).arrayBuffer();
  let textbuffer = "";
  let memory = new WebAssembly.Memory({ initial: 16 });
  let stop = false;
  let wasm = await WebAssembly.instantiate(blob, {
    env: {
      memory: memory,
      putchar: (n) => {
        textbuffer += String.fromCharCode(n);
        setText(textbuffer);
      },
      emu_exit: () => {
        stop = true;
      },
      panic: () => {
        stop = true;
        alert("wasm panic");
      },
      gettime64: () => BigInt(new Date().getTime() * 10 * 1000),
    },
  });

  const memBuffer = new Uint8Array(memory.buffer);
  let offset = memBuffer.byteLength;
  const encoder = new TextEncoder();
  const strBytes = encoder.encode(str);
  const requiredBytes = strBytes.length;
  const pageSize = 64 * 1024;
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
  console.log(wasm.instance.exports);
  wasm.instance.exports.assemble(offset, requiredBytes);
  let count = 0;
  while (!stop && count < 10000) {
    wasm.instance.exports.emulate(offset, requiredBytes);
    count++;
  }
}

function doRiscV(str: string, setText) {
  loadWasm(str, setText);
}
import { For } from "solid-js";
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

  return (
    <div class="text-sm font-mono grid-cols-[repeat(auto-fit,minmax(18ch,1fr))] grid overflow-hidden">
      <For each={registers}>
        {(reg, idx) => (
          <div class="justify-between flex flex-row box-content theme-border border-l border-b py-0.5">
            <div class=" pl-1.5 font-bold pr-1" style="font-size: 0.8rem;">
              {regnames[idx()]}/x{idx() + 1}
            </div>
            <div class="pr-1.5" style="font-size: 0.8rem;">
              {reg}
            </div>
          </div>
        )}
      </For>
      {/* Dummy for the left border of the last element */}
      <div class="theme-border border-l"></div>
    </div>
  );
};

const App: Component = () => {
  let editor;
  let handle;

  const [size, setSize] = createSignal(200);
  const [text, setText] = createSignal("");

  const [resizeState, setResizeState] = createSignal(null);

  const resizeUp = (e) => {
    setResizeState(null);
    document.body.style.pointerEvents = "";
    document.body.style.userSelect = "";
    handle.style.pointerEvents = "";
  };

  const resizeDown = (e) => {
    e.preventDefault();
    const clientY = e.clientY ?? e.touches[0]?.clientY;
    document.body.style.pointerEvents = "none";
    document.body.style.userSelect = "none";
    handle.style.pointerEvents = "auto";
    setResizeState({ origSize: size(), origY: clientY });
  };

  const resizeMove = (e) => {
    if (resizeState() === null) return;
    const clientY = e.clientY ?? e.touches[0]?.clientY;
    setSize(resizeState().origSize - (clientY - resizeState().origY));
  };

  onMount(() => {
    document.addEventListener("mousemove", resizeMove);
    document.addEventListener("touchmove", resizeMove);
    document.addEventListener("mouseup", resizeUp);
    document.addEventListener("touchend", resizeUp);

    const theme = EditorView.theme({
      "&.cm-editor": { height: "100%" },
      ".cm-scroller": { overflow: "auto" },
    });
    const state = EditorState.create({
      doc: "",
      extensions: [basicSetup, theme, cmTheme.of(gruvboxLight)],
    });
    view = new EditorView({ state, parent: editor });

    onCleanup(() => {
      view.destroy();
    });
  });

  let [clicked, setClicked] = createSignal("regs");

  const doRun = () => {
    doRiscV(view.state.doc.toString(), setText);
  };
  let [regsArr, setRegsArr] = createSignal(Array(31));
  return (
    <div class="h-dvh m-h-dvh flex flex-col justify-between overflow-hidden">
      <Navbar onRun={doRun} changeTheme={doChangeTheme}></Navbar>
      <main class="w-full h-full overflow-hidden" ref={editor} />
      <div
        on:mousedown={resizeDown}
        on:touchstart={resizeDown}
        ref={handle}
        style={{ "flex-shrink": 0 }}
        class="w-4 w-full theme-separator cursor-row-resize"
      >
        <button
          class="px-1 theme-fg"
          classList={{ "theme-bg": clicked() == "console" }}
          onClick={() => setClicked("console")}
          on:touchstart={() => setClicked("console")}
        >
          console
        </button>
        <button
          class="px-1 theme-fg"
          classList={{ "theme-bg": clicked() == "regs" }}
          onClick={() => setClicked("regs")}
          on:touchstart={() => setClicked("regs")}
        >
          regs
        </button>
      </div>
      {/* flex-shrink: 0 here is very important! */}
      <div
        class="w-full h-full theme-bg theme-fg"
        style={{ "flex-shrink": 0, height: `${size()}px` }}
      >
        <Show when={clicked() == "console"}>
          <div
            innerText={text()}
            class="w-full h-full overflow-auto theme-fg theme-bg"
            style={{ "font-family": "monospace" }}
          ></div>
        </Show>
        <Show when={clicked() == "regs"}>
          <div class="overflow-auto">
            <RegisterTable />
          </div>
        </Show>
      </div>
    </div>
  );
};

export default App;
