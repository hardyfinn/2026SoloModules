// Checks 2 to 7 of the build spec, on the real planner and core.   node ref/checks.js
// (Check 1, machine hands against the reference engine, is ref/check_words.py.)
const sim = require('./sim.js');
const makeCore = require('./core_sim.js');
const path = require('path');
const fs = require('fs');
const OUT = path.join(__dirname, 'out');
const REC = path.join(OUT, 'builtin.wav');
let fails = 0;
function report(name, ok, detail) { console.log((ok ? 'ok    ' : 'FAIL  ') + name + (detail ? ': ' + detail : '')); if (!ok) fails++; }

// record every segment and fader event the planner writes
function instrument(r) { return r; }
function runLogged(script) {
	const segs = [];
	const D = require('../src/skratch.js');
	const r = sim.run(Object.assign({}, script, {hook: true}));
	return r;
}

// --- 2. speeds and cue moves, 64 bars of Instinct at hands 50 and 100
for (const h of [50, 100]) {
	const script = {record: REC, tempo: 96, beats: 64 * 4, hands: h, seed: 3, ctl: {mode: 2, walk: 100}, logsegs: true};
	const r = sim.run(script);
	let worst = 0, worstLazer = 0, n = 0;
	for (const s of r.segs) {
		if (s.kind !== 1) continue;
		n++;
		const avg = Math.abs(s.p1 - s.p0) / s.dur;
		if (s.audible) { if (s.vcap > 3.01) worstLazer = Math.max(worstLazer, avg); else worst = Math.max(worst, avg); }
	}
	report('2. stroke speed, hands ' + h, worst <= 3.0001 && worstLazer <= 4.5001,
		n + ' strokes, fastest heard ' + worst.toFixed(2) + 'x, lazer flick ' + worstLazer.toFixed(2) + 'x');
	let leak = 0;
	const ramp = Math.ceil((0.0006 + 0.0016 * h / 100) * 44100) + 2;     // the fader takes FADE to close and open
	for (const m of r.cueMoves) for (let t = m[0] + ramp; t < m[1] - ramp; t++) if (r.gain[t] > 0.05) { leak++; }
	report('2. fader closed during cue moves, hands ' + h, leak === 0, r.cueMoves.length + ' moves, ' + leak + ' samples open');
}

// --- 3. playing speed is flat: Release from the start of a noise record
{
	const sr = 44100, n = sr * 3, L = new Float32Array(n);
	let a = 12345; for (let i = 0; i < n; i++) { a = (a * 1103515245 + 12345) & 0x7fffffff; L[i] = (a / 0x7fffffff - 0.5) * 0.5; }
	sim.writeWav(path.join(OUT, 'noise.wav'), L, L, sr);
	const r = sim.run({record: path.join(OUT, 'noise.wav'), cues: [0], tempo: 120, beats: 6, hands: 0, ctl: {quant: 3},
		events: [{beat: 0.5, note: 36 + 13, vel: 127}, {beat: 5.5, note: 36 + 13, vel: 0}]});
	// compare band levels of output and input over 1 s of steady play
	const spb = sr * 60 / 120, o0 = Math.round(1.0 * spb + 0.3 * sr), m = sr;
	let best = 0, bd = 1e9;
	for (let lag = -400; lag < 400; lag++) {     // the record starts a little after the let-go
		let d = 0; for (let i = 0; i < 4000; i += 4) { const x = r.outL[o0 + i] - L[o0 + i - Math.round(0.5 * spb) + lag + 0]; d += x * x; }
		if (d < bd) { bd = d; best = lag; }
	}
	const fft = require('./fft.js');
	const bandsIn = fft.bands(L.subarray(o0 - Math.round(0.5 * spb) + best, o0 - Math.round(0.5 * spb) + best + m), sr);
	const bandsOut = fft.bands(r.outL.subarray(o0, o0 + m), sr);
	let worst = 0, det = [];
	for (let i = 0; i < bandsIn.length; i++) {
		const db = 10 * Math.log10(bandsOut[i].p / bandsIn[i].p);
		det.push(bandsIn[i].lo + '-' + bandsIn[i].hi + ' ' + db.toFixed(2));
		worst = Math.max(worst, Math.abs(db));
	}
	report('3. playing speed flat within 0.5 dB, 30 Hz to 16 kHz', worst < 0.5, det.join(', ') + ' dB');
}

// --- 4. a held record is silent with the fader open
{
	const SR = 44100, F = 400000;
	const BUF = {cut: {frames: F, ch: [new Float32Array(F), new Float32Array(F)]}, q: {frames: 3072, ch: [new Float32Array(3072)]},
		fq: {frames: 2048, ch: [new Float32Array(2048)]}, st: {frames: 16, ch: [new Float32Array(16)]}};
	for (let i = 0; i < F; i++) BUF.cut.ch[0][i] = BUF.cut.ch[1][i] = Math.sin(i * 0.05) * 3;
	const c = makeCore(BUF, SR);
	Object.assign(c.P, {haverec: 1, rlen: F, recsr4: SR * 4, fgen: 1, tremor: 0, playing: 0});
	BUF.fq.ch[0].set([0, 1, 1, 0], 0); c.P.ftail = 1;
	c.P.posset = 0.5; c.P.poscmd = 1;
	let pk = 0, g = 0;
	for (let i = 0; i < SR; i++) { const o = c.step(0); if (i > SR / 2) pk = Math.max(pk, Math.abs(o[0])); g = c.H.h_g; }
	report('4. held record silent', pk < 1e-4 && g > 0.99, 'peak ' + (20 * Math.log10(pk + 1e-12)).toFixed(1) + ' dB with the fader at ' + g.toFixed(2));
}

// --- 5. keys
{
	const BABY = 36, REL = 36 + 13, CHIRP = 39;
	// held baby: units of 2u = half a beat, from beat 1; let go at 2.6 -> units at 1, 1.5, 2, 2.5 and no more
	let r = sim.run({record: REC, tempo: 96, beats: 6, hands: 0, logsegs: true, events: [{beat: 0.9, note: BABY, vel: 64}, {beat: 2.6, note: BABY, vel: 0}]});
	let starts = r.words.map(w => +w.b.toFixed(3));
	report('5. held key repeats in whole units', JSON.stringify(starts) === JSON.stringify([1, 1.5, 2, 2.5]), 'units at ' + starts.join(', '));
	// release plays until the key comes up (at 3.1 -> cut at 3.25)
	r = sim.run({record: REC, tempo: 96, beats: 6, hands: 0, logsegs: true, events: [{beat: 0.9, note: REL, vel: 127}, {beat: 2.1, note: REL, vel: 0}]});
	const spb = 44100 * 60 / 96;
	const openT = r.gain.findIndex(x => x > 0.5), closeT = r.gain.findIndex((x, i) => i > openT && x < 0.5);
	report('5. release plays until the key comes up', Math.abs(openT / spb - 1) < 0.02 && Math.abs(closeT / spb - 2.29) < 0.03,
		'fader open ' + (openT / spb).toFixed(3) + ' to ' + (closeT / spb).toFixed(3) + ' beats (key up at 2.1, next 1/16 is 2.25, swung 2.29)');
	// a second key takes over at the unit boundary
	r = sim.run({record: REC, tempo: 96, beats: 6, hands: 0, logsegs: true,
		events: [{beat: 0.9, note: BABY, vel: 64}, {beat: 1.2, note: CHIRP, vel: 64}, {beat: 1.25, note: BABY, vel: 0}, {beat: 2.2, note: CHIRP, vel: 0}]});
	const ws = r.words.map(w => w.w + '@' + w.b);
	report('5. a new key takes over at the end of the unit', JSON.stringify(ws) === JSON.stringify(['baby@1', 'chirp@1.5', 'chirp@2']), ws.join(', '));
	// transport stopped
	r = sim.run({record: REC, tempo: 96, beats: 4, hands: 0, stopped: true, logsegs: true, events: [{beat: 0.9, note: BABY, vel: 64}, {beat: 1.2, note: BABY, vel: 0}]});
	let pk = 0; for (const x of r.outL) pk = Math.max(pk, Math.abs(x));
	report('5. plays with the transport stopped', r.words.length === 1 && pk > 0.05, r.words.length + ' unit, peak ' + pk.toFixed(2));
}

// --- 6. Instinct shape and word switches
{
	let r = sim.run({record: REC, tempo: 96, beats: 4 * 8 + 1, hands: 50, seed: 5, ctl: {mode: 2}, logsegs: true});
	const ph = r.phrases;
	const okShape = ph.length >= 1 && ph.every(p => p.bars.length === 4) && ph.some(p => JSON.stringify(p.bars[0]) === JSON.stringify(p.bars[1]));
	report('6. Instinct phrase shape A, A (or varied), B, ending', okShape, ph.map(p => p.bars.map(b => b.join(' ')).join(' | ')).join('  //  '));
	const off = {}; for (const w of ['forward', 'stab', 'chirp', 'flare1', 'flare2', 'crab', 'tear', 'scribble', 'transform', 'drag', 'tip', 'lazer', 'release', 'echo', 'reverse']) off['on_' + w] = 0;
	r = sim.run({record: REC, tempo: 96, beats: 4 * 16, hands: 50, seed: 7, ctl: Object.assign({mode: 2, density: 100}, off), logsegs: true});
	const used = new Set(r.cells);
	const okSw = [...used].every(c => ['rest', 'baby8', 'baby16', 'pickup', 'baby-rest'].includes(c));
	report('6. only Baby switched on: only babies and rests', okSw && used.size > 1, [...used].join(', '));
}

// --- 7. queues never overrun: 10 minutes of dense Instinct at 160 BPM
{
	const r = sim.run({record: REC, tempo: 160, beats: 160 * 10, hands: 70, seed: 9, ctl: {mode: 2, density: 100, skill: 100}});
	report('7. queues never overrun (10 min, 160 BPM, dense)', r.maxQ < 254 && r.maxF < 510 && !r.dropped, 'deepest hand queue ' + r.maxQ + ' of 256, fader ' + r.maxF + ' of 512');
}

console.log(fails ? fails + ' FAILED' : 'ALL CHECKS PASS');
process.exit(fails ? 1 : 0);
