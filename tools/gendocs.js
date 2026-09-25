#!/usr/bin/env node
// Generates docs/reference/: one page per header, from the header itself, and an index.
//   node tools/gendocs.js
//
// The headers are the documentation: each opens with a comment that says what it is for, and its declarations
// carry their own comments. A page is that opening comment as prose, then the rest of the header - grouped under
// its "// ---- name ----" lines - as C, comments and all. Nothing is written by hand, so the pages cannot drift
// from the code; run this again after changing a header.
"use strict";
const fs = require("fs");
const path = require("path");

const Root = path.resolve(__dirname, "..");
const include = path.join(Root, "include");
const outDir = path.join(Root, "docs", "reference");

function headers(dir, prefix = "") {
    const out = [];
    for (const entry of fs.readdirSync(dir, { withFileTypes: true }).sort((a, b) => a.name.localeCompare(b.name))) {
        const rel = prefix + entry.name;
        if (entry.isDirectory()) out.push(...headers(path.join(dir, entry.name), rel + "/"));
        else if (entry.name.endsWith(".h") && !entry.name.startsWith("__")) out.push(rel);
    }
    return out;
}

// The page for one header: its opening comment, then its body in sections.
function page(rel) {
    const lines = fs.readFileSync(path.join(include, rel), "utf8").replace(/\r\n/g, "\n").split("\n");
    let i = 0;
    const skip = () => { while (i < lines.length && (/^\s*$/.test(lines[i]) || /^#pragma once/.test(lines[i]) || /^#include/.test(lines[i]))) i++; };
    skip();
    const intro = [];
    while (i < lines.length && /^\s*\/\//.test(lines[i]) && !/^\s*\/\/ ----/.test(lines[i])) {
        intro.push(lines[i].replace(/^\s*\/\/ ?/, ""));
        i++;
    }
    skip();
    // The body, cut where a "// ---- name ----" line starts a section.
    const sections = [];
    let current = { title: null, lines: [] };
    for (; i < lines.length; i++) {
        const m = lines[i].match(/^\s*\/\/ -{2,}\s*(.*?)\s*-{2,}\s*$/);
        if (m) {
            if (current.lines.some((l) => l.trim())) sections.push(current);
            current = { title: m[1], lines: [] };
            continue;
        }
        current.lines.push(lines[i]);
    }
    if (current.lines.some((l) => l.trim())) sections.push(current);

    // The intro as prose: paragraphs, with the indented lines (examples) kept as code.
    const md = [`# \`<${rel}>\``, ""];
    let code = [];
    let prose = [];
    const flushProse = () => { if (prose.length) { md.push(prose.join(" ").replace(/\s+/g, " ").trim(), ""); prose = []; } };
    const flushCode = () => { if (code.length) { md.push("```c", ...code.map((l) => l.replace(/^ {2}/, "")), "```", ""); code = []; } };
    for (const line of intro) {
        if (/^\s{2,}\S/.test(line)) { flushProse(); code.push(line); continue; }
        flushCode();
        if (!line.trim()) { flushProse(); continue; }
        prose.push(line);
    }
    flushProse();
    flushCode();
    for (const s of sections) {
        if (s.title) md.push(`## ${s.title[0].toUpperCase()}${s.title.slice(1)}`, "");
        while (s.lines.length && !s.lines[0].trim()) s.lines.shift();
        while (s.lines.length && !s.lines[s.lines.length - 1].trim()) s.lines.pop();
        md.push("```c", ...s.lines, "```", "");
    }
    const first = (intro.join(" ").replace(/\s+/g, " ").trim().split(/(?<=\.)\s/)[0] || "");
    return { md: md.join("\n"), summary: first.length > 160 ? first.slice(0, 157).replace(/\s+\S*$/, "") + "..." : first };
}

fs.rmSync(outDir, { recursive: true, force: true });
fs.mkdirSync(outDir, { recursive: true });
const all = headers(include);
const index = ["# The Ceres standard library: reference", "",
    "Generated from the headers by `node tools/gendocs.js` - one page each, their own comments and declarations.", ""];
for (const group of [["The C library", (h) => !h.includes("/")], ["Ceres: the machine and the extras", (h) => h.includes("/")]]) {
    index.push(`## ${group[0]}`, "", "| Header | What it is |", "| --- | --- |");
    for (const rel of all.filter(group[1])) {
        const { md, summary } = page(rel);
        const file = rel.replace(/\//g, "_").replace(/\.h$/, ".md");
        fs.writeFileSync(path.join(outDir, file), md);
        index.push(`| [\`<${rel}>\`](${file}) | ${summary.replace(/\|/g, "\\|")} |`);
    }
    index.push("");
}
fs.writeFileSync(path.join(outDir, "README.md"), index.join("\n"));
console.log(`wrote docs/reference: ${all.length} pages and an index`);
