// Extract component placements from a VCV module widget constructor by
// transpiling the C++ body to JS and running it against stubs.
// usage: node extract.js <repo> <WidgetName> -> JSON on stdout
const fs = require('fs'), path = require('path');
const [repo, widgetName, panelWmm] = process.argv.slice(2);
const srcDir = path.join(repo, 'src');
const files = fs.readdirSync(srcDir).filter(f => /\.(cpp|hpp|h)$/.test(f)).map(f => path.join(srcDir, f));
{ const own = files.filter(f => new RegExp(`struct\\s+${widgetName}\\b`).test(fs.readFileSync(f, 'utf8'))); for (const f of own) { files.splice(files.indexOf(f), 1); files.push(f); } }
const all = files.map(f => fs.readFileSync(f, 'utf8').replace(/\/\/.*$/gm, '').replace(/\/\*[\s\S]*?\*\//g, '')).join('\n');

function balanced(s, i, open, close) { // s[i]===open; return index after matching close
  let d = 0;
  for (let j = i; j < s.length; j++) { if (s[j] === open) d++; else if (s[j] === close) { d--; if (!d) return j + 1; } }
  throw new Error('unbalanced');
}
function tr(code) {
  let s = code;
  // template args -> string first argument
  s = s.replace(/\b(create\w*|new)\s*<|\b(createWidget\w*|createParam\w*|createInput\w*|createOutput\w*|createLight\w*)\s*</g, m => m);
  let out = '', i = 0;
  const re = /\b(create\w*)\s*</g; let m;
  while ((m = re.exec(s))) {
    const st = m.index + m[0].length - 1; const en = balanced(s, st, '<', '>');
    const T = s.slice(st + 1, en - 1).trim();
    let after = s.slice(en).replace(/^\s*\(/, '');
    out += s.slice(i, m.index) + `${m[1]}(${JSON.stringify(T)}, `; i = en; s = s.slice(0, en) + s.slice(en).replace(/^\s*\(/, ''); re.lastIndex = en;
  }
  out += s.slice(i);
  s = out;
  s = s.replace(/\bnew\s+([\w:]+(?:<[^;()]*>)?)\s*(\([^;]*?\))?/g, (m, T, a) => `newW(${JSON.stringify(T)})`);
  s = s.replace(/->/g, '.').replace(/::/g, '.');
  s = s.replace(/\[([\w\s+*-]+?)\s*\/\s*(\d+)\s*\]/g, '[Math.trunc(($1) / $2)]');
  s = s.replace(/(\d+\.?\d*(?:e[+-]?\d+)?)f\b/gi, '$1').replace(/\(\s*(float|double|int|size_t)\s*\)/g, '');
  s = s.replace(/static_cast<\w+>/g, '');
  s = s.replace(/\bstd\.(sin|cos|sqrt|floor|round|abs|fabs|min|max|pow|atan2)\b/g, 'Math.$1').replace(/\bfabs\b/g, 'Math.abs');
  s = s.replace(/\b(sinf|cosf)\(/g, (m, f) => `Math.${f.slice(0, -1)}(`);
  s = s.replace(/\b(?:static\s+)?(?:constexpr\s+|const\s+)*(?:unsigned\s+)?int\s+([A-Za-z_]\w*)\s*=\s*([^;{]+);/g, 'var $1 = Math.trunc($2);');
  // declarations: const float A[3] = {..}; float x = ..; auto* p = ...
  s = s.replace(/\bin\b/g, 'in_').replace(/\bfunction\b/g, 'function_');
  s = s.replace(/(?<=(?:^|[;{}(])\s*)(?:static\s+)?(?:constexpr\s+|const\s+)*(?:unsigned\s+)?(float|double|int|bool|size_t|auto|Vec|std\.string|math\.Vec|NVGcolor|[A-Z]\w*)\s*[*&]?\s+([A-Za-z_]\w*)\s*((?:\[[^\]]*\])*)\s*(=|;|\{)/g,
    (m, ty, name, arr, op) => {
      if (/^(return|else|new)$/.test(ty)) return m;
      return `var ${name}${op === '{' ? ' = {' : ' ' + op}`;
    });
  s = s.replace(/\busing\s+namespace\s+[\w.:]+\s*;/g, '').replace(/\busing\s+(\w+)\s*=\s*[\w:]+\s*;/g, 'var $1 = dummy;');
  s = s.replace(/\[[&=]?\]\s*\(([^)]*)\)\s*(?:->\s*\w+\s*)?\{/g, (m, a) => '(' + a.split(',').map(x => x.trim().split(/[\s*&]+/).pop()).filter(x => x).join(', ') + ') => {');
  for (let k = 0; k < 4; k++) s = s.replace(/\{([^{};]*)\}/g, (m, inner) => /^[\s\d.,+\-*/()\w"\[\]]*$/.test(inner) && /,|^\s*[-\d.]+\s*$/.test(inner) && !/\bvar\b|return/.test(inner) ? `[${inner}]` : m);
  s = s.replace(/\bVec\(/g, 'V(').replace(/\bmath\.V\(/g, 'V(');
  return s;
}
// constants
let consts = []; const constsPush = x => consts.push(x);
const cre = /(?:^|\n)\s*(?:static\s+)?(?:constexpr|const)\s+(?:static\s+)?(?:float|double|int|size_t|unsigned|Vec|math::Vec)\s+[A-Za-z_]\w*\s*(?:\[[^\]]*\])*\s*=\s*[^;]+;/g;
for (const m of all.matchAll(cre)) consts.push(tr(m[0]));
for (const m of all.matchAll(/#define\s+([A-Z_]\w*)\s+([-\d.]+f?)\s*$/gm)) consts.push(`var ${m[1]} = ${m[2].replace(/f$/, '')};`);
// enums: NAME = value
for (const m of all.matchAll(/enum\s*\w*\s*\{([^}]*)\}/g)) {
  let v = 0;
  for (let part of m[1].split(',')) { part = part.trim(); if (!part) continue;
    const e = part.match(/^([A-Za-z_]\w*)\s*=\s*([-\d]+)$/); const en = part.match(/^ENUMS\((\w+),\s*(\w+)\)/);
    if (e) { consts.push(`var ${e[1]} = ${e[2]};`); v = +e[2] + 1; } else if (en) { v += 1; } else if (/^\w+$/.test(part)) { if (/^NUM_|_LEN$|^N_/.test(part)) {} }
  }
}
// widget ctor
const wi = all.search(new RegExp(`struct\\s+${widgetName}\\b[^{]*\\{`));
if (wi < 0) throw new Error('no widget ' + widgetName);
const cm = new RegExp(`${widgetName}\\s*\\([^)]*\\)\\s*\\{`).exec(all.slice(wi));
const bs = wi + cm.index + cm[0].length - 1;
const body = all.slice(bs + 1, balanced(all, bs, '{', '}') - 1);
const js = tr(body);
const items = [];
const PX = 75 / 25.4;
function V(x, y) { return { x, y, plus(o) { return V(x + o.x, y + o.y); }, minus(o) { return V(x - o.x, y - o.y); }, mult(k) { return V(x * k, y * k); }, div(k){return V(x/k,y/k);} }; }
function mm2px(v) { return typeof v === 'number' ? v * PX : V(v.x * PX, v.y * PX); }
function W(type, pos, centered, kind) { const w = { type, kind, box: { pos: pos || V(0, 0), size: V(0, 0) }, centered, module: null, children: [] }; return new Proxy(w, { get: (t, k) => k in t ? t[k] : dummy, set: (t, k, v) => { t[k] = v; return true; } }); }
const hid = k => { let h = 7; for (const c of k) h = (h * 31 + c.charCodeAt(0)) % 100003; return 1000 + h; };
const dummy = new Proxy(function () {}, { get: (t, k) => k === Symbol.toPrimitive ? () => 0 : k === 'x' || k === 'y' ? 0 : (typeof k === 'string' && /_(PARAM|INPUT|OUTPUT|LIGHT|BUTTON)$/.test(k)) ? hid(k) : dummy, apply: () => dummy, construct: () => dummy });
const mk = kind => (T, pos, ...rest) => { const w = W(T, pos, /Centered/.test(kind), kind); items.push(w); return w; };
const env = {
  V, mm2px, Math, dummy, items,
  createParamCentered: mk('ParamCentered'), createParam: mk('Param'), createInputCentered: mk('InputCentered'), createInput: mk('Input'),
  createOutputCentered: mk('OutputCentered'), createOutput: mk('Output'), createLightCentered: mk('LightCentered'), createLight: mk('Light'),
  createLightParamCentered: mk('ParamCentered'), createLightParam: mk('Param'), createWidget: mk('Widget'), createWidgetCentered: mk('WidgetCentered'),
  newW: T => { const w = W(T, V(0, 0), false, 'new'); items.push(w); return w; },
  RACK_GRID_WIDTH: 15, RACK_GRID_HEIGHT: 380, M_PI: Math.PI, true: true, false: false,
  box: { size: V((+panelWmm || 0) * PX, 380) }, module: null,
  createPanel: () => dummy, setPanel: () => {}, setModule: () => {}, asset: dummy, pluginInstance: null,
  addParam: w => w, addInput: w => w, addOutput: w => w, addChild: w => w, string: dummy,
};
for (const kv of (process.env.EXTRA_CONSTS || '').split(',').filter(Boolean)) { const [k, v] = kv.split('='); env[k] = +v; }
const scope = new Proxy(env, { has: (t, k) => typeof k === 'string' && !['undefined', 'JSON', 'Number', 'console', 'Infinity', 'NaN', 'Array', 'Object', 'String', 'parseFloat', 'Boolean'].includes(k), get: (t, k) => k in t ? t[k] : (k === Symbol.unscopables ? undefined : dummy), set: (t, k, v) => { t[k] = v; return true; } });
const panelW = (all.match(/box\.size\s*=\s*Vec\(\s*RACK_GRID_WIDTH\s*\*\s*(\d+)/) || [])[1];
for (const m of all.matchAll(/static\s+(?:inline\s+)?(?:float|int|double)\s+(\w+)\s*\(([^)]*)\)\s*\{\s*return\s+([^;]+);\s*\}/g)) {
  const args = m[2].split(',').map(x => x.trim().split(/\s+/).pop()).filter(Boolean).join(',');
  consts.push(`var ${m[1]} = function(${args}) { return ${m[3]}; };`);
}
for (const c of consts) { try { (new Function('scope', 'with (scope) {\n' + c.replace(/^\s*var\s+/, '') + '\n}'))(scope); } catch (e) {} }
let code = js;
try { (new Function('scope', 'with (scope) {\n' + code + '\n}'))(scope); }
catch (e) { fs.writeFileSync('/tmp/claude-0/w/fail.js', code); console.error('ERR', e.message); process.exit(1); }
const out = items.filter(w => w.type).map(w => {
  const p = w.box.pos, s = w.box.size;
  return { type: w.type, kind: w.kind, x: +(p.x / PX).toFixed(3), y: +(p.y / PX).toFixed(3), w: +((s.x || 0) / PX).toFixed(3), h: +((s.y || 0) / PX).toFixed(3), centered: w.centered };
});
console.log(JSON.stringify(out, null, 0));
