#!/usr/bin/env node
// img2tiles: a PNG into what the GPU's level V2 draws (ceres/tiles.h, ceres/sprite.h) - a palette, tiles and a map
// for a tile layer, or the pictures of sprites - as C (a header of static arrays) or CASM (@rodata).
//
//   node tools/img2tiles.js level.png --tile 8 --bpp 4 -o level.h
//   node tools/img2tiles.js hero.png --sprites 16x16 -o hero.h
//   node tools/img2tiles.js level.png --asm -o level.casm
//
// TILES (the default). The picture is cut into tiles of 8x8 or 16x16 pixels (--tile), left to right, top to bottom.
// Tiles that are the same - or the same flipped, unless --no-flip, or the same in another palette bank - are kept
// once (unless --no-dedupe), and tile 0 is always the clear one, so an empty entry of the map shows nothing. The map has an entry for every tile of the
// picture, padded with tile 0 to a size a layer takes (32, 64 or 128 tiles a side; --no-pad keeps the picture's).
//
// SPRITES (--sprites WxH, each 8, 16, 32 or 64). The picture is cut into frames of that size, left to right, top to
// bottom, and each becomes a sprite's picture, starting on a multiple of 32 bytes; the graphic numbers go with them.
//
// COLOURS. A pixel with alpha under 128 is clear (colour 0), and so is --key RRGGBB. At 4 bits a pixel (--bpp 4) each
// tile or frame takes its colours from one bank of 15 (plus clear): the tool shares the banks out, up to 8 for a
// map (its entries have 3 bits of palette) and 16 for sprites. At 8 bits there is one palette of 255 colours.
//
// Needs nothing but Node. PNG: 8-bit grey, RGB, RGBA, grey+alpha and palette (1, 2, 4 or 8 bits), not interlaced.
"use strict";

const fs = require("fs");
const path = require("path");
const zlib = require("zlib");

function fail(message) {
    process.stderr.write(`img2tiles: ${message}\n`);
    process.exit(1);
}

// ---- PNG ----

function readPng(file) {
    const data = fs.readFileSync(file);
    const signature = [0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a];
    if (data.length < 8 || signature.some((b, i) => data[i] !== b))
        fail(`${file} is not a PNG`);
    let at = 8, width = 0, height = 0, depth = 0, type = 0, interlace = 0;
    let palette = null, trns = null;
    const idat = [];
    while (at + 8 <= data.length) {
        const length = data.readUInt32BE(at);
        const kind = data.toString("latin1", at + 4, at + 8);
        const body = data.subarray(at + 8, at + 8 + length);
        at += 12 + length;
        if (kind === "IHDR") {
            width = body.readUInt32BE(0);
            height = body.readUInt32BE(4);
            depth = body[8];
            type = body[9];
            interlace = body[12];
        } else if (kind === "PLTE") palette = body;
        else if (kind === "tRNS") trns = body;
        else if (kind === "IDAT") idat.push(body);
        else if (kind === "IEND") break;
    }
    if (interlace !== 0) fail(`${file}: interlaced PNGs are not read`);
    const channels = { 0: 1, 2: 3, 3: 1, 4: 2, 6: 4 }[type];
    if (channels === undefined) fail(`${file}: colour type ${type} is not read`);
    if (depth !== 8 && !(type === 3 && [1, 2, 4].includes(depth)) && !(type === 0 && [1, 2, 4].includes(depth)))
        fail(`${file}: ${depth}-bit ${type === 3 ? "palette" : "channels"} are not read`);
    const raw = zlib.inflateSync(Buffer.concat(idat));
    const bitsPerPixel = depth * channels;
    const stride = Math.ceil(width * bitsPerPixel / 8);
    const step = Math.max(1, bitsPerPixel >> 3);
    const rows = [];
    let previous = Buffer.alloc(stride);
    for (let y = 0; y < height; y++) {
        const filter = raw[y * (stride + 1)];
        const line = Buffer.from(raw.subarray(y * (stride + 1) + 1, (y + 1) * (stride + 1)));
        for (let i = 0; i < stride; i++) {
            const a = i >= step ? line[i - step] : 0;
            const b = previous[i];
            const c = i >= step ? previous[i - step] : 0;
            let add = 0;
            if (filter === 1) add = a;
            else if (filter === 2) add = b;
            else if (filter === 3) add = (a + b) >> 1;
            else if (filter === 4) {
                const p = a + b - c, pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
                add = pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
            } else if (filter !== 0) fail(`${file}: bad filter ${filter}`);
            line[i] = (line[i] + add) & 255;
        }
        rows.push(line);
        previous = line;
    }
    // Every pixel as [r, g, b, a].
    const pixels = new Array(width * height);
    const sample = (line, index) => {
        const bit = index * depth;
        return (line[bit >> 3] >> (8 - depth - (bit & 7))) & ((1 << depth) - 1);
    };
    for (let y = 0; y < height; y++) {
        const line = rows[y];
        for (let x = 0; x < width; x++) {
            let rgba;
            if (type === 3) {
                const i = sample(line, x);
                if (!palette || 3 * i + 2 >= palette.length) fail(`${file}: a pixel names a colour the palette does not have`);
                rgba = [palette[3 * i], palette[3 * i + 1], palette[3 * i + 2], trns && i < trns.length ? trns[i] : 255];
            } else if (type === 0) {
                const v = Math.round(sample(line, x) * 255 / ((1 << depth) - 1));
                rgba = [v, v, v, 255];
            } else if (type === 2) rgba = [line[3 * x], line[3 * x + 1], line[3 * x + 2], 255];
            else if (type === 4) rgba = [line[2 * x], line[2 * x], line[2 * x], line[2 * x + 1]];
            else rgba = [line[4 * x], line[4 * x + 1], line[4 * x + 2], line[4 * x + 3]];
            pixels[y * width + x] = rgba;
        }
    }
    return { width, height, pixels };
}

// ---- the options ----

function parseArguments(argv) {
    const options = { tile: 8, bpp: 4, sprites: null, key: null, dedupe: true, flip: true, pad: true, asm: false, name: null, output: null, input: null };
    for (let i = 0; i < argv.length; i++) {
        const arg = argv[i];
        const value = () => {
            if (i + 1 >= argv.length) fail(`${arg} needs a value`);
            return argv[++i];
        };
        if (arg === "--tile") options.tile = Number(value());
        else if (arg === "--bpp") options.bpp = Number(value());
        else if (arg === "--sprites") {
            const m = /^(\d+)x(\d+)$/.exec(value());
            if (!m) fail("--sprites takes WxH, as 16x16");
            options.sprites = { width: Number(m[1]), height: Number(m[2]) };
        } else if (arg === "--key") {
            const text = value();
            if (!/^#?[0-9a-fA-F]{6}$/.test(text)) fail("--key takes RRGGBB");
            options.key = parseInt(text.replace("#", ""), 16);
        } else if (arg === "--no-dedupe") options.dedupe = false;
        else if (arg === "--no-flip") options.flip = false;
        else if (arg === "--no-pad") options.pad = false;
        else if (arg === "--asm") options.asm = true;
        else if (arg === "--name") options.name = value();
        else if (arg === "-o" || arg === "--output") options.output = value();
        else if (arg === "-h" || arg === "--help") {
            process.stdout.write(fs.readFileSync(__filename, "utf8").split("\n").slice(1, 22).map((l) => l.replace(/^\/\/ ?/, "")).join("\n") + "\n");
            process.exit(0);
        } else if (arg.startsWith("-")) fail(`unknown option ${arg}`);
        else if (options.input === null) options.input = arg;
        else fail(`one picture at a time (${options.input}, then ${arg})`);
    }
    if (options.input === null) fail("no picture given (node tools/img2tiles.js --help)");
    if (![8, 16].includes(options.tile)) fail("--tile is 8 or 16");
    if (![4, 8].includes(options.bpp)) fail("--bpp is 4 or 8");
    if (options.sprites && (![8, 16, 32, 64].includes(options.sprites.width) || ![8, 16, 32, 64].includes(options.sprites.height)))
        fail("a sprite is 8, 16, 32 or 64 pixels a side");
    if (options.name === null)
        options.name = path.basename(options.input, path.extname(options.input)).replace(/[^A-Za-z0-9_]/g, "_").replace(/^([0-9])/, "_$1");
    if (!/^[A-Za-z_][A-Za-z0-9_]*$/.test(options.name)) fail(`${options.name} is not a name C or CASM takes`);
    return options;
}

// ---- colours ----

// A block of the picture (a tile or a frame) as colours: 0x00RRGGBB, or -1 where it is clear.
function block(image, left, top, width, height, key) {
    const out = new Array(width * height);
    for (let y = 0; y < height; y++)
        for (let x = 0; x < width; x++) {
            const [r, g, b, a] = image.pixels[(top + y) * image.width + left + x];
            const rgb = (r << 16) | (g << 8) | b;
            out[y * width + x] = a < 128 || rgb === key ? -1 : rgb;
        }
    return out;
}

// The banks of 15 colours the blocks share (4 bpp): the blocks with the most colours are placed first, each in the
// bank it has the most colours in common with where they fit, else in a new one.
function shareBanks(blocks, maxBanks, what) {
    const sets = blocks.map((b) => new Set(b.filter((c) => c >= 0)));
    sets.forEach((set, i) => { if (set.size > 15) fail(`${what} ${i} has ${set.size} colours; 4 bits a pixel take 15 (and clear)`); });
    const order = sets.map((_, i) => i).sort((a, b) => sets[b].size - sets[a].size || a - b);
    const banks = [];
    const bankOf = new Array(blocks.length);
    for (const i of order) {
        let best = -1, bestShared = -1;
        banks.forEach((bank, n) => {
            let shared = 0, added = 0;
            for (const c of sets[i]) (bank.has(c) ? shared++ : added++);
            if (bank.size + added <= 15 && shared > bestShared) { best = n; bestShared = shared; }
        });
        if (best < 0) {
            banks.push(new Set());
            best = banks.length - 1;
        }
        for (const c of sets[i]) banks[best].add(c);
        bankOf[i] = best;
    }
    if (banks.length > maxBanks) fail(`the colours need ${banks.length} banks of 15; there are ${maxBanks}`);
    // Each bank's colours in the order they first appear, for a palette that is the same from run to run.
    const lists = banks.map(() => []);
    blocks.forEach((b, i) => {
        for (const c of b) if (c >= 0 && !lists[bankOf[i]].includes(c)) lists[bankOf[i]].push(c);
    });
    return { banks: lists, bankOf };
}

// Colour indices for every block and the palette they index: banks of 16 (index 0 clear) at 4 bpp, one of 256 at 8.
function index(blocks, bpp, maxBanks, what) {
    if (bpp === 8) {
        const colours = [];
        for (const b of blocks) for (const c of b) if (c >= 0 && !colours.includes(c)) colours.push(c);
        if (colours.length > 255) fail(`the picture has ${colours.length} colours; 8 bits a pixel take 255 (and clear)`);
        const palette = [0, ...colours];
        while (palette.length < 256) palette.push(0);
        const lookup = new Map(colours.map((c, i) => [c, i + 1]));
        return { palette, indices: blocks.map((b) => b.map((c) => (c < 0 ? 0 : lookup.get(c)))), bankOf: blocks.map(() => 0) };
    }
    const { banks, bankOf } = shareBanks(blocks, maxBanks, what);
    const palette = [];
    for (const bank of banks) {
        palette.push(0, ...bank);
        while (palette.length % 16 !== 0) palette.push(0);
    }
    if (palette.length === 0) palette.push(...new Array(16).fill(0));
    const indices = blocks.map((b, i) => b.map((c) => (c < 0 ? 0 : banks[bankOf[i]].indexOf(c) + 1)));
    return { palette, indices, bankOf };
}

// Indices as bytes: at 4 bpp two to a byte, the left one in the high half.
function pack(indices, bpp) {
    if (bpp === 8) return indices.slice();
    const out = [];
    for (let i = 0; i < indices.length; i += 2) out.push((indices[i] << 4) | (indices[i + 1] ?? 0));
    return out;
}

function flipped(pixels, size, fx, fy) {
    const out = new Array(pixels.length);
    for (let y = 0; y < size; y++)
        for (let x = 0; x < size; x++)
            out[y * size + x] = pixels[(fy ? size - 1 - y : y) * size + (fx ? size - 1 - x : x)];
    return out;
}

// ---- the two kinds of output ----

function makeTiles(image, options) {
    const size = options.tile;
    if (image.width % size || image.height % size) fail(`the picture is ${image.width}x${image.height}, not a whole number of ${size}x${size} tiles`);
    const cols = image.width / size, rows = image.height / size;
    const blocks = [];
    for (let row = 0; row < rows; row++)
        for (let col = 0; col < cols; col++) blocks.push(block(image, col * size, row * size, size, size, options.key));
    const { palette, indices, bankOf } = index(blocks, options.bpp, 8, "tile");

    // Tile 0 is the clear one; then every tile the map needs, once. A tile is its colour indices: the bank they are
    // looked up in is the map entry's, so the same shape in other colours is the same tile.
    const tiles = [new Array(size * size).fill(0)];
    const known = new Map([[tiles[0].join(","), 0]]);
    const map = [];
    indices.forEach((pixels, i) => {
        let entry = -1;
        if (options.dedupe) {
            const clear = pixels.every((p) => p === 0);
            if (clear) entry = 0;
            const variants = options.flip ? [[0, 0], [1, 0], [0, 1], [1, 1]] : [[0, 0]];
            for (const [fx, fy] of variants) {
                if (entry >= 0) break;
                const found = known.get(flipped(pixels, size, fx, fy).join(","));
                if (found !== undefined) entry = found | (fx ? 0x2000 : 0) | (fy ? 0x4000 : 0);
            }
        }
        if (entry < 0) {
            tiles.push(pixels);
            entry = tiles.length - 1;
            known.set(pixels.join(","), entry);
        }
        if (tiles.length > 1024) fail("the picture needs more than 1024 tiles; a map entry numbers 1024");
        map.push(entry | (options.bpp === 4 && (entry & 0x3ff) !== 0 ? bankOf[i] << 10 : 0));
    });

    const mapSize = (n) => (n <= 32 ? 32 : n <= 64 ? 64 : n <= 128 ? 128 : fail(`the map is ${n} tiles across; a layer takes 128`));
    const mapWidth = options.pad ? mapSize(cols) : cols;
    const mapHeight = options.pad ? mapSize(rows) : rows;
    const padded = new Array(mapWidth * mapHeight).fill(0);
    for (let row = 0; row < rows; row++)
        for (let col = 0; col < cols; col++) padded[row * mapWidth + col] = map[row * cols + col];

    const bytes = [];
    for (const t of tiles) bytes.push(...pack(t, options.bpp));
    return {
        summary: `${tiles.length} tiles of ${size}x${size} at ${options.bpp} bpp, a map of ${mapWidth}x${mapHeight} (the picture's ${cols}x${rows})`,
        constants: [["TILE_COUNT", tiles.length], ["TILE_SIZE", size], ["TILE_BYTES", size * size * options.bpp / 8], ["MAP_WIDTH", mapWidth], ["MAP_HEIGHT", mapHeight], ["BPP", options.bpp]],
        arrays: [["palette", "u32", palette], ["tiles", "u8", bytes], ["map", "u16", padded]],
    };
}

function makeSprites(image, options) {
    const { width, height } = options.sprites;
    if (image.width % width || image.height % height) fail(`the picture is ${image.width}x${image.height}, not a whole number of ${width}x${height} frames`);
    const blocks = [];
    for (let top = 0; top < image.height; top += height)
        for (let left = 0; left < image.width; left += width) blocks.push(block(image, left, top, width, height, options.key));
    const { palette, indices, bankOf } = index(blocks, options.bpp, 16, "frame");
    const bytes = [], graphics = [];
    for (const pixels of indices) {
        graphics.push(bytes.length / 32);
        bytes.push(...pack(pixels, options.bpp));
        while (bytes.length % 32 !== 0) bytes.push(0);
    }
    const sizeCode = (n) => ({ 8: 0, 16: 1, 32: 2, 64: 3 })[n];
    return {
        summary: `${blocks.length} sprite pictures of ${width}x${height} at ${options.bpp} bpp`,
        constants: [["FRAMES", blocks.length], ["WIDTH", width], ["HEIGHT", height], ["BPP", options.bpp],
            ["ATTRIBUTES", ((sizeCode(width) << 20) | (sizeCode(height) << 22) | (options.bpp === 8 ? 1 << 28 : 0)) >>> 0]],
        arrays: [["palette", "u32", palette], ["pictures", "u8", bytes], ["graphic", "u16", graphics], ["bank", "u8", bankOf.map((b) => (options.bpp === 8 ? 0 : b))]],
    };
}

// ---- writing it ----

function hex(value, type) {
    const digits = { u8: 2, u16: 4, u32: 6 }[type];
    return "0x" + value.toString(16).toUpperCase().padStart(digits, "0");
}

function lines(values, type, perLine) {
    const out = [];
    for (let i = 0; i < values.length; i += perLine) out.push("    " + values.slice(i, i + perLine).map((v) => hex(v, type)).join(", ") + ",");
    return out;
}

function writeC(result, options, source) {
    const upper = options.name.toUpperCase();
    const ctype = { u8: "unsigned char", u16: "unsigned short", u32: "unsigned int" };
    const out = [`// Made by tools/img2tiles.js from ${source}: ${result.summary}.`, "#pragma once", ""];
    for (const [name, value] of result.constants) out.push(`#define ${upper}_${name} ${value}`);
    for (const [name, type, values] of result.arrays) {
        out.push("", `static const ${ctype[type]} ${options.name}_${name}[${values.length}] = {`);
        out.push(...lines(values, type, type === "u8" ? 16 : 8));
        out.push("};");
    }
    return out.join("\n") + "\n";
}

function writeCasm(result, options, source) {
    const upper = options.name.toUpperCase();
    const out = [`// Made by tools/img2tiles.js from ${source}: ${result.summary}.`, ""];
    for (const [name, value] of result.constants) out.push(`global const ${upper}_${name} = ${value}`);
    out.push("", "@rodata");
    // An array literal of CASM is on one line.
    for (const [name, type, values] of result.arrays)
        out.push("", `global let ${options.name}_${name}: ${type}[${values.length}] = [${values.map((v) => hex(v, type)).join(", ")}]`);
    return out.join("\n") + "\n";
}

function main() {
    const options = parseArguments(process.argv.slice(2));
    const image = readPng(options.input);
    const result = options.sprites ? makeSprites(image, options) : makeTiles(image, options);
    const source = path.basename(options.input) + (options.sprites ? ` --sprites ${options.sprites.width}x${options.sprites.height}` : ` --tile ${options.tile}`) + ` --bpp ${options.bpp}`;
    const text = options.asm ? writeCasm(result, options, source) : writeC(result, options, source);
    if (options.output) fs.writeFileSync(options.output, text);
    else process.stdout.write(text);
}

main();
