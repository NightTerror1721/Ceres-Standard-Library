#!/usr/bin/env node
// The pictures of the retro examples (platformer.c, maze.c), drawn here as text - a character a pixel, '.' clear -
// and written as PNGs; tools/img2tiles.js turns each into the header the example includes:
//
//   node examples/art/make_art.js        writes examples/art/*.png
//   node tools/img2tiles.js ...          the headers: the commands are in tools/img2tiles.cases, which the tests
//                                        run to check the headers still match the pictures
"use strict";

const fs = require("fs");
const path = require("path");
const zlib = require("zlib");

// ---- PNG: RGBA, 8 bits, filter 0 on every row ----

const crcTable = Array.from({ length: 256 }, (_, n) => {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    return c >>> 0;
});

function crc32(bytes) {
    let c = 0xffffffff;
    for (const b of bytes) c = crcTable[(c ^ b) & 255] ^ (c >>> 8);
    return (c ^ 0xffffffff) >>> 0;
}

function chunk(kind, body) {
    const head = Buffer.alloc(8);
    head.writeUInt32BE(body.length, 0);
    head.write(kind, 4, "latin1");
    const crc = Buffer.alloc(4);
    crc.writeUInt32BE(crc32(Buffer.concat([head.subarray(4), body])), 0);
    return Buffer.concat([head, body, crc]);
}

function writePng(file, width, height, rgba) {
    const header = Buffer.alloc(13);
    header.writeUInt32BE(width, 0);
    header.writeUInt32BE(height, 4);
    header[8] = 8;   // bits
    header[9] = 6;   // RGBA
    const raw = Buffer.alloc(height * (width * 4 + 1));
    for (let y = 0; y < height; y++)
        for (let x = 0; x < width * 4; x++) raw[y * (width * 4 + 1) + 1 + x] = rgba[y * width * 4 + x];
    const png = Buffer.concat([Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]), chunk("IHDR", header),
        chunk("IDAT", zlib.deflateSync(raw, { level: 9 })), chunk("IEND", Buffer.alloc(0))]);
    fs.writeFileSync(file, png);
}

// Pictures drawn side by side (each a list of rows of the same length) into one PNG, with `colours` naming what each
// character is (0xRRGGBB; '.' is clear).
function sheet(name, colours, pictures) {
    const height = pictures[0].length;
    const widths = pictures.map((p) => p[0].length);
    const width = widths.reduce((a, b) => a + b, 0);
    const rgba = new Array(width * height * 4).fill(0);
    let left = 0;
    pictures.forEach((picture, n) => {
        if (picture.length !== height) throw new Error(`${name}: picture ${n} is ${picture.length} rows, not ${height}`);
        picture.forEach((row, y) => {
            if (row.length !== widths[n]) throw new Error(`${name}: picture ${n} row ${y} is ${row.length} wide, not ${widths[n]}`);
            [...row].forEach((ch, x) => {
                if (ch === ".") return;
                if (!(ch in colours)) throw new Error(`${name}: no colour for '${ch}'`);
                const at = (y * width + left + x) * 4;
                rgba[at] = (colours[ch] >> 16) & 255;
                rgba[at + 1] = (colours[ch] >> 8) & 255;
                rgba[at + 2] = colours[ch] & 255;
                rgba[at + 3] = 255;
            });
        });
        left += widths[n];
    });
    writePng(path.join(__dirname, `${name}.png`), width, height, rgba);
}

// ---- the maze (micro: 8x8 tiles, 4 bits) ----

sheet("maze_tiles", { B: 0x5a3a8a, b: 0x8a6ac0, d: 0x2a1a4a, Y: 0xf8d830, y: 0xc09020, o: 0x60c0f0 }, [
    // 1: a wall of bricks
    ["BBBdBBBd", "bbbdbbbd", "dddddddd", "BdBBBdBB", "bdbbbdbb", "dddddddd", "BBBdBBBd", "bbbdbbbd"],
    // 2: the way out, a star
    ["...Y....", "...Y....", "YYYYYYY.", ".YYYYY..", "..YyY...", ".YY.YY..", ".Y...Y..", "........"],
    // 3: a step of the way found
    ["........", "........", "........", "...oo...", "...oo...", "........", "........", "........"],
]);

sheet("maze_hero", { R: 0xe04040, r: 0x902020, W: 0xffffff, k: 0x101010 }, [
    ["..RRRR..", ".RRRRRR.", ".RWkRWk.", ".RRRRRR.", ".RRRRRR.", ".RRRRRR.", ".R.RR.R.", "R..R..R."],
    ["..RRRR..", ".RRRRRR.", ".RkWRkW.", ".RRRRRR.", ".RRRRRR.", ".RRRRRR.", "R.RR.R.R", ".R..R..."],
]);

// ---- the platformer (retro: 8x8 tiles, 4 bits; a 16x16 hero, 8x8 coins) ----

sheet("platformer_tiles", { G: 0x40c040, g: 0x208020, D: 0x9a6030, d: 0x6a4020, K: 0xc04830, k: 0x802818, L: 0xe0d0c0,
    Q: 0xf0b020, q: 0xa06010, W: 0xffffff, w: 0xd0e0f0, H: 0x3a7a5a, h: 0x2a5a42 }, [
    // 1: grass on the ground
    ["GGGGGGGG", "GgGGGgGG", "gGgggGgg", "DgDDDgDD", "DDDdDDDD", "DdDDDDdD", "DDDDdDDD", "dDDDDDDd"],
    // 2: the ground under it
    ["DDDdDDDD", "DdDDDDdD", "DDDDdDDD", "dDDDDDDd", "DDdDDDDD", "DDDDDdDD", "DdDDDDDD", "DDDDDdDd"],
    // 3: bricks
    ["KKKKKKKL", "KkkkkkkL", "KkkkkkkL", "LLLLLLLL", "KKKLKKKK", "kkkLkkkk", "kkkLkkkk", "LLLLLLLL"],
    // 4: a block to hit
    ["QQQQQQQq", "QqqqqqQq", "QqQQqqQq", "QqqqQqQq", "QqqQqqQq", "QqqqqqQq", "QqqQqqQq", "qqqqqqqq"],
    // 5, 6: a cloud (the background layer)
    ["........", "...WWW..", "..WWWWW.", ".WWWWWWW", "WWWwwwWW", "WWwwwwwW", ".wwwwwww", "........"],
    ["........", "..WW....", ".WWWW...", "WWWWWW..", "WwwwWWW.", "wwwwwWW.", "wwwwww..", "........"],
    // 7, 8: a hill (the background layer)
    ["......HH", "....HHHH", "...HHhHH", "..HHHHHH", ".HHhHHHH", ".HHHHHhH", "HHHHHHHH", "HhHHHHHH"],
    ["HH......", "HHHH....", "HHhHH...", "HHHHHH..", "HHHHhHH.", "HhHHHHH.", "HHHHHHHH", "HHHHHhHH"],
    // 9: inside a hill
    ["HHHHHHHH", "HHhHHHHH", "HHHHHHhH", "HHHHHHHH", "HhHHHHHH", "HHHHHhHH", "HHHHHHHH", "HHHhHHHH"],
]);

sheet("platformer_hero", { B: 0x3050d0, b: 0x203090, S: 0xf0c090, s: 0xc09060, R: 0xd03030, K: 0x301810, W: 0xffffff }, [
    ["......RRRRR.....", ".....RRRRRRRR...", ".....KKKSSKS....", "....KSKSSSKSSS..", "....KSKKSSSKSSS.", "....KKSSSSKKKK..",
     "......SSSSSSS...", ".....RRBRRR.....", "....RRRBRRBRRR..", "...RRRRBBBBRRRR.", "...SSRBWBBWBRSS.", "...SSSBBBBBBSSS.",
     "...SSBBBBBBBBSS.", ".....BBB..BBB...", "....KKK....KKK..", "...KKKK....KKKK."],
    ["......RRRRR.....", ".....RRRRRRRR...", ".....KKKSSKS....", "....KSKSSSKSSS..", "....KSKKSSSKSSS.", "....KKSSSSKKKK..",
     "......SSSSSSS...", ".....RRRRBRR....", "....RRRRRBBRR...", "....RRRRBBWBB...", "....SSRBBBBBB...", "....SSSBBBBBB...",
     ".....SBBBBBB....", "......BBBBB.....", ".....KKKKKK.....", ".....KKKK......."],
    ["......RRRRR.....", ".....RRRRRRRR...", ".....KKKSSKS....", "....KSKSSSKSSS..", "....KSKKSSSKSSS.", "....KKSSSSKKKK..",
     "......SSSSSSS...", "....RRRRBRRR....", "...RRRRRRBBRRSS.", "..SRRRRRBBWBBSS.", "..SS.BBBBBBBB...", "....BBBBBBBBB...",
     "...BBBBBBBBBB...", "..KBBB....BBBK..", "..KK.......KKK..", "...K........K..."],
    ["......RRRRR.....", ".....RRRRRRRR...", ".....KKKSSKS....", "....KSKSSSKSSS..", "....KSKKSSSKSSS.", "....KKSSSSKKKK..",
     "......SSSSSSS...", ".....RRRRBRR....", "....RRRRRBBRR...", "....RRRRBBWBB...", "....SSRBBBBBB...", "....SSSBBBBBB...",
     ".....SBBBBBB....", "......BBBBB.....", ".....KKKKKK.....", ".....KKKK......."],
]);

sheet("platformer_coin", { Y: 0xf8d830, y: 0xc09020, W: 0xffffe0 }, [
    ["..YYYY..", ".YWYYYY.", "YWYyyYYy", "YWYyYYYy", "YYYyYYYy", "YYYyYYYy", ".YYYYYy.", "..yyyy.."],
    ["...YY...", "..YWYY..", "..WYyY..", "..WYyY..", "..YYyY..", "..YYyY..", "..YYYy..", "...yy..."],
    ["...YY...", "...WY...", "...WY...", "...Yy...", "...Yy...", "...Yy...", "...Yy...", "...yy..."],
    ["...YY...", "..YYWY..", "..YyYW..", "..YyYW..", "..YyYY..", "..YyYY..", "..yYYY..", "...yy..."],
]);
