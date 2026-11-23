// Doubles: logic for the Max for Live device.
// Plans the juggle (which deck plays what, when), handles the record, finds the hits
// ("stickers"), runs the Routine and Instinct modes, and keeps the record with the set.
// Sample-accurate timing lives in gen~; this file only decides things ahead of time.
//
// Arguments: rec-buffer queue-buffer status-buffer save-buffer load-buffer
// Inlet: messages "name value" from the panel, "init", "strip ..." from the strip.
// Outlet 0: parameter messages for gen~.  Outlet 1: messages for the panel.

autowatch = 1;
inlets = 1;
outlets = 2;

// ------------------------------------------------------------------ settings
var LOOKAHEAD = 1.5;   // beats of plays sent to gen~ ahead of now
var GENAHEAD = 2.5;    // beats of plan generated ahead of now
var QN = 64;           // queue slots per deck
var QF = 16;           // fields per queue entry

var FILLS = [
	'K.KS.K.SK.KS.SSS', 'KK.S.KKS.K.SKSKS', 'K..SK.S.KKSS.KSS', 'KSKKS.KSKSS.SSSS',
	'K.K.S..KK.S.KSSS', 'K...S.K.K.S.SKSK', 'KKSSKKSSK.S.KSSS', 'K.S.K.S.KSKSKSSS'
];
var MOVES = ['straight', 'loop', 'double', 'triple', 'chase', 'shuffle', 'pause', 'fill', 'spin', 'phase'];

// ------------------------------------------------------------------ state
var S = {
	// record
	L: 0, Lb: 8, anchor: 0, sr: 44100, have: 0, recSR: 44100,
	stk: [], stkOff: {}, stkAdd: [], K: -1, S: -1, padStk: [], barK: [], barS: [],
	file: '',
	// transport
	tempo: 120, playing: 0, B: 0,
	// controls
	ctl: {
		looplen: 1, doubledist: 0.5, chaseunit: 1, shuffle: 0.5, pauselen: 0.5, fillpat: 1, spinback: 4,
		quant: 1, latch: 0, mode: 0, fillk: 0, fills: 0, density: 0.6, funk: 0.5, arc: 16, slotlen: 4,
		timing: 0.5, weight: 0.5, slop: 0.5, risk: 0.5, copy: 0.5, length: 8,
		scratch: 0.3
	},
	forceScratch: false, // BABY pressed while both decks are busy: scratch into the next cue
	held: null,          // {move, b} the move button in force
	pad: null,
	routine: [],
	routineAnchor: 0,
	instinctBar0: 0,
	lastFill: -1,
	prevBarMove: null
};
for (var i = 0; i < 16; i++) S.routine.push('straight');

// planner
var TL = [[], []];       // timeline of plays per deck
var sentIdx = [0, 0];    // plays already written to the queue
var tail = [0, 0];       // queue write counters
var gen = 0;             // flush generation
var planEnd = null;      // beat up to which the plan has been generated
var seg = null;          // the segment being generated
var rng = Math.random;

// host hooks, replaced in Max (bottom of file) or by the test harness
var HOST = {
	gen: function (name, v) {},
	ui: function () {},
	queueWrite: function (d, slot, vals) {},
	heads: function () { return [0, 0]; },
	log: function (s) {}
};

// ------------------------------------------------------------------ helpers
function mod(x, m) { var y = x % m; return y < 0 ? y + m : y; }
function wrapc(x, m) { return mod(x + m / 2, m) - m / 2; }
function spbRec() { return S.L / S.Lb; }                 // record samples per record beat
function rbS(b) { return mod(b - S.anchor, S.Lb); }       // straight position at song beat b
function gauss() { return (rng() + rng() + rng() + rng() + rng() + rng() - 3) * 1.41421356; }

function snapSamples(rb) {
	// record beats -> record samples, snapped to a sticker within 40 ms, cued 2 ms early
	var p = mod(rb, S.Lb) * spbRec();
	var lim = 0.040 * S.recSR, best = null, bd = 1e30;
	for (var i = 0; i < S.stk.length; i++) {
		var d = wrapc(S.stk[i] - p, S.L);
		if (Math.abs(d) < bd) { bd = Math.abs(d); best = d; }
	}
	if (best !== null && bd < lim) p = p + best;
	return mod(p - 0.002 * S.recSR, S.L);
}

// ------------------------------------------------------------------ plays
// play: {bs, be, rb (record beats or null), p0 (samples), kind 0 play 1 hold 2 spin,
//        cm 0 cue 1 let-go 2 continuous, rate, gm, gd, g0, amp, dist, str}
function lastPlay(d, b) {
	var t = TL[d];
	for (var i = t.length - 1; i >= 0; i--) if (t[i].bs < b - 1e-9) return t[i];
	return null;
}
function deckEnd(d, b) { var p = lastPlay(d, b); return p ? Math.min(p.be, b) : -1e9; }
function deckRbEnd(d, b) {
	var p = lastPlay(d, b);
	if (!p || p.rb === null || p.kind !== 0) return null;
	return mod(p.rb + (Math.min(p.be, b) - p.bs) * p.rate, S.Lb);
}
var liveDeck = 0;
function freeDeck(b, rb) {
	var l = liveDeck, o = 1 - l, ds = [l, o];
	if (rb !== null && rb !== undefined) {
		for (var i = 0; i < 2; i++) {
			var d = ds[i];
			if (Math.abs(deckEnd(d, b) - b) < 1e-9) {
				var re = deckRbEnd(d, b);
				if (re !== null && Math.abs(wrapc(re - rb, S.Lb)) < 1e-6) return d;
			}
		}
	}
	return deckEnd(o, b) < deckEnd(l, b) ? o : l;
}
// The deck that can be at record beat rb by song beat b with the most time to spare.
// A deck that can simply carry on always wins.
function readyDeck(b, rb) {
	var c = freeDeck(b, rb);
	if (Math.abs(deckEnd(c, b) - b) < 1e-9) {
		var re = deckRbEnd(c, b);
		if (re !== null && Math.abs(wrapc(re - rb, S.Lb)) < 1e-6) return c;
	}
	var w = S.ctl.weight, vmax = 6 / Math.max(w, 0.05), best = c, bslack = -1e9;
	for (var d = 0; d < 2; d++) {
		var e = deckEnd(d, b), lp = lastPlay(d, b), at;
		if (!lp) return d;                                  // never used: it is running in time
		if (b - e > 0.5) return d;                          // idle a while: running in time, muted
		at = lp.rb === null ? rbS(e) : mod(lp.rb + (Math.min(lp.be, b) - lp.bs) * lp.rate, S.Lb);
		var dist = Math.abs(wrapc(rb - at, S.Lb)) * 60 / S.tempo;     // seconds of record
		var need = 0.045 * w + dist / vmax;
		var slack = (b - e) * 60 / S.tempo - need;
		if (slack > bslack) { bslack = slack; best = d; }
	}
	return best;
}

function addPlay(d, p) {
	var prev = lastPlay(d, p.bs + 1e-9);
	p.d = d;
	if (p.kind === undefined) p.kind = 0;
	if (p.rate === undefined) p.rate = 1;
	if (p.gm === undefined) p.gm = 0;
	if (p.gd === undefined) p.gd = 1;
	if (p.g0 === undefined) p.g0 = 0;
	if (p.amp === undefined) p.amp = 1;
	if (p.dist === undefined) p.dist = 0;
	if (p.str === undefined) p.str = 0;
	if (p.cm === undefined) {
		p.cm = 0;
		if (p.kind === 0 && p.rb !== null && prev && prev.kind === 0 && prev.rb !== null &&
			Math.abs(prev.be - p.bs) < 1e-9 && prev.rate === p.rate &&
			Math.abs(wrapc(mod(prev.rb + (prev.be - prev.bs) * prev.rate, S.Lb) - p.rb, S.Lb)) < 1e-6) p.cm = 2;
	}
	if (p.p0 === undefined) {
		if (p.rb === null) p.p0 = 0;
		else if (p.cm === 2 || p.nosnap) p.p0 = mod(p.rb, S.Lb) * spbRec();
		else p.p0 = snapSamples(p.rb);
	}
	if (p.kind === 0 && p.cm === 0 && p.gm === 0 && !p.scn) ornament(d, p, prev);
	TL[d].push(p);
	return p;
}

// ------------------------------------------------------------------ baby scratches
// A baby scratch is one push forward through a hit and one pull back to its cue, fader
// open. DJs flick one in just ahead of the beat as they bring a record back. The planner
// only asks for one; gen~ plays it if the hand turns out to have the time.
//   pattern: [pairs of strokes, stroke length in beats, weight]
var SCRATCHES = [[1, 0.125, 5], [1, 0.25, 3], [2, 0.125, 2], [1, 1 / 6, 1]];
function scratchPattern(gapBeats) {
	var bs = 60 / S.tempo, w = S.ctl.weight, ok = [], tot = 0, i, r;
	var room = gapBeats - (0.045 * w + 0.05) / bs;        // the hand has to get to the cue first
	for (i = 0; i < SCRATCHES.length; i++) {
		var sc = SCRATCHES[i], st = sc[1] * bs;
		if (st < 0.05 || st > 0.19) continue;               // too fast for a hand, or a drag not a scratch
		if (2 * sc[0] * sc[1] > room) continue;
		var wt = sc[2] * (i === 3 ? 2 * S.ctl.funk : 1);
		if (wt <= 0) continue;
		ok.push([sc, wt]); tot += wt;
	}
	if (!ok.length) return null;
	r = rng() * tot;
	for (i = 0; i < ok.length; i++) { r -= ok[i][1]; if (r <= 0) return ok[i][0]; }
	return ok[ok.length - 1][0];
}
function scratchTravel(strokeBeats) {
	// how much record one forward stroke covers: the hand moves at about playing speed,
	// and never goes further than one drum hit
	var st = strokeBeats * 60 / S.tempo;
	return Math.min(st * (0.75 + 0.3 * rng()), 0.095) * S.recSR;
}
function ornament(d, p, prev) {
	var amt = S.ctl.scratch, force = S.forceScratch;
	if (amt <= 0 && !force) return;
	var gap = prev ? p.bs - Math.min(prev.be, p.bs) : 4;
	if (gap < 0.25) return;
	var f = mod(p.bs, 1), pos = (f < 1e-6 || f > 1 - 1e-6) ? 1 : (Math.abs(f - 0.5) < 1e-6 ? 0.7 : 0.35);
	if (!force && rng() >= amt * pos) return;
	var sc = scratchPattern(gap);
	if (!sc) return;
	p.scn = sc[0]; p.scd = sc[1]; p.dist = scratchTravel(sc[1]);
	S.forceScratch = false;
}
// BABY: a scratch on its own, on the record that is not playing, into the next beat.
function babyPress() {
	if (!S.have || !S.playing) return;
	var now = S.B, q = Math.max(S.ctl.quant, 0.5), d, i, free = -1;
	for (var tries = 0; tries < 2 && free < 0; tries++) {
		var be = quantNext(now + 0.3 + tries * q, q);
		var sc = scratchPattern(be - now);
		if (!sc) continue;
		var bs = be - 2 * sc[0] * sc[1];
		for (d = 1; d >= 0 && free < 0; d--) {
			var dd = (liveDeck + d) % 2, busy = false;
			for (i = 0; i < TL[dd].length; i++) if (TL[dd][i].be > now - 1e-9) busy = true;
			if (!busy) free = dd;
		}
		if (free >= 0) {
			addPlay(free, {bs: bs, be: be, rb: null, kind: 3, cm: 1, p0: snapSamples(rbS(be)),
				scn: sc[0], scd: sc[1], dist: scratchTravel(sc[1])});
			send(now);
			return;
		}
	}
	S.forceScratch = true;
}

// ------------------------------------------------------------------ moves
// Each move gets a segment {move, b0, b1, ...} and emits its plays for [from, to).
function segInit(sg) {
	var b0 = sg.b0;
	sg.rb0 = rbS(b0);
	if (sg.move === 'straight' || sg.move === 'double' || sg.move === 'phase') sg.first = readyDeck(b0, sg.rb0);
	else if (sg.move === 'fill') sg.first = freeDeck(b0, null);
	else if (sg.move === 'pause' || sg.move === 'spin') sg.first = liveDeck;
	else sg.first = freeDeck(b0, sg.rb0);
	sg.done = b0;
}

function emit(sg, to) {
	var from = sg.done, b0 = sg.b0, b1 = sg.b1, m = sg.move, k, b, d, u;
	to = Math.min(to, b1);
	if (to <= from + 1e-9) return;
	if (m === 'straight') {
		if (from <= b0 + 1e-9) { addPlay(sg.first, {bs: b0, be: b1, rb: sg.rb0, str: 1, nosnap: 1}); liveDeck = sg.first; }
	} else if (m === 'loop' || m === 'triple') {
		u = m === 'triple' ? 1 / 3 : sg.unit;
		k = Math.max(0, Math.ceil((from - b0) / u - 1e-9));
		for (; b0 + k * u < to - 1e-9; k++) {
			b = b0 + k * u; d = (sg.first + k) % 2;
			addPlay(d, {bs: b, be: Math.min(b + u, b1), rb: sg.rb0}); liveDeck = d;
		}
	} else if (m === 'double') {
		if (from <= b0 + 1e-9) {
			var X = sg.first, Y = 1 - X, dd = sg.dist;
			addPlay(X, {bs: b0, be: b1, rb: sg.rb0, gm: 1, gd: dd, g0: b0, nosnap: 1});
			addPlay(Y, {bs: b0 + dd, be: b1, rb: sg.rb0, gm: 2, gd: dd, g0: b0, nosnap: 1});
			liveDeck = X;
		}
	} else if (m === 'chase' || m === 'shuffle') {
		u = sg.unit; var sh = m === 'shuffle' ? sg.shuffle : 0;
		if (sg.ck === undefined) { sg.ck = 0; sg.cb = b0; }
		while (sg.cb < to - 1e-9) {
			var dur = (sg.ck % 2 === 0) ? u * (1 + sh) : u * (1 - sh);
			d = (sg.first + sg.ck) % 2;
			addPlay(d, {bs: sg.cb, be: Math.min(sg.cb + dur, b1), rb: mod(sg.rb0 + Math.floor(sg.ck / 2) * u, S.Lb)});
			liveDeck = d; sg.cb += dur; sg.ck++;
		}
	} else if (m === 'pause') {
		if (from <= b0 + 1e-9) {
			d = sg.first;
			addPlay(d, {bs: b0, be: b0 + sg.hold, rb: null, kind: 1, cm: 1});
			addPlay(d, {bs: b0 + sg.hold, be: b1, rb: null, kind: 0, cm: 1});
			liveDeck = d;
		}
	} else if (m === 'fill') {
		var pat = sg.pattern, step = sg.step, X2 = sg.first;
		if (!sg.ks) sg.ks = fillStickers(b0);
		var kdeck = X2, sdeck = 1 - X2;
		if (pat.charAt(0) === 'S') { sdeck = X2; kdeck = 1 - X2; }
		var hits = [];
		for (var i = 0; i < pat.length; i++) if (pat.charAt(i) !== '.') hits.push(i);
		for (var j = 0; j < hits.length; j++) {
			var i2 = hits[j], c = pat.charAt(i2), isK = c === 'K';
			var bs = b0 + i2 * step;
			if (bs < from - 1e-9 || bs >= to - 1e-9) continue;
			var nxt = j + 1 < hits.length ? hits[j + 1] : pat.length;
			var durh = Math.min(nxt - i2, 2) * step;
			for (var q = j + 1; q < hits.length; q++) {
				if ((pat.charAt(hits[q]) === 'K') === isK) {
					if (hits[q] - i2 <= 2) durh = Math.min(durh, (hits[q] - i2) * step * 0.45);
					break;
				}
			}
			d = isK ? kdeck : sdeck;
			var p0 = mod((isK ? sg.ks[0] : sg.ks[1]) - 0.002 * S.recSR, S.L);
			addPlay(d, {bs: bs, be: Math.min(bs + durh, b1), rb: (p0 + 0.002 * S.recSR) / spbRec(), p0: p0, cm: 0});
			liveDeck = d;
		}
	} else if (m === 'spin') {
		if (from <= b0 + 1e-9) {
			d = sg.first;
			addPlay(d, {bs: b0, be: b1, rb: null, kind: 2, cm: 1, dist: -sg.back * spbRec()});
			liveDeck = d;
		}
	} else if (m === 'phase') {
		if (from <= b0 + 1e-9) {
			var X3 = sg.first, Y3 = 1 - X3;
			addPlay(X3, {bs: b0, be: b1, rb: sg.rb0, amp: 0.72, nosnap: 1});
			addPlay(Y3, {bs: b0, be: b1, rb: mod(sg.rb0 - 0.009 * S.tempo / 60, S.Lb), amp: 0.72, rate: 1.0018, nosnap: 1});
			liveDeck = X3;
		}
	}
	sg.done = to;
}

function makeSeg(move, b0, b1, opts) {
	var c = S.ctl, sg = {move: move, b0: b0, b1: b1};
	sg.unit = c.looplen; sg.dist = c.doubledist; sg.shuffle = c.shuffle; sg.hold = c.pauselen;
	sg.back = c.spinback; sg.step = 0.25; sg.pattern = FILLS[(c.fillpat - 1) % 8];
	if (move === 'chase' || move === 'shuffle') sg.unit = c.chaseunit;
	if (opts) for (var k in opts) sg[k] = opts[k];
	return sg;
}

// ------------------------------------------------------------------ where plans come from
function barOf(b) { return Math.floor(b / 4 + 1e-9); }

function nextSegments(b) {
	// returns a list of segments starting at b
	var c = S.ctl, nb = (barOf(b) + 1) * 4;
	if (S.held) return heldSegments(b);
	if (c.mode === 1) return routineSegments(b);
	if (c.mode === 2) return instinctSegments(b);
	return [makeSeg('straight', b, nb)];
}

function heldSegments(b) {
	var h = S.held, m = h.move, nb = (barOf(b) + 1) * 4;
	if (m === 'triple') { S.held = null; return [makeSeg('triple', b, b + 1)]; }
	if (m === 'spin') {
		var sb = nb - 1;
		if (sb < b - 1e-9) { nb += 4; sb = nb - 1; }
		S.held = null;
		var out = [];
		if (sb > b + 1e-9) out.push(makeSeg('straight', b, sb));
		out.push(makeSeg('spin', sb, nb));
		return out;
	}
	if (m === 'pause') {
		var end = nb;
		if (end - b < S.ctl.pauselen + 0.25) end += 4;
		S.held = null;
		return [makeSeg('pause', b, end)];
	}
	if (m === 'fill') return [makeSeg('fill', b, b + 4)];
	return [makeSeg(m, b, b + 256)];
}

function slotRender(name, b0, b1) {
	var out = [], c = S.ctl, i;
	if (name.indexOf('fill') === 0) {
		var n = parseInt(name.substr(4), 10) || 1, pat = FILLS[(n - 1) % 8];
		if (b1 - b0 < 4 - 1e-9) pat = pat.substr(16 - Math.round((b1 - b0) * 4));
		return [makeSeg('fill', b0, b1, {pattern: pat})];
	}
	if (name === 'triple') {
		for (i = b0; i < b1 - 1e-9; i += 1) {
			var beatInBar = mod(Math.round(i), 4);
			out.push(makeSeg(beatInBar === 1 || beatInBar === 3 ? 'triple' : 'straight', i, i + 1));
		}
		return out;
	}
	if (name === 'pause') {
		var pre = b1 - b0 >= 4 ? 2 : 1;
		return [makeSeg('straight', b0, b0 + pre), makeSeg('pause', b0 + pre, b1)];
	}
	if (name === 'spin') {
		if (b1 - b0 > 1) out.push(makeSeg('straight', b0, b1 - 1));
		out.push(makeSeg('spin', b1 - 1, b1));
		return out;
	}
	return [makeSeg(name, b0, b1)];
}

function routineSegments(b) {
	var sl = S.ctl.slotlen, k = Math.floor((b - S.routineAnchor) / sl + 1e-9);
	var s0 = S.routineAnchor + k * sl, s1 = s0 + sl;
	var name = S.routine[mod(k, 16)];
	if (b > s0 + 1e-9) return [makeSeg('straight', b, s1)];   // joined mid-slot
	return slotRender(name, s0, s1);
}

function wpick(w, dens) {
	var keys = [], ps = [], tot = 0;
	for (var k in w) {
		var p = w[k] * (k === 'straight' ? 1 : 2 * dens);
		keys.push(k); ps.push(p); tot += p;
	}
	var r = rng() * tot;
	for (var i = 0; i < keys.length; i++) { r -= ps[i]; if (r <= 0) return keys[i]; }
	return keys[keys.length - 1];
}
function choose(a) { return a[Math.floor(rng() * a.length)]; }

function instinctMove(m, b0, b1) {
	if (m === 'loop') return makeSeg('loop', b0, b1, {unit: b1 - b0 >= 2 ? choose([0.5, 1, 1, 2]) : 0.5});
	if (m === 'double') return makeSeg('double', b0, b1, {dist: choose([0.25, 0.5, 0.5, 0.5])});
	if (m === 'chase') return makeSeg('chase', b0, b1, {unit: choose([0.5, 1])});
	if (m === 'shuffle') return makeSeg('shuffle', b0, b1, {unit: 0.5, shuffle: choose([1 / 3, 0.5])});
	return makeSeg('straight', b0, b1);
}

function instinctSegments(b) {
	var c = S.ctl, bar = barOf(b), b0 = bar * 4;
	if (b > b0 + 1e-9) return [makeSeg('straight', b, b0 + 4)];
	var n = bar - S.instinctBar0, ph = mod(n, 4);
	var prog = mod(n, c.arc) / Math.max(c.arc - 1, 1);
	var dens = c.density * (0.6 + 0.8 * prog), f = c.funk;
	var W = [
		{straight: 6, loop: 2.5, double: 1.5},
		{straight: 4, double: 3, loop: 1.5, chase: 1.5},
		{straight: 2.5, double: 2.5, chase: 2.5, loop: 1.5, pause: 1, shuffle: 3 * f},
		{straight: 1, double: 2, chase: 2, shuffle: 4 * f, loop: 1}
	];
	var E = {fill: 4, triple: 2.5, spin: 2, chase8: 1.5, straight: 1};
	var out = [], m;
	if (ph < 3) {
		var prev = S.prevBarMove;
		if ((ph === 1 || ph === 2) && prev && prev !== 'straight' && prev !== 'pause' && rng() < 0.45) m = prev;
		else m = wpick(W[ph], dens);
		if (m === 'pause') {
			out.push(makeSeg('straight', b0, b0 + 2));
			out.push(makeSeg('pause', b0 + 2, b0 + 4, {hold: choose([0.25, 0.5])}));
		} else out.push(instinctMove(m, b0, b0 + 4));
		S.prevBarMove = m;
	} else {
		m = wpick(W[3], dens);
		out.push(instinctMove(m === 'pause' ? 'straight' : m, b0, b0 + 2));
		var e = wpick(E, dens);
		if (e === 'fill') {
			var fi;
			do { fi = Math.floor(rng() * 5); } while (fi === S.lastFill);
			S.lastFill = fi;
			out.push(makeSeg('fill', b0 + 2, b0 + 4, {pattern: FILLS[fi].substr(8)}));
		} else if (e === 'triple') {
			out.push(makeSeg('triple', b0 + 2, b0 + 3)); out.push(makeSeg('triple', b0 + 3, b0 + 4));
		} else if (e === 'spin') {
			out.push(makeSeg('straight', b0 + 2, b0 + 3)); out.push(makeSeg('spin', b0 + 3, b0 + 4));
		} else if (e === 'chase8') {
			out.push(makeSeg('chase', b0 + 2, b0 + 4, {unit: 0.25}));
		} else out.push(makeSeg('straight', b0 + 2, b0 + 4));
		S.prevBarMove = null;
	}
	return out;
}

// ------------------------------------------------------------------ the planner
var segQ = [];
function plan(now) {
	if (!S.have || !S.playing) return;
	if (planEnd === null) {
		planEnd = Math.ceil(now + 0.5);
		resetTimeline(planEnd);
		HOST.gen('bootb', planEnd);
	}
	var guard = 0;
	while (guard++ < 64) {
		if (!seg) {
			if (!segQ.length) {
				if (planEnd > now + GENAHEAD) break;
				segQ = nextSegments(planEnd);
			}
			seg = segQ.shift();
			segInit(seg);
		}
		emit(seg, Math.min(seg.b1, now + GENAHEAD + 1));
		if (seg.done >= seg.b1 - 1e-9) { planEnd = seg.b1; seg = null; }
		else break;
	}
	send(now);
}

function send(now) {
	var heads = HOST.heads();
	for (var d = 0; d < 2; d++) {
		var t = TL[d];
		while (sentIdx[d] < t.length && t[sentIdx[d]].bs < now + LOOKAHEAD) {
			if (tail[d] - heads[d] >= QN - 1) break;
			var p = t[sentIdx[d]];
			HOST.queueWrite(d, mod(tail[d], QN), [p.bs, p.be, p.p0, p.kind, p.cm, p.rate, p.gm, p.gd, p.g0,
				p.amp, p.dist, gen, p.str, p.scn || 0, p.scd || 0, tail[d]]);
			tail[d]++;
			sentIdx[d]++;
		}
		HOST.gen('qt' + d, tail[d]);
	}
}

// Cut everything planned from beat b on, in the timeline and in gen~.
function cutAt(b) {
	for (var d = 0; d < 2; d++) {
		var t = TL[d], keep = [];
		for (var i = 0; i < t.length; i++) {
			if (t[i].bs >= b - 1e-9) continue;
			if (t[i].be > b) t[i].be = b;
			keep.push(t[i]);
		}
		TL[d] = keep;
		sentIdx[d] = Math.min(sentIdx[d], keep.length);
	}
	gen++;
	HOST.gen('fbeat', b);
	HOST.gen('fgen', gen);
	seg = null; segQ = [];
	planEnd = b;
	// who was live at b
	var best = -1e9;
	for (var d2 = 0; d2 < 2; d2++) {
		var p = lastPlay(d2, b);
		if (p && p.bs > best) { best = p.bs; liveDeck = d2; }
	}
}

function resetTimeline(b) {
	TL = [[], []]; sentIdx = [0, 0]; seg = null; segQ = [];
	gen++;
	HOST.gen('fbeat', b === undefined ? -1e9 : b);
	HOST.gen('fgen', gen);
}

function quantNext(now, q) {
	return Math.ceil((now + 0.02) / q - 1e-9) * q;
}

// ------------------------------------------------------------------ panel events
var QUANTS = [0.25, 0.5, 1, 4];
function movePress(m, on) {
	var now = S.B, c = S.ctl;
	if (!S.have) return;
	if (m === 'drag') { if (on) dragPulse(); return; }
	if (m === 'baby') { if (on) babyPress(); return; }
	var b = S.playing ? quantNext(now, c.quant) : now;
	if (on) {
		S.held = {move: m, b: b};
		if (S.playing) cutAt(b);
	} else {
		if (S.held && S.held.move === m) {
			S.held = null;
			if (S.playing) cutAt(b);
		}
	}
	plan(now);
}
var dragN = 0;
function dragPulse() { dragN++; HOST.gen('dragseq', dragN); }

function padPress(n, on) {
	// a pad plays its sticker now, on its own deck, until the next hit or for one beat
	if (!S.have || !S.playing || !on) return;
	var now = S.B, b = now + 0.02;
	cutAt(b);
	var d = n < 4 ? 0 : 1, idx = S.padStk[n % 4];
	var p0 = idx !== undefined && idx >= 0 && idx < S.stk.length ? mod(S.stk[idx] - 0.002 * S.recSR, S.L) : 0;
	addPlay(d, {bs: b, be: b + 1, rb: (p0 + 0.002 * S.recSR) / spbRec(), p0: p0, cm: 0});
	liveDeck = d;
	planEnd = b + 1;
	send(now);
}

// ------------------------------------------------------------------ hands
function setHands(v) {
	HOST.ui('timing', v); HOST.ui('weight', v); HOST.ui('slop', v); HOST.ui('risk', v);
}

// ------------------------------------------------------------------ record analysis
// Stickers: positive jumps in short-time energy (port of find_stickers in doubles_engine.py).
function findStickers(mono, maxMarks) {
	var hop = 128, n = mono.length, nfr = Math.floor(n / hop);
	var e = new Float64Array(nfr), e2 = new Float64Array(nfr), prev = 0, i, f, s, s2, x;
	for (f = 0; f < nfr; f++) {
		s = 0; s2 = 0;
		for (i = f * hop; i < f * hop + hop; i++) {
			x = mono[i] - (i > 0 ? mono[i - 1] : 0);
			s += x * x; s2 += mono[i] * mono[i];
		}
		e[f] = Math.log(1 + 200 * Math.sqrt(s / hop));
		e2[f] = Math.log(1 + 200 * Math.sqrt(s2 / hop));
	}
	var flux = new Float64Array(nfr), mx = 0;
	for (f = 0; f < nfr; f++) {
		var a = e[f] - e[f === 0 ? nfr - 1 : f - 1], b = e2[f] - e2[f === 0 ? nfr - 1 : f - 1];
		flux[f] = (a > 0 ? a : 0) + (b > 0 ? b : 0);
		if (flux[f] > mx) mx = flux[f];
	}
	var thr = 0.25 * mx, mind = Math.floor(0.07 * S.recSR / hop);
	var order = [];
	for (f = 0; f < nfr; f++) if (flux[f] >= thr) order.push(f);
	order.sort(function (p, q) { return flux[q] - flux[p]; });
	var marks = [];
	for (var k = 0; k < order.length && marks.length < maxMarks; k++) {
		var fi = order[k], ok = true;
		for (var j = 0; j < marks.length; j++) {
			var dd = Math.abs(fi - marks[j]);
			if (Math.min(dd, nfr - dd) < mind) { ok = false; break; }
		}
		if (ok) marks.push(fi);
	}
	marks.sort(function (p, q) { return p - q; });
	var out = [];
	for (k = 0; k < marks.length; k++) {
		var a0 = Math.max(marks[k] * hop - hop, 1), seg2 = [], m2 = 0;
		for (i = a0; i < Math.min(a0 + 3 * hop, n); i++) {
			var v = Math.abs(mono[i] - mono[i - 1]);
			seg2.push(v); if (v > m2) m2 = v;
		}
		var at = 0;
		for (i = 0; i < seg2.length; i++) if (seg2[i] > 0.35 * m2) { at = i; break; }
		out.push(a0 + at);
	}
	return out;
}

// Kick and snare for fills: band energy over 60 ms after each sticker.
function biquadBand(x, f0, q, type) {
	var w0 = 2 * Math.PI * f0 / S.recSR, al = Math.sin(w0) / (2 * q), cw = Math.cos(w0), b0, b1, b2;
	if (type === 'lp') { b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2; }
	else if (type === 'hp') { b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2; }
	else { b0 = al; b1 = 0; b2 = -al; }
	var a0 = 1 + al, a1 = -2 * cw, a2 = 1 - al;
	var y = new Float64Array(x.length), x1 = 0, x2 = 0, y1 = 0, y2 = 0;
	for (var i = 0; i < x.length; i++) {
		var o = (b0 * x[i] + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0;
		x2 = x1; x1 = x[i]; y2 = y1; y1 = o; y[i] = o;
	}
	return y;
}
function classify(mono) {
	// For each bar of the record: the kick nearest its first beat with the most low end,
	// and the snare on its beat 2 or 4 with the most mid and top. Fills use the bar
	// they start in, so the hands never have far to go.
	var lo = biquadBand(mono, 110, 0.707, 'lp');
	var mid = biquadBand(mono, 245, 1.0, 'bp');
	var hi = biquadBand(mono, 3500, 0.6, 'bp');
	var w = Math.floor(0.06 * S.recSR), beat = spbRec(), tol = 0.040 * S.recSR;
	var nbar = Math.max(1, Math.round(S.Lb / 4)), K = [], Sn = [];
	for (var r = 0; r < nbar; r++) { K.push([-1, -1]); Sn.push([-1, -1]); }
	for (var k = 0; k < S.stk.length; k++) {
		var p = S.stk[k], l = 0, m = 0, h = 0;
		for (var i = p; i < Math.min(p + w, mono.length); i++) { l += lo[i] * lo[i]; m += mid[i] * mid[i]; h += hi[i] * hi[i]; }
		var nb = Math.round(p / beat), off = Math.abs(p - nb * beat);
		if (off > tol) continue;
		var bar = Math.floor(nb / 4) % nbar, inbar = mod(nb, 4);
		if (inbar === 0 && l > K[bar][1]) K[bar] = [k, l];
		if ((inbar === 1 || inbar === 3) && m * h > Sn[bar][1]) Sn[bar] = [k, m * h];
	}
	// bars without a clear hit borrow from the nearest bar that has one
	var outK = [], outS = [];
	for (r = 0; r < nbar; r++) {
		var kk = -1, ss = -1;
		for (var dd = 0; dd < nbar && (kk < 0 || ss < 0); dd++) {
			var r1 = mod(r + dd, nbar), r2 = mod(r - dd, nbar);
			if (kk < 0) kk = K[r1][0] >= 0 ? K[r1][0] : K[r2][0];
			if (ss < 0) ss = Sn[r1][0] >= 0 ? Sn[r1][0] : Sn[r2][0];
		}
		outK.push(kk); outS.push(ss);
	}
	return [outK, outS];
}
function fillStickers(b0) {
	// sample positions of the kick and snare a fill starting at song beat b0 should use
	var c = S.ctl, k, s;
	if (c.fillk > 0 && c.fillk <= S.stk.length) k = S.stk[c.fillk - 1];
	else {
		var bar = Math.floor(rbS(b0) / 4) % Math.max(S.barK.length, 1), ki = S.barK[bar];
		k = ki >= 0 ? S.stk[ki] : 0;
	}
	if (c.fills > 0 && c.fills <= S.stk.length) s = S.stk[c.fills - 1];
	else {
		var bar2 = Math.floor(rbS(b0) / 4) % Math.max(S.barS.length, 1), si = S.barS[bar2];
		s = si >= 0 ? S.stk[si] : spbRec();
	}
	return [k, s];
}

// ------------------------------------------------------------------ test entry point
function _setRecord(L, Lb, anchor, recSR, mono) {
	S.L = L; S.Lb = Lb; S.anchor = mod(anchor, Lb); S.recSR = recSR; S.have = 1;
	S.stk = findStickers(mono, Math.min(256, 4 * Lb));
	var ks = classify(mono);
	S.barK = ks[0]; S.barS = ks[1];
	S.K = S.barK[0] >= 0 ? S.stk[S.barK[0]] : -1; S.S = S.barS[0] >= 0 ? S.stk[S.barS[0]] : -1;
	S.padStk = [S.barK[0], S.barS[0], S.barK[1 % S.barK.length], S.barS[1 % S.barS.length]];
	planEnd = null;
}


// ====================================================================== Max glue
// Everything below runs only inside Max.
var NAMES = {rec: 'dbl_rec', q: 'dbl_q', st: 'dbl_stat', save: 'dbl_save', load: 'dbl_load'};
if (typeof jsarguments !== 'undefined' && jsarguments.length > 5) {
	NAMES.rec = jsarguments[1]; NAMES.q = jsarguments[2]; NAMES.st = jsarguments[3];
	NAMES.save = jsarguments[4]; NAMES.load = jsarguments[5];
}
var bRec = null, bQ = null, bSt = null, bSave = null, bLoad = null;
var ready = false, initAt = 0, handsGuard = true;
var reccmd = 0, sbseq = 0, recArmed = false, lastRecState = 0;
var apiPlay = null, apiTempo = null, apiTime = null;
var restoreState = null;
var IN_MAX = (typeof outlet === 'function') && (typeof post === 'function') && (typeof Task !== 'undefined');
var tick = null;
if (IN_MAX) {
	tick = new Task(onTick, this);
	tick.interval = 20;
}

var LENGTHS = [1, 2, 4, 8, 16, 0];
var LOOPLENS = [0.25, 0.5, 1, 2, 4];
var DDISTS = [0.25, 0.5, 1];
var CUNITS = [0.5, 1];
var PLENS = [0.25, 0.5, 0.75];
var SBACKS = [1, 2, 4, 8];
var ARCS = [8, 16, 32];
var SLOTLENS = [4, 2];
var HELD = ['loop', 'double', 'chase', 'shuffle', 'fill', 'phase'];
var SLOTNAMES = ['straight', 'loop', 'double', 'triple', 'chase', 'shuffle', 'pause',
	'fill1', 'fill2', 'fill3', 'fill4', 'fill5', 'fill6', 'fill7', 'fill8', 'spin', 'phase'];

if (IN_MAX) {
	HOST.gen = function (n, v) { outlet(0, n, v); };
	HOST.ui = function () { outlet(1, arrayfromargs(arguments)); };
	HOST.queueWrite = function (d, slot, vals) { if (bQ) bQ.poke(1, (d * QN + slot) * QF, vals); };
	HOST.heads = function () {
		if (!bSt) return [0, 0];
		var h = bSt.peek(1, 5, 2);
		return [h[0], h[1]];
	};
	HOST.log = function (s) { post('Doubles: ' + s + '\n'); };
}

function buffers() {
	bRec = new Buffer(NAMES.rec); bQ = new Buffer(NAMES.q); bSt = new Buffer(NAMES.st);
	bSave = new Buffer(NAMES.save); bLoad = new Buffer(NAMES.load);
}

function status(t) { outlet(1, 'status', 'set', t); }
function showRecord() {
	if (!S.have) { status('EMPTY'); return; }
	status((S.Lb / 4) + (S.Lb === 4 ? ' BAR  ' : ' BARS  ') + S.tempo.toFixed(1) + ' BPM');
}

// --- start up
function names(rec, q, st, save, load) {
	NAMES.rec = String(rec); NAMES.q = String(q); NAMES.st = String(st);
	NAMES.save = String(save); NAMES.load = String(load);
}
function init() {
	buffers();
	S.sr = 44100;
	try {
		apiPlay = new LiveAPI(function (a) { if (a[0] === 'is_playing') setPlaying(a[1]); }, 'live_set');
		apiPlay.property = 'is_playing';
		apiTempo = new LiveAPI(function (a) { if (a[0] === 'tempo') setTempo(a[1]); }, 'live_set');
		apiTempo.property = 'tempo';
		apiTime = new LiveAPI(function (a) { if (a[0] === 'current_song_time') setSongTime(a[1]); }, 'live_set');
		apiTime.property = 'current_song_time';
		setTempo(apiTempo.get('tempo')[0]);
		setPlaying(apiPlay.get('is_playing')[0]);
	} catch (e) { HOST.log('Live API not available: ' + e); }
	ready = true;
	initAt = Date.now();
	tick.repeat();
	if (restoreState) { restore(restoreState); restoreState = null; }
	showRecord();
	outlet(1, 'strip', 'statname', NAMES.st);
	sendStrip();
}

function setPlaying(v) {
	v = v ? 1 : 0;
	if (v !== S.playing) {
		S.playing = v;
		outlet(0, 'playing', v);
		planEnd = null; TL = [[], []]; sentIdx = [0, 0]; seg = null; segQ = [];
	}
}
function setTempo(v) { S.tempo = v; outlet(0, 'tempo', v); }
function setSongTime(v) { sbseq++; outlet(0, 'songbeat', v); outlet(0, 'sbseq', sbseq); }

// --- the 20 ms tick
function onTick() {
	if (!bSt) return;
	var st = bSt.peek(1, 0, 16);
	S.B = st[0];
	var rs = st[2];
	if (rs !== lastRecState) {
		if (rs === 1) status('ARMED');
		if (rs === 2) status('RECORDING');
		if (rs === 3) finishRecording(st[3], st[4], st[14]);
		if (rs === 0 && lastRecState === 2) { status('STOPPED'); outlet(1, 'record', 'set', 0); }
		lastRecState = rs;
	}
	if (handsGuard && Date.now() - initAt > 1500) handsGuard = false;
	if (S.have && S.playing) plan(S.B);
	stripTick(st);
}

// --- record
function arm(on) {
	reccmd++;
	if (on) {
		var len = LENGTHS[S.ctl.length] === undefined ? 8 : LENGTHS[S.ctl.length];
		var secs = (len || 16) * 4 * 60 / S.tempo;
		if (secs > bRec.framecount() / S.sr) { status('TOO LONG AT THIS TEMPO'); outlet(1, 'record', 'set', 0); return; }
		outlet(0, 'reclen', len * 4);
		outlet(0, 'recstop', 0);
		outlet(0, 'recmode', 1);
		outlet(0, 'reccmd', reccmd);
		recArmed = true;
		status('ARMED');
	} else {
		if (LENGTHS[S.ctl.length] === 0 && lastRecState === 2) { outlet(0, 'recstop', 1); return; }
		outlet(0, 'recmode', 0);
		outlet(0, 'reccmd', reccmd);
		recArmed = false;
		showRecord();
	}
}

function finishRecording(L, recstart, recb) {
	reccmd++;
	outlet(0, 'recmode', 0);
	outlet(0, 'reccmd', reccmd);
	outlet(1, 'record', 'set', 0);
	recArmed = false;
	var Lb = recb > 0 ? recb : 8;
	useRecord(L, Lb, mod(recstart, Lb), S.sr);
	saveRecord();
}

function monoOf(buf, L) {
	var m = new Float32Array(L), CH = 65536, nch = Math.min(buf.channelcount(), 2);
	for (var a = 0; a < L; a += CH) {
		var n = Math.min(CH, L - a), x = buf.peek(1, a, n), y = nch > 1 ? buf.peek(2, a, n) : x;
		if (n === 1) { x = [x]; y = [y]; }
		for (var i = 0; i < n; i++) m[a + i] = 0.5 * (x[i] + y[i]);
	}
	return m;
}

function useRecord(L, Lb, anchor, recSR) {
	outlet(0, 'haverec', 0);
	outlet(0, 'L', L); outlet(0, 'Lb', Lb); outlet(0, 'anchor', anchor);
	var mono = monoOf(bRec, L);
	S.stkOff = {}; S.stkAdd = [];
	_setRecord(L, Lb, anchor, recSR, mono);
	S.wave = overview(mono, 300);
	planEnd = null; TL = [[], []]; sentIdx = [0, 0]; seg = null; segQ = [];
	outlet(0, 'haverec', 1);
	showRecord();
	sendStrip();
}

function overview(mono, n) {
	var out = [], per = mono.length / n;
	for (var k = 0; k < n; k++) {
		var m = 0;
		for (var i = Math.floor(k * per); i < Math.floor((k + 1) * per); i += 8) { var v = Math.abs(mono[i]); if (v > m) m = v; }
		out.push(Math.round(m * 1000) / 1000);
	}
	return out;
}

function deviceFolder() {
	var p = this.patcher ? this.patcher.filepath : '';
	if (!p) return '';
	return p.substring(0, p.lastIndexOf('/') + 1);
}

function stamp() {
	var d = new Date();
	function z(n) { return n < 10 ? '0' + n : '' + n; }
	return d.getFullYear() + z(d.getMonth() + 1) + z(d.getDate()) + '-' + z(d.getHours()) + z(d.getMinutes()) + z(d.getSeconds());
}

function saveRecord() {
	var dir = deviceFolder();
	if (!dir) { HOST.log('device folder unknown, record not saved to disk'); return; }
	var L = S.L, CH = 65536;
	bSave.send('sizeinsamps', L, 2);
	for (var a = 0; a < L; a += CH) {
		var n = Math.min(CH, L - a);
		for (var c = 1; c <= 2; c++) {
			var x = bRec.peek(c, a, n);
			bSave.poke(c, a, n === 1 ? [x] : x);
		}
	}
	S.file = dir + 'doubles-' + stamp() + '.wav';
	bSave.send('writewave', S.file);
	HOST.log('record saved to ' + S.file);
	notifyclients();
}

function loadFile(path, Lb, anchor) {
	bLoad.send('replace', path);
	var fr = bLoad.framecount(), nch = bLoad.channelcount();
	if (!fr) { status('RECORD MISSING'); return false; }
	var L = Math.min(fr, bRec.framecount() - 2), CH = 65536;
	for (var a = 0; a < L; a += CH) {
		var n = Math.min(CH, L - a);
		for (var c = 1; c <= 2; c++) {
			var x = bLoad.peek(Math.min(c, nch), a, n);
			bRec.poke(c, a, n === 1 ? [x] : x);
		}
	}
	var fileSR = bLoad.framecount() / (bLoad.length() / 1000);
	if (!Lb) {
		var bars = fr / fileSR * S.tempo / 240, b2 = 1;
		while (b2 * 2 <= 16 && Math.abs(b2 * 2 - bars) < Math.abs(b2 - bars)) b2 *= 2;
		Lb = b2 * 4;
	}
	useRecord(L, Lb, anchor || 0, fileSR);
	return true;
}

// --- keeping the record with the Live set (js is a stored parameter)
function getvalueof() {
	if (!S.have) return 'empty';
	var off = [];
	for (var k in S.stkOff) off.push(k);
	return JSON.stringify({file: S.file, Lb: S.Lb, anchor: S.anchor, off: off, add: S.stkAdd});
}
function setvalueof(v) {
	if (!v || v === 'empty' || v === 0) return;
	try { var o = JSON.parse(v); } catch (e) { return; }
	if (ready) restore(o); else restoreState = o;
}
function restore(o) {
	if (!o.file) return;
	if (loadFile(o.file, o.Lb, o.anchor)) {
		S.file = o.file;
		for (var i = 0; o.off && i < o.off.length; i++) S.stkOff[o.off[i]] = 1;
		applyStickerEdits();
	}
}

// --- stickers edited on the strip
function applyStickerEdits() {
	var base = findStickers(monoOf(bRec, S.L), Math.min(256, 4 * S.Lb)), out = [];
	for (var i = 0; i < base.length; i++) if (!S.stkOff[base[i]]) out.push(base[i]);
	for (i = 0; i < S.stkAdd.length; i++) out.push(S.stkAdd[i]);
	out.sort(function (a, b) { return a - b; });
	S.stk = out;
	sendStrip();
}
function stripClick(x, shift) {
	if (!S.have) return;
	var p = Math.round(x * S.L);
	if (shift) { S.stkAdd.push(p); applyStickerEdits(); notifyclients(); return; }
	var best = -1, bd = 1e30;
	for (var i = 0; i < S.stk.length; i++) { var d = Math.abs(S.stk[i] - p); if (d < bd) { bd = d; best = i; } }
	if (best >= 0 && bd < 0.02 * S.L) {
		var s = S.stk[best], ai = S.stkAdd.indexOf(s);
		if (ai >= 0) S.stkAdd.splice(ai, 1); else S.stkOff[s] = 1;
		applyStickerEdits();
		notifyclients();
	}
}

// --- strip display
function sendStrip() {
	if (!S.have) { outlet(1, 'strip', 'clear'); return; }
	if (S.wave) outlet(1, ['strip', 'wave'].concat(S.wave));
	var st = [];
	for (var i = 0; i < S.stk.length; i++) st.push(Math.round(S.stk[i] / S.L * 10000) / 10000);
	outlet(1, ['strip', 'stk'].concat(st.length ? st : [-1]));
	var ks = fillStickers(S.B || 0);
	outlet(1, 'strip', 'ks', ks[0] / S.L, ks[1] / S.L);
	outlet(1, 'strip', 'bars', S.Lb / 4);
}
var lastLate = 0;
function stripTick(st) {
	if (st[11] !== lastLate) { lastLate = st[11]; outlet(1, 'strip', 'late'); }
}

// --- panel messages
function anything() {
	var a = arrayfromargs(arguments), v = a.length ? a[0] : 0, m = messagename, c = S.ctl;
	switch (m) {
	case 'init': init(); return;
	case 'record': if (ready) arm(v); return;
	case 'length': c.length = v; return;
	case 'clear':
		if (!v) return;
		S.have = 0; S.file = ''; outlet(0, 'haverec', 0); planEnd = null; TL = [[], []];
		showRecord(); sendStrip(); notifyclients(); return;
	case 'drop': if (ready) { if (loadFile(a.join(' '))) { S.file = a.join(' '); notifyclients(); } } return;
	case 'strip': if (a[0] === 'click') stripClick(a[1], a[2]); return;
		case 'quant': c.quant = QUANTS[v] || 1; return;
	case 'looplen': c.looplen = LOOPLENS[v] || 1; return;
	case 'doubledist': c.doubledist = DDISTS[v] || 0.5; return;
	case 'chaseunit': c.chaseunit = CUNITS[v] || 1; return;
	case 'shuffleamt': c.shuffle = v / 100; return;
	case 'pauselen': c.pauselen = PLENS[v] || 0.5; return;
	case 'fillpat': c.fillpat = v; return;
	case 'spinback': c.spinback = SBACKS[v] || 4; return;
	case 'fillk': c.fillk = v; sendStrip(); return;
	case 'fills': c.fills = v; sendStrip(); return;
	case 'hands': if (!handsGuard) setHands(v); return;
	case 'timing': case 'weight': case 'slop': case 'risk':
		c[m] = v / 100; outlet(0, m, v / 100); return;      // the dials read 0 to 100
	case 'copy': c.copy = v / 100; outlet(0, 'wear', v / 100); return;
	case 'mode':
		c.mode = v;
		if (v === 1) S.routineAnchor = Math.ceil((S.B + 0.25) / 4) * 4;
		if (v === 2) S.instinctBar0 = Math.ceil((S.B + 0.25) / 4);
		if (S.playing && S.have) cutAt(Math.ceil((S.B + 0.25) / 4) * 4);
		return;
	case 'scratch': c.scratch = v / 100; return;
	case 'density': c.density = v / 100; return;
	case 'funk': c.funk = v / 100; return;
	case 'arc': c.arc = ARCS[v] || 16; return;
	case 'slotlen': c.slotlen = SLOTLENS[v] || 4; return;
	case 'reset': if (v) { S.routineAnchor = Math.ceil((S.B + 0.25) / 4) * 4; if (S.playing && S.have && c.mode === 1) cutAt(S.routineAnchor); } return;
	case 'straight': outlet(0, 'strlev', v / 100); return;
	case 'level': outlet(0, 'level', Math.pow(10, v / 20)); return;
	case 'brake': outlet(0, 'brake', v); return;
	}
	if (m.indexOf('slot') === 0) {
		var n = parseInt(m.substr(4), 10);
		if (n >= 1 && n <= 16) S.routine[n - 1] = SLOTNAMES[v] || 'straight';
		return;
	}
	if (m.indexOf('pad') === 0) { padPress(parseInt(m.substr(3), 10) - 1, v ? 1 : 0); return; }
	if (MOVES.indexOf(m) >= 0 || m === 'drag' || m === 'baby') { movePress(m, v ? 1 : 0); return; }
	HOST.log('unknown message ' + m);
}
function bang() {}


if (!IN_MAX && typeof module !== 'undefined' && module) {
	module.exports = {
		S: S, HOST: HOST, plan: plan, movePress: movePress, padPress: padPress, cutAt: cutAt,
		setRecord: _setRecord, findStickers: findStickers, TL: function () { return TL; },
		setRng: function (f) { rng = f; }, reset: function () { planEnd = null; resetTimeline(); },
		FILLS: FILLS
	};
}
