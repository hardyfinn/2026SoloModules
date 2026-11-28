var KIND = "mutator";
// transform_core.js: the brain and the display of the three Hær MIDI transform devices
// (Mutator, Euclid Morph, Markov Remix). It runs in a [jsui] inside each device.
// The build puts one line in front of this file:  var KIND = "mutator" | "euclid" | "markov";
//
// How the device is split up:
//   * The PATCH (high priority, tight timing) plays the pattern. Every 16th note it works out
//     which step of the loop it is on, looks that step up in a [coll] and plays what it finds,
//     each note delayed by its own small offset so the feel survives.
//   * This SCRIPT (low priority) only decides WHAT is in the [coll]: it learns the notes you
//     play in, changes the pattern, writes it into the [coll] a 16th before it is needed,
//     and draws the display.
//
// Messages in (from the patch):
//   tick <g>             a 16th-note step went past (g counts 16ths from the start of the song)
//   note <pitch> <vel> <pos>   a note came in at song position pos (in 16ths, with fractions)
//   running <0/1>        Live's transport started or stopped
//   restore <list...>    the saved learned pattern (from the [pattr] that lives in the Live set)
//   and one message per control (learn, thru, length, every, morph ...)
// Messages out (one outlet, sorted by a [route] in the patch):
//   sw <step>            the step where the device takes over (before it: notes pass through)
//   len <steps>          loop length in 16ths
//   coll clear / coll store <k> <p v d o ...>   the pattern, step by step
//   panic                stop every note the device is playing
//   save <list...>       the learned pattern, to be kept with the Live set

inlets = 1;
outlets = 1;
mgraphics.init();
mgraphics.relative_coords = 0;
mgraphics.autofill = 0;

var NONE = 1000000000;        // "never": the device never takes over
var mode = "thru";            // thru | armed | learning | play
var learnBars = 1;            // LENGTH control (used at the next LEARN)
var L = 16;                   // loop length in 16ths of the pattern being played
var s0 = 0;                   // song step where the learned loop started
var S = NONE;                 // song step where the device takes over
var cur16 = -1;               // last step seen
var rec = [];                 // notes heard while not playing: {p, v, on, off}
var held = {};                // pitch -> note still held while recording
var orig = [];                // the learned pattern: [{p, v, pos, d}]  (pos in 16ths, 0..L)
var cur = [];                 // the pattern being played now
var lastMsg = "";             // what the display says happened last
var lastMsgStep = -1;
var playFrom = -NONE;          // song step where playing the learned pattern began

// ---- small helpers -------------------------------------------------------------------
function clone(evs) {
	var o = [];
	for (var i = 0; i < evs.length; i++) o.push({ p: evs[i].p, v: evs[i].v, pos: evs[i].pos, d: evs[i].d });
	return o;
}
function wrap(x, n) { x = x % n; return x < 0 ? x + n : x; }
function rnd(n) { return Math.floor(Math.random() * n); }
function step(e) { return wrap(Math.floor(e.pos + 0.5), L); }      // nearest 16th
var NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
function noteName(p) { return NOTE_NAMES[p % 12] + (Math.floor(p / 12) - 2); }  // Live: 60 = C3
function say(s) { lastMsg = s; lastMsgStep = cur16; }
function sendSw(v) { S = v; outlet(0, "sw", v); }

// ---- writing the pattern into the [coll] -------------------------------------------------
// Each step holds "pitch velocity duration offset" for every note that starts in it.
// duration is in 16ths, offset is how far after the step the note starts (0..1 of a 16th).
// The keys are song steps modulo L, so the loop stays where you played it in the bar.
function writeColl(evs) {
	var byKey = {};
	var shift = wrap(s0, L);
	for (var i = 0; i < evs.length; i++) {
		var e = evs[i];
		if (e.v < 1) continue;
		var a = wrap(e.pos + shift, L);
		var k = Math.floor(a + 1e-6);
		var o = a - k;
		if (o < 0) o = 0;
		k = wrap(k, L);
		if (!byKey[k]) byKey[k] = [];
		byKey[k].push(e.p, Math.max(1, Math.min(127, Math.round(e.v))), Math.max(0.05, e.d), Math.round(o * 1000) / 1000);
	}
	outlet(0, "coll", "clear");
	for (var key in byKey) outlet(0, ["coll", "store", Number(key)].concat(byKey[key]));
}

// ---- saving with the Live set --------------------------------------------------------------
// save L s0 n  p v pos d  p v pos d ...
function saveState() {
	var a = ["save", L, wrap(s0, L), orig.length];
	for (var i = 0; i < orig.length; i++) a.push(orig[i].p, orig[i].v, orig[i].pos, orig[i].d);
	outlet(0, a);
}

function restore() {
	var a = arrayfromargs(arguments);
	if (a.length < 3 || a[2] < 1) return;
	L = a[0]; s0 = a[1];
	orig = [];
	for (var i = 0; i < a[2]; i++) {
		var j = 3 + i * 4;
		if (j + 3 >= a.length) break;
		orig.push({ p: a[j], v: a[j + 1], pos: a[j + 2], d: a[j + 3] });
	}
	cur = clone(orig);
	playFrom = -NONE;
	onLearned(true);
	mode = "play";
	outlet(0, "len", L);
	sendSw(-NONE);
	refresh();
	say("pattern loaded from the set");
	mgraphics.redraw();
}

// sent by the patch when the device has loaded: tell the player where things stand
function init() {
	outlet(0, "len", L);
	outlet(0, "sw", S);
	mgraphics.redraw();
}

// ---- input ------------------------------------------------------------------------------
function note(p, v, pos) {
	if (mode == "play") return;            // the device is playing: input is ignored
	if (v > 0) {
		if (held[p]) held[p].off = pos;
		var n = { p: p, v: v, on: pos, off: -1 };
		held[p] = n;
		rec.push(n);
		if (rec.length > 4000) rec.splice(0, 1000);
	} else if (held[p]) {
		held[p].off = pos;
		delete held[p];
	}
}

function running(r) {
	if (!r) {
		outlet(0, "panic");
		if (mode == "play") sendSw(-NONE);     // from now on, play from anywhere in the song
		if (mode == "learning") {              // learning starts again at the next bar line
			mode = "armed"; rec = []; sendSw(NONE);
			say("stopped: learning starts again at the next bar line");
		}
		held = {};
	}
	mgraphics.redraw();
}

function tick(g) {
	cur16 = g;
	if (mode == "armed" && wrap(g, 16) == 0) {
		// learning starts on this bar line
		L = learnBars * 16;
		s0 = g;
		outlet(0, "len", L);
		sendSw(g + L);
		mode = "learning";
		say("learning " + learnBars + (learnBars > 1 ? " bars" : " bar"));
	}
	if (mode == "learning" && g >= S - 1) finishLearning();
	if (mode == "play") {
		if (S > -NONE && g >= S) sendSw(-NONE);
		if (wrap(g + 1, 16) == 0) barAhead(g + 1);   // a bar line is one 16th away
	}
	mgraphics.redraw();
}

// the notes heard between s0 and S become the learned pattern
function finishLearning() {
	var evs = [];
	for (var i = 0; i < rec.length; i++) {
		var n = rec[i];
		if (n.on < s0 - 0.5 || n.on >= S - 0.5) continue;
		var off = n.off >= 0 ? n.off : S;
		var d = Math.max(0.1, Math.min(off - n.on, L));
		evs.push({ p: n.p, v: n.v, pos: wrap(n.on - s0, L), d: d });
	}
	rec = [];
	evs.sort(function (a, b) { return a.pos - b.pos || a.p - b.p; });
	if (!evs.length) {
		mode = "thru";
		sendSw(NONE);
		say("heard nothing: press LEARN again");
		return;
	}
	orig = evs;
	cur = clone(orig);
	playFrom = S;
	onLearned(false);
	mode = "play";
	refresh();
	saveState();
	say("learned " + evs.length + " notes, playing");
}

// ---- controls shared by all three -------------------------------------------------------------
function learn() {
	outlet(0, "panic");
	mode = "armed";
	rec = []; held = {};
	sendSw(NONE);
	say("LEARN: starts at the next bar line");
	mgraphics.redraw();
}

function thru() {
	if (mode == "play" || mode == "learning" || mode == "armed") {
		mode = "thru";
		sendSw(NONE);
		outlet(0, "panic");
		say("THRU: you hear what comes in");
	} else if (orig.length) {
		mode = "play";
		outlet(0, "len", L);
		sendSw(-NONE);
		refresh();
		say("playing again");
	}
	mgraphics.redraw();
}

function learnlen(i) { learnBars = [1, 2, 4, 8][Math.max(0, Math.min(3, i | 0))]; }

// ================================================================================================
// MUTATOR: every N bars, one small change. REVERT goes back to what you played.
// ================================================================================================
var every = 1, everyIdx = 0, freeze = 0, allow = { rotate: 1, drop: 1, shift: 1, add: 1 };
var bench = [];       // notes DROP took out (ADD may bring them back)
var changes = 0;

function everyset(i) { everyIdx = i | 0; every = [1, 2, 4, 8, 16][Math.max(0, Math.min(4, everyIdx))]; }
function hold(v) { freeze = v ? 1 : 0; say(freeze ? "HOLD: no more changes" : "changing again"); mgraphics.redraw(); }
function rotate(v) { allow.rotate = v ? 1 : 0; }
function drop(v) { allow.drop = v ? 1 : 0; }
function shift(v) { allow.shift = v ? 1 : 0; }
function add(v) { allow.add = v ? 1 : 0; }

function lanesOf(evs) {
	var m = {};
	for (var i = 0; i < evs.length; i++) {
		if (!m[evs[i].p]) m[evs[i].p] = [];
		m[evs[i].p].push(i);
	}
	return m;
}
function occupied(evs, p, st, skip) {
	for (var i = 0; i < evs.length; i++) if (i != skip && evs[i].p == p && step(evs[i]) == st) return true;
	return false;
}

// one small change to "cur"; returns a sentence saying what it did, or "" if it could not
function mutateOnce(evs) {
	var kinds = [];
	if (allow.rotate) kinds.push("rotate");
	if (allow.drop && evs.length > 1) kinds.push("drop");
	if (allow.shift) kinds.push("shift");
	if (allow.add) kinds.push("add");
	if (!kinds.length || !evs.length) return "";
	for (var tries = 0; tries < 12; tries++) {
		var k = kinds[rnd(kinds.length)];
		var dir = Math.random() < 0.5 ? -1 : 1;
		if (k == "rotate") {
			var lanes = lanesOf(evs), keys = [];
			for (var p in lanes) keys.push(p);
			var p0 = Number(keys[rnd(keys.length)]);
			for (var i = 0; i < evs.length; i++) if (evs[i].p == p0) evs[i].pos = wrap(evs[i].pos + dir, L);
			return "rotated the " + noteName(p0) + " lane " + (dir > 0 ? "later" : "earlier");
		}
		if (k == "drop") {
			var j = rnd(evs.length);
			var gone = evs.splice(j, 1)[0];
			bench.push(gone);
			if (bench.length > 32) bench.shift();
			return "dropped " + noteName(gone.p) + " on step " + (step(gone) + 1);
		}
		if (k == "shift") {
			var j2 = rnd(evs.length), e = evs[j2];
			var to = wrap(step(e) + dir, L);
			if (occupied(evs, e.p, to, j2)) continue;
			e.pos = wrap(e.pos + dir, L);
			return "moved " + noteName(e.p) + " from step " + (wrap(step(e) - dir, L) + 1) + " to " + (step(e) + 1);
		}
		if (k == "add") {
			if (bench.length && Math.random() < 0.6) {
				var b = bench.splice(rnd(bench.length), 1)[0];
				if (occupied(evs, b.p, step(b), -1)) continue;
				evs.push(b);
				return "brought back " + noteName(b.p) + " on step " + (step(b) + 1);
			}
			// a quiet ghost copy of a hit, one step away
			var src = evs[rnd(evs.length)];
			var gpos = wrap(src.pos + dir, L);
			if (occupied(evs, src.p, wrap(Math.floor(gpos + 0.5), L), -1)) continue;
			evs.push({ p: src.p, v: Math.max(1, Math.round(src.v * 0.55)), pos: gpos, d: src.d });
			return "added a ghost " + noteName(src.p) + " on step " + (wrap(Math.floor(gpos + 0.5), L) + 1);
		}
	}
	return "";
}

function now() { if (KIND == "mutator" && mode == "play") { var s = mutateOnce(cur); if (s) { changes++; say(s); writeColl(cur); } } else if (KIND == "markov") newbar(); mgraphics.redraw(); }

function revert() {
	if (!orig.length) return;
	cur = clone(orig); bench = []; changes = 0;
	if (mode == "play") writeColl(cur);
	say("REVERT: back to what you played");
	mgraphics.redraw();
}

function keep() {
	if (!cur.length) return;
	orig = clone(cur); bench = []; changes = 0;
	saveState();
	say("KEEP: this is the new original");
	mgraphics.redraw();
}

// ================================================================================================
// EUCLID MORPH: each lane's hits slide towards the same number of hits spread evenly.
// ================================================================================================
var morphAmt = 0, hitsAdj = 0, rotAdj = 0, together = 0;
function morph(v) { morphAmt = Math.max(0, Math.min(1, v / 100)); refresh(); mgraphics.redraw(); }
function hits(v) { hitsAdj = v | 0; refresh(); mgraphics.redraw(); }
function turn(v) { rotAdj = v | 0; refresh(); mgraphics.redraw(); }
function lanes(v) { together = v ? 1 : 0; refresh(); mgraphics.redraw(); }

function cdist(a, b, n) { var d = Math.abs(wrap(a - b, n)); return Math.min(d, n - d); }
function cdelta(from, to, n) { var d = wrap(to - from, n); return d > n / 2 ? d - n : d; }

// even spread of k hits over n steps (the Euclidean / Bresenham pattern)
function evenSpread(k, n) {
	var t = [];
	for (var i = 0; i < k; i++) t.push(Math.floor(i * n / k));
	return t;
}

// match the hits (positions) to targets, nearest first; returns pairs [hitIndex, targetIndex]
function matchUp(pos, tgt, n) {
	var c = [];
	for (var i = 0; i < pos.length; i++) for (var j = 0; j < tgt.length; j++) c.push([cdist(pos[i], tgt[j], n), i, j]);
	c.sort(function (a, b) { return a[0] - b[0] || a[1] - b[1] || a[2] - b[2]; });
	var ui = {}, uj = {}, pairs = [], cost = 0;
	for (var q = 0; q < c.length; q++) {
		if (ui[c[q][1]] || uj[c[q][2]]) continue;
		ui[c[q][1]] = 1; uj[c[q][2]] = 1;
		pairs.push([c[q][1], c[q][2]]);
		cost += c[q][0];
	}
	return { pairs: pairs, cost: cost };
}

// groups: lists of event indexes that sound together (one "hit")
function euclidLane(src, groups, out) {
	var pos = [];
	for (var g = 0; g < groups.length; g++) pos.push(src[groups[g][0]].pos);
	var k = pos.length;
	var k2 = Math.max(1, Math.min(L, k + hitsAdj));
	var base = evenSpread(k2, L);
	// the rotation that moves the hits least
	var best = null, bestR = 0;
	for (var r = 0; r < L; r++) {
		var t = [];
		for (var i = 0; i < k2; i++) t.push(wrap(base[i] + r, L));
		var m = matchUp(pos, t, L);
		if (!best || m.cost < best.cost - 1e-9) { best = m; best.t = t; bestR = r; }
	}
	var tgt = best.t;
	if (rotAdj) {
		for (var i2 = 0; i2 < tgt.length; i2++) tgt[i2] = wrap(tgt[i2] + rotAdj, L);
		best = matchUp(pos, tgt, L);
		best.t = tgt;
	}
	var usedHit = {}, usedTgt = {};
	for (var q = 0; q < best.pairs.length; q++) {
		var hi = best.pairs[q][0], ti = best.pairs[q][1];
		usedHit[hi] = 1; usedTgt[ti] = 1;
		var d = cdelta(pos[hi], tgt[ti], L);
		for (var e = 0; e < groups[hi].length; e++) {
			var ev = src[groups[hi][e]];
			out.push({ p: ev.p, v: ev.v, pos: wrap(ev.pos + morphAmt * d, L), d: ev.d });
		}
	}
	// hits with no partner fade out, new hits fade in (they swap at MORPH 50%)
	for (var h = 0; h < k; h++) if (!usedHit[h] && morphAmt < 0.5)
		for (var e2 = 0; e2 < groups[h].length; e2++) out.push(clone([src[groups[h][e2]]])[0]);
	for (var tj = 0; tj < tgt.length; tj++) if (!usedTgt[tj] && morphAmt >= 0.5) {
		// copy the nearest original hit
		var nb = 0, nd = 1e9;
		for (var h2 = 0; h2 < k; h2++) { var dd = cdist(pos[h2], tgt[tj], L); if (dd < nd) { nd = dd; nb = h2; } }
		for (var e3 = 0; e3 < groups[nb].length; e3++) {
			var s = src[groups[nb][e3]];
			out.push({ p: s.p, v: s.v, pos: tgt[tj], d: s.d });
		}
	}
}

function euclidAll(src) {
	var out = [];
	if (together) {
		// all notes are one lane; notes starting within a quarter of a 16th count as one hit
		var idx = [];
		for (var i = 0; i < src.length; i++) idx.push(i);
		idx.sort(function (a, b) { return src[a].pos - src[b].pos; });
		var groups = [];
		for (var q = 0; q < idx.length; q++) {
			var g = groups.length ? groups[groups.length - 1] : null;
			if (g && Math.abs(src[idx[q]].pos - src[g[0]].pos) < 0.25) g.push(idx[q]);
			else groups.push([idx[q]]);
		}
		euclidLane(src, groups, out);
	} else {
		var lanes = lanesOf(src);
		for (var p in lanes) {
			var gs = [];
			for (var j = 0; j < lanes[p].length; j++) gs.push([lanes[p][j]]);
			euclidLane(src, gs, out);
		}
	}
	out.sort(function (a, b) { return a.pos - b.pos || a.p - b.p; });
	return out;
}

// ================================================================================================
// MARKOV REMIX: new bars made from what tends to follow what in the bars you played.
// ================================================================================================
var strict = 0.6, markovEvery = 1, showOrig = 0, newPending = 0, split = 0, gens = 0;
function strictness(v) { strict = Math.max(0, Math.min(1, v / 100)); }
function newevery(i) { markovEvery = [1, 2, 4, 8, 0][Math.max(0, Math.min(4, i | 0))]; }
function original(v) { showOrig = v ? 1 : 0; if (mode == "play") writeColl(showOrig ? orig : cur); say(showOrig ? "ORIGINAL: playing what you played" : "playing the remix"); mgraphics.redraw(); }
function newbar() { newPending = 1; say("NEW: a fresh remix at the next bar line"); mgraphics.redraw(); }
function kitmode(v) { split = v ? 1 : 0; }

// One lane of states. sig[j] = what happens at step j ("" = nothing), evAt[j] = its notes.
// Walks a pointer through the source: with chance STRICTNESS it simply goes on to the next
// step (so 100% plays the original); otherwise it may jump to any step that comes after the
// same thing(s) as the pointer does now, preferring steps in the same place in the beat.
function markovLane(sig, evAt, n, ctx) {
	var out = [], ptr = n - 1, hist = [];
	for (var c = 0; c < ctx; c++) hist.push(sig[wrap(n - ctx + c, n)]);
	for (var i = 0; i < n; i++) {
		var next = wrap(ptr + 1, n);
		if (Math.random() >= strict) {
			var cands = [], wsum = 0;
			for (var j = 0; j < n; j++) {
				var ok = true;
				for (var c2 = 0; c2 < ctx; c2++) if (sig[wrap(j - ctx + c2, n)] != hist[c2]) { ok = false; break; }
				if (!ok) continue;
				var w = 1 + (wrap(j, 4) == wrap(i, 4) ? 4 : 0) + (wrap(j, 16) == wrap(i, 16) ? 2 : 0);
				cands.push([j, w]); wsum += w;
			}
			if (cands.length) {
				var r = Math.random() * wsum;
				for (var q = 0; q < cands.length; q++) { r -= cands[q][1]; if (r <= 0) { next = cands[q][0]; break; } }
			}
		}
		ptr = next;
		var evs = evAt[ptr] || [];
		for (var e = 0; e < evs.length; e++) {
			var s = evs[e];
			out.push({ p: s.p, v: s.v, pos: wrap(i + (s.pos - ptr), n), d: s.d });
		}
		hist.push(sig[ptr]); hist.shift();
	}
	return out;
}

function markovGen(src) {
	var out = [];
	function build(list) {
		var evAt = {}, sig = [];
		for (var i = 0; i < list.length; i++) {
			var st = wrap(Math.floor(list[i].pos + 0.5), L);
			if (!evAt[st]) evAt[st] = [];
			// keep pos relative to its nearest step
			var e = list[i], rel = e.pos;
			if (rel > L - 0.5) rel -= L;
			evAt[st].push({ p: e.p, v: e.v, pos: rel, d: e.d });
		}
		for (var j = 0; j < L; j++) {
			var ps = [];
			if (evAt[j]) for (var k = 0; k < evAt[j].length; k++) ps.push(evAt[j][k].p);
			ps.sort(function (a, b) { return a - b; });
			sig.push(ps.join("."));
		}
		return { evAt: evAt, sig: sig };
	}
	if (!split) {
		var m = build(src);
		out = markovLane(m.sig, m.evAt, L, 1);
	} else {
		var lanes = lanesOf(src);
		for (var p in lanes) {
			var l = [];
			for (var q = 0; q < lanes[p].length; q++) l.push(src[lanes[p][q]]);
			var m2 = build(l);
			out = out.concat(markovLane(m2.sig, m2.evAt, L, 2));
		}
	}
	out.sort(function (a, b) { return a.pos - b.pos || a.p - b.p; });
	return out;
}

// ---- what each device does at a bar line (one 16th ahead) and when settings change ---------
function barAhead(gNext) {
	var bar = Math.floor(gNext / 16);
	if (KIND == "mutator") {
		// you hear what you played for EVERY bars first
		if (freeze || bar % every != 0 || gNext < playFrom + 16 * every) return;
		var s = mutateOnce(cur);
		if (s) { changes++; say(s); writeColl(cur); }
	} else if (KIND == "markov") {
		if (showOrig) return;
		// the first time round plays what you played
		var due = newPending || (markovEvery > 0 && bar % markovEvery == 0 && gNext >= playFrom + L);
		if (!due) return;
		newPending = 0;
		cur = markovGen(orig);
		gens++;
		writeColl(cur);
		say("remix " + gens + " (strictness " + Math.round(strict * 100) + "%)");
	}
}

// a new pattern was learned (or loaded)
function onLearned(loaded) {
	bench = []; changes = 0; gens = 0;
	if (KIND == "euclid") cur = euclidAll(orig);
	if (KIND == "markov") cur = clone(orig);   // the first pass plays what you played
}

// write what should be playing now
function refresh() {
	if (KIND == "euclid" && orig.length) cur = euclidAll(orig);
	if (mode == "play") writeColl(KIND == "markov" && showOrig ? orig : cur);
}

// ================================================================================================
// the display: one row per note (lane), one column per 16th.
// grey outline = where the notes were when you played them, filled = what plays now.
// ================================================================================================
function rgb(c) { mgraphics.set_source_rgba(c[0], c[1], c[2], c.length > 3 ? c[3] : 1); }
var C_BG = [0.07, 0.07, 0.08], C_GRID = [0.16, 0.16, 0.18], C_BEAT = [0.26, 0.26, 0.29];
var C_INK = [0.93, 0.91, 0.85], C_SOFT = [0.6, 0.58, 0.55], C_ORANGE = [0.94, 0.63, 0.13];
var C_GHOST = [0.5, 0.5, 0.52], C_RED = [0.94, 0.3, 0.25], C_GREEN = [0.35, 0.8, 0.45];

function text(s, x, y, size, c, align) {
	mgraphics.select_font_face("Arial Bold");
	mgraphics.set_font_size(size);
	rgb(c);
	var w = mgraphics.text_measure(s)[0];
	if (align == "right") x -= w;
	if (align == "center") x -= w / 2;
	mgraphics.move_to(x, y);
	mgraphics.show_text(s);
}
function fillBox(x, y, w, h, c, fill) {
	rgb(c);
	mgraphics.rectangle(x, y, w, h);
	if (fill) mgraphics.fill(); else { mgraphics.set_line_width(1); mgraphics.stroke(); }
}

function paint() {
	var W = mgraphics.size[0], H = mgraphics.size[1];
	fillBox(0, 0, W, H, C_BG, 1);
	// status line
	var title = { mutator: "MUTATOR", euclid: "EUCLID MORPH", markov: "MARKOV REMIX" }[KIND];
	var st;
	if (mode == "thru") st = orig.length ? "THRU (press THRU to play again)" : "THRU: press LEARN";
	else if (mode == "armed") st = "ARMED: waits for the bar line";
	else if (mode == "learning") st = "LEARNING " + Math.min(learnBars, Math.floor(Math.max(0, cur16 - s0) / 16) + 1) + " / " + learnBars;
	else st = "PLAYING";
	var stc = mode == "learning" ? C_RED : (mode == "armed" ? C_ORANGE : (mode == "play" ? C_GREEN : C_SOFT));
	text(title, 6, 15, 12, C_ORANGE);
	text(st, W - 6, 15, 12, stc, "right");

	// grid
	var top = 22, bottom = H - 20, left = 30, right = W - 4;
	var showEvs = (KIND == "markov" && showOrig) ? orig : cur;
	var ps = {};
	for (var i = 0; i < orig.length; i++) ps[orig[i].p] = 1;
	for (var j = 0; j < showEvs.length; j++) ps[showEvs[j].p] = 1;
	var rows = [];
	for (var p in ps) rows.push(Number(p));
	rows.sort(function (a, b) { return b - a; });
	var n = mode == "learning" || mode == "armed" ? learnBars * 16 : L;
	var cw = (right - left) / n;
	fillBox(left, top, right - left, bottom - top, C_GRID, 1);
	for (var c = 0; c <= n; c += 4) {
		rgb(c % 16 == 0 ? C_SOFT : C_BEAT);
		mgraphics.set_line_width(c % 16 == 0 ? 1.5 : 1);
		mgraphics.move_to(left + c * cw, top);
		mgraphics.line_to(left + c * cw, bottom);
		mgraphics.stroke();
	}
	if (!rows.length || mode == "armed" || mode == "learning") {
		var hint = mode == "learning" ? "listening..." : (mode == "armed" ? "play into the track" : "put this before an instrument, play a clip, press LEARN");
		text(hint, (left + right) / 2, (top + bottom) / 2 + 4, 11, C_SOFT, "center");
	} else {
		var nr = Math.min(rows.length, 16);
		var rh = (bottom - top) / nr;
		function rowOf(p) { var r = rows.indexOf(p); return Math.floor(r * nr / rows.length); }
		// labels
		var lastR = -1;
		for (var r = 0; r < rows.length; r++) {
			var rr = rowOf(rows[r]);
			if (rr == lastR) continue;
			lastR = rr;
			if (rh >= 8) text(noteName(rows[r]), left - 3, top + rr * rh + rh / 2 + 4, Math.min(11, rh), C_SOFT, "right");
		}
		// original positions (outline)
		for (var a = 0; a < orig.length; a++) {
			var e = orig[a];
			fillBox(left + e.pos * cw + 0.5, top + rowOf(e.p) * rh + 1.5, Math.max(2, cw - 1), Math.max(2, rh - 3), C_GHOST, 0);
		}
		// what plays now (filled, brightness = velocity)
		for (var b = 0; b < showEvs.length; b++) {
			var f = showEvs[b];
			var lv = 0.45 + 0.55 * f.v / 127;
			var col = [C_ORANGE[0] * lv, C_ORANGE[1] * lv, C_ORANGE[2] * lv];
			fillBox(left + f.pos * cw + 1, top + rowOf(f.p) * rh + 2, Math.max(2, cw - 2), Math.max(2, rh - 4), col, 1);
		}
	}
	// play head
	if (cur16 >= 0 && mode != "thru") {
		var ph = wrap(cur16 - s0, n);
		fillBox(left + ph * cw, top, Math.max(1.5, cw), bottom - top, [1, 1, 1, 0.18], 1);
	}
	// bottom line: what happened last
	var info = lastMsg;
	if (KIND == "mutator" && mode == "play") info = (freeze ? "HOLD  " : "") + (changes ? changes + " changes. " : "") + lastMsg;
	text(info, 6, H - 6, 11, C_INK);
}

function bang() { mgraphics.redraw(); }
function onresize() { mgraphics.redraw(); }
function anything() { }
