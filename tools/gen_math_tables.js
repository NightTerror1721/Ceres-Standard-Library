// Generates tests/math_tables.inc (node tools/gen_math_tables.js tests/math_tables.inc): reference values for test_math.c, computed in IEEE double by node's
// Math.* from the exact float inputs and rounded to float. The output is committed, so node is only needed to change the sample points.
const fs = require("fs");
const f = Math.fround;
const out = process.argv[2];


// References the Math object does not have, in double: erf by its series and erfc by its continued fraction,
// gamma by Lanczos (g = 7) with the reflection formula, lgamma by Stirling past 100.
function erfD(x) {
  const ax = Math.abs(x);
  let r;
  if (ax < 2.5) {
    let sum = 0, term = ax;
    for (let n = 0; ; n++) { const t = term / (2 * n + 1); sum += n % 2 ? -t : t; if (Math.abs(t) < 1e-18 * Math.abs(sum) || n > 200) break; term *= ax * ax / (n + 1); }
    r = 2 / Math.sqrt(Math.PI) * sum;
  } else r = 1 - erfcD(ax);
  return x < 0 ? -r : r;
}
function erfcD(x) {
  if (x < 0) return 2 - erfcD(-x);
  if (x < 2.5) return 1 - erfD(x);
  let t = x;
  for (let k = 400; k >= 1; k--) t = x + (k / 2) / t;
  return Math.exp(-x * x) / Math.sqrt(Math.PI) / t;
}
const lanczos = [0.99999999999980993, 676.5203681218851, -1259.1392167224028, 771.32342877765313, -176.61502916214059,
  12.507343278686905, -0.13857109526572012, 9.9843695780195716e-6, 1.5056327351493116e-7];
function gammaD(x) {
  if (x < 0.5) return Math.PI / (Math.sin(Math.PI * x) * gammaD(1 - x));
  x -= 1;
  let a = lanczos[0];
  const t = x + 7.5;
  for (let i = 1; i < 9; i++) a += lanczos[i] / (x + i);
  return Math.sqrt(2 * Math.PI) * Math.pow(t, x + 0.5) * Math.exp(-t) * a;
}
function lgammaD(x) {
  if (x > 100) return (x - 0.5) * Math.log(x) - x + 0.5 * Math.log(2 * Math.PI) + 1 / (12 * x) - 1 / (360 * x * x * x);
  return Math.log(Math.abs(gammaD(x)));
}

const one = {
  sin: { fn: Math.sin, xs: [-6000, -1000, -100, -25.5, -10, -7, -6.2831855, -3.1415927, -1.5707964, -1, -0.5, -0.001, -1e-5, 1e-5, 0.001, 0.1, 0.5, 0.7853982, 1, 1.5, 1.5707964, 2, 3, 3.1415927, 4, 5, 6, 6.2831855, 7, 10, 25.5, 100, 1000, 6000, 6500, 12345.678, 1e5, 1e6, 31415926, 1e10, -1e15, 1e20, 1e30, 3e38] },
  cos: { fn: Math.cos, xs: [-6000, -1000, -100, -25.5, -10, -7, -6.2831855, -3.1415927, -1.5707964, -1, -0.5, -0.001, -1e-5, 1e-5, 0.001, 0.1, 0.5, 0.7853982, 1, 1.5, 1.5707964, 2, 3, 3.1415927, 4, 5, 6, 6.2831855, 7, 10, 25.5, 100, 1000, 6000, 6500, 12345.678, 1e5, 1e6, 31415926, 1e10, -1e15, 1e20, 1e30, 3e38] },
  tan: { fn: Math.tan, xs: [-1000, -100, -10, -3, -1.5, -1, -0.5, -1e-3, 1e-5, 0.1, 0.5, 0.7853982, 1, 1.5, 1.5707964, 2, 3, 3.1415927, 4, 10, 100, 1000, 1e5, 1e10, -1e20, 3e38] },
  asin: { fn: Math.asin, xs: [-1, -0.99, -0.9, -0.5, -0.1, -1e-4, 1e-4, 0.1, 0.3, 0.5, 0.7071068, 0.9, 0.99, 0.9999999, 1] },
  acos: { fn: Math.acos, xs: [-1, -0.99, -0.9, -0.5, -0.1, -1e-4, 0, 1e-4, 0.1, 0.3, 0.5, 0.7071068, 0.9, 0.99, 0.9999999] },
  atan: { fn: Math.atan, xs: [-1e6, -100, -10, -2, -1, -0.5, -0.1, -1e-3, 1e-5, 0.001, 0.1, 0.4142136, 0.5, 1, 1.5, 2, 10, 100, 1e6, 1e20] },
  exp: { fn: Math.exp, xs: [-87, -50, -20, -10, -5, -1, -0.5, -0.1, -1e-5, 1e-5, 0.1, 0.3465736, 0.5, 1, 2, 5, 10, 20, 50, 80, 88, 88.7] },
  exp2: { fn: (x) => Math.pow(2, x), xs: [-126, -100, -10, -1.5, -1, -0.5, -0.1, 0.1, 0.5, 1, 1.5, 3, 10, 30, 100, 127] },
  expm1: { fn: Math.expm1, xs: [-10, -2, -1, -0.5, -0.3, -1e-3, -1e-6, 1e-8, 1e-6, 1e-3, 0.1, 0.3, 0.5, 1, 5, 20, 80] },
  log: { fn: Math.log, xs: [1.5e-38, 1e-10, 1e-3, 0.1, 0.5, 0.9, 0.99999994, 1.0000001, 1.5, 2, 2.718282, 10, 100, 12345.678, 1e10, 1e30, 3e38] },
  log2: { fn: Math.log2, xs: [1.5e-38, 1e-10, 1e-3, 0.1, 0.5, 0.9, 1.0000001, 1.5, 2, 3, 8, 10, 100, 1000, 12345.678, 1e10, 1e30, 3e38] },
  log10: { fn: Math.log10, xs: [1.5e-38, 1e-10, 1e-3, 0.1, 0.5, 0.9, 1.0000001, 1.5, 2, 10, 100, 1000, 12345.678, 1e10, 1e30, 3e38] },
  log1p: { fn: Math.log1p, xs: [-0.99, -0.5, -0.1, -1e-3, -1e-7, 1e-8, 1e-5, 1e-3, 0.1, 0.24, 0.25, 0.5, 1, 10, 1e10] },
  sinh: { fn: Math.sinh, xs: [-20, -5, -1, -0.5, -1e-3, 1e-5, 0.1, 0.5, 1, 2, 5, 10, 20, 50, 88, 89] },
  cosh: { fn: Math.cosh, xs: [-20, -5, -1, -0.5, -1e-3, 1e-5, 0.1, 0.5, 1, 2, 5, 10, 20, 50, 88, 89] },
  tanh: { fn: Math.tanh, xs: [-8, -2, -1, -0.5, -1e-3, 1e-5, 0.1, 0.5, 1, 2, 5, 8, 8.9] },
  asinh: { fn: Math.asinh, xs: [-1e10, -100, -10, -1, -0.1, -1e-4, 1e-6, 0.01, 0.5, 1, 2, 10, 100, 1e10, 1e20] },
  acosh: { fn: Math.acosh, xs: [1.0001, 1.5, 2, 5, 10, 100, 1e5, 1e10, 1e20] },
  atanh: { fn: Math.atanh, xs: [-0.99, -0.5, -0.1, -1e-4, 1e-6, 0.1, 0.5, 0.9, 0.99, 0.999] },
  cbrt: { fn: Math.cbrt, xs: [-1000, -27, -8, -2, -0.125, -1e-3, 1e-3, 0.5, 1, 2, 3, 8, 27, 100, 1e6, 1e12, 1e30, 3e38, 1e-30, 1e-40] },
  erf: { fn: erfD, xs: [-3, -1.5, -1, -0.5, -1e-3, 1e-6, 0.1, 0.3, 0.5, 0.9, 0.99, 1, 1.2, 1.5, 2, 2.5, 3, 3.5, 3.9, 4.5] },
  erfc: { fn: erfcD, xs: [-2, -1, -0.5, 0, 1e-4, 0.3, 0.8, 1, 1.3, 1.9, 2.1, 3, 3.9, 4.1, 5, 6, 7.5, 9] },
  tgamma: { fn: gammaD, xs: [-3.5, -2.5, -1.5, -0.5, -1e-3, 1e-5, 0.1, 0.5, 1, 1.5, 2, 2.5, 3, 4.5, 5, 7.25, 10, 15.5, 20, 25, 30, 34, 35] },
  lgamma: { fn: lgammaD, xs: [-20.5, -3.5, -0.5, 0.1, 0.5, 0.9, 1.1, 1.5, 2.5, 3, 5, 10, 20, 34, 35.5, 50, 100, 1000, 1e6, 1e20, 1e30] },
};

const two = {
  pow: { fn: Math.pow, ps: [[2, 10], [2, 0.5], [2, -3], [10, -2], [10, 0.5], [1.5, 64], [7.5, 3.3], [0.5, 8], [0.1, 2.5], [100, 0.25], [-2, 3], [-2, 4], [-3, -3], [2, 100], [2, -100], [1.0001, 10000], [3, 20.5], [1.01, 1000], [9, 0.5], [2.5, 2.5], [0.001, 3], [1e10, 3.5], [5, 33], [1.1, -40]] },
  atan2: { fn: Math.atan2, ps: [[1, 1], [1, -1], [-1, -1], [-1, 1], [0.5, 2], [2, 0.5], [1e-3, -5], [-1e-3, -5], [3, 4], [1e20, 1e-20], [1e-20, 1e20], [1, 0], [-1, 0], [0.3, -0.7], [-100, 3]] },
  hypot: { fn: Math.hypot, ps: [[3, 4], [5, 12], [1e20, 1e20], [1e30, 1e30], [1e-30, 1e-30], [1e-20, 1], [1, 1e-20], [0.1, 0.2], [-3, 4], [7, -24], [1e-38, 1e-38]] },
};

const num = (v) => {
  const s = f(v).toPrecision(9);
  return (s.includes(".") || s.includes("e") ? s : s + ".0") + "f";
};
let text = "// Generated by tools/gen_math_tables.js: reference values from IEEE double (node Math.*), rounded to float.\n// Do not edit by hand.\n";
for (const [name, t] of Object.entries(one)) {
  const xs = t.xs.map(f);
  const ys = xs.map((x) => f(t.fn(x)));
  text += `static const float t_${name}_x[${xs.length}] = { ${xs.map(num).join(", ")} };\n`;
  text += `static const float t_${name}_y[${xs.length}] = { ${ys.map(num).join(", ")} };\n`;
}
for (const [name, t] of Object.entries(two)) {
  const a = t.ps.map((p) => f(p[0])), b = t.ps.map((p) => f(p[1]));
  const ys = a.map((x, i) => f(t.fn(x, b[i])));
  text += `static const float t_${name}_a[${a.length}] = { ${a.map(num).join(", ")} };\n`;
  text += `static const float t_${name}_b[${a.length}] = { ${b.map(num).join(", ")} };\n`;
  text += `static const float t_${name}_y[${a.length}] = { ${ys.map(num).join(", ")} };\n`;
}
fs.writeFileSync(out, text);
console.log("wrote", out, text.length, "bytes");
