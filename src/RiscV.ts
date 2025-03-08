interface WasmExports {
    emulate(): void;
    assemble: (offset: number, len: number) => void;
    __heap_base: number;
    g_regs: number;
    g_heap_size: number;
    g_mem_written_addr: number;
    g_mem_written_len: number;
    g_reg_written: number;
    g_ram: number;
    g_pc: number;
    g_ram_by_linenum: number;
}

export class WasmInterface {
    private memory: WebAssembly.Memory;
    private wasmInstance?: WebAssembly.Instance;
    private exports?: WasmExports;
    private loadedPromise?: Promise<void>;
    private originalMemory?: Uint8Array;
    public textBuffer: string = "";
    public stopExecution: boolean;
    public regsArr?: Uint32Array;
    public memWrittenLen?: Uint32Array;
    public memWrittenAddr?: Uint32Array;
    public regWritten?: Uint32Array;
    public riscvRam?: Uint8Array;
    public pc?: Uint32Array;
    public ramByLinenum?: Uint32Array;
    public regArr: Array<number> = new Array(31);

    constructor() {
        this.memory = new WebAssembly.Memory({ initial: 7 });
    }

    async loadModule(setText: (str: string) => void): Promise<void> {
        if (this.loadedPromise) return this.loadedPromise;
        this.loadedPromise = (async () => {
            const res = await fetch("http://localhost:3000/main.wasm");
            const buffer = await res.arrayBuffer();
            const { instance } = await WebAssembly.instantiate(buffer, {
                env: {
                    memory: this.memory,
                    putchar: (n: number) => {
                        this.textBuffer += String.fromCharCode(n);
                        setText(this.textBuffer)
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
            // Save a snapshot of the original memory to restore between builds.
            this.originalMemory = new Uint8Array(this.memory.buffer.slice(0));
            console.log("Wasm module loaded");
        })();
        return this.loadedPromise;
    }

    async build(setText: (str: string) => void, source: string): Promise<void> {
        if (!this.wasmInstance) {
            await this.loadModule(setText);
        }

        this.exports = this.wasmInstance.exports as unknown as WasmExports;

        this.stopExecution = false;
        this.textBuffer = "";

        new Uint8Array(this.memory.buffer).set(this.originalMemory);

        const encoder = new TextEncoder();
        const strBytes = encoder.encode(source);
        const strLen = strBytes.length;
        const offset = this.exports.__heap_base;

        if (offset + strLen > this.memory.buffer.byteLength) {
            const pages = Math.ceil((offset + strLen - this.memory.buffer.byteLength) / 65536);
            this.memory.grow(pages);
        }

        new Uint32Array(this.memory.buffer, this.exports.g_heap_size, 1)[0] = strLen;

        const strMem = new Uint8Array(this.memory.buffer, offset, strLen);
        strMem.set(strBytes);

        this.exports.assemble(offset, strLen);
        this.regsArr = new Uint32Array(this.memory.buffer, this.exports.g_regs + 4, 31);
        this.memWrittenAddr = new Uint32Array(this.memory.buffer, this.exports.g_mem_written_addr, 1);
        this.memWrittenLen = new Uint32Array(this.memory.buffer, this.exports.g_mem_written_len, 1);
        this.regWritten = new Uint32Array(this.memory.buffer, this.exports.g_reg_written, 1);
        this.riscvRam = new Uint8Array(this.memory.buffer, this.exports.g_ram, 65536);
        this.pc = new Uint32Array(this.memory.buffer, this.exports.g_pc, 1);
        this.ramByLinenum = new Uint32Array(this.memory.buffer, this.exports.g_ram_by_linenum, 65536);
    }

    run(): void {
        this.exports.emulate();
        let pc_word = this.pc[0] / 4;
        let lineno = this.ramByLinenum[pc_word];
        for (let i = 0; i < 31; i++) this.regArr[i] = this.regsArr[i];
    }
}
