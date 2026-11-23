// Runs the whole device offline: the gen~ core (translated) and doubles.js together,
// sample by sample, on a loop, with a script of panel actions.  Usage:
//   node ref/sim.js script.json out.wav
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
	const fr = data.length / (2 * fmt.ch), L = new Float32Array(fr), R = new Float32Array(fr);
	for (let k = 0; k < fr; k++) {
		L[k] = data.readInt16LE(k * 2 * fmt.ch) / 32768;
		R[k] = fmt.ch > 1 ? data.readInt16LE(k * 2 * fmt.ch + 2) / 32768 : L[k];
	}
	return {sr: fmt.sr, L, R};
}
function writeWav(p, L, R, sr) {
	const n = L.length, b = Buffer.alloc(44 + n * 4);
	b.write('RIFF', 0); b.writeUInt32LE(36 + n * 4, 4); b.write('WAVE', 8); b.write('fmt ', 12);
	b.writeUInt32LE(16, 16); b.writeUInt16LE(1, 20); b.writeUInt16LE(2, 22); b.writeUInt32LE(sr, 24);
	b.writeUInt32LE(sr * 4, 28); b.writeUInt16LE(4, 32); b.writeUInt16LE(16, 34); b.write('data', 36); b.writeUInt32LE(n * 4, 40);
	for (let k = 0; k < n; k++) {
		b.writeInt16LE(Math.max(-32767, Math.min(32767, Math.round(L[k] * 32767))), 44 + k * 4);
		b.writeInt16LE(Math.max(-32767, Math.min(32767, Math.round(R[k] * 32767))), 46 + k * 4);
	}
	fs.writeFileSync(p, b);
}

// seeded random so runs repeat
function mulberry(a) { return function () { a |= 0; a = a + 0x6D2B79F5 | 0; let t = Math.imul(a ^ a >>> 15, 1 | a); t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296; }; }

function run(script) {
	const src = readWav(script.loop);
	const SR = src.sr, bars = script.bars, LB = bars * 4;
	const tempo = script.tempo || LB * 60 / (src.L.length / SR);
	const spb = SR * 60 / tempo;
	const RECMAX = Math.ceil(100 * SR);
	const BUF = {
		rec: {frames: RECMAX, ch: [new Float32Array(RECMAX), new Float32Array(RECMAX)]},
		q: {frames: 2048, ch: [new Float32Array(2048)]},
		st: {frames: 32, ch: [new Float32Array(32)]},
		gcut: {frames: 256, ch: [new Float32Array(256)]}
	};
	const core = makeCore(BUF, SR, mulberry(script.seed || 1));
	delete require.cache[require.resolve('../src/doubles.js')];
	const D = require('../src/doubles.js');
	D.setRng(mulberry((script.seed || 1) + 99));
	D.HOST.gen = (n, v) => { core.P[n] = v; };
	D.HOST.queueWrite = (d, slot, vals) => { const base = (d * 64 + slot) * 16; for (let i = 0; i < 16; i++) BUF.q.ch[0][base + i] = vals[i]; };
	D.HOST.heads = () => [BUF.st.ch[0][5], BUF.st.ch[0][6]];
	const P = core.P;
	const hv = script.hands === undefined ? 0.5 : script.hands;
	for (const k of ['timing', 'weight', 'slop', 'risk']) P[k] = script[k] === undefined ? hv : script[k];
	P.wear = script.copy === undefined ? 0.5 : script.copy;
	P.tempo = tempo; P.playing = 1;
	Object.assign(D.S.ctl, script.ctl || {});

	const total = Math.ceil(script.beats * spb);
	const outL = new Float32Array(total), outR = new Float32Array(total);
	const ev = (script.events || []).slice().sort((a, b) => a.beat - b.beat);
	let evi = 0, sbseq = 0, recDone = false;
	// arm the recording at the start; the loop clip plays from beat 0
	P.recmode = 1; P.reclen = LB; P.reccmd = 1;
	const tickEvery = Math.round(0.02 * SR), songEvery = Math.round(0.05 * SR);
	for (let n = 0; n < total; n++) {
		const Bs = n / spb;
		if (n % songEvery === 0) { P.songbeat = Bs - 0.01; P.sbseq = ++sbseq; }
		const ph = Bs - Math.floor(Bs);
		const si = n % src.L.length;
		const o = core.step(src.L[si], src.R[si], ph);
		outL[n] = o[0]; outR[n] = o[1];
		if (n % tickEvery === 0) {
			const st = BUF.st.ch[0];
			D.S.B = st[0]; D.S.playing = 1; D.S.tempo = tempo;
			if (!recDone && st[2] === 3) {
				recDone = true;
				const L = st[3], recstart = st[4], Lb = st[14];
				P.L = L; P.Lb = Lb; P.anchor = ((recstart % Lb) + Lb) % Lb;
				P.recmode = 0; P.reccmd = 2;
				const mono = new Float32Array(L);
				for (let i = 0; i < L; i++) mono[i] = 0.5 * (BUF.rec.ch[0][i] + BUF.rec.ch[1][i]);
				D.setRecord(L, Lb, P.anchor, SR, mono);
				P.haverec = 1;
				if (script.log) console.log('record: L', L, 'Lb', Lb, 'start', recstart, 'stickers', D.S.stk.length, 'K', D.S.K, 'S', D.S.S, 'at beat', st[0].toFixed(3));
			}
			while (evi < ev.length && ev[evi].beat <= st[0]) {
				const e = ev[evi++];
				if (e.ctl) Object.assign(D.S.ctl, e.ctl);
				if (e.press) D.movePress(e.press, 1);
				if (e.release) D.movePress(e.release, 0);
				if (e.padon !== undefined) D.padPress(e.padon, 1);
				if (e.padoff !== undefined) D.padPress(e.padoff, 0);
				if (e.set) for (const k in e.set) P[k] = e.set[k];
				if (e.routine) { D.S.routine = e.routine; }
				if (e.anchorRoutine) { D.S.routineAnchor = e.anchorRoutine; }
				if (e.instinct0 !== undefined) { D.S.instinctBar0 = e.instinct0; }
			}
			if (recDone) D.plan(st[0]);
		}
	}
	const st = BUF.st.ch[0];
	if (script.log) console.log('late', st[11], 'fluffed', st[12], 'heads', st[5], st[6]);
	return {outL, outR, SR, core, D};
}

if (require.main === module) {
	const script = JSON.parse(fs.readFileSync(process.argv[2]));
	const r = run(script);
	writeWav(process.argv[3], r.outL, r.outR, r.SR);
	if (script.dumpTimeline) fs.writeFileSync(script.dumpTimeline, JSON.stringify(r.D.TL()));
}
module.exports = {run, readWav, writeWav};
