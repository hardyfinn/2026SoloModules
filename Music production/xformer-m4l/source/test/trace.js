// Runs an engine the way the firmware simulator does (1 ms steps at 30 BPM)
// and writes "tick gate cv" on every change, for diffing against ref/*.out
const load = require('./load');
const fs = require('fs');
const [,, engineName, scen, outPath] = process.argv;
const files = { tuesday: ['common.js', 'tuesday.js'], stochastic: ['common.js', 'stochastic.js'], note: ['common.js', 'note.js'],
  indexed: ['common.js', 'indexed.js'], discrete: ['common.js', 'discrete.js'], phaseflux: ['common.js', 'phaseflux.js'],
  fractal: ['common.js', 'note.js', 'fractal.js'] }[engineName];
const C = load(files);
const ctor = { tuesday: 'TuesdayEngine', stochastic: 'StochasticEngine', note: 'NoteEngine', indexed: 'IndexedEngine',
  discrete: 'DiscreteEngine', phaseflux: 'PhaseFluxEngine', fractal: 'FractalEngine' }[engineName];
let simMs = 0;
// reference traces for the newer engines run with XF_FROZEN_US, so the module's
// Clock::tickPosition() is exactly the tick being processed, like in the device
const host = { gate() {}, stepInfo() {}, tickPos: (t) => t };
const eng = new C[ctor](host);
eng.quirks = true;   // reproduce the module bit for bit
// Fractal listens to a Note track (track 0, "p ..." lines); it is track 1 on the module
let parent = null;
if (engineName === 'fractal') {
  parent = new C.NoteEngine({ gate() {}, stepInfo() {}, tickPos: (t) => t });
  parent.quirks = true;
  eng.rngSeed = 0x6f7261 + 1;
  eng.inlineSource = { gate: () => !!parent._gateOutput, cv: () => Math.fround(parent._cvOutput) };
}
let ticks = 2000, renew = false;
for (const line of fs.readFileSync(scen, 'utf8').split('\n')) {
  const t = line.trim().split(/\s+/); if (!t[0]) continue;
  if (t[0] === 'engine') continue;
  if (t[0] === 'ticks') { ticks = +t[1]; continue; }
  if (t[0] === 'renew') { renew = true; continue; }
  if (parent && t[0] === 'p') { parent.setRefParam ? parent.setRefParam(t.slice(1)) : parent.setParam(t[1], +t[2]); continue; }
  if (eng.setRefParam) eng.setRefParam(t); else eng.setParam(t[0], +t[1]);
}
if (eng.refInit) eng.refInit(renew);
if (parent) { if (parent.refInit) parent.refInit(false); parent.reset(); }
eng.reset();
const out = [];
let lastExtra = 0, lastGate = -1, lastCv = -1000, tick = 0;
const f4 = (x) => (Math.round(Math.fround(x) * 10000) / 10000).toFixed(4);
for (let ms = 0; ; ms++) {
  simMs = ms;
  // the simulator's clock: tick n happens at (n+1) * 10416 us (30 BPM)
  while ((tick + 1) * 10416 <= ms * 1000) {
    // Engine.cpp: after a tick that updated the CV, one extra update(0), at most every 25 ms
    if (eng.barBoundary && tick > 0 && tick % 768 === 0) eng.barBoundary();
    if (parent) {
      if (tick > 0 && tick % 768 === 0) parent.barBoundary();
      const a = parent.tick(tick), b = eng.tick(tick);
      if ((a || b) && ms - lastExtra >= 25) { lastExtra = ms; parent.update(); eng.update(); }
    } else
    if (eng.tick(tick) && eng.update && ms - lastExtra >= 25) { lastExtra = ms; eng.update(); }
    tick++;
  }
  if (parent) parent.update();
  if (eng.update) eng.update();
  if (tick >= ticks) break;
  const g = eng._gateOutput ? 1 : 0, cv = Math.fround(eng._cvOutput);
  if (process.env.XF_ALL) out.push(`ms ${ms} ${Math.max(0, tick - 1)} ${g} ${f4(cv)}`); else
  if (g !== lastGate || (g && cv !== lastCv)) { out.push(`${tick - 1} ${g} ${f4(cv)}`); lastGate = g; lastCv = cv; }
}
fs.writeFileSync(outPath, out.join('\n') + '\n');
