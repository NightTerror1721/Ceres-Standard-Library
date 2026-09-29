#!/usr/bin/env node
// The pictures tools/img2tiles.js is tested with (tools/img2tiles.cases lists the runs and what they must give):
// every kind of PNG it reads - palette with transparency, grey, RGB, RGBA - with every row filter, and tiles that
// repeat, flipped, in colours that need two banks.
//
//   node tests/img2tiles/make_pictures.js      writes tests/img2tiles/*.png
"use strict";

const fs = require("fs");
const path = require("path");
const zlib = require("zlib");

const crcTable = Array.from({ length: 256 }, (_, n) => {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    return c >>> 0;
});
const crc32 = (bytes) => {
    let c = 0xffffffff;
    for (const b of bytes) c = crcTable[(c ^ b) & 255] ^ (c >>> 8);
    return (c ^ 0xffffffff) >>> 0;
};
function chunk(kind, body) {
    const head = Buffer.alloc(8);
    head.writeUInt32BE(body.length, 0);
    head.write(kind, 4, "latin1");
    const crc = Buffer.alloc(4);
    crc.writeUInt32BE(crc32(Buffer.concat([head.subarray(4), body])), 0);
    return Buffer.concat([head, body, crc]);
}

// Rows of raw bytes, each filtered with filter (row % 5): none, sub, up, average, Paeth.
function filtered(rows, bytesPerPixel) {
    const out = [];
    let previous = Buffer.alloc(rows[0].length);
    rows.forEach((row, y) => {
        const filter = y % 5;
        const line = Buffer.alloc(row.length);
        for (let i = 0; i < row.length; i++) {
            const a = i >= bytesPerPixel ? row[i - bytesPerPixel] : 0, b = previous[i], c = i >= bytesPerPixel ? previous[i - bytesPerPixel] : 0;
            let predict = 0;
            if (filter === 1) predict = a;
            else if (filter === 2) predict = b;
            else if (filter === 3) predict = (a + b) >> 1;
            else if (filter === 4) {
                const p = a + b - c, pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
                predict = pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
            }
            line[i] = (row[i] - predict) & 255;
        }
        out.push(Buffer.from([filter]), line);
        previous = row;
    });
    return Buffer.concat(out);
}

function png(file, width, height, type, depth, rows, bytesPerPixel, extra = []) {
    const header = Buffer.alloc(13);
    header.writeUInt32BE(width, 0);
    header.writeUInt32BE(height, 4);
    header[8] = depth;
    header[9] = type;
    fs.writeFileSync(path.join(__dirname, file), Buffer.concat([Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
        chunk("IHDR", header), ...extra, chunk("IDAT", zlib.deflateSync(filtered(rows, bytesPerPixel))), chunk("IEND", Buffer.alloc(0))]));
}

// A 32x16 picture of 8x8 tiles: a tile, it flipped each way, it again, and one in other colours; the second row has
// a clear tile, a tile of 12 colours (a bank of its own) and the first again.
function colourAt(x, y) {
    const col = x >> 3, row = y >> 3, tx = x & 7, ty = y & 7;
    const shape = (px, py) => ((px * 3 + py * 5) % 7 === 0 ? -1 : 1 + ((px + 2 * py) % 3));   // a lopsided pattern, 3 colours and clear
    const base = [0xff0000, 0x00ff00, 0x0000ff];
    if (row === 0) {
        const [px, py] = [[tx, ty], [7 - tx, ty], [tx, 7 - ty], [tx, ty]][col] ?? [tx, ty];
        const i = shape(px, py);
        if (i < 0) return -1;
        return col === 3 ? [0xffff00, 0x00ffff, 0xff00ff][i - 1] : base[i - 1];
    }
    if (col === 0) return -1;
    if (col === 1) return 0x101010 * (1 + ((tx + ty * 2) % 12));
    const i = shape(tx, ty);
    return i < 0 ? -1 : base[i - 1];
}

// RGBA, filters on 4-byte pixels.
{
    const rows = [];
    for (let y = 0; y < 16; y++) {
        const row = Buffer.alloc(32 * 4);
        for (let x = 0; x < 32; x++) {
            const c = colourAt(x, y);
            if (c >= 0) row.writeUInt32BE(((c << 8) | 255) >>> 0, 4 * x);
        }
        rows.push(row);
    }
    png("tiles_rgba.png", 32, 16, 6, 8, rows, 4);
}

// RGB, the clear colour a key (--key 00FF80).
{
    const rows = [];
    for (let y = 0; y < 16; y++) {
        const row = Buffer.alloc(32 * 3);
        for (let x = 0; x < 32; x++) {
            const c = colourAt(x, y) < 0 ? 0x00ff80 : colourAt(x, y);
            row[3 * x] = c >> 16;
            row[3 * x + 1] = (c >> 8) & 255;
            row[3 * x + 2] = c & 255;
        }
        rows.push(row);
    }
    png("tiles_rgb.png", 32, 16, 2, 8, rows, 3);
}

// A palette of 16 at 4 bits a pixel, entry 0 clear through tRNS: two sprite frames of 16x8.
{
    const palette = Buffer.alloc(16 * 3);
    for (let i = 0; i < 16; i++) {
        palette[3 * i] = i * 16;
        palette[3 * i + 1] = 255 - i * 16;
        palette[3 * i + 2] = (i * 97) & 255;
    }
    const rows = [];
    for (let y = 0; y < 8; y++) {
        const row = Buffer.alloc(16);
        for (let x = 0; x < 32; x++) {
            const i = (x + y) % 5 === 0 ? 0 : x < 16 ? 1 + ((x * y) % 7) : 8 + ((x + y) % 8);
            row[x >> 1] |= x & 1 ? i : i << 4;
        }
        rows.push(row);
    }
    png("frames_palette.png", 32, 8, 3, 4, rows, 1, [chunk("PLTE", palette), chunk("tRNS", Buffer.from([0]))]);
}

// Grey, 8 bits: sixty shades for one 8-bit tile layer.
{
    const rows = [];
    for (let y = 0; y < 16; y++) {
        const row = Buffer.alloc(16);
        for (let x = 0; x < 16; x++) row[x] = ((x * 16 + y) * 4) % 240 + 8;
        rows.push(row);
    }
    png("shades_grey.png", 16, 16, 0, 8, rows, 1);
}
