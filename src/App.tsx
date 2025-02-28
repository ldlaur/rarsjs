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
  const r = Math.round(r1 + (r2 - r1) * percent);
  const g = Math.round(g1 + (g2 - g1) * percent);
  const b = Math.round(b1 + (b2 - b1) * percent);

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

/*const RegisterTable = () => {
  // Generate 31 dummy registers
  const registers = Array.from({ length: 31 }, (_, i) => ({
    id: i + 1,
    name: i === 0 ? "x1/ra" : `x${i + 1}`,
    value: "0xdeadbeef",
  }));

  return (
      <div class="grid grid-cols-[repeat(auto-fit,minmax(8rem,1fr))] gap-2">
        <For each={registers}>
          {(reg) => (
            <div class="flex flex-col">
              <div class="text-sm truncate">
                {reg.name}
              </div>
              <div class="font-mono text-xs text-gray-800 truncate">
                {reg.value}
              </div>
            </div>
          )}
        </For>
      </div>
  );
};*/

function useElementsInRow(container, setElementsInRow) {
  const updateElementsInRow = () => {
      let perRow = 0;
	  let startY = 0;
      for (let item of container.children) {
		let currY = item.getBoundingClientRect().top;
		if (perRow == 0) startY = currY;
		if (currY != startY) break;
		perRow++;
      }
	  setElementsInRow(perRow);
  };

  createEffect(() => {
    updateElementsInRow();
    const handleResize = () => {
      updateElementsInRow();
    };
    window.addEventListener("resize", handleResize);
    onCleanup(() => {
      window.removeEventListener("resize", handleResize);
    });
  });

}

const RegisterTable = () => {
  // Generate 31 dummy registers
  const regnames = ["ra", "sp", "gp", "tp", "tp", "t0", "t1", "t2", "fp", "s1", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"];
  const registers = Array.from({ length: 31 }, (_, i) => ({
    name: `x${i + 1}/${regnames[i]}`,
    value: `0x${'deadbeef'}`
  }));
  let container;
  const [elementsInRow, setElementsInRow] = createSignal(0);
  onMount(() => useElementsInRow(container, setElementsInRow));
  return (
    <div class="flex flex-wrap w-full" ref={container}>
      {registers.map((reg) => (
        <div class="relative flex-grow flex-shrink basis-[0%] min-w-[var(--item-min-size)] pb-2 ">
          <div class="">
            <div class="text-center">
              <div class="text-sm font-mono font-medium">
                {reg.name}
              </div>
              <div class="text-xs font-mono">
                {reg.value}
              </div>
            </div>
          </div>
        </div>
      ))}
	  
      
    <div class="flex-grow flex-shrink basis-[0%] " 
           style={{ "flex-grow": elementsInRow()-31%elementsInRow() }} />
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
