import wasmUrl from "../main.wasm?url";

interface WasmExports {
  emulate(): void;
  assemble: (offset: number, len: number) => void;
  __heap_base: number;
  g_regs: number;
  g_heap_size: number;
  g_mem_written_addr: number;
  g_mem_written_len: number;
  g_reg_written: number;
  g_pc: number;
  g_text_by_linenum: number;
  g_text_by_linenum_len: number;
  g_error: number;
  g_error_line: number;
  g_runtime_error_pc: number;
  g_runtime_error_addr: number;
  g_runtime_error_type: number;
}

export class WasmInterface {
  private memory: WebAssembly.Memory;
  private wasmInstance?: WebAssembly.Instance;
  private exports?: WasmExports;
  private loadedPromise?: Promise<void>;
  private originalMemory?: Uint8Array;
  private setText: (str: string) => void;
  public textBuffer: string = "";
  public stopExecution: boolean;
  public regsArr?: Uint32Array;
  public memWrittenLen?: Uint32Array;
  public memWrittenAddr?: Uint32Array;
  public regWritten?: Uint32Array;
  public pc?: Uint32Array;
  public textByLinenum?: Uint32Array;
  public textByLinenumLen?: Uint32Array;
  public regArr: Array<number> = new Array(31);
  public runtimeErrorAddr?: Uint32Array;
  public runtimeErrorType?: Uint32Array;
  public LOAD: (addr: number, pow: number) => number;

  constructor() {
    this.memory = new WebAssembly.Memory({ initial: 7 });
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
            if (this.setText) this.setText(this.textBuffer);
          },
          emu_exit: () => {
            console.log("EXIT");
            this.stopExecution = true;
          },
          panic: () => {
            alert("wasm panic");
          },
          gettime64: () => BigInt(new Date().getTime() * 10 * 1000),
        },
      });
      this.wasmInstance = instance;
      this.LOAD = instance.exports["LOAD"] as (addr: number, pow: number) => number;
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

    const createU8 = (off: number) => new Uint8Array(this.memory.buffer, off);
    const createU32 = (off: number) => new Uint32Array(this.memory.buffer, off);

    this.exports = this.wasmInstance.exports as unknown as WasmExports;

    this.stopExecution = false;
    this.textBuffer = "";

    createU8(0).set(this.originalMemory);

    const encoder = new TextEncoder();
    const strBytes = encoder.encode(source);
    const strLen = strBytes.length;
    const offset = this.exports.__heap_base;

    this.memWrittenAddr = createU32(this.exports.g_mem_written_addr);
    this.memWrittenLen = createU32(this.exports.g_mem_written_len);
    this.regWritten = createU32(this.exports.g_reg_written);
    this.pc = createU32(this.exports.g_pc);
    this.regsArr = createU32(this.exports.g_regs + 4);
    this.runtimeErrorAddr = createU32(this.exports.g_runtime_error_addr);
    this.runtimeErrorType = createU32(this.exports.g_runtime_error_type);
    if (offset + strLen > this.memory.buffer.byteLength) {
      const pages = Math.ceil(
        (offset + strLen - this.memory.buffer.byteLength) / 65536,
      );
      this.memory.grow(pages);
    }

    createU8(offset).set(strBytes);
    createU32(this.exports.g_heap_size)[0] = (strLen + 7) & (~7); // align up to 8
    this.exports.assemble(offset, strLen);
    const textByLinenumPtr = createU32(this.exports.g_text_by_linenum)[0];
    this.textByLinenum = createU32(textByLinenumPtr);
    this.textByLinenumLen = createU32(this.exports.g_text_by_linenum_len);

    const errorLine = createU32(this.exports.g_error_line)[0];
    const errorPtr = createU32(this.exports.g_error)[0];
    if (errorPtr) {
      const error = createU8(errorPtr);
      const errorLen = error.indexOf(0);
      const errorStr = new TextDecoder("utf8").decode(error.slice(0, errorLen));
      return { line: errorLine, message: errorStr };
    }

    for (let i = 0; i < 31; i++) this.regArr[i] = this.regsArr[i];
    
    return null;
  }

  run(setText: (str: string) => void): void {
    this.setText = setText;
    this.exports.emulate();
    for (let i = 0; i < 31; i++) this.regArr[i] = this.regsArr[i];
    if (this.runtimeErrorType[0] != 0) {
      const errorType = this.runtimeErrorType[0];
      switch (errorType) {
        case 1: this.textBuffer += `ERROR: cannot fetch instruction from PC=0x${this.pc[0].toString(16)}\n`; break;
        case 2: this.textBuffer += `ERROR: cannot load from address 0x${this.runtimeErrorAddr[0].toString(16)} at PC=0x${this.pc[0].toString(16)}\n`; break;
        case 3: this.textBuffer += `ERROR: cannot store to address 0x${this.runtimeErrorAddr[0].toString(16)} at PC=0x${this.pc[0].toString(16)}\n`; break;
        default: this.textBuffer += `ERROR: PC=0x${this.pc[0].toString(16)}\n`; break;
      }
      setText(this.textBuffer);
      this.stopExecution = true;
    }
  }
}
