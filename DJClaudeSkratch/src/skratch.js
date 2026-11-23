// DJ Claude Skratch: the planner for the Max for Live device.
// Turns keys, the Routine and Instinct into words, and words into what two hands do:
// segments of record movement (skr_q) and fader moves (skr_f). gen~ plays them.
// This is a port of class Scratch in ref/scratch_engine.py and of the Instinct in
// ref/make_demos.py. Times inside the planner are in samples on a timeline tied to the
// beat (b2t, t2b). The human error is decided here, so gen~ decides nothing.
//
// Inlet: messages from the panel and the keys.  Outlet 0: gen~.  Outlet 1: the panel.

autowatch = 1;
inlets = 1;
outlets = 2;

// ------------------------------------------------------------------ settings
var QN = 256, QF = 12;          // hand segment queue
var FN = 512, FF = 4;           // fader event queue
var MAXREC = 20;                // seconds of record
var CUTOS = 4;                  // the cartridge runs at 4 x the sample rate
var WORDS = ['baby', 'forward', 'stab', 'chirp', 'flare1', 'flare2', 'crab', 'tear',
	'scribble', 'transform', 'drag', 'tip', 'lazer', 'release', 'echo', 'reverse'];
var WORDLABEL = ['BABY', 'FORWARD', 'STAB', 'CHIRP', '1-FLARE', '2-FLARE', 'CRAB', 'TEAR',
	'SCRIBBLE', 'TRANSFORM', 'DRAG', 'TIP', 'LAZER', 'RELEASE', 'ECHO', 'REVERSE'];
var KEY0 = 36, CUEKEY0 = 60;    // C1 = first word, C3 = cue point 1
var SPEEDS = [0.5, 0.25, 1 / 6, 0.125];            // menu order: 1/8, 1/16, 1/16T, 1/32
var SPEEDORDER = [0.125, 1 / 6, 0.25, 0.5];        // fastest to slowest
var QUANTS = [0.25, 0.5, 1, 0];                    // 1/16, 1/8, 1/4, OFF
var PHRASEON = [1, 2, 4, 8], PHRASEOFF = [0, 1, 2, 4, 8];

// low-pass after the cartridge, elliptic, 6th order, 19 kHz, at 4 x these rates (ref/device_form.py)
var LOWPASS = {
	44100: [0.002236969227, 0.003004859885, 0.002236969227, 1, -1.404566950689, 0.520916262544, 1, -0.301113647742, 1, 1, -1.39470843125, 0.683955485214, 1, -0.803222196871, 1, 1, -1.435075008069, 0.892138899374],
	48000: [0.001835153443, 0.002270420218, 0.001835153443, 1, -1.451515384263, 0.551060540451, 1, -0.476104569712, 1, 1, -1.456312918364, 0.703810297851, 1, -0.950091566844, 1, 1, -1.508868400447, 0.899129673083],
	88200: [6.445252882798e-04, 1.169251651210e-04, 6.445252882798e-04, 1, -1.695767917097, 7.278504391235e-01, 1, -1.407278316763, 1, 1, -1.743384632744, 8.223195056549e-01, 1, -1.634400393697, 1, 1, -1.819131643240, 9.408334279014e-01],
	96000: [5.841811313529e-04, 5.501918411655e-06, 5.841811313529e-04, 1, -1.719833139755, 7.471747779841e-01, 1, -1.489195152173, 1, 1, -1.768146095456, 8.352774586663e-01, 1, -1.687804006956, 1, 1, -1.842156074597, 9.453253040630e-01]
};

// the built-in record: 16-bit mono PCM at 44.1 kHz, base64. Filled in by tools/build_device.py.
var BUILTIN_B64 = /*BUILTIN*/'';

// ------------------------------------------------------------------ state
var S = {
	sr: 44100, tempo: 120, playing: 0, B: 0, jumps: -1, gen: 1,
	Ban: 0,                       // the beat at timeline sample 0
	ctl: {
		speed: 0.25, quant: 0.25, mode: 0, phon: 4, phoff: 0,
		density: 0.5, skill: 0.75, motif: 0.6, release: 0.6, funk: 0.5, walk: 0,
		timing: 0.5, weight: 0.5, slop: 0.5, risk: 0.5, level: 1
	},
	on: {},                       // word switches
	held: [],                     // keys down, in order: {w, vel}
	fresh: null,                  // a key pressed whose first unit has not been planned yet
	echoN: 0, lastKeyWord: null, unitLen: 0.5,
	gridEnd: -1e9,                // where on the grid the last planned word ends
	calls: [],                    // planned Routine or Instinct cells not yet turned into strokes
	genNext: null,                // beat where the next Routine cycle or Instinct phrase goes
	routine: [], routineCue: [],
	cueIdx: 0,
	velT: 1, velA: 1, swingHold: null,
	rel: null                     // a held Release: {open, tEnd, b}
};
for (var wi = 0; wi < WORDS.length; wi++) S.on[WORDS[wi]] = 1;

var R = {have: 0, sr: 44100, n: 0, L: null, R: null, cues: [], wave: [], file: '', builtin: 1, cutting: 0};

var H = null;                   // the hands, from the four dials
var D = null;                   // the record hand and fader hand, as the planner sees them
var out = {seg: [], fad: []};   // waiting to be written to the queues
var qtail = 0, ftail = 0;
var rng = Math.random;

// host hooks, replaced in Max (bottom of file) or by the test harness
var HOST = {
	gen: function (name, v) {},
	ui: function () {},
	status: function () { return null; },   // [B, playing, qh, fh, pos, v, g, alive, sr, jumps, act, gen]
	writeSeg: function (slot, vals) {},
	writeFad: function (slot, vals) {},
	log: function (s) {},
	trace: null                             // test harness only
};

// ------------------------------------------------------------------ helpers
function gauss() { var u = 1 - rng(), v = rng(); return Math.sqrt(-2 * Math.log(u)) * Math.cos(2 * Math.PI * v); }
function normal(sd) { return gauss() * sd; }
function spbS() { return S.sr * 60 / S.tempo; }
function b2t(b) { return (b - S.Ban) * spbS(); }
function t2b(t) { return S.Ban + t / spbS(); }
function swingOf() { return S.swingHold !== null ? S.swingHold : 0.5 + 0.16 * S.ctl.funk; }
function sw(b) {
	var w = Math.floor(b * 2), x = b * 2 - w, s = swingOf();
	x = x < 0.5 ? x * s / 0.5 : s + (x - 0.5) * (1 - s) / 0.5;
	return (w + x) / 2;
}
function qUp(b, q) { if (!q) return b; return Math.ceil(b / q - 1e-9) * q; }

// ------------------------------------------------------------------ the hands (class Hands)
function makeHands(c) {
	var T = c.timing, W = c.weight, Sl = c.slop, Rk = c.risk;
	return {
		T: T, W: W, S: Sl, R: Rk,
		grid_jit: 0.006 * T, turn_jit: 0.010 * T, drift: 0.6, fad_jit: 0.005 * T,
		trav: 0.22 * Sl, land: 0.003 * Sl, land_per: 0.05 * Sl, lvl: 0.06 * Sl,
		fade: 0.0006 + 0.0016 * Sl, leak: 0.010 * Sl,
		skew: 0.80 - 0.22 * W, tau_rel: 0.006 + 0.022 * W, tau_stop: 0.003 + 0.010 * W,
		tremor: 0.02 * Sl, flub: 0.10 * Rk
	};
}
function sendHands() {
	H = makeHands(S.ctl);
	HOST.gen('taustop', H.tau_stop);
	HOST.gen('tremor', H.tremor);
	HOST.gen('fadet', H.fade);
}

// ------------------------------------------------------------------ the deck (class Deck)
function newDeck(t, p, v) {
	var lo = 0, hi = R.have ? R.n / R.sr - 0.002 : 1;
	return {t: Math.floor(t), p: p, v: v || 0, cue: cueSec(), lo: lo, hi: hi, late: 0,
		opens: [], clicks: []};
}
function cueSec() { return R.cues.length ? R.cues[Math.min(S.cueIdx, R.cues.length - 1)] : 0; }

function seg(kind, t0, m, p1, a, k, e, x1, x2, x3) {
	out.seg.push([t2b(t0), kind, m / S.sr, p1, a, k, e, x1 || 0, x2 || 0, x3 || 0, S.gen, 0]);
}

function decaySum(v0, r, m) { return v0 * r * (1 - Math.pow(r, m)) / (1 - r); }

function hold(t1) {
	var m = Math.floor(t1) - D.t;
	if (m <= 0) return;
	var v0 = D.v;
	if (Math.abs(v0) > 1e-3) {
		var r = Math.exp(-1 / (H.tau_stop * S.sr));
		D.p += decaySum(v0, r, m) / S.sr;
		D.v = v0 * Math.pow(r, m);
	} else D.v = 0;
	D.t += m;
}

var SHAPE = {push: 0, pull: 1, flick: 2, even: 3};
function stroke(t1, travel, kind, vcap) {
	if (vcap === undefined) vcap = 3.0;
	t1 = Math.floor(t1);
	var v0 = D.v, SR = S.sr;
	if (Math.abs(v0) > 0.05) {                                   // the hand lands on a moving record
		var md = Math.floor(Math.min((t1 - D.t) / 4, 4 * H.tau_stop * SR));
		if (md > 0) {
			var r = Math.exp(-1 / (H.tau_stop * SR)), dp = decaySum(v0, r, md) / SR;
			seg(4, D.t, md, D.p + dp, 0, H.tau_stop, 1);
			D.p += dp; D.v = v0 * Math.pow(r, md); D.t += md;
			travel -= dp;
		}
	}
	var tgt = Math.min(Math.max(D.p + travel, D.lo), D.hi);
	travel = tgt - D.p;
	if (vcap) t1 = Math.max(t1, D.t + Math.ceil(Math.abs(travel) / vcap * SR));   // a hand has a top speed: it is late instead
	var m = t1 - D.t;
	if (m < 8) return;
	var k = kind === 'push' ? H.skew : kind === 'flick' ? H.skew * 0.75 : 1.0, e = kind === 'even' ? 0.6 : 1.0;
	var sum = 0, last = 0;
	for (var i = 0; i < m; i++) {
		var sh = Math.sin(Math.PI * Math.pow((i + 0.5) / m, k));
		if (e !== 1) sh = Math.pow(Math.max(sh, 0), e);
		sum += sh; last = sh;
	}
	var a = sum > 0 ? travel * SR / sum : 0;
	if (HOST.trace) HOST.trace('stroke', {tb: t2b(D.t), te: t2b(t1), p0: D.p, p1: tgt, dur: m / SR, vcap: vcap || 0});
	seg(1, D.t, m, tgt, a, k, e);
	D.p = tgt; D.v = last * a; D.t = t1;
}
function to(t1, target, kind, vcap) { stroke(t1, target - D.p, kind || 'pull', vcap); }

function play(t1, rate) {
	if (rate === undefined) rate = 1.0;
	var m = Math.floor(t1) - D.t;
	if (m <= 0) return;
	var v0 = D.v, r = Math.exp(-1 / (H.tau_rel * S.sr));
	var sum = rate * m + decaySum(v0 - rate, r, m);
	var p1 = D.p + sum / S.sr;
	seg(2, D.t, m, p1, rate, H.tau_rel, 1);
	D.p = p1; D.v = rate + (v0 - rate) * Math.pow(r, m); D.t += m;
}

function wiggle(t1, f, amp) {
	var m = Math.floor(t1) - D.t;
	if (m <= 0) return;
	var cyc = Math.max(1, Math.round(m / S.sr * f)), depth = H.S * 0.15, fw = 1.3 + rng();
	var tw = 2 * Math.PI * fw, norm = 1 + depth * (1 - Math.cos(tw)) / tw, sum = 0, last = 0, i, c;
	function at(i) {
		var kk = i / m, kn = (i + 1) / m, wn = (kn + depth * (1 - Math.cos(tw * kn)) / tw) / norm;
		return amp * Math.sin(2 * Math.PI * cyc * wn) * Math.min(1, Math.min(kk, 1 - kk) * 12);
	}
	for (i = 0; i < m; i++) sum += at(i);
	var mean = sum / m;
	last = at(m - 1) - mean;
	seg(3, D.t, m, D.p, amp, cyc, depth, fw, mean, 0);
	D.v = last; D.t += m;
}

function open(t0, t1, amp, exact) {
	if (amp === undefined) amp = 1.0;
	if (!exact) {
		t0 += normal(H.fad_jit) * S.sr;
		t1 += (normal(H.fad_jit) + H.leak * rng()) * S.sr;
	}
	if (t1 - t0 > 0.004 * S.sr) {
		var o = {a: Math.floor(t0), b: Math.floor(t1), amp: amp * S.velA * Math.min(1 + normal(H.lvl), 1.12), sa: 0, sb: 0};
		D.opens.push(o);
		return o;
	}
	return null;
}
function click(t, closed) {
	if (closed === undefined) closed = 0.022;
	if (rng() < H.flub * 1.5) return;                     // a click that does not make it
	t += normal(H.fad_jit) * S.sr;
	var w = closed * (1 + 0.3 * H.S * gauss()) * S.sr;
	D.clicks.push({a: Math.floor(t - w / 2), b: Math.floor(t + w / 2), sa: 0, sb: 0});
}

// fader: turn the opens and clicks into gain events, for edges that can no longer change.
// gain(t) = 0 inside a click, else the largest open containing t (as Deck.gain in the reference)
function gainAt(t) {
	var i, g = 0;
	for (i = 0; i < D.clicks.length; i++) if (D.clicks[i].a <= t && t < D.clicks[i].b) return 0;
	for (i = 0; i < D.opens.length; i++) { var o = D.opens[i]; if (o.a <= t && t < o.b && o.amp > g) g = o.amp; }
	return g;
}
function emitFader(frontier) {
	var edges = [], all = D.opens.concat(D.clicks), i;
	for (i = 0; i < all.length; i++) {
		var x = all[i];
		if (!x.sa && x.a <= frontier) { edges.push(x.a); x.sa = 1; }
		if (!x.sb && x.b <= frontier) { edges.push(x.b); x.sb = 1; }
	}
	if (!edges.length) return;
	edges.sort(function (p, q) { return p - q; });
	var half = H.fade * S.sr / 2;
	for (i = 0; i < edges.length; i++) {
		if (i && edges[i] === edges[i - 1]) continue;
		var g1 = gainAt(edges[i]), g0 = gainAt(edges[i] - 1);
		if (g1 !== g0) out.fad.push([t2b(edges[i] - half), g1, S.gen, 0]);
	}
	var keep = frontier - 2 * S.sr;
	D.opens = D.opens.filter(function (o) { return !(o.sa && o.sb && o.b < keep); });
	D.clicks = D.clicks.filter(function (c) { return !(c.sa && c.sb && c.b < keep); });
}

// ------------------------------------------------------------------ class Scratch: timing
function at(b, jit) {
	var sd = jit === 'turn' ? H.turn_jit : H.grid_jit;
	D.late = H.drift * D.late + (1 - H.drift) * normal(H.grid_jit * 1.5);
	return Math.floor(b2t(sw(b)) + (D.late + normal(sd)) * S.sr);
}
function err(dist) {
	var e = normal(H.land + H.land_per * dist);
	if (rng() < H.flub * 0.5) e -= 0.03 + 0.03 * rng();      // pulled back too far
	return e;
}
function trav(u, sp, cap) {
	var t = u * spbS() / S.sr * sp * Math.max(0.4, 1 + normal(H.trav)) * S.velT;
	return cap ? Math.min(t, cap) : t;
}
function start(b) {
	var tb = at(b);
	if (Math.abs(D.p - D.cue) > 0.08) {                     // get to the cue first, fader closed
		var dist = Math.abs(D.p - D.cue), need = Math.floor(Math.min(0.06 + dist / 4.0, 0.40) * S.sr), ta = D.t;
		to(D.t + need, D.cue + err(dist), 'pull', null);
		D.clicks.push({a: Math.floor(ta - 0.004 * S.sr), b: D.t, sa: 0, sb: 0});
		if (HOST.trace) HOST.trace('cuemove', {tb: t2b(ta), te: t2b(D.t)});
	}
	hold(tb);
	return D.t;
}
function fwd(b1, travel, kind) { stroke(at(b1, 'turn'), travel, kind || 'push'); return D.t; }
function back(b1, kind) { to(at(b1), D.cue + err(Math.abs(D.p - D.cue)), kind || 'pull'); return D.t; }
function silentBack(b1) {
	var dist = Math.abs(D.p - D.cue), t1 = at(b1);
	var quick = D.t + Math.floor((0.06 + dist / 2.5) * S.sr), least = D.t + Math.floor((0.04 + dist / 5.0) * S.sr);
	to(Math.max(Math.min(t1, quick), least), D.cue + err(dist), 'pull', null);
}
var MS2 = 0.002;

// ------------------------------------------------------------------ the words
var W = {
	baby: function (b, u, n, sp, cap, kind) {
		if (n === undefined) n = 1; if (sp === undefined) sp = 1.0; kind = kind || 'push';
		var t0 = start(b);
		for (var i = 0; i < n; i++) {
			fwd(b + (2 * i + 1) * u, trav(u, sp, cap), kind);
			back(b + (2 * i + 2) * u, kind === 'push' ? 'pull' : kind);
		}
		open(t0 - MS2 * S.sr, D.t);
	},
	drag: function (b, u, n, sp) { W.baby(b, u, n || 1, sp || 0.35, null, 'even'); },
	tip: function (b, u, n) { W.baby(b, u, n || 2, 0.9, 0.045); },
	forward: function (b, snd, ret, amp, frac) {
		if (amp === undefined) amp = 1.0; if (frac === undefined) frac = 0.86;
		var t0 = start(b); play(at(b + snd * frac)); var t1 = D.t;
		open(t0 - MS2 * S.sr, t1, amp); silentBack(b + snd + ret);
	},
	reverse: function (b, snd, pre, sp) {
		start(b - pre); stroke(at(b), trav(snd, sp || 1.0), 'push'); var t1 = D.t;
		back(b + snd); open(t1, D.t);
	},
	stab: function (b, snd, ret, sp) {
		if (snd === undefined) snd = 0.125; if (ret === undefined) ret = 0.125;
		var t0 = start(b); fwd(b + snd, trav(snd, sp || 1.7), 'flick'); var t1 = D.t;
		open(t0 - MS2 * S.sr, t0 + 0.72 * (t1 - t0)); silentBack(b + snd + ret);
	},
	lazer: function (b, snd, ret, travel, tail) {
		if (snd === undefined) snd = 0.125; if (ret === undefined) ret = 0.375;
		if (travel === undefined) travel = 0.26; if (tail === undefined) tail = true;
		var t0 = start(b);
		stroke(at(b + snd, 'turn'), travel * (1 + normal(H.trav)) * S.velT, 'flick', 4.5); var t1 = D.t;
		if (tail) { back(b + snd + ret, 'even'); open(t0 - MS2 * S.sr, D.t); }
		else { open(t0 - MS2 * S.sr, t1); silentBack(b + snd + ret); }
	},
	chirp: function (b, u, n, sp) {
		if (n === undefined) n = 1; if (sp === undefined) sp = 1.1;
		var t0 = start(b), a = t0 - MS2 * S.sr;
		for (var i = 0; i < n; i++) {
			var f0 = D.t, f1 = fwd(b + (2 * i + 1) * u, trav(u, sp)), b1 = back(b + (2 * i + 2) * u);
			open(a, f0 + 0.52 * (f1 - f0)); a = f1 + 0.50 * (b1 - f1);
		}
		open(a, D.t);
	},
	flare: function (b, u, n, clicks, sp) {
		if (sp === undefined) sp = 1.0;
		var t0 = start(b);
		for (var i = 0; i < 2 * n; i++) {
			var a = D.t, z = (i % 2 === 0) ? fwd(b + (i + 1) * u, trav(u, sp)) : back(b + (i + 1) * u);
			for (var c = 0; c < clicks; c++) click(a + (c + 1) / (clicks + 1) * (z - a));
		}
		open(t0 - MS2 * S.sr, D.t);
	},
	crab: function (b, u, taps, both, sp) {
		if (taps === undefined) taps = 4; if (both === undefined) both = true; if (sp === undefined) sp = 0.9;
		start(b);
		for (var i = 0; i < (both ? 2 : 1); i++) {
			var a = D.t, z = i === 0 ? fwd(b + u, trav(u, sp)) : back(b + 2 * u);
			var gap = Math.min((z - a) / taps, 0.060 * S.sr) * (1 + 0.1 * H.T * gauss());
			for (var k = 0; k < taps; k++) open(a + k * gap, a + (k + 0.52) * gap);
		}
		if (!both) silentBack(b + 2 * u);
	},
	tear: function (b, u, form, sp) {
		if (sp === undefined) sp = 1.1;
		var t0 = start(b), tr = trav(u, sp), c = D.cue;
		if ((form || 'f-bb') === 'f-bb') { fwd(b + u, tr); to(at(b + 2 * u, 'turn'), c + tr * 0.48, 'pull'); back(b + 3 * u); }
		else { fwd(b + u, tr * 0.5); fwd(b + 2 * u, tr * 0.5); back(b + 3 * u); }
		open(t0 - MS2 * S.sr, D.t);
	},
	scribble: function (b, dur, f, amp) {
		var t0 = start(b); wiggle(at(b + dur), f || 15.0, (amp || 1.25) * S.velT); open(t0 - MS2 * S.sr, D.t);
	},
	transform: function (b, dur, step, duty, pat, mode, sp) {
		if (duty === undefined) duty = 0.5; mode = mode || 'drag'; if (sp === undefined) sp = 0.45;
		var t0 = start(b);
		if (mode === 'drag') { stroke(at(b + dur / 2, 'turn'), trav(dur / 2, sp), 'even'); back(b + dur, 'even'); }
		else play(at(b + dur));
		for (var k = 0; k * step < dur - 1e-6; k++) {
			if (!pat || pat[k % pat.length]) {
				var g0 = at(b + k * step), g1 = at(b + (k + duty) * step);
				if (g1 > t0) open(Math.max(g0, t0), g1);
			}
		}
		if (mode !== 'drag') silentBack(b + dur + 0.5);
	},
	release: function (b, n, ret) {
		var t0 = start(b); play(at(b + n)); open(t0 - MS2 * S.sr, D.t); silentBack(b + n + ret);
	},
	echo: function (b, u, n, k) { for (var i = 0; i < n; i++) W.forward(b + i * u, u * 0.55, u * 0.45, Math.pow(k, i)); }
};

// ------------------------------------------------------------------ keys: one unit of each word
// returns the unit length in beats; u is the SPEED in beats
var UNIT = {
	baby: [1, function (b, u) { W.baby(b, u); return 2 * u; }],
	forward: [1, function (b, u) { W.forward(b, u, u); return 2 * u; }],
	stab: [0.5, function (b, u) { W.stab(b, u / 2, u / 2); return u; }],
	chirp: [1, function (b, u) { W.chirp(b, u); return 2 * u; }],
	flare1: [2, function (b, u) { W.flare(b, 2 * u, 1, 1); return 4 * u; }],
	flare2: [2, function (b, u) { W.flare(b, 2 * u, 1, 2); return 4 * u; }],
	crab: [2, function (b, u) { W.crab(b, 2 * u); return 4 * u; }],
	tear: [1, function (b, u) { W.tear(b, u); return 4 * u; }],
	scribble: [2, function (b, u) { W.scribble(b, 2 * u); return 2 * u; }],
	transform: [1, function (b, u) { W.transform(b, 8 * u, u, 0.5, [1, 1, 0, 1]); return 8 * u; }],
	drag: [2, function (b, u) { W.drag(b, 2 * u); return 4 * u; }],
	tip: [0.5, function (b, u) { W.tip(b, u / 2, 2); return 2 * u; }],
	lazer: [0.5, function (b, u) { W.lazer(b, u / 2, 1.5 * u); return 2 * u; }],
	echo: [1, function (b, u, i) { W.forward(b, 1.1 * u, 0.9 * u, Math.pow(0.6, i)); return 2 * u; }],
	reverse: [1, function (b, u) { W.reverse(b + u, u, u); return 2 * u; }]
};
function speedFor(w) {
	var u = S.ctl.speed, f = UNIT[w] ? UNIT[w][0] : 1, i = SPEEDORDER.indexOf(u);
	while (f * u * 60 / S.tempo < 0.045 && i < SPEEDORDER.length - 1) u = SPEEDORDER[++i];
	return u;
}

// ------------------------------------------------------------------ Instinct cells (make_demos.py)
// name: [beats, weight, difficulty, words used, function]
var CELLS = {
	'rest': [1, 3, 0, [], function (b) {}],
	'baby8': [1, 4, 1, ['baby'], function (b) { W.baby(b, 0.5); }],
	'baby16': [1, 5, 2, ['baby'], function (b) { W.baby(b, 0.25); W.baby(b + 0.5, 0.25); }],
	'pickup': [1, 4, 1, ['baby'], function (b) { W.baby(b + 0.5, 0.25); }],
	'baby-rest': [1, 3, 1, ['baby'], function (b) { W.baby(b, 0.25); }],
	'fwd-fwd': [1, 4, 2, ['forward'], function (b) { W.forward(b, 0.25, 0.25); W.forward(b + 0.5, 0.25, 0.25); }],
	'fwd-baby': [1, 5, 2, ['forward', 'baby'], function (b) { W.forward(b, 0.25, 0.25); W.baby(b + 0.5, 0.25); }],
	'stabs': [1, 3, 3, ['stab'], function (b) { W.stab(b); W.stab(b + 0.25); W.stab(b + 0.5); }],
	'stab-off': [1, 3, 2, ['stab'], function (b) { W.stab(b + 0.5); W.stab(b + 0.75); }],
	'chirp': [1, 4, 3, ['chirp'], function (b) { W.chirp(b, 0.25, 2); }],
	'chirp-baby': [1, 3, 2, ['chirp', 'baby'], function (b) { W.chirp(b, 0.25, 1); W.baby(b + 0.5, 0.25); }],
	'flare1': [1, 3, 3, ['flare1'], function (b) { W.flare(b, 0.5, 1, 1); }],
	'flare2': [1, 2, 4, ['flare2'], function (b) { W.flare(b, 0.5, 1, 2); }],
	'crab': [1, 2, 4, ['crab'], function (b) { W.crab(b, 0.5); }],
	'tear': [1, 3, 3, ['tear', 'stab'], function (b) { W.tear(b, 0.25); W.stab(b + 0.75); }],
	'tear-trip': [1, 2, 3, ['tear'], function (b) { W.tear(b, 1 / 3); }],
	'scribble': [1, 2, 3, ['scribble', 'baby'], function (b) { W.scribble(b, 0.5); W.baby(b + 0.5, 0.25); }],
	'tips': [1, 2, 4, ['tip', 'baby'], function (b) { W.tip(b, 0.125, 2); W.baby(b + 0.5, 0.25); }],
	'lazer': [1, 1.5, 2, ['lazer', 'baby'], function (b) { W.lazer(b, 0.125, 0.375); W.baby(b + 0.5, 0.25); }],
	'transform': [2, 2, 3, ['transform'], function (b) { W.transform(b, 2, 0.25, 0.5, [1, 1, 0, 1]); }],
	'transform-trip': [2, 1.2, 4, ['transform'], function (b) { W.transform(b, 2, 1 / 6); }],
	'drag': [2, 1.5, 1, ['drag'], function (b) { W.drag(b, 1.0); }],
	'echo': [2, 1.2, 2, ['echo'], function (b) { W.echo(b, 0.5, 4, 0.6); }],
	'release': [2, 0, 1, ['release'], function (b) { W.release(b, 1.5, 0.5); }]
};
var CELLNAMES = [];
for (var cn in CELLS) CELLNAMES.push(cn);
var ROUTINE_DEFAULT = ['tear-trip', 'tear-trip', 'chirp', 'fwd-fwd', 'tear-trip', 'tear-trip', 'chirp', 'fwd-fwd',
	'fwd-baby', 'stabs', 'rest', 'baby16', 'pickup', 'stabs', 'rest', 'baby16'];
for (var ri = 0; ri < 16; ri++) { S.routine.push(ROUTINE_DEFAULT[ri]); S.routineCue.push(0); }

function cellOn(nm) {
	var ws = CELLS[nm][3];
	for (var i = 0; i < ws.length; i++) if (!S.on[ws[i]]) return false;
	return true;
}
function cellWeight(nm) {
	var c = CELLS[nm], d = S.ctl.density;
	if (nm === 'rest') return c[1] * (1.3 - 1.25 * d) * 1.5;
	if (!cellOn(nm)) return 0;
	if (c[2] > 1 + 3.2 * S.ctl.skill) return 0;
	var w = c[1] * Math.exp(-Math.abs(c[2] - (1.6 + 2.6 * d)) * 0.6);
	if (nm === 'tear-trip' || nm === 'transform-trip') {
		var f = S.ctl.funk;
		w *= f < 0.5 ? 0.3 + 1.4 * f : 1 + 2 * (f - 0.5);
	}
	return w;
}
function pick(names, ws) {
	var tot = 0, i;
	for (i = 0; i < ws.length; i++) tot += ws[i];
	if (!(tot > 0)) return null;
	var r = rng() * tot;
	for (i = 0; i < ws.length; i++) { r -= ws[i]; if (r < 0) return names[i]; }
	return names[names.length - 1];
}
function instBar() {
	var o = [], left = 4;
	while (left > 0) {
		var ok = [], ws = [];
		for (var i = 0; i < CELLNAMES.length; i++) {
			var n = CELLNAMES[i];
			if (CELLS[n][0] > left || (o.length && o[o.length - 1] === 'rest' && n === 'rest')) continue;
			ok.push(n); ws.push(cellWeight(n));
		}
		var nm = pick(ok, ws) || 'rest';
		o.push(nm); left -= CELLS[nm][0];
	}
	return o;
}
function vary(a) {
	a = a.slice();
	var i = Math.floor(rng() * a.length), n = CELLS[a[i]][0], ok = [], ws = [];
	for (var j = 0; j < CELLNAMES.length; j++) {
		var x = CELLNAMES[j];
		if (CELLS[x][0] === n && x !== a[i] && cellWeight(x) > 0) { ok.push(x); ws.push(cellWeight(x)); }
	}
	var nm = pick(ok, ws);
	if (nm) a[i] = nm;
	return a;
}
function ending(A, B) {
	var roomForRelease = R.have && (D.hi - cueSec()) >= 60 / S.tempo;
	if (rng() < S.ctl.release && S.on.release && roomForRelease) {
		var C = vary(A), n = 0, keep = [];
		for (var i = 0; i < C.length; i++) if (n + CELLS[C[i]][0] <= 2) { keep.push(C[i]); n += CELLS[C[i]][0]; }
		while (n < 2) { keep.push(S.on.baby ? 'baby16' : 'rest'); n += 1; }
		return keep.concat(['release']);
	}
	return vary(B);
}
// one Instinct phrase of nb bars (1, 2 or 4) from beat b
function instPhrase(b, nb) {
	if (S.ctl.walk > 0 && R.cues.length > 1 && rng() < S.ctl.walk) {
		var ci = Math.floor(rng() * R.cues.length);
		S.calls.push({b: b, len: 0, run: function () { setCue(ci); }});
	}
	var A = instBar(), A2 = rng() < S.ctl.motif ? A : vary(A), B = instBar(), C = ending(A, B);
	var bars = nb >= 4 ? [A, A2, B, C] : nb === 2 ? [A, C] : [C];
	if (HOST.trace) HOST.trace('phrase', {b: b, bars: bars});
	for (var k = 0; k < bars.length; k++) {
		var bb = b + 4 * k;
		for (var i = 0; i < bars[k].length; i++) {
			var nm = bars[k][i];
			S.calls.push({b: bb, len: CELLS[nm][0], cell: nm});
			bb += CELLS[nm][0];
		}
	}
}
function generate() {
	var c = S.ctl, b = S.genNext;
	if (c.mode === 2) {
		var on = c.phon, off = c.phoff;
		for (var k = 0; k < on; k += 4) instPhrase(b + 4 * k, Math.min(4, on - k));
		S.genNext = b + 4 * (on + off);
	} else if (c.mode === 1) {
		var skip = 0;
		for (var i = 0; i < 16; i++) {
			var cue = S.routineCue[i];
			if (cue > 0) (function (ci) { S.calls.push({b: b + i, len: 0, run: function () { setCue(ci); }}); })(cue - 1);
			if (skip) { skip = 0; continue; }
			var nm = S.routine[i] || 'rest';
			if (!cellOn(nm)) nm = 'rest';
			if (CELLS[nm][0] === 2) { if (i === 15) nm = 'rest'; else skip = 1; }
			S.calls.push({b: b + i, len: CELLS[nm][0], cell: nm});
		}
		S.genNext = b + 16;
	}
}

// ------------------------------------------------------------------ planning
function keyWord() {
	if (S.fresh) return S.fresh;
	return S.held.length ? S.held[S.held.length - 1] : null;
}
function readStatus() {
	var st = HOST.status();
	if (!st) return null;
	S.B = st[0];
	if (st[8] > 1000 && st[8] !== S.sr) { S.sr = st[8]; sendFilters(); if (R.have && !R.cutting) startCut(); resetPlan(true); }
	if (st[9] !== S.jumps) { var first = S.jumps < 0; S.jumps = st[9]; if (!first) resetPlan(true); }
	return st;
}
function nowB() { var st = readStatus(); return st ? st[0] : S.B; }

// start the timeline again from where gen~ is now (after a jump, a new record, a tempo change)
function resetPlan(flush, p) {
	var st = HOST.status(), b = st ? st[0] : S.B;
	if (flush) { S.gen++; HOST.gen('fgen', S.gen); }
	S.Ban = b;
	D = newDeck(0, p !== undefined ? p : st ? st[4] : cueSec(), 0);
	out.seg = []; out.fad = [];
	S.gridEnd = -1e9; S.calls = []; S.genNext = null; S.rel = null;
}

function execCell(c) {
	if (c.run) { c.run(); return; }
	if (D.t < b2t(nowB()) + 0.005 * S.sr) hold(b2t(nowB()) + 0.005 * S.sr);
	D.cue = cueSec();
	S.velT = 1; S.velA = 1; S.swingHold = null;
	if (HOST.trace) HOST.trace('cell', {cell: c.cell, b: c.b});
	CELLS[c.cell][4](c.b);
	S.gridEnd = Math.max(S.gridEnd, c.b + c.len);
}

function planKeyUnit(k, b) {
	var w = k.w;
	if (D.t < b2t(nowB()) + 0.005 * S.sr) hold(b2t(nowB()) + 0.005 * S.sr);
	D.cue = cueSec();
	S.velT = 0.8 + 0.4 * k.vel / 127; S.velA = 0.7 + 0.3 * k.vel / 127;
	if (w === 'release') { startRelease(b); return; }
	var u = speedFor(w);
	S.swingHold = Math.abs(u - 1 / 6) < 1e-9 ? 0.5 : null;
	if (w === 'echo') S.echoN = (S.lastKeyWord === 'echo') ? S.echoN + 1 : 0;
	if (HOST.trace) HOST.trace('unit', {w: w, b: b});
	var len = UNIT[w][1](b, u, S.echoN);
	S.unitLen = len;
	S.swingHold = null; S.velT = 1; S.velA = 1;
	S.gridEnd = b + len;
	S.lastKeyWord = w;
}

// Release: let go and keep playing until the key comes up (or the record ends)
function startRelease(b) {
	var t0 = start(b), hi = D.hi, pS = D.p, vS = D.v;
	var tEnd = D.t + Math.max(Math.floor((hi - D.p) * S.sr), 1);
	play(tEnd);
	var o = open(t0 - MS2 * S.sr, D.t);
	S.rel = {open: o, tEnd: D.t, b: b, t0: t0, pStart: pS, vStart: vS};
	if (HOST.trace) HOST.trace('unit', {w: 'release', b: b});
	S.gridEnd = t2b(D.t);
	S.lastKeyWord = 'release';
	S.velT = 1; S.velA = 1;
}
function cutRelease(bq) {
	var r = S.rel; S.rel = null;
	if (!r) return;
	var tq = Math.max(Math.floor(b2t(sw(bq))), r.t0 + Math.floor(0.03 * S.sr));
	if (tq < r.tEnd) {
		// where the record is at tq, from the play segment's closed form
		var m = tq - r.t0, rr = Math.exp(-1 / (H.tau_rel * S.sr));
		D.t = tq; D.p = r.pStart + (m + decaySum(r.vStart - 1, rr, m)) / S.sr;
		D.v = 1 + (r.vStart - 1) * Math.pow(rr, m);
		if (r.open) r.open.b = Math.floor(tq + (normal(H.fad_jit) + H.leak * rng()) * S.sr);
	}
	silentBack(t2b(D.t) + 0.25);
	S.gridEnd = qUp(t2b(D.t), 0.25);
}

function plan() {
	if (!R.have || R.cutting || !H || !D) return;
	var B = nowB(), nowT = b2t(B), SR = S.sr;
	var LA = Math.max(0.5 * spbS(), 0.15 * SR);
	var k = keyWord();
	// a held Release ends at the record's end even if the key is still down
	if (S.rel && S.rel.tEnd < nowT + LA) cutRelease(t2b(S.rel.tEnd));
	if (k && !S.rel) {
		var unitBeats = S.lastKeyWord === k.w ? S.unitLen : 2 * speedFor(k.w);
		var bNext = Math.max(S.gridEnd, B + 0.012 * S.tempo / 60);
		if (bNext > S.gridEnd + 1e-6) bNext = qUp(bNext, S.ctl.quant);
		var la = Math.max(0.5 * unitBeats * spbS(), 0.1 * SR);
		var done = !S.fresh && S.lastKeyWord === k.w && (k.w === 'release' || (k.w === 'echo' && S.echoN >= 5));
		if (b2t(bNext) - nowT < la && !done) {
			S.calls = []; S.genNext = null;
			planKeyUnit(k, bNext);
			if (S.fresh === k) S.fresh = null;
		}
	} else if (!k && !S.rel && S.ctl.mode > 0) {
		if (S.genNext === null) S.genNext = Math.ceil((Math.max(S.gridEnd, B) + 0.05) / 4) * 4;
		while (!S.calls.length || S.calls[S.calls.length - 1].b < B + 4) {
			var before = S.calls.length; generate();
			if (S.calls.length === before && S.genNext > B + 64) break;
		}
		while (S.calls.length && b2t(S.calls[0].b) < nowT + LA) {
			var c = S.calls.shift();
			if (c.b < B - 0.1) continue;
			execCell(c);
		}
	}
	if (!k && S.ctl.mode === 0 && S.lastKeyWord !== null && S.gridEnd < B) S.lastKeyWord = null;
	// fader events that can no longer change
	// (a word planned later starts after D.t, or after a key press at now + 12 ms; an edge it
	// adds before one already sent is applied late, by at most the margin, so keep it small)
	var frontier = Math.max(D.t - 0.03 * SR, nowT + 0.06 * SR);
	if (S.rel) frontier = Math.max(nowT - 0.018 * SR, S.rel.open ? S.rel.open.a : 0);
	emitFader(frontier);
	flushOut();
}

function flushOut() {
	var st = HOST.status(), qh = st ? st[2] : qtail, fh = st ? st[3] : ftail;
	var wrote = false;
	while (out.seg.length && qtail - qh < QN - 2) { HOST.writeSeg(qtail % QN, out.seg.shift()); qtail++; wrote = true; }
	if (wrote) HOST.gen('qtail', qtail);
	wrote = false;
	while (out.fad.length && ftail - fh < FN - 2) { HOST.writeFad(ftail % FN, out.fad.shift()); ftail++; wrote = true; }
	if (wrote) HOST.gen('ftail', ftail);
}

// ------------------------------------------------------------------ keys and buttons
function wordPress(w, vel) {
	if (WORDS.indexOf(w) < 0) return;
	var i;
	if (vel > 0) {
		for (i = S.held.length - 1; i >= 0; i--) if (S.held[i].w === w) S.held.splice(i, 1);
		var k = {w: w, vel: vel};
		S.held.push(k); S.fresh = k;
		if (S.rel && w !== 'release') cutRelease(qUp(nowB() + 0.012 * S.tempo / 60, S.ctl.quant || 0.0001));
		if (S.ctl.mode > 0) { S.calls = []; S.genNext = null; }
	} else {
		for (i = S.held.length - 1; i >= 0; i--) if (S.held[i].w === w) S.held.splice(i, 1);
		if (w === 'release' && S.rel) {
			var B = nowB(), bq = qUp(Math.max(B + 0.012 * S.tempo / 60, S.rel.b + 0.25), S.ctl.quant || 0.0001);
			cutRelease(bq);
		}
		if (!S.held.length && S.ctl.mode > 0) S.genNext = null;
	}
	plan();
}
function noteIn(pitch, vel) {
	if (pitch >= KEY0 && pitch < KEY0 + 16) { wordPress(WORDS[pitch - KEY0], vel); return; }
	if (pitch >= CUEKEY0 && pitch < CUEKEY0 + 8 && vel > 0) setCue(pitch - CUEKEY0, true);
}
function setCue(i, fromKey) {
	if (!R.cues.length) return;
	S.cueIdx = Math.max(0, Math.min(i, R.cues.length - 1));
	HOST.ui('cue', 'set', S.cueIdx + 1);
	HOST.ui('strip', 'sel', S.cueIdx);
}

// ------------------------------------------------------------------ the cartridge stage
function section(fz, fp, fs) {
	var a = Math.tan(Math.PI * fz / fs), b = Math.tan(Math.PI * fp / fs);
	var n0 = 1 + 1 / a, n1 = 1 - 1 / a, d0 = 1 + 1 / b, d1 = 1 - 1 / b;
	return [n0 / d0, n1 / d0, d1 / d0];      // b0 b1 a1, a0 = 1
}
function phonoSections(fs) {
	var T1 = 3180e-6, T2 = 318e-6, T3 = 75e-6, F4 = 20000.0;
	var F1 = 1 / (2 * Math.PI * T1), F2 = 1 / (2 * Math.PI * T2), F3 = 1 / (2 * Math.PI * T3);
	return [section(F2, F1, fs), section(F4, F3, fs)];
}
function gainAt1k(secs, fs) {
	var w = 2 * Math.PI * 1000 / fs, re = Math.cos(w), im = -Math.sin(w), g = 1;
	for (var i = 0; i < secs.length; i++) {
		var s = secs[i];
		var nr = s[0] + s[1] * re, ni = s[1] * im, dr = 1 + s[2] * re, di = s[2] * im;
		g *= Math.sqrt((nr * nr + ni * ni) / (dr * dr + di * di));
	}
	return g;
}
function sendFilters() {
	var fs = S.sr * CUTOS, ps = phonoSections(fs);
	HOST.gen('pb0a', ps[0][0]); HOST.gen('pb1a', ps[0][1]); HOST.gen('pa1a', ps[0][2]);
	HOST.gen('pb0b', ps[1][0]); HOST.gen('pb1b', ps[1][1]); HOST.gen('pa1b', ps[1][2]);
	HOST.gen('g1k', 1 / gainAt1k(ps, fs));
	var best = 44100, bd = 1e9;
	for (var r in LOWPASS) { var d = Math.abs(r - S.sr); if (d < bd) { bd = d; best = r; } }
	var L = LOWPASS[best], names = ['a', 'b', 'c'];
	for (var i = 0; i < 3; i++) {
		HOST.gen('lb0' + names[i], L[i * 6]); HOST.gen('lb1' + names[i], L[i * 6 + 1]); HOST.gen('lb2' + names[i], L[i * 6 + 2]);
		HOST.gen('la1' + names[i], L[i * 6 + 4]); HOST.gen('la2' + names[i], L[i * 6 + 5]);
	}
}

// upsample 4 times and apply the recording curve (inverse of the phono stage), in slices
var KER = null, KN = 64;
function besselI0(x) { var s = 1, t = 1; for (var k = 1; k < 30; k++) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; }
function kernel() {
	if (KER) return KER;
	KER = new Float64Array(2 * KN + 1);
	var beta = 9.0, i0b = besselI0(beta);
	for (var i = 0; i <= 2 * KN; i++) {
		var k = i - KN, x = k / CUTOS * 0.92, sinc = x === 0 ? 1 : Math.sin(Math.PI * x) / (Math.PI * x);
		var r = k / KN, win = besselI0(beta * Math.sqrt(Math.max(0, 1 - r * r))) / i0b;
		KER[i] = sinc * 0.92 * win;
	}
	return KER;
}
var CUT = null, poscmd = 0;
function startCut() {
	var fs = R.sr * CUTOS, ps = phonoSections(fs), g = gainAt1k(ps, fs);
	CUT = {pos: 0, ch: [R.L, R.R], st: [[0, 0, 0, 0], [0, 0, 0, 0]], ps: ps, g: g, n: R.n};
	R.cutting = 1;
	HOST.gen('haverec', 0);
	HOST.cutBegin(R.n * CUTOS);
	HOST.ui('status', 'set', 'CUTTING');
}
// returns true when finished
function cutSlice(maxIn) {
	if (!CUT) return true;
	var h = kernel(), n = CUT.n, a = CUT.pos, b = Math.min(n, a + maxIn), ps = CUT.ps;
	// inverse sections: y = (x + a1 * xprev - b1 * yprev) / b0  (numerator and denominator swapped)
	var i0a = 1 / ps[0][0], i0b = 1 / ps[1][0];
	for (var c = 0; c < 2; c++) {
		var x = CUT.ch[c], st = CUT.st[c], y = new Float32Array((b - a) * CUTOS);
		var xa = st[0], ya = st[1], xb = st[2], yb = st[3];
		for (var n0 = a; n0 < b; n0++) {
			for (var r = 0; r < CUTOS; r++) {
				var s = 0;
				for (var d = -16; d <= 16; d++) {
					var hi = 4 * d + r + KN;
					if (hi < 0 || hi > 2 * KN) continue;
					var xi = n0 - d;
					if (xi >= 0 && xi < n) s += x[xi] * h[hi];
				}
				var ta = (s + ps[0][2] * xa - ps[0][1] * ya) * i0a; xa = s; ya = ta;
				var tb = (ta + ps[1][2] * xb - ps[1][1] * yb) * i0b; xb = ta; yb = tb;
				y[(n0 - a) * CUTOS + r] = tb * CUT.g;
			}
		}
		st[0] = xa; st[1] = ya; st[2] = xb; st[3] = yb;
		HOST.cutWrite(c, a * CUTOS, y);
	}
	CUT.pos = b;
	if (b >= n) {
		CUT = null; R.cutting = 0;
		HOST.gen('recsr4', R.sr * CUTOS); HOST.gen('rlen', n * CUTOS);
		HOST.gen('posset', cueSec()); HOST.gen('poscmd', ++poscmd);
		resetPlan(true, cueSec());
		HOST.gen('haverec', 1);
		showRecord();
		return true;
	}
	return false;
}

// ------------------------------------------------------------------ the record
function findCues(mono, sr) {
	var hop = Math.round(0.010 * sr), nf = Math.floor(mono.length / hop), db = new Float64Array(nf), f, i;
	for (f = 0; f < nf; f++) {
		var s = 0;
		for (i = f * hop; i < (f + 1) * hop; i++) s += mono[i] * mono[i];
		db[f] = 10 * Math.log(s / hop + 1e-12) / Math.LN10;
	}
	var first = 0;
	for (i = 0; i < mono.length; i++) if (Math.abs(mono[i]) > 0.01) { first = i; break; }
	var c0 = Math.max(0, first / sr - 0.005), back = 5, rises = [];
	for (f = back; f < nf; f++) {
		var r = db[f] - db[f - back];
		if (r >= 9 && db[f] > -45 && (f === back || db[f - 1] - db[f - 1 - back] < 9)) rises.push([f, r]);
	}
	rises.sort(function (a, b) { return b[1] - a[1]; });
	var cues = [c0], mind = 0.12;
	for (i = 0; i < rises.length && cues.length < 8; i++) {
		var t = Math.max(0, (rises[i][0] - back + 1) * hop / sr);
		// slide to where the rise starts: the first frame after the quiet one
		for (var g = rises[i][0] - back + 1; g <= rises[i][0]; g++) if (db[g] - db[g - 1] > 3) { t = g * hop / sr; break; }
		t = Math.max(0, t - 0.005);
		var ok = true;
		for (var j = 0; j < cues.length; j++) if (Math.abs(cues[j] - t) < mind) ok = false;
		if (ok) cues.push(t);
	}
	cues.sort(function (a, b) { return a - b; });
	return cues;
}
function overview(mono, n) {
	var o = [], per = mono.length / n;
	for (var k = 0; k < n; k++) {
		var m = 0;
		for (var i = Math.floor(k * per); i < Math.floor((k + 1) * per); i += 4) { var v = Math.abs(mono[i]); if (v > m) m = v; }
		o.push(Math.round(m * 1000) / 1000);
	}
	return o;
}
function useRecord(L, Rch, sr, cues) {
	var n = Math.min(L.length, Math.floor(MAXREC * sr), HOST.cutCapacity ? Math.floor(HOST.cutCapacity() / CUTOS) : 1e12);
	R.L = L.subarray(0, n); R.R = Rch.subarray(0, n); R.sr = sr; R.n = n; R.have = 1;
	var mono = new Float32Array(n), pk = 0, i;
	for (i = 0; i < n; i++) { mono[i] = 0.5 * (R.L[i] + R.R[i]); pk = Math.max(pk, Math.abs(mono[i])); }
	if (pk > 0) for (i = 0; i < n; i++) mono[i] /= pk;
	R.cues = cues && cues.length ? cues.slice() : findCues(mono, sr);
	R.wave = overview(mono, 300);
	S.cueIdx = 0;
	R.trimmed = L.length > n;
	sendStrip();
	setCue(0);
	startCut();
}
function decodeBuiltin() {
	var tbl = {}, chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/', i;
	for (i = 0; i < 64; i++) tbl[chars.charAt(i)] = i;
	var s = BUILTIN_B64, bytes = [], buf = 0, bits = 0;
	for (i = 0; i < s.length; i++) {
		var c = s.charAt(i);
		if (c === '=' || tbl[c] === undefined) continue;
		buf = (buf << 6) | tbl[c]; bits += 6;
		if (bits >= 8) { bits -= 8; bytes.push((buf >> bits) & 255); }
	}
	var n = bytes.length >> 1, x = new Float32Array(n);
	for (i = 0; i < n; i++) { var v = bytes[2 * i] | (bytes[2 * i + 1] << 8); if (v > 32767) v -= 65536; x[i] = v / 32768; }
	return x;
}
function useBuiltin() {
	var x = decodeBuiltin();
	R.file = ''; R.builtin = 1;
	if (!x.length) { R.have = 0; HOST.ui('status', 'set', 'DROP A SOUND'); return; }
	useRecord(x, x, 44100, null);
}
function showRecord() {
	if (!R.have) { HOST.ui('status', 'set', 'DROP A SOUND'); return; }
	var name = R.builtin ? 'BUILT-IN' : R.file.substring(R.file.lastIndexOf('/') + 1).toUpperCase();
	if (name.length > 22) name = name.substring(0, 21) + '~';
	HOST.ui('status', 'set', name + '  ' + (R.n / R.sr).toFixed(1) + ' S' + (R.trimmed ? ' (CUT TO 20)' : '') + '  ' + R.cues.length + ' CUES');
}
function sendStrip() {
	if (!R.have) { HOST.ui('strip', 'clear'); return; }
	HOST.ui.apply(null, ['strip', 'wave'].concat(R.wave));
	var c = [], len = R.n / R.sr;
	for (var i = 0; i < R.cues.length; i++) c.push(Math.round(R.cues[i] / len * 100000) / 100000);
	HOST.ui.apply(null, ['strip', 'cues'].concat(c.length ? c : [-1]));
	HOST.ui('strip', 'len', len);
	HOST.ui('strip', 'sel', S.cueIdx);
}
// edits on the strip: x is 0..1 along the record
function stripEdit(what, i, x) {
	if (!R.have) return;
	var len = R.n / R.sr, t = Math.max(0, Math.min(1, x)) * len;
	if (what === 'pick') setCue(i);
	else if (what === 'move' && i >= 0 && i < R.cues.length) { R.cues[i] = t; }
	else if (what === 'moved') { sortCues(i); }
	else if (what === 'add' && R.cues.length < 8) { R.cues.push(t); sortCues(R.cues.length - 1); }
	else if (what === 'remove' && R.cues.length > 1 && i >= 0 && i < R.cues.length) {
		R.cues.splice(i, 1); if (S.cueIdx >= R.cues.length) S.cueIdx = R.cues.length - 1; setCue(S.cueIdx);
	}
	sendStrip();
	HOST.dirty();
}
function sortCues(i) {
	var v = R.cues[i];
	R.cues.sort(function (a, b) { return a - b; });
	var j = R.cues.indexOf(v);
	if (i === S.cueIdx) setCue(j);
}

// ------------------------------------------------------------------ panel messages
function control(m, v) {
	var c = S.ctl;
	switch (m) {
	case 'speed': c.speed = SPEEDS[v] !== undefined ? SPEEDS[v] : 0.25; return true;
	case 'quant': c.quant = QUANTS[v] !== undefined ? QUANTS[v] : 0.25; return true;
	case 'mode': c.mode = v; S.calls = []; S.genNext = null; if (v === 0) S.calls = []; return true;
	case 'phon': c.phon = PHRASEON[v] || 4; S.calls = []; S.genNext = null; return true;
	case 'phoff': c.phoff = PHRASEOFF[v] !== undefined ? PHRASEOFF[v] : 0; return true;
	case 'density': case 'skill': case 'motif': case 'release': case 'funk': case 'walk':
		c[m] = v / 100; return true;
	case 'timing': case 'weight': case 'slop': case 'risk':
		c[m] = v / 100; sendHands(); return true;
	case 'level': HOST.gen('level', Math.pow(10, v / 20)); return true;
	case 'cue': setCue(v - 1); return true;
	}
	if (m.indexOf('btn_') === 0) { wordPress(m.substr(4), v ? 100 : 0); return true; }
	if (m.indexOf('on_') === 0) { S.on[m.substr(3)] = v ? 1 : 0; return true; }
	if (m.indexOf('slot') === 0) {
		var n = parseInt(m.substr(4), 10);
		if (n >= 1 && n <= 16) S.routine[n - 1] = CELLNAMES[v] || 'rest';
		return true;
	}
	if (m.indexOf('scue') === 0) {
		var k = parseInt(m.substr(4), 10);
		if (k >= 1 && k <= 16) S.routineCue[k - 1] = v;
		return true;
	}
	return false;
}

// ------------------------------------------------------------------ Max host
var IN_MAX = (typeof outlet === 'function') && (typeof post === 'function') && (typeof Task !== 'undefined');
var NAMES = {rec: 'skr_rec', cut: 'skr_cut', q: 'skr_q', f: 'skr_f', st: 'skr_stat'};
var bRec = null, bCut = null, bQ = null, bF = null, bSt = null;
var tick = null, cutTask = null, ready = false, restoreState = null, handsGuard = true, initAt = 0;
var apiPlay = null, apiTempo = null, apiTime = null, sbseq = 0;

if (IN_MAX) {
	HOST.gen = function (n, v) { outlet(0, n, v); };
	HOST.ui = function () { outlet(1, arrayfromargs(arguments)); };
	HOST.status = function () { return bSt ? bSt.peek(1, 0, 12) : null; };
	HOST.writeSeg = function (slot, vals) { if (bQ) bQ.poke(1, slot * QF, vals); };
	HOST.writeFad = function (slot, vals) { if (bF) bF.poke(1, slot * FF, vals); };
	HOST.log = function (s) { post('DJ Claude Skratch: ' + s + '\n'); };
	HOST.cutCapacity = function () { return bCut ? bCut.framecount() : 0; };
	HOST.cutBegin = function (n) {};
	HOST.cutWrite = function (c, off, y) {
		var CH = 16384;
		for (var a = 0; a < y.length; a += CH) {
			var part = Array.prototype.slice.call(y.subarray(a, Math.min(y.length, a + CH)));
			bCut.poke(c + 1, off + a, part);
		}
	};
	HOST.dirty = function () { notifyclients(); };
	tick = new Task(onTick, this);
	tick.interval = 20;
	cutTask = new Task(function () {
		if (cutSlice(Math.floor(R.sr * 0.5))) { cutTask.cancel(); }
	}, this);
	cutTask.interval = 5;
}
HOST.dirty = HOST.dirty || function () {};

function names(rec, cut, q, f, st) {
	NAMES.rec = String(rec); NAMES.cut = String(cut); NAMES.q = String(q); NAMES.f = String(f); NAMES.st = String(st);
}
function init() {
	bRec = new Buffer(NAMES.rec); bCut = new Buffer(NAMES.cut); bQ = new Buffer(NAMES.q);
	bF = new Buffer(NAMES.f); bSt = new Buffer(NAMES.st);
	var st = HOST.status();
	if (st && st[8] > 1000) S.sr = st[8];
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
	sendHands(); sendFilters();
	HOST.gen('fgen', S.gen);
	ready = true; initAt = Date.now();
	resetPlan(false);
	if (restoreState) { restore(restoreState); restoreState = null; } else useBuiltin();
	tick.repeat();
}
function setPlaying(v) { v = v ? 1 : 0; if (v !== S.playing) { S.playing = v; HOST.gen('playing', v); } }
function setTempo(v) {
	if (Math.abs(v - S.tempo) < 1e-6) return;
	var b = nowB();
	S.tempo = v; HOST.gen('tempo', v);
	if (D) resetPlan(true);
}
function setSongTime(v) { sbseq++; HOST.gen('songbeat', v); HOST.gen('sbseq', sbseq); }

function onTick() {
	if (handsGuard && Date.now() - initAt > 1500) handsGuard = false;
	if (R.cutting && !cutTask.running) cutTask.repeat();
	plan();
}

function loadFile(path) {
	bRec.send('replace', path);
	var fr = bRec.framecount(), nch = bRec.channelcount();
	if (!fr) { HOST.ui('status', 'set', 'FILE MISSING'); return false; }
	var sr = fr / (bRec.length() / 1000), n = Math.min(fr, Math.floor(MAXREC * sr) + 1);
	var L = new Float32Array(n), Rc = new Float32Array(n), CH = 65536;
	for (var a = 0; a < n; a += CH) {
		var m = Math.min(CH, n - a), x = bRec.peek(1, a, m), y = nch > 1 ? bRec.peek(2, a, m) : x;
		if (m === 1) { x = [x]; y = [y]; }
		for (var i = 0; i < m; i++) { L[a + i] = x[i]; Rc[a + i] = y[i]; }
	}
	R.file = path; R.builtin = 0;
	useRecord(L, Rc, sr, null);
	return true;
}

// the record and its cue points are kept with the Live set (the v8 object is a stored parameter)
function getvalueof() {
	if (!R.have || R.builtin) return JSON.stringify({file: '', cues: R.have ? R.cues : [], cue: S.cueIdx});
	return JSON.stringify({file: R.file, cues: R.cues, cue: S.cueIdx});
}
function setvalueof(v) {
	if (!v || v === 0) return;
	try { var o = JSON.parse(v); } catch (e) { return; }
	if (ready) restore(o); else restoreState = o;
}
function restore(o) {
	if (o.file) {
		if (!loadFile(o.file)) { useBuiltin(); HOST.ui('status', 'set', 'FILE MISSING: BUILT-IN'); return; }
	} else useBuiltin();
	if (o.cues && o.cues.length) { R.cues = o.cues.slice(); sendStrip(); }
	setCue(o.cue || 0);
}

function anything() {
	var a = arrayfromargs(arguments), v = a.length ? a[0] : 0, m = messagename;
	switch (m) {
	case 'init': init(); return;
	case 'note': if (ready) noteIn(a[0], a[1]); return;
	case 'drop': if (ready) { if (loadFile(a.join(' '))) notifyclients(); } return;
	case 'clear': if (ready && v) { useBuiltin(); notifyclients(); } return;
	case 'strip': if (ready) stripEdit(a[0], a[1], a[2]); return;
	case 'hands':
		if (handsGuard) return;
		HOST.ui('timing', v); HOST.ui('weight', v); HOST.ui('slop', v); HOST.ui('risk', v);
		return;
	}
	if (control(m, v)) { if (ready) plan(); return; }
	HOST.log('unknown message ' + m);
}
function bang() {}
function list() {}

if (!IN_MAX && typeof module !== 'undefined' && module) {
	module.exports = {
		S: S, R: R, HOST: HOST, W: W, CELLS: CELLS, CELLNAMES: CELLNAMES, UNIT: UNIT,
		plan: plan, control: control, wordPress: wordPress, noteIn: noteIn, setCue: setCue,
		useRecord: useRecord, cutSlice: cutSlice, sendHands: sendHands, sendFilters: sendFilters,
		resetPlan: resetPlan, findCues: findCues, emitFader: emitFader, flushOut: flushOut,
		deck: function () { return D; }, hands: function () { return H; }, at: at, start: start,
		setRng: function (f) { rng = f; }, b2t: b2t, t2b: t2b, decodeBuiltin: decodeBuiltin,
		setBuiltin: function (s) { BUILTIN_B64 = s; }, useBuiltin: useBuiltin, out: out,
		stripEdit: stripEdit, instPhrase: instPhrase, generate: generate
	};
}
