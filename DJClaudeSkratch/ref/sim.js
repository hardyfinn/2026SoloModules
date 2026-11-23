// Runs the device offline: the gen~ core (translated by tools/genexpr.py) and skratch.js
// together, sample by sample, with a script of key presses and panel changes.
//   node ref/sim.js script.json out.wav
// script: {record: wav, cues: [s...] (optional), tempo, beats, seed, hands or timing/weight/slop/risk (0..100),
//          ctl: {name: panel value}, events: [{beat, note, vel} | {beat, ctl: {...}}], dump: prefix}
const fs = require('fs');
const path = require('path');
const makeCore = require('./core_sim.js');

function readWav(p) {
	const b = fs.readFileSync(p);
	let i = 12, fmt = null, data = null;
	while (i < b.length) {
		const id = b.toString('ascii', i, i + 4), n = b.readUInt32LE(i + 4);
		if (id === 'fmt ') fmt = {ch: b.readUInt16LE(i + 10), sr: b.readUInt32LE(i + 12), bits: b.readUInt16LE(i + 22)};
		if (id === 'data') data = b.subarray(i + 8, i + 8 + n);
		i += 8 + n + (n & 1);
	}
	const fr = Math.floor(data.length / (2 * fmt.ch)), L = new Float32Array(fr), R = new Float32Array(fr);
	for (let k = 0; k < fr; k++) {
		L[k] = data.readInt16LE(k * 2 * fmt.ch) / 32768;
		R[k] = fmt.ch > 1 ? data.readInt16LE(k * 2 * fmt.ch + 2) / 32768 : L[k];
	}
	return {sr: fmt.sr, L, R};
}
function writeWav(p, L, R, sr, norm) {
	const n = L.length, b = Buffer.alloc(44 + n * 4);
	let pk = 1e-9;
	if (norm) for (let k = 0; k < n; k++) pk = Math.max(pk, Math.abs(L[k]), Math.abs(R[k]));
	const g = norm ? norm / pk : 1;
	b.write('RIFF', 0); b.writeUInt32LE(36 + n * 4, 4); b.write('WAVE', 8); b.write('fmt ', 12);
	b.writeUInt32LE(16, 16); b.writeUInt16LE(1, 20); b.writeUInt16LE(2, 22); b.writeUInt32LE(sr, 24);
	b.writeUInt32LE(sr * 4, 28); b.writeUInt16LE(4, 32); b.writeUInt16LE(16, 34); b.write('data', 36); b.writeUInt32LE(n * 4, 40);
	for (let k = 0; k < n; k++) {
		b.writeInt16LE(Math.max(-32767, Math.min(32767, Math.round(L[k] * g * 32767))), 44 + k * 4);
		b.writeInt16LE(Math.max(-32767, Math.min(32767, Math.round(R[k] * g * 32767))), 46 + k * 4);
	}
	fs.writeFileSync(p, b);
}
function mulberry(a) { return function () { a |= 0; a = a + 0x6D2B79F5 | 0; let t = Math.imul(a ^ a >>> 15, 1 | a); t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296; }; }

function run(script) {
	const src = readWav(script.record);
	const SR = script.sr || 44100, tempo = script.tempo || 96, spb = SR * 60 / tempo;
	const CUTF = Math.ceil(80 * SR);
	const BUF = {
		cut: {frames: CUTF, ch: [new Float32Array(CUTF), new Float32Array(CUTF)]},
		q: {frames: 256 * 12, ch: [new Float32Array(256 * 12)]},
		fq: {frames: 512 * 4, ch: [new Float32Array(512 * 4)]},
		st: {frames: 16, ch: [new Float32Array(16)]}
	};
	const core = makeCore(BUF, SR);
	delete require.cache[require.resolve('../src/skratch.js')];
	const D = require('../src/skratch.js');
	D.setRng(mulberry(script.seed || 1));
	const P = core.P;
	D.HOST.gen = (n, v) => { if (!(n in P)) throw new Error('no Param ' + n); P[n] = v; };
	D.HOST.status = () => Array.from(BUF.st.ch[0].subarray(0, 12));
	D.HOST.writeSeg = (slot, vals) => { for (let i = 0; i < 12; i++) BUF.q.ch[0][slot * 12 + i] = vals[i]; };
	D.HOST.writeFad = (slot, vals) => { for (let i = 0; i < 4; i++) BUF.fq.ch[0][slot * 4 + i] = vals[i]; };
	D.HOST.cutCapacity = () => CUTF;
	D.HOST.cutBegin = () => {};
	D.HOST.cutWrite = (c, off, y) => { BUF.cut.ch[c].set(y, off); };
	D.HOST.log = (s) => console.log(s);
	const T = {segs: [], cueMoves: [], words: [], phrases: [], cells: []};
	D.HOST.trace = script.logsegs ? (k, o) => {
		if (k === 'stroke') T.segs.push(Object.assign({kind: 1}, o));
		else if (k === 'cuemove') T.cueMoves.push([Math.round(o.tb * spb), Math.round(o.te * spb)]);
		else if (k === 'unit') T.words.push(o);
		else if (k === 'phrase') T.phrases.push(o);
		else if (k === 'cell') T.cells.push(o.cell);
	} : null;
	const ui = [];
	D.HOST.ui = function () { ui.push(Array.from(arguments)); };
	P.tempo = tempo; P.playing = script.stopped ? 0 : 1;
	D.S.tempo = tempo; D.S.sr = SR;
	const hv = script.hands === undefined ? 50 : script.hands;
	for (const k of ['timing', 'weight', 'slop', 'risk']) D.control(k, script[k] === undefined ? hv : script[k]);
	for (const k in (script.ctl || {})) D.control(k, script.ctl[k]);
	D.sendFilters();
	for (let n = 0; n < 4; n++) core.step(n / spb - Math.floor(n / spb));
	D.resetPlan(false);
	D.useRecord(src.L, src.R, src.sr, script.cues || null);
	while (!D.cutSlice(1 << 20)) {}
	const total = Math.ceil(script.beats * spb);
	const outL = new Float32Array(total), outR = new Float32Array(total);
	const keep = script.dump || script.logsegs;
	const dpos = keep ? new Float32Array(total) : null, dg = keep ? new Float32Array(total) : null;
	const ev = (script.events || []).slice().sort((a, b) => a.beat - b.beat);
	let evi = 0, sbseq = 0;
	const tickEvery = Math.round(0.02 * SR), songEvery = Math.round(0.05 * SR);
	let maxQ = 0, maxF = 0;
	for (let n = 0; n < total; n++) {
		const Bs = n / spb;
		if (!script.stopped && n % songEvery === 0) { P.songbeat = Bs; P.sbseq = ++sbseq; }
		while (evi < ev.length && ev[evi].beat <= core.H.h_B) {
			const e = ev[evi++];
			if (e.ctl) { for (const k in e.ctl) D.control(k, e.ctl[k]); D.plan(); }
			if (e.note !== undefined) D.noteIn(e.note, e.vel === undefined ? 100 : e.vel);
			if (e.cues) { D.R.cues = e.cues; }
		}
		const o = core.step(Bs - Math.floor(Bs));
		outL[n] = o[0]; outR[n] = o[1];
		if (dpos) { dpos[n] = core.H.h_pos; dg[n] = core.H.h_g; }
		if (n % tickEvery === 0) D.plan();
		const st = BUF.st.ch[0];
		maxQ = Math.max(maxQ, P.qtail - st[2]); maxF = Math.max(maxF, P.ftail - st[3]);
	}
	if (script.dump) { fs.writeFileSync(script.dump + '.pos.f32', Buffer.from(dpos.buffer)); fs.writeFileSync(script.dump + '.g.f32', Buffer.from(dg.buffer)); }
	for (const sg of T.segs) { const mid = Math.round((sg.tb + sg.te) / 2 * spb); sg.audible = dg && mid < total && dg[mid] > 0.5; }
	return {outL, outR, SR, core, D, maxQ, maxF, ui, gain: dg, pos: dpos, segs: T.segs, cueMoves: T.cueMoves, words: T.words,
		phrases: T.phrases, cells: T.cells, dropped: D.out.seg.length > 200 || D.out.fad.length > 400};
}

if (require.main === module) {
	const script = JSON.parse(fs.readFileSync(process.argv[2]));
	const r = run(script);
	writeWav(process.argv[3], r.outL, r.outR, r.SR, script.normalise);
	console.log('queue depth max: hand', r.maxQ, 'fader', r.maxF);
}
module.exports = {run, readWav, writeWav};
