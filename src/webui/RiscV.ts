import { convertNumber } from "./EmulatorState";
import wasmUrl from "./main.wasm?url";

interface WasmExports {
  emulate(): void;
  assemble: (offset: number, len: number, allow_externs: boolean) => void;
  pc_to_label: (pc: number) => void;
  emu_load: (addr: number, size: number) => number;
  __heap_base: number;
  g_regs: number;
  g_heap_size: number;
  g_mem_written_addr: number;
  g_mem_written_len: number;
  g_reg_written: number;
  g_pc: number;
  g_text_by_linenum: number;
  g_error: number;
  g_error_line: number;
  g_runtime_error_pc: number;
  g_runtime_error_params: number;
  g_runtime_error_type: number;
  g_pc_to_label_txt: number;
  g_pc_to_label_len: number;
  g_shadow_stack: number;
  g_callsan_stack_written_by: number;
}

const INSTRUCTION_LIMIT: number = 100 * 1000;

export class WasmInterface {
  private memory: WebAssembly.Memory;
  private wasmInstance?: WebAssembly.Instance;
  private exports?: WasmExports;
  private loadedPromise?: Promise<void>;
  private originalMemory?: Uint8Array;
  public textBuffer: string = "";
  public successfulExecution: boolean;
  public regsArr?: Uint32Array;
  public memWrittenLen?: Uint32Array;
  public memWrittenAddr?: Uint32Array;
  public regWritten?: Uint32Array;
  public pc?: Uint32Array;
  public textByLinenum?: Uint32Array;
  public textByLinenumLen?: Uint32Array;
  public runtimeErrorParams?: Uint32Array;
  public runtimeErrorType?: Uint32Array;
  public hasError: boolean = false;
  public instructions: number;
  public shadowStackPtr?: Uint32Array;
  public shadowStack?: Uint32Array;
  public shadowStackLen?: Uint32Array;
  public callsanWrittenBy?: Uint8Array;

  public emu_load: (addr: number, size: number) => number;

  constructor() {
    this.memory = new WebAssembly.Memory({ initial: 7 });
  }

  createU8(off: number) {
    return new Uint8Array(this.memory.buffer, off);
  }
  createU32(off: number) {
    return new Uint32Array(this.memory.buffer, off);
  }

  async loadModule(): Promise<void> {
    if (this.loadedPromise) return this.loadedPromise;
    this.loadedPromise = (async () => {
      const res = await fetch(wasmUrl);
      const buffer = await res.arrayBuffer();
      const { instance } = await WebAssembly.instantiate(buffer, {
        env: {
          memory: this.memory,
          putchar: (n: number) => {
            this.textBuffer += String.fromCharCode(n);
          },
          emu_exit: () => {
            console.log("EXIT");
            this.successfulExecution = true;
          },
          panic: () => {
            alert("wasm panic");
          },
          gettime64: () => BigInt(new Date().getTime() * 10 * 1000),
        },
      });
      this.wasmInstance = instance;
      this.exports = this.wasmInstance.exports as unknown as WasmExports;
      this.emu_load = this.exports.emu_load;
      // Save a snapshot of the original memory to restore between builds.
      this.originalMemory = new Uint8Array(this.memory.buffer.slice(0));
      console.log("Wasm module loaded");
    })();
    return this.loadedPromise;
  }

  async build(
    source: string,
  ): Promise<{ line: number; message: string } | null> {
    if (!this.wasmInstance) {
      await this.loadModule();
    }

    this.successfulExecution = false;
    this.instructions = 0;
    this.hasError = false;
    this.textBuffer = "";

    this.createU8(0).set(this.originalMemory);

    const encoder = new TextEncoder();
    const strBytes = encoder.encode(source);
    const strLen = strBytes.length;
    const offset = this.exports.__heap_base;

    this.memWrittenAddr = this.createU32(this.exports.g_mem_written_addr);
    this.memWrittenLen = this.createU32(this.exports.g_mem_written_len);
    this.regWritten = this.createU32(this.exports.g_reg_written);
    this.pc = this.createU32(this.exports.g_pc);
    this.regsArr = this.createU32(this.exports.g_regs + 4);
    this.runtimeErrorParams = this.createU32(
      this.exports.g_runtime_error_params,
    );
    this.runtimeErrorType = this.createU32(this.exports.g_runtime_error_type);
    this.shadowStackLen = this.createU32(this.exports.g_shadow_stack);
    this.shadowStackPtr = this.createU32(this.exports.g_shadow_stack + 8);
    this.callsanWrittenBy = this.createU8(
      this.exports.g_callsan_stack_written_by,
    );
    if (offset + strLen > this.memory.buffer.byteLength) {
      const pages = Math.ceil(
        (offset + strLen - this.memory.buffer.byteLength) / 65536,
      );
      this.memory.grow(pages);
    }

    this.createU8(offset).set(strBytes);
    this.createU32(this.exports.g_heap_size)[0] = (strLen + 7) & ~7; // align up to 8
    this.exports.assemble(offset, strLen, false);
    const textByLinenumPtr = this.createU32(this.exports.g_text_by_linenum)[2];
    this.textByLinenum = this.createU32(textByLinenumPtr);
    this.textByLinenumLen = this.createU32(this.exports.g_text_by_linenum);

    const errorLine = this.createU32(this.exports.g_error_line)[0];
    const errorPtr = this.createU32(this.exports.g_error)[0];
    if (errorPtr) {
      const error = this.createU8(errorPtr);
      const errorLen = error.indexOf(0);
      const errorStr = new TextDecoder("utf8").decode(error.slice(0, errorLen));
      return { line: errorLine, message: errorStr };
    }

    return null;
  }
  getShadowStack(): Uint32Array {
    return this.createU32(this.shadowStackPtr[0]);
  }

  getStringFromPc(pc: number): string {
    this.exports.pc_to_label(pc);
    const labelPtr = this.createU32(this.exports.g_pc_to_label_txt)[0];
    if (labelPtr) {
      const labelLen = this.createU32(this.exports.g_pc_to_label_len)[0];
      const label = this.createU8(labelPtr);
      const labelStr = new TextDecoder("utf8").decode(label.slice(0, labelLen));
      return labelStr;
    }
    return "0x" + pc.toString(16);
  }

  getRegisterName(idx: number): string {
    const regnames = [
      "zero",
      "ra",
      "sp",
      "gp",
      "tp",
      "t0",
      "t1",
      "t2",
      "fp/s0",
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
    return regnames[idx];
  }
  run(): void {
    this.exports.emulate();
    this.instructions++;
    if (this.instructions > INSTRUCTION_LIMIT) {
      this.textBuffer += `ERROR: instruction limit ${INSTRUCTION_LIMIT} reached\n`;
      this.hasError = true;
    } else if (this.runtimeErrorType[0] != 0) {
      const errorType = this.runtimeErrorType[0];
      const pcString = `PC=0x${this.pc[0].toString(16)}`;
      const runtimeParam1 = this.runtimeErrorParams[0];
      const runtimeParam2 = this.runtimeErrorParams[1];
      let regname = "";
      let oldVal = "";
      let newVal = "";
      let str = "";
      switch (errorType) {
        case 1:
          this.textBuffer += `ERROR: cannot fetch instruction from ${pcString}\n`;
          break;
        case 2:
          str = convertNumber(runtimeParam1, false);
          this.textBuffer += `ERROR: cannot load from address 0x${str} at ${pcString}\n`;
          break;
        case 3:
          str = convertNumber(runtimeParam1, false);
          this.textBuffer += `ERROR: cannot store to address 0x${str} at ${pcString}\n`;
          break;
        case 4:
          this.textBuffer += `ERROR: unhandled instruction at ${pcString}\n`;
          break;
        case 5:
          regname = this.getRegisterName(runtimeParam1);
          this.textBuffer += `CallSan: ${pcString}\nAttempted to read from uninitialized register ${regname}. Check the calling convention!\n`;
          break;
        case 6:
          regname = this.getRegisterName(runtimeParam1);
          oldVal = convertNumber(runtimeParam2, false);
          newVal = convertNumber(this.regsArr[runtimeParam1 - 1], false);
          this.textBuffer += `CallSan: ${pcString}\nCallee-saved register ${regname} has different value at the beginning and end of the function.\nPrev: ${oldVal}\nCurr: ${newVal}\nCheck the calling convention!\n`;
          break;
        case 7:
          oldVal = convertNumber(runtimeParam2, false);
          newVal = convertNumber(this.regsArr[2 - 1], false);
          this.textBuffer += `CallSan: ${pcString}\nRegister sp has different value at the beginning and end of the function.\nPrev: ${oldVal}\nCurr: ${newVal}\nCheck the calling convention!\n`;
          break;
        case 8:
          oldVal = convertNumber(runtimeParam2, false);
          newVal = convertNumber(this.regsArr[1 - 1], false);
          this.textBuffer += `CallSan: ${pcString}\nRegister ra has different value at the beginning and end of the function.\nPrev: ${oldVal}\nCurr: ${newVal}\nCheck the calling convention!\n`;
          break;
        case 9:
          this.textBuffer += `CallSan: ${pcString}\nReturn without matching call!\n`;
          break;
        case 10:
          str = convertNumber(runtimeParam1, false);
          this.textBuffer += `CallSan: ${pcString}\nAttempted to read from stack address 0x${str}, which hasn't been written to in the current function.\n`;
          break;
        default:
          this.textBuffer += `ERROR${errorType}: ${pcString} ${this.runtimeErrorParams[0].toString(
            16,
          )}\n`;
          break;
      }
      this.hasError = true;
    }
  }
}
