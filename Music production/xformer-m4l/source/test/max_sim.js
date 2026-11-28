// Runs the BUILT device files in a fake Max: v8 glue + the patch around it + both jsui views.
// Usage: node max_sim.js <distdir>
const fs = require('fs'), vm = require('vm'), path = require('path');
const dist = process.argv[2];
let failed = false;
const fail = (m) => { console.log('FAIL', m); failed = true; };

function makeDisplay(device, view, sink) {
  const calls = [];
  const mg = new Proxy({ size: view === 'editor' ? [820, 300] : [236, 116],
    text_measure: (s) => [String(s).length * 6, 10] }, {
    get: (t, k) => (k in t ? t[k] : (...a) => { calls.push(k); }) , set: (t, k, v) => { t[k] = v; return true; } });
  const ctx = { mgraphics: mg, jsarguments: ['xf_display.js', device, view], Math, String, Array, Object,
    outlet: (n, ...a) => sink(a), post: console.log, arrayfromargs: (a) => Array.prototype.slice.call(a), messagename: '' };
  vm.createContext(ctx);
  vm.runInContext(fs.readFileSync(path.join(dist, 'xf_display.js'), 'utf8'), ctx);
  ctx.send = (list) => {
    const name = String(list[0]), args = list.slice(1);
    ctx.messagename = name;
    if (typeof ctx[name] === 'function' && !['anything'].includes(name)) ctx[name](...args);
    else ctx.anything(...args);
    ctx.paint();
  };
  return ctx;
}

function makeDevice(device, controlValues) {
  const rig = { coll: {}, on: {}, events: [], problems: [], pattr: null, slot: 0, ui: [], sets: [] };
  const emit = (p, v) => { if (v > 0) { if (rig.on[p] !== undefined) rig.problems.push('double on ' + p); rig.on[p] = rig.slot; } else delete rig.on[p]; rig.events.push([rig.slot, p, v]); };
  let ctx;
  const tasks = [];
  const outlet = (n, ...a) => {
    const l = a.length === 1 && Array.isArray(a[0]) ? a[0] : a;
    if (n === 0) { if (l[0] === 'clear') rig.coll = {}; else if (l[0] === 'remove') delete rig.coll[l[1]]; else rig.coll[l[0]] = l.slice(1); }
    else if (n === 1) emit(l[0], l[1]);
    else if (n === 2) { for (const p of Object.keys(rig.on)) emit(+p, 0); }
    else if (n === 3) { rig.ui.forEach(u => u.send(l)); }
    else if (n === 4) { rig.pattr = l.slice(); send(['xfstate'].concat(l)); }   // pattr echoes back
  };
  function Task(fn) { this.fn = fn; this.schedule = () => { if (!tasks.includes(this)) tasks.push(this); }; }
  const controls = Object.entries(controlValues || {}).map(([k, v]) => ({ varname: k, getvalueof: () => v, message: (...a) => rig.sets.push([k].concat(a)) }));
  ctx = { Math, String, Array, Object, JSON, Float32Array, console, outlet, post: (s) => { rig.problems.push('post: ' + s); }, Task,
    arrayfromargs: (a) => Array.prototype.slice.call(a), messagename: '', autowatch: 0 };
  ctx.patcher = { applydeep: (fn) => controls.forEach(fn) };
  vm.createContext(ctx);
  vm.runInContext(fs.readFileSync(path.join(dist, 'xf_' + device + '.js'), 'utf8'), ctx);
  function send(list) {
    const name = String(list[0]); ctx.messagename = name;
    if (typeof ctx[name] === 'function') ctx[name].apply({ patcher: ctx.patcher }, list.slice(1));
    else ctx.anything.apply(null, list.slice(1));
  }
  rig.send = send;
  rig.runTasks = () => { while (tasks.length) tasks.shift().fn(); };
  rig.run = (from, to) => {
    for (let s = from; s < to; s++) {
      rig.slot = s;
      const l = rig.coll[s];
      if (l) for (let i = 0; i < l.length; i += 2) emit(l[i], l[i + 1]);
      delete rig.coll[s];
      if (rig.onSlot) rig.onSlot(s);
      send(['pos', s]);
      if (s % 50 === 0) rig.runTasks();
    }
  };
  rig.stop = () => { send(['playing', 0]); };
  rig.ui.push(makeDisplay(device, 'main', (a) => send(a)), makeDisplay(device, 'editor', (a) => send(a)));
  return rig;
}

const devs = {
  algo: { algorithm: 7, flow: 9, power: 14, divisor: 3, scale: 2, rootNote: 3, loopLength: 16, rstAny: 0, fixedVel: 100 },
  stochastic: { complexity: 70, burst: 40, rhythmMode: 1, melodyMode: 0, divisor: 3, scale: 1, size: 12, first: 2 },
  note: { firstStep: 1, lastStep: 16, divisor: 3, runMode: 0, fill: 0 },
  indexed: { activeLength: 8, firstStep: 1, stretch: 1, runMode: 0, loop: 1, scale: 1, modA: 30, routeASource: 1, routeATarget: 2, routeAGroups: 0 },
  discrete: { divisor: 11, clockSource: 1, gateLength: 40, rangeHigh: 100, rangeLow: -100, input: 0, scanner: 0, loop: 1, playMode: 1 },
  phaseflux: { divisor: 3, traversal: 5, pitchMode: 0, firstStage: 1, lastStage: 16, pulseNudge: 1, warpNudge: 10 },
  fractal: { divisor: 3, record: 0, lock: 0, cadence: 0, fidelity: 1, loopFirst: 1, loopLast: 16, recordFirst: 1, recordLast: 16,
    ornFirst: 1, ornLast: 16, ornRate: 40, ornIntensity: 80, branchCount: 2, thru: 0 },
};
for (const [dev, vals] of Object.entries(devs)) {
  const r = makeDevice(dev, vals);
  r.send(['tempo', 128]); r.send(['timesig', 4, 4]);
  r.send(['loaded']); r.runTasks();
  if (dev !== 'algo' && !r.pattr) fail(dev + ': no pattern stored on load');
  r.run(0, 3000);
  if (dev === 'note') { r.send(['edit', 'layer', 'note']); r.send(['edit', 'step', 3, 12]); r.send(['edit', 'page', 1]); r.send(['edit', 'copypage', 0, 1]); r.send(['act', 'initSteps']); }
  if (dev === 'stochastic') { r.send(['act', 'newRhythm']); r.send(['edit', 'ticket', 'deg', 2, -1]); r.send(['act', 'undoRhythm']); r.send(['act', 'newMelody']); }
  if (dev === 'algo') r.send(['act', 'reseed']);
  if (['indexed', 'discrete', 'phaseflux', 'fractal'].includes(dev)) {
    r.send(['edit', 'layer', { indexed: 'noteIndex', discrete: 'noteIndex', phaseflux: 'basePitch', fractal: 'pitch' }[dev]]);
    r.send(['edit', 'step', 2, 5]); r.send(['edit', 'page', 1]); r.send(['edit', 'page', 0]); r.send(['edit', 'copypage', 0, 1]);
    r.send(['getdisplay']);
  }
  if (dev === 'fractal') {
    // record two bars of a played line (Section capture, 1/16 cells), then listen to it come back
    const line = [48, 50, 51, 55, 58, 55, 51, 50];
    const before = r.pattr.slice();
    r.send(['p', 'record', 1]);
    r.onSlot = (s) => {
      if (s < 5000 || s >= 5000 + 768) return;
      const k = s - 5000;
      if (k % 48 === 0) r.send(['midinote', line[(k / 48) % 8], 100]);
      if (k % 48 === 30) r.send(['midinote', line[(k / 48 | 0) % 8], 0]);
    };
    r.run(3000, 5000 + 800);
    r.send(['p', 'record', 0]); r.onSlot = null;
    r.run(5800, 7400); r.runTasks();
    const fe = r.events.filter(e => e[0] >= 5900 && e[2] > 0).map(e => e[1]);
    const played = new Set(line);
    const heard = fe.filter(p => played.has(p)).length;
    if (heard < fe.length * 0.5) fail('fractal: replay after recording does not use the recorded notes ' + fe.slice(0, 20));
    if (JSON.stringify(before) === JSON.stringify(r.pattr)) fail('fractal: recording did not change the stored trunk');
    console.log('fractal replay after recording:', fe.slice(0, 24).join(' '));
  }
  r.send(['act', 'reset']); r.send(['p', 'rstAny', 1]); r.send(['midinote', 40, 100]); r.send(['midinote', 40, 0]);
  r.send(['p', 'rstAny', 0]);
  const t0 = dev === 'fractal' ? 7400 : 3000;
  r.run(t0, t0 + 2000); r.stop(); r.runTasks();
  const ons = r.events.filter(e => e[2] > 0).length;
  if (!ons) fail(dev + ': no notes');
  if (Object.keys(r.on).length) fail(dev + ': stuck notes');
  if (r.problems.length) fail(dev + ': ' + r.problems.slice(0, 3).join(' | '));
  // reload from the stored pattern: a second device must play the same notes
  if (dev !== 'algo') {
    const r2 = makeDevice(dev, vals); r2.send(['tempo', 128]);
    r2.send(['xfstate'].concat(r.pattr)); r2.send(['loaded']); r2.runTasks();
    const r3 = makeDevice(dev, vals); r3.send(['tempo', 128]);
    r3.send(['xfstate'].concat(r.pattr)); r3.send(['loaded']); r3.runTasks();
    r2.run(0, 2000); r3.run(0, 2000);
    const a = JSON.stringify(r2.events), b = JSON.stringify(r3.events);
    if (a !== b || !r2.events.length) fail(dev + ': restored pattern does not replay the same');
  }
  console.log(dev + ': ' + ons + ' notes, UI sets ' + JSON.stringify(r.sets) + ', state ' + (r.pattr ? r.pattr.length : 0) + ' numbers');
}
process.exit(failed ? 1 : 0);
