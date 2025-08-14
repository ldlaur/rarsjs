import { createVirtualizer } from "@tanstack/solid-virtual";
import { Component, createSignal, onMount, createEffect, For, Show } from "solid-js";
import { TabSelector } from "./TabSelector";
import { DATA_BASE, shadowStackAugmented, ShadowStackAugmentedEnt, STACK_LEN, STACK_TOP, TEXT_BASE, wasmRuntime } from "./EmulatorState";
const ROW_HEIGHT: number = 24;

export const MemoryView: Component<{ version: () => any, writeAddr: number, writeLen: number, sp: number, load: (addr: number, pow: number) => number | null }> = (props) => {
    let parentRef: HTMLDivElement | undefined;
    let dummyChunk: HTMLDivElement | undefined;
    const [containerWidth, setContainerWidth] = createSignal<number>(0);
    const [chunkWidth, setChunkWidth] = createSignal<number>(0);
    const [chunksPerLine, setChunksPerLine] = createSignal<number>(1);
    const [lineCount, setLineCount] = createSignal<number>(0);
    const [addrSelect, setAddrSelect] = createSignal<number>(-1);

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

    // FIXME: query size instead of hardcoding 64k
    createEffect(() => {
        const cw = chunkWidth();
        const cWidth = containerWidth();
        if (cw > 0 && cWidth > 0) {
            const count = Math.floor(cWidth / cw);
            setChunksPerLine(count);
            if (count < 2) setLineCount(65536 / 4 + 1);
            else setLineCount(Math.ceil(65536 / 4 / (count - 1)));
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

    const [activeTab, setActiveTab] = createSignal(".text");

    // stack starts at the end, others at the start
    createEffect(() => {
        if (parentRef) {
            if (activeTab() == "stack") {
                const lastIndex = lineCount() - 1;
                rowVirtualizer.scrollToIndex(lastIndex);
            } else {
                rowVirtualizer.scrollToIndex(0);
            }
        }
    });

    const getStartAddr = () => {
        if (activeTab() == ".text") return TEXT_BASE;
        else if (activeTab() == ".data") return DATA_BASE;
        else if (activeTab() == "stack") return STACK_TOP - 65536; // TODO: runtime stack size detection
        return 0;
    }
    // FIXME: selecting data should not also select the address column
    return (
        <div class="h-full flex flex-col" style={{ contain: "strict" }} onMouseDown={(e) => { setAddrSelect(-1); }}>
            <TabSelector tab={activeTab()} setTab={setActiveTab} tabs={[".text", ".data", "stack", "frames"]} />
            <div ref={parentRef} class="font-mono text-lg overflow-auto theme-scrollbar ml-2">
                <div ref={dummyChunk} class="invisible absolute ">{"000000000"}</div>
                <Show when={activeTab() == "frames"}>
                    <ShadowStack
                        shadowStackAugmented={(wasmRuntime.status == "debug" || wasmRuntime.status == "error")
                            ? shadowStackAugmented(wasmRuntime.shadowStack, props.load, props.writeAddr, props.writeLen) : []}
                        version={props.version} />
                </Show>
                <Show when={activeTab() != "frames"}>
                    <div style={{ height: `${rowVirtualizer.getTotalSize()}px`, width: "100%", position: "relative" }}>
                        <For each={rowVirtualizer.getVirtualItems()}>
                            {(virtRow) => (
                                <div style={{ position: "absolute", top: `${virtRow.start}px`, width: "100%" }}>
                                    <Show when={chunksPerLine() > 1}>
                                        <a class={"theme-fg2 pr-2 " + ((addrSelect() == virtRow.index) ? "select-text" : "select-none")} onMouseDown={(e) => { setAddrSelect(virtRow.index); e.stopPropagation(); }}>
                                            {(getStartAddr() + virtRow.index * (chunksPerLine() - 1) * 4).toString(16).padStart(8, "0")}
                                        </a>
                                    </Show>
                                    {(() => {
                                        props.version();
                                        let start = getStartAddr();
                                        let chunks = chunksPerLine() - 1;
                                        let idx = virtRow.index;
                                        if (chunksPerLine() < 2) chunks = 1;
                                        let components = new Array(chunks * 4);
                                        let select = (addrSelect() == -1) ? "select-text" : "select-none";
                                        for (let i = 0; i < chunks; i++) {
                                            for (let j = 0; j < 4; j++) {
                                                let style = select;
                                                let ptr = start + (idx * chunks + i) * 4 + j;
                                                if ((idx * chunks + i) * 4 + j >= 65536) break;
                                                let isAnimated = ptr >= props.writeAddr && ptr < props.writeAddr + props.writeLen;
                                                let grayedOut = activeTab() == "stack" && ptr < props.sp;
                                                if (grayedOut) style = "theme-fg2";
                                                if (ptr >= props.sp && ptr < props.sp + 4) style = "frame-highlight";
                                                if (isAnimated) style = "animate-fade-highlight";
                                                let text = props.load ? props.load(ptr, 1).toString(16).padStart(2, "0") : "00";
                                                if (j == 3) style += " mr-[1ch]";
                                                components[i * 4 + j] = <a class={style}>{text}</a>;
                                            }
                                        }
                                        return components;
                                    })()}

                                </div>
                            )}
                        </For>
                    </div>
                </Show>
            </div>
        </div>
    );
};

const ShadowStack: Component<{ version: () => any, shadowStackAugmented: ShadowStackAugmentedEnt[] }> = (props) =>
    <For each={props.shadowStackAugmented}>
        {(elem, idx) => {
            return <div class="flex flex-col pb-4">
                <div >{elem.name}</div>
                <For each={elem.elems}>
                    {(elem, idx) =>
                        <div class="flex flex-row">
                            <a class="theme-fg2 pr-2">
                                {elem.addr}
                            </a>
                            <div class={elem.isAnimated ? "animate-fade-highlight" : ""}>{elem.text}</div>
                        </div>
                    }
                </For>
            </div>
        }}
    </For>