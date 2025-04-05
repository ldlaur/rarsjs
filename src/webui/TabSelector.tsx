import { Component } from "solid-js";

export const TabSelector: Component<{
    tab: string;
    setTab: (newTab: string) => void;
    tabs: string[];
}> = (props) => {
    return <div class="w-full">
        <div class="w-full flex flex-wrap justify-stretch theme-bg theme-fg">
            {props.tabs.map((currTab) => (
                <button
                    class={`grow text-center px-2 font-semibold ${props.tab == currTab ? "border-b-2 theme-bg theme-fg" : "border-b-1 theme-fg2"}`}
                    onClick={() => props.setTab(currTab)}
                >
                    {currTab}
                </button>
            ))}
        </div>
    </div>;
}