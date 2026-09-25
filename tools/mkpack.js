// Builds a resource pack (ceres/pack.h) from files on the host.
//   node tools/mkpack.js <out> [--store] [--root <dir>] <file or directory>...
//
// A directory goes in with everything under it. An entry is named by its path relative to --root (the current
// directory by default), with '/' between the parts: `node tools/mkpack.js game.cart art levels` names them
// "art/hero.qoi", "levels/one.txt"... Each entry is LZ4-compressed unless that does not make it smaller (or
// --store says not to try). The pack is padded to whole 512-byte sectors, so it can be plugged into a peripheral
// port as a cartridge: ceres run --cart 1=game.cart ...
const fs = require("fs");
const path = require("path");

const NAME_MAX = 31;
const SECTOR = 512;

// ---- CRC-32 (IEEE 802.3), as hash_crc32 ----
const crcTable = new Uint32Array(256);
for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1;
    crcTable[n] = c >>> 0;
}
function crc32(bytes) {
    let c = 0xFFFFFFFF;
    for (const b of bytes) c = crcTable[(c ^ b) & 255] ^ (c >>> 8);
    return (c ^ 0xFFFFFFFF) >>> 0;
}

// ---- LZ4 blocks: a greedy compressor, and a decompressor to check it with ----
function lz4Compress(src) {
    const out = [];
    const n = src.length;
    const table = new Map();
    let anchor = 0;
    const read32 = (i) => (src[i] | (src[i + 1] << 8) | (src[i + 2] << 16) | (src[i + 3] << 24)) >>> 0;
    const putLength = (extra) => {
        while (extra >= 255) { out.push(255); extra -= 255; }
        out.push(extra);
    };
    const sequence = (at, offset, length) => {
        const literals = at - anchor;
        const token = out.length;
        out.push(0);
        let t = Math.min(literals, 15) << 4;
        if (literals >= 15) putLength(literals - 15);
        for (let i = anchor; i < at; i++) out.push(src[i]);
        if (length > 0) {
            out.push(offset & 255, offset >> 8);
            const extra = length - 4;
            t |= Math.min(extra, 15);
            if (extra >= 15) putLength(extra - 15);
        }
        out[token] = t;
    };
    if (n >= 13) {
        const limit = n - 12, matchLimit = n - 5;
        let ip = 0;
        while (ip < limit) {
            const seq = read32(ip);
            const ref = table.get(seq);
            table.set(seq, ip);
            if (ref === undefined || ip - ref > 65535) { ip++; continue; }
            let length = 4;
            while (ip + length < matchLimit && src[ref + length] === src[ip + length]) length++;
            sequence(ip, ip - ref, length);
            ip += length;
            anchor = ip;
        }
    }
    sequence(n, 0, 0);
    return Buffer.from(out);
}

function lz4Decompress(src, size) {
    const out = Buffer.alloc(size);
    let ip = 0, op = 0;
    for (;;) {
        const token = src[ip++];
        let literals = token >> 4;
        if (literals === 15) { let b; do { b = src[ip++]; literals += b; } while (b === 255); }
        src.copy(out, op, ip, ip + literals);
        ip += literals; op += literals;
        if (ip >= src.length) break;
        const offset = src[ip] | (src[ip + 1] << 8);
        ip += 2;
        let length = token & 15;
        if (length === 15) { let b; do { b = src[ip++]; length += b; } while (b === 255); }
        length += 4;
        for (let i = 0; i < length; i++, op++) out[op] = out[op - offset];
    }
    if (op !== size) throw new Error("the LZ4 check failed");
    return out;
}

// ---- the pack ----
function main(argv) {
    let store = false, root = ".";
    const inputs = [];
    let output = null;
    for (let i = 0; i < argv.length; i++) {
        if (argv[i] === "--store") store = true;
        else if (argv[i] === "--root") root = argv[++i];
        else if (output === null) output = argv[i];
        else inputs.push(argv[i]);
    }
    if (output === null || inputs.length === 0) {
        console.error("usage: node tools/mkpack.js <out> [--store] [--root <dir>] <file or directory>...");
        process.exit(2);
    }

    const files = [];
    const walk = (p) => {
        if (fs.statSync(p).isDirectory())
            for (const name of fs.readdirSync(p).sort()) walk(path.join(p, name));
        else
            files.push(p);
    };
    for (const input of inputs) walk(input);

    const entries = files.map((file) => {
        const name = path.relative(root, file).split(path.sep).join("/");
        if (Buffer.byteLength(name) > NAME_MAX || name.startsWith(".."))
            throw new Error(`"${name}" is not a name a pack can hold (at most ${NAME_MAX} bytes, under --root)`);
        const data = fs.readFileSync(file);
        let stored = data;
        if (!store && data.length > 0) {
            const packed = lz4Compress(data);
            lz4Decompress(packed, data.length);            // the pack is only written if every entry comes back
            if (packed.length < data.length) stored = packed;
        }
        return { name, data, stored };
    });
    const names = new Set();
    for (const e of entries) {
        if (names.has(e.name)) throw new Error(`"${e.name}" is in the pack twice`);
        names.add(e.name);
    }

    const HEADER = 16, ENTRY = 48;
    let offset = HEADER + ENTRY * entries.length;
    const parts = [];
    const header = Buffer.alloc(HEADER);
    header.write("CPAK", 0, "latin1");
    header.writeUInt16LE(1, 4);
    header.writeUInt16LE(entries.length, 6);
    header.writeUInt32LE(HEADER, 8);
    parts.push(header);
    const directory = Buffer.alloc(ENTRY * entries.length);
    entries.forEach((e, i) => {
        const at = i * ENTRY;
        directory.write(e.name, at, "utf8");
        directory.writeUInt32LE(offset, at + 32);
        directory.writeUInt32LE(e.data.length, at + 36);
        directory.writeUInt32LE(e.stored.length, at + 40);
        directory.writeUInt32LE(crc32(e.data), at + 44);
        offset += e.stored.length;
    });
    parts.push(directory);
    for (const e of entries) parts.push(e.stored);
    const padding = (SECTOR - (offset % SECTOR)) % SECTOR;
    parts.push(Buffer.alloc(padding));
    fs.writeFileSync(output, Buffer.concat(parts));

    for (const e of entries)
        console.log(`  ${e.name.padEnd(NAME_MAX)} ${String(e.data.length).padStart(8)} ${e.stored === e.data ? "stored" : String(e.stored.length).padStart(8) + " lz4"}`);
    console.log(`wrote ${output}: ${entries.length} entries, ${offset + padding} bytes`);
}

main(process.argv.slice(2));
