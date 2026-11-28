// Simulates the Max patch around host.js: the clock reports one slot at a time,
// the [coll] plays what was written for that slot, [midiflush] ends hanging notes.
// Checks for stuck notes, double note-ons, and that starting mid-song gives the
// same notes as playing through (position lock).
const load = require('./load');
const C = load(['common.js', 'tuesday.js', 'stochastic.js', 'note.js', 'indexed.js', 'discrete.js', 'phaseflux.js', 'fractal.js', 'host.js']);

function makeRig(ctorName, setup) {
  const rig = { coll: {}, events: [], on: {}, slot: 0, problems: [] };
  const emit = (p, v) => {
    if (v > 0) { if (rig.on[p]) rig.problems.push(`double on ${p} @${rig.slot}`); rig.on[p] = rig.slot; }
    else delete rig.on[p];
    rig.events.push([rig.slot, p, v]);
  };
  const out = {
    coll: (l) => { if (l[0] === 'clear') rig.coll = {}; else if (l[0] === 'remove') delete rig.coll[l[1]]; else rig.coll[l[0]] = l.slice(1); },
    now: (pair) => emit(pair[0], pair[1]),
    flush: () => { for (const p of Object.keys(rig.on)) emit(+p, 0); },
    display: () => {}, state: () => {}
  };
  rig.host = new C.Host(C[ctorName], out);
  if (setup) setup(rig.host.engine, rig.host);
  rig.run = (from, to) => {
    for (let s = from; s < to; s++) {
      rig.slot = s;
      const l = rig.coll[s];
      if (l) for (let i = 0; i < l.length; i += 2) emit(l[i], l[i + 1]);
      delete rig.coll[s];
      rig.host.pos(s);
    }
  };
  rig.stop = () => { rig.host.stop(); out.flush(); };
  return rig;
}

function notesFrom(events, minSlot) {
  return events.filter(e => e[2] > 0 && e[0] >= minSlot).map(e => e.join(':'));
}

const BAR = 384, configs = {
  TuesdayEngine: (e) => { e.setParam('algorithm', 7); e.setParam('power', 14); e.setParam('glide', 60); e.setParam('trill', 80); },
  StochasticEngine: (e) => { e.initFresh(); e.setParam('burst', 60); e.setParam('legatoProb', 30); e.setParam('slide', 40); e.setParam('rhythmMode', 0); e.setParam('melodyMode', 0); },
  NoteEngine: (e) => { e.initFresh(); e.setStep(2, 'retrigger', 3); e.setStep(4, 'length', 0); e.setStep(6, 'pulseCount', 2); e.setStep(6, 'gateMode', 2); e.setStep(3, 'slide', 1); e.setStep(5, 'accumulator', 1); },
  IndexedEngine: (e) => { e.initFresh(); e.setStep(2, 'slide', 1); e.setParam('runMode', 4); e.setStep(5, 'gateLength', 96); },
  DiscreteEngine: (e) => { e.initFresh(); e.setParam('clockSource', 1); e.setParam('divisor', 96); e.setParam('gateLength', 60); },
  PhaseFluxEngine: (e) => { e.initFresh(); e.setParam('traversal', 5); e.setStage(1, 'accumulatorStep', 2); e.setStage(2, 'temporalRepeat', 2); },
  FractalEngine: (e) => { e.initFresh(); e.setParam('ornRate', 60); e.setParam('ornIntensity', 90); e.setParam('branchCount', 3); e.setParam('orderMode', 3); e.setParam('trackDelay', 2); },
};
let failed = false;
for (const [name, setup] of Object.entries(configs)) {
  for (const algo of name === 'TuesdayEngine' ? [0, 1, 2, 3, 5, 7, 9, 11, 12, 13, 15] : [0]) {
    const s2 = (e, h) => { setup(e, h); if (name === 'TuesdayEngine') e.setParam('algorithm', algo); };
    const a = makeRig(name, s2); a.run(0, BAR * 16); a.stop();
    const b = makeRig(name, s2); b.run(BAR * 8, BAR * 16); b.stop();
    const na = notesFrom(a.events, BAR * 8 + 40), nb = notesFrom(b.events, BAR * 8 + 40);
    const same = JSON.stringify(na) === JSON.stringify(nb);
    const stuck = Object.keys(a.on).length + Object.keys(b.on).length;
    const ons = a.events.filter(e => e[2] > 0).length;
    const msg = `${name}${name === 'TuesdayEngine' ? ' algo ' + algo : ''}: ${ons} notes, position-lock ${same ? 'ok' : 'DIFFERENT'}, stuck ${stuck}, problems ${a.problems.length + b.problems.length}`;
    console.log(msg);
    if (!same || stuck || a.problems.length) { failed = true; if (a.problems.length) console.log('  ', a.problems.slice(0, 3)); }
  }
}
for (const [name, setup] of Object.entries(configs)) for (const bars of [2, 3]) {
  const s2 = (e, h) => { setup(e, h); h.param('resetBars', bars); };
  const a = makeRig(name, s2); a.run(0, BAR * 16); a.stop();
  const b = makeRig(name, s2); b.run(BAR * 9, BAR * 16); b.stop();
  const na = notesFrom(a.events, BAR * 9 + 40), nb = notesFrom(b.events, BAR * 9 + 40);
  const same = JSON.stringify(na) === JSON.stringify(nb), stuck = Object.keys(a.on).length + Object.keys(b.on).length;
  console.log(`${name} reset every ${bars} bars: position-lock ${same ? 'ok' : 'DIFFERENT'}, stuck ${stuck}, problems ${a.problems.length + b.problems.length}`);
  if (!same || stuck || a.problems.length || b.problems.length) failed = true;
}
// manual reset + stop/start + reset every 2 bars + loop jump on the Note device
const r = makeRig('NoteEngine', (e, h) => { configs.NoteEngine(e); h.param('resetBars', 2); });
r.run(0, 1000); r.host.manualReset(); r.run(1000, 3000); r.run(500, 1500); r.stop();
r.run(0, 800); r.host.midiIn(60, 100); r.run(800, 2000); r.stop();
console.log(`Note resets/jumps: stuck ${Object.keys(r.on).length}, problems ${r.problems.length}`);
if (Object.keys(r.on).length || r.problems.length) { failed = true; console.log(r.problems.slice(0, 5)); }
process.exit(failed ? 1 : 0);
