import wasmUrl from "../main.wasm?url";

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
  g_text_by_linenum_len: number;
  g_error: number;
  g_error_line: number;
  g_runtime_error_pc: number;
  g_runtime_error_addr: number;
  g_runtime_error_type: number;
  g_pc_to_label_txt: number;
  g_pc_to_label_len: number;
  g_shadow_stack: number;
  g_shadow_stack_len: number;
}

const INSTRUCTION_LIMIT: number = 100 * 1000;

export class WasmInterface {
  private memory: WebAssembly.Memory;
  private wasmInstance?: WebAssembly.Instance;
  private exports?: WasmExports;
  private loadedPromise?: Promise<void>;
  private originalMemory?: Uint8Array;
  private setText: (str: string) => void;
  public textBuffer: string = "";
  public successfulExecution: boolean;
  public regsArr?: Uint32Array;
  public memWrittenLen?: Uint32Array;
  public memWrittenAddr?: Uint32Array;
  public regWritten?: Uint32Array;
  public pc?: Uint32Array;
  public textByLinenum?: Uint32Array;
  public textByLinenumLen?: Uint32Array;
  public runtimeErrorAddr?: Uint32Array;
  public runtimeErrorType?: Uint32Array;
  public hasError: boolean = false;
  public instructions: number;
  public shadowStackPtr?: Uint32Array;
  public shadowStack?: Uint32Array;
  public shadowStackLen?: Uint32Array;

  public emu_load: (addr: number, size: number) => number;

  constructor() {
    this.memory = new WebAssembly.Memory({ initial: 7 });
  }


  createU8(off: number) { return new Uint8Array(this.memory.buffer, off) }
  createU32(off: number) { return new Uint32Array(this.memory.buffer, off) }


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
    this.runtimeErrorAddr = this.createU32(this.exports.g_runtime_error_addr);
    this.runtimeErrorType = this.createU32(this.exports.g_runtime_error_type);
    this.shadowStackLen = this.createU32(this.exports.g_shadow_stack_len);
    this.shadowStackPtr = this.createU32(this.exports.g_shadow_stack);
    
    if (offset + strLen > this.memory.buffer.byteLength) {
      const pages = Math.ceil(
        (offset + strLen - this.memory.buffer.byteLength) / 65536,
      );
      this.memory.grow(pages);
    }

    this.createU8(offset).set(strBytes);
    this.createU32(this.exports.g_heap_size)[0] = (strLen + 7) & (~7); // align up to 8
    this.exports.assemble(offset, strLen, false);
    const textByLinenumPtr = this.createU32(this.exports.g_text_by_linenum)[0];
    this.textByLinenum = this.createU32(textByLinenumPtr);
    this.textByLinenumLen = this.createU32(this.exports.g_text_by_linenum_len);

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

  run(setText: (str: string) => void): void {
    this.setText = setText;
    this.exports.emulate();
    this.instructions++;
    if (this.instructions > INSTRUCTION_LIMIT) {
      this.textBuffer += `ERROR: instruction limit ${INSTRUCTION_LIMIT} reached\n`;
      setText(this.textBuffer);
      this.hasError = true;
    } else if (this.runtimeErrorType[0] != 0) {
      const errorType = this.runtimeErrorType[0];
      switch (errorType) {
        case 1: this.textBuffer += `ERROR: cannot fetch instruction from PC=0x${this.pc[0].toString(16)}\n`; break;
        case 2: this.textBuffer += `ERROR: cannot load from address 0x${this.runtimeErrorAddr[0].toString(16)} at PC=0x${this.pc[0].toString(16)}\n`; break;
        case 3: this.textBuffer += `ERROR: cannot store to address 0x${this.runtimeErrorAddr[0].toString(16)} at PC=0x${this.pc[0].toString(16)}\n`; break;
        case 4: this.textBuffer += `ERROR: unhandled instruction at PC=0x${this.pc[0].toString(16)}\n`; break;
        default: this.textBuffer += `ERROR: PC=0x${this.pc[0].toString(16)}\n`; break;
      }
      setText(this.textBuffer);
      this.hasError = true;
    } else if (this.successfulExecution) {
      const needsNewline = this.textBuffer.length && this.textBuffer[this.textBuffer.length-1] != '\n';
      setText(this.textBuffer + (needsNewline ? "\nExecuted successfully." : "Executed successfully."));
    }
  }
}
