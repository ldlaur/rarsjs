import { createSignal, onMount, onCleanup } from "solid-js";
import { JSX } from "solid-js/jsx-runtime";

export function PaneResize(
    firstSize: number,
    direction: "vertical" | "horizontal",
    a: JSX.Element,
    b: JSX.Element,
): JSX.Element {
    let handle: HTMLDivElement | undefined;
    let container: HTMLDivElement | undefined;

    const [size, setSize] = createSignal<number>(0);
    const [containerSize, setContainerSize] = createSignal<number>(0);

    const [resizeState, setResizeState] = createSignal<{
        origSize: number;
        orig: number;
    } | null>(null);

    const resizeUp = (e: MouseEvent | TouchEvent) => {
        setResizeState(null);
        document.body.style.pointerEvents = "";
        document.body.style.userSelect = "";
        handle!.style.pointerEvents = "";
    };

    const resizeDown = (e: MouseEvent | TouchEvent) => {
        e.preventDefault();
        document.body.style.pointerEvents = "none";
        document.body.style.userSelect = "none";
        handle!.style.pointerEvents = "auto";
        const client =
            direction == "vertical"
                ? (e as MouseEvent).clientY ?? (e as TouchEvent).touches[0]?.clientY
                : (e as MouseEvent).clientX ?? (e as TouchEvent).touches[0]?.clientX;
        setResizeState({ origSize: size(), orig: client });
    };

    const resizeMove = (e: MouseEvent | TouchEvent) => {
        if (resizeState() === null) return;
        const client =
            direction == "vertical"
                ? (e as MouseEvent).clientY ?? (e as TouchEvent).touches[0]?.clientY
                : (e as MouseEvent).clientX ?? (e as TouchEvent).touches[0]?.clientX;
        const calcSize = resizeState()!.origSize + (client - resizeState()!.orig);
        const dim = direction == "vertical"
            ? container!.clientHeight
            : container!.clientWidth;
        setSize(Math.max(0, Math.min(calcSize, dim - 4)));
    };

    const updateSize = () => {
        const newSize =
            direction == "vertical"
                ? container!.clientHeight
                : container!.clientWidth;
        if (newSize === 0) return;
        setSize((size() / containerSize()) * newSize);
        setContainerSize(newSize);
    };

    onMount(() => {
        const initialSize =
            direction == "vertical"
                ? container!.clientHeight
                : container!.clientWidth;
        setSize(initialSize * firstSize);
        setContainerSize(initialSize);

        const ro = new ResizeObserver(() => updateSize());
        ro.observe(container!);

        document.addEventListener("mousemove", resizeMove);
        document.addEventListener("touchmove", resizeMove);
        document.addEventListener("mouseup", resizeUp);
        document.addEventListener("touchend", resizeUp);
        onCleanup(() => {
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
                        ? "w-full h-[4px] theme-separator cursor-row-resize"
                        : "h-full w-[4px] theme-separator cursor-col-resize"
                }
            ></div>
            <div class="theme-bg theme-fg flex-grow flex-shrink overflow-hidden">
                {b}
            </div>{" "}
        </div>
    );
}