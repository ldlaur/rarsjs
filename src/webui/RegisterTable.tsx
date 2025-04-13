import { Component, For, Index } from "solid-js";

export const RegisterTable: Component<{ pc: number, regs: number[], regWritten: number }> = (props) => {
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
    <div class="overflow-auto flex-grow h-full self-start flex-shrink text-md font-mono theme-scrollbar-slim theme-border" style={{contain: "strict"}}>
      <div class="ml-[-1px] grid-cols-[repeat(auto-fit,minmax(20ch,1fr))] grid">
        <div class="justify-between flex flex-row box-content theme-border border-l border-b py-[0.5ch] ">
          <div class="self-center pl-[1ch] font-bold">pc</div>
          <div class="self-center pr-[1ch]">{"0x" + props.pc.toString(16).padStart(8, "0")}</div>
        </div>
        {/* using Index here would optimize it, but it gets messy with animations
            naively keeping it as is and making regWritten a signal would still cause everything to be recomputed
        */}
        {props.regs.map((reg, idx) => (
            <div class="justify-between flex flex-row box-content theme-border border-l border-b py-[0.5ch]">
              <div class="self-center pl-[1ch] font-bold">
                {regnames[idx]}/x{idx + 1}
              </div>
              <div class={"self-center mr-[1ch] " + (idx + 1 == props.regWritten ? "animate-fade-highlight" : "")}>
                {"0x" + reg.toString(16).padStart(8, "0")}
              </div>
            </div>
          ))}
        {/* dummy left border of the last element */}
        <div class="theme-border border-l"></div>
      </div>
    </div>
  );
};