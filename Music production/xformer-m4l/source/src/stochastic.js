// =====================================================================
//  STOCHASTIC ENGINE  (port of engine/StochasticTrackEngine.cpp,
//  StochasticGenerator.cpp, StochasticCache.cpp)
//
//  A probabilistic phrase generator. Rhythm and melody are two separate
//  "domains", each either LIVE (rolled fresh on every note) or LOOP (a stored
//  pattern of up to 64 steps that repeats, and can slowly evolve through
//  Patience, Mutate and Jump).
//
//  Playback reads a "cache": one cell per step holding duration, pitch,
//  gate length, rest, legato and slide. The cache is rebuilt from the stored
//  pattern + seeds whenever a shaping knob moves, so knobs reshape the loop
//  deterministically instead of re-randomising it.
// =====================================================================

var STEP_COUNT = 64;
var USER_SCALE_SIZE = 32;
var MAX_BURST = 8;
var MIN_AUDIBLE_GATE = 6;        // ticks (~16 ms at 120 BPM)
var MIN_CLUSTER_CELL = 12;       // ticks
var MAX_CELL_DURATION = 4095;
var MIN_BURST_PARENT = 96;
var DURATION_LUT = [[8, 1], [4, 1], [3, 1], [2, 1], [4, 3], [1, 1], [2, 3], [1, 2]];
var BURST_COUNT_LUT = [2, 3, 4, 5, 6, 7, 8];
var BURST_SPACING_LUT = [2, 3, 4, 5, 6];
var CENTRALITY_MAX = 30;
var F = Math.fround;

function durationFraction(i) { return DURATION_LUT[clamp(i, 0, 7)]; }

// "How tonal" a scale degree is: root strongest, then fifth, thirds, quarters
function pitchCentrality(degInOct, N) {
    if (N <= 1) return 0;
    var halfWidth = Math.max(1, idiv(N, 6));
    function kernel(target, weight) {
        var dist = Math.abs(degInOct - target);
        var wrap = N - dist;
        if (wrap < dist) dist = wrap;
        var w = idiv((halfWidth - dist) * weight, halfWidth);
        return w > 0 ? w : 0;
    }
    var b = kernel(0, CENTRALITY_MAX);
    if (N >= 2) b += kernel(idiv(N, 2), 20);
    if (N >= 3) b += kernel(idiv(N, 3), 10);
    if (N >= 3) b += kernel(idiv(2 * N, 3), 10);
    if (N >= 4) b += kernel(idiv(N, 4), 5);
    if (N >= 4) b += kernel(idiv(3 * N, 4), 5);
    return b;
}

// Burst Hold modes: bit0 = Fit/Over timing, bit1 = Hold/Roll pitch
var HOLD_FIT = 0, HOLD_OVER = 1, ROLL_FIT = 2, ROLL_OVER = 3;
function burstIsRoll(m) { return m === ROLL_FIT || m === ROLL_OVER; }
function burstIsFit(m) { return m === HOLD_FIT || m === ROLL_FIT; }
// above 50% Burst the mode is randomised per cell (deterministic per seed)
function burstHoldForCell(burst, configured, seed, cell) {
    if (burst <= 50) return configured;
    var rng = new Random(u32(cellSeed(seed, cell) ^ 0xB47570D1));
    var roll = rng.nextRange(2) !== 0;
    var fit = rng.nextRange(2) !== 0;
    if (roll) return fit ? ROLL_FIT : ROLL_OVER;
    return fit ? HOLD_FIT : HOLD_OVER;
}

// One stored step (the firmware packs this into 6 bytes; here plain fields)
function StepContent() { this.clear(); }
StepContent.prototype.clear = function () {
    this.durationIndex = 0; this.densityRank = 0; this.burstTails = 0; this.burstRate = 0;
    this.rest = false; this.legato = false; this.slide = false; this.rhythmValid = false;
    this.degree = 0; this.octave = 0; this.melodyValid = false;
};
StepContent.prototype.clearRhythm = function () {
    this.durationIndex = 0; this.densityRank = 0; this.burstTails = 0; this.burstRate = 0;
    this.rest = false; this.legato = false; this.slide = false; this.rhythmValid = false;
};
StepContent.prototype.mergeRhythmFrom = function (e) {
    this.durationIndex = e.durationIndex; this.densityRank = e.densityRank;
    this.burstTails = e.burstTails; this.burstRate = e.burstRate;
    this.rest = e.rest; this.legato = e.legato; this.slide = e.slide; this.rhythmValid = e.rhythmValid;
};
StepContent.prototype.mergeMelodyFrom = function (e) {
    this.degree = e.degree; this.octave = e.octave; this.melodyValid = e.melodyValid;
};
StepContent.prototype.copy = function () {
    var c = new StepContent(); c.mergeRhythmFrom(this); c.mergeMelodyFrom(this); return c;
};
// field widths of the packed firmware format (values wrap like the bitfields)
StepContent.prototype.setDegree = function (v) { this.degree = v & 0x7f; };
StepContent.prototype.setOctave = function (v) { this.octave = v & 0x1f; };

// ---------------------------------------------------------------------
// libstdc++ std::sort, reproduced so equal-weight steps land in exactly the
// same order as on the module (std::sort is not stable for >16 items).
// ---------------------------------------------------------------------
function gccSort(a, less) {
    function swap(i, j) { var t = a[i]; a[i] = a[j]; a[j] = t; }
    function insertionSort(first, last) {
        for (var i = first + 1; i < last; i++) {
            var val = a[i];
            if (less(val, a[first])) {
                for (var k = i; k > first; k--) a[k] = a[k - 1];
                a[first] = val;
            } else {
                var j = i;
                while (less(val, a[j - 1])) { a[j] = a[j - 1]; j--; }
                a[j] = val;
            }
        }
    }
    function unguardedInsertion(first, last) {
        for (var i = first; i < last; i++) {
            var val = a[i], j = i;
            while (less(val, a[j - 1])) { a[j] = a[j - 1]; j--; }
            a[j] = val;
        }
    }
    function medianToFirst(r, x, y, z) {
        if (less(a[x], a[y])) {
            if (less(a[y], a[z])) swap(r, y);
            else if (less(a[x], a[z])) swap(r, z);
            else swap(r, x);
        } else if (less(a[x], a[z])) swap(r, x);
        else if (less(a[y], a[z])) swap(r, z);
        else swap(r, y);
    }
    function partition(first, last, pivot) {
        for (;;) {
            while (less(a[first], a[pivot])) first++;
            last--;
            while (less(a[pivot], a[last])) last--;
            if (!(first < last)) return first;
            swap(first, last);
            first++;
        }
    }
    function introLoop(first, last, depth) {
        while (last - first > 16) {
            if (depth === 0) {
                // heap-sort fallback never triggers at 64 items with median-of-3
                var part = a.slice(first, last).sort(function (p, q) { return less(p, q) ? -1 : (less(q, p) ? 1 : 0); });
                for (var i = 0; i < part.length; i++) a[first + i] = part[i];
                return;
            }
            depth--;
            var mid = first + ((last - first) >> 1);
            medianToFirst(first, first + 1, mid, last - 1);
            var cut = partition(first + 1, last, first);
            introLoop(cut, last, depth);
            last = cut;
        }
    }
    var n = a.length;
    if (n < 2) return;
    introLoop(0, n, 2 * Math.floor(Math.log2(n)));
    if (n > 16) { insertionSort(0, 16); unguardedInsertion(16, n); }
    else insertionSort(0, n);
}

// ---------------------------------------------------------------------
// GENERATOR
// ---------------------------------------------------------------------

// triangular weighted pick: knob 0 = first entry, 100 = last entry
function pickFromLutTriangular(lut, knob, rng) {
    var n = lut.length, weights = [], total = 0, i;
    for (i = 0; i < n; i++) {
        var center = n > 1 ? idiv(i * 100, n - 1) : 50;
        var w = 50 - Math.abs(knob - center);
        if (w < 0) w = 0;
        weights.push(w); total += w;
    }
    if (total <= 0) return lut[rng.nextRange(n)];
    var roll = rng.nextRange(total), sum = 0;
    for (i = 0; i < n; i++) { sum += weights[i]; if (roll < sum) return lut[i]; }
    return lut[n - 1];
}
function pickBurstSpacingSlot(knob, rng) {
    return BURST_SPACING_LUT.indexOf(pickFromLutTriangular(BURST_SPACING_LUT, knob, rng));
}

// Note Duration + Variation: three blended zones (home / tickets / explore)
function pickDuration(seq, rng) {
    var entries = 8;
    var center = clamp(seq.noteDuration, 0, 7);
    var V = clamp(seq.variation, 0, 100);
    var mirror = 7 - center;
    var homeAmount = Math.max(0, 100 - 2 * V);
    var absVMid = V > 50 ? V - 50 : 50 - V;
    var ticketAmount = Math.max(0, 100 - 2 * absVMid);
    var exploreAmount = Math.max(0, 2 * V - 100);
    var weights = [], total = 0, i;
    for (i = 0; i < entries; i++) {
        var t = seq.durationTickets[i];
        if (t < 0) { weights.push(0); continue; }
        var dh = Math.abs(i - center);
        var homeBell = (1 > dh ? 1 - dh : 0) * 10;
        var ticketW = t > 0 ? t : 1;
        var dm = Math.abs(i - mirror);
        var mirrorBell = (2 > dm ? 2 - dm : 0) * 10;
        var exploreW = 5 + mirrorBell;
        var w = homeBell * homeAmount + ticketW * ticketAmount + exploreW * exploreAmount;
        weights.push(w); total += w;
    }
    if (total <= 0) return center;
    var roll = rng.nextRange(total), sum = 0;
    for (i = 0; i < entries; i++) { sum += weights[i]; if (roll < sum) return i; }
    return center;
}

// mask ranks: order steps long-to-short (plus a little seeded jitter)
function generateMaskRanks(seq, size, seed) {
    var rng = new Random(seed);
    if (size <= 0) return;
    size = Math.min(size, STEP_COUNT);
    var items = [];
    for (var k = 0; k < size; k++) {
        var durSlot = clamp(seq.steps[k].durationIndex, 0, 7);
        var axis = F(F(durSlot - 3.5) / F(3.5));
        var jitter = F(F(F(rng.nextRange(1000)) / F(1000)) * F(0.2));
        items.push({ index: k, weight: F(axis + jitter) });
    }
    gccSort(items, function (a, b) { return a.weight < b.weight; });
    for (k = 0; k < size; k++) seq.steps[items[k].index].densityRank = k & 0x3f;
}

function generateRhythmEvent(seq, rng) {
    var ev = new StepContent();
    ev.durationIndex = pickDuration(seq, rng);
    ev.densityRank = 0;
    ev.rest = rng.nextRange(100) < seq.rest;
    ev.legato = false; ev.slide = false;
    ev.rhythmValid = true;
    ev.burstTails = 0; ev.burstRate = 0;
    if (rng.nextRange(100) < seq.burst) {
        var total = pickFromLutTriangular(BURST_COUNT_LUT, seq.burstCount, rng);
        ev.burstTails = Math.max(1, total - 1) & 7;
        var slot = BURST_SPACING_LUT.indexOf(pickFromLutTriangular(BURST_SPACING_LUT, seq.burstRate, rng));
        ev.burstRate = slot < 0 ? 0 : slot;
    }
    return ev;
}

// effective ticket after Degree Rotation and Mask Rotation
function effectiveDegreeTicket(seq, degree, activeNotes) {
    var dRot = ((seq.degreeRotation % activeNotes) + activeNotes) % activeNotes;
    var srcIdx = (degree - dRot + activeNotes * 2) % activeNotes;
    if (seq.degreeTickets[srcIdx] < 0) return -1;
    var nonNeg = [], eff = [], i;
    for (i = 0; i < activeNotes; i++) {
        var idx = (i - dRot + activeNotes * 2) % activeNotes;
        eff[i] = seq.degreeTickets[idx];
        if (eff[i] >= 0) nonNeg.push(eff[i]);
    }
    var mRot = seq.maskRotation, cnt = nonNeg.length;
    if (cnt > 1 && mRot !== 0) {
        var rotSteps = ((mRot % cnt) + cnt) % cnt, pos = 0;
        for (i = 0; i < activeNotes; i++) {
            if (eff[i] >= 0) { eff[i] = nonNeg[(pos - rotSteps + cnt) % cnt]; pos++; }
        }
    }
    return eff[degree];
}

function recencyPenaltyMilli(run) {
    if (run <= 0) return 1000;
    if (run === 1) return 550;
    if (run === 2) return 200;
    return 50;
}

// Bias + Spread shaping of a 0..1000 roll (Marbles-style)
function betaShapeMilli(u, biasKnob, spreadKnob) {
    var bias = clamp(biasKnob, 0, 100) * 10;
    var spread = clamp(spreadKnob, 0, 100) * 10;
    if (u > 1000) u = 1000;
    if (spread < 500) {
        var pull = Math.floor(((500 - spread) * 1000) / 500);
        return Math.floor((bias * pull + u * (1000 - pull)) / 1000);
    }
    var bernPull = Math.floor(((spread - 500) * 1000) / 500);
    var edge = u < bias ? 0 : 1000;
    return Math.floor((u * (1000 - bernPull) + edge * bernPull) / 1000);
}

function newPitchState() { return { lastDegree: -1, lastClass: -1, classRunLength: 0 }; }

// The five-step pitch law: class roll with tickets + recency, repeat check
// (Complexity), octave candidates (Range), region roll (Bias/Spread),
// direction nudge (Contour). Returns an absolute scale degree.
function generateDegree(seq, notesPerOctave, state, rng) {
    var N = clamp(notesPerOctave, 1, USER_SCALE_SIZE);
    var range = clamp(seq.range, 0, 100);
    var complexity = clamp(seq.complexity, 0, 100);
    var contour = clamp(seq.contour, -100, 100);
    var allowed = [], tickets = [], c, i;
    for (c = 0; c < N; c++) {
        var t = effectiveDegreeTicket(seq, c, N);
        if (t >= 0) { allowed.push(c); tickets.push(t); }
    }
    if (allowed.length === 0) return 0;

    var chosenClass = -1;
    for (var retry = 0; retry < 4; retry++) {
        var weights = [], total = 0;
        for (i = 0; i < allowed.length; i++) {
            var base = tickets[i] > 0 ? tickets[i] : 1;
            var penalty = allowed[i] === state.lastClass ? recencyPenaltyMilli(state.classRunLength) : 1000;
            weights.push(base * penalty); total += base * penalty;
        }
        var picked = 0;
        if (total > 0) {
            var roll = rng.nextRange(total), sum = 0;
            for (i = 0; i < allowed.length; i++) { sum += weights[i]; if (roll < sum) { picked = i; break; } }
        }
        chosenClass = allowed[picked];
        if (chosenClass === state.lastClass && state.lastClass >= 0) {
            if (rng.nextRange(100) < complexity) continue;
        }
        break;
    }
    if (chosenClass < 0) chosenClass = allowed[0];

    var currentOctave = state.lastDegree >= 0 ? idiv(state.lastDegree, N) : 0;
    var cand = [];
    if (range > 50) {
        var width = clamp(1 + idiv((range - 50) * 3, 50), 1, 4);
        for (var k = 0; k < width; k++) cand.push(chosenClass + k * N);
    } else if (range < 50) {
        cand.push(chosenClass + currentOctave * N);
        if (rng.nextRange(100) < (50 - range) * 2) {
            if (currentOctave - 1 >= 0) cand.push(chosenClass + (currentOctave - 1) * N);
            cand.push(chosenClass + (currentOctave + 1) * N);
        }
    } else {
        cand.push(chosenClass + currentOctave * N);
    }
    cand.sort(function (a, b) { return a - b; });

    var chosen = cand[0];
    if (cand.length > 1) {
        var u = rng.nextRange(1001);
        var shaped = betaShapeMilli(u, seq.marblesBias, seq.marblesSpread);
        var ix = clamp(Math.floor((shaped * (cand.length - 1) + 500) / 1000), 0, cand.length - 1);
        chosen = cand[ix];
    }
    if (contour !== 0 && state.lastDegree >= 0 && cand.length > 1) {
        var want = contour > 0 ? 1 : -1;
        var got = chosen > state.lastDegree ? 1 : (chosen < state.lastDegree ? -1 : 0);
        if (got !== want && rng.nextRange(100) < Math.abs(contour)) {
            var best = -1, bestDist = 0;
            for (i = 0; i < cand.length; i++) {
                var d = cand[i] - state.lastDegree;
                var s = d > 0 ? 1 : (d < 0 ? -1 : 0);
                if (s === want && (best < 0 || Math.abs(d) < bestDist)) { best = i; bestDist = Math.abs(d); }
            }
            if (best >= 0) chosen = cand[best];
        }
    }
    var cls = chosen % N;
    if (cls === state.lastClass) state.classRunLength++;
    else { state.lastClass = cls; state.classRunLength = 0; }
    state.lastDegree = chosen;
    return chosen;
}

function generateMelodyEvent(seq, N, state, rng) {
    var ev = new StepContent();
    var abs = generateDegree(seq, N, state, rng);
    ev.setDegree(abs % N);
    ev.setOctave(idiv(abs, N));
    ev.melodyValid = true;
    return ev;
}

function generateRhythm(seq, seed) {
    var rng = new Random(seed);
    for (var i = 0; i < STEP_COUNT; i++) {
        if (i < seq.size) seq.steps[i].mergeRhythmFrom(generateRhythmEvent(seq, rng));
        else seq.steps[i].clearRhythm();
    }
    generateMaskRanks(seq, seq.size, u32(seed ^ 0xdeadbeef));
    seq.rhythmValid = true;
    seq.rhythmSeed = u32(seed);
}

function generateMelody(seq, N, seed) {
    var rng = new Random(seed);
    var state = newPitchState();
    for (var i = 0; i < STEP_COUNT; i++) {
        if (i < seq.size) seq.steps[i].mergeMelodyFrom(generateMelodyEvent(seq, N, state, rng));
        else seq.steps[i].melodyValid = false;
    }
    seq.melodyValid = true;
    seq.melodySeed = u32(seed);
}

function mutateRhythmOne(seq, rng) {
    var size = seq.size;
    if (size <= 0) return;
    var first = clamp(seq.first, 0, size - 1), last = clamp(size - 1, first, size - 1);
    var i = first + rng.nextRange(last - first + 1);
    seq.steps[i].mergeRhythmFrom(generateRhythmEvent(seq, rng));
    generateMaskRanks(seq, size, u32(seq.rhythmSeed ^ 0xdeadbeef));
}

function mutateMelodyOne(seq, N, rng) {
    var size = seq.size;
    if (size <= 0) return;
    var first = clamp(seq.first, 0, size - 1), last = clamp(size - 1, first, size - 1);
    var i = first + rng.nextRange(last - first + 1);
    var abs = generateDegree(seq, N, newPitchState(), rng);
    var n = clamp(N, 1, USER_SCALE_SIZE);
    var m = new StepContent();
    m.setDegree(abs % n); m.setOctave(idiv(abs, n)); m.melodyValid = true;
    seq.steps[i].mergeMelodyFrom(m);
}

// Repeat path bursts: child notes inside the replayed event
function evaluateBurst(seq, ev, N, anchorNote, durationTicks, rng, holdSeed, holdCell) {
    var out = [];
    var count = ev.burstTails;
    var roll = burstIsRoll(burstHoldForCell(seq.burst, seq.burstHold, holdSeed, holdCell));
    if (durationTicks < MIN_BURST_PARENT) count = 0;
    var slot = clamp(ev.burstRate, 0, BURST_SPACING_LUT.length - 1);
    var spacing = F(F(durationTicks) / F(BURST_SPACING_LUT[slot]));
    spacing = Math.max(F(MIN_AUDIBLE_GATE + 1), spacing);
    for (var i = 0; i < MAX_BURST && i < count; i++) {
        var offset = Math.floor(F((i + 1) * spacing));
        var gate = Math.max(MIN_AUDIBLE_GATE, Math.floor(F(spacing * F(0.5))));
        if (offset + gate >= durationTicks) {
            if (offset + MIN_AUDIBLE_GATE < durationTicks) {
                gate = durationTicks - offset - 1;
                if (gate < MIN_AUDIBLE_GATE) break;
            } else break;
        }
        var note = roll ? generateDegree(seq, N, newPitchState(), rng) : anchorNote;
        out.push({ tickOffset: offset, gateTicks: gate, note: note, roll: roll });
    }
    return out;
}

// ---------------------------------------------------------------------
// CACHE: one playable cell per step
// ---------------------------------------------------------------------
function computeFeelScaleQ16(feel, naturalSum, beatTicks) {
    var ONE = 0x10000;
    if (feel >= 45 && feel <= 55) return ONE;
    if (naturalSum === 0 || beatTicks === 0) return ONE;
    var targetBeats = feel < 45 ? F(F(4) - F(F(45 - feel) / F(45))) : F(F(4) + F(F(feel - 55) / F(45)));
    var targetTicks = F(targetBeats * F(beatTicks));
    var scale = F(targetTicks / F(naturalSum));
    if (scale < 0.25) scale = F(0.25);
    if (scale > 4) scale = F(4);
    return Math.floor(F(F(scale * F(ONE)) + F(0.5)));
}
function applyFeelScale(ticks, q16) { return Math.floor(ticks * q16 / 65536); }

function rebuildStepCache(cache, seq, divisor, seed, N, capturedRhythm, capturedMelody) {
    cache.count = 0; cache.cycleTicks = 0; cache.cells = [];
    var size = Math.max(0, Math.min(seq.size, STEP_COUNT));
    if (size <= 0) return 0;
    function lutTicks(idx) {
        var f = durationFraction(idx);
        var t = Math.floor(divisor * f[0] / f[1]);
        return t > 0 ? t : 1;
    }
    var cycleTicks = 0, prevDur = 0, clusterRemaining = 0, clusterIdx = 0;
    var clusterDurs = [];
    function curveFromKnob(knob) {
        var k = F(Math.max(0, Math.min(100, knob)));
        var expo = F(F(F(0.9163) * F(k - F(50))) / F(50));
        return F(Math.exp(expo));
    }
    function buildFitCluster(total, cells, r) {
        if (cells <= 0) cells = 1;
        if (cells > MAX_BURST + 1) cells = MAX_BURST + 1;
        var budget = Math.max(0, total - MIN_CLUSTER_CELL * cells);
        var w = [], sum = F(0), rk = F(1), k;
        for (k = 0; k < cells; k++) { w[k] = rk; sum = F(sum + rk); rk = F(rk * r); }
        if (sum <= 0) sum = F(1);
        for (k = 0; k < cells; k++) clusterDurs[k] = MIN_CLUSTER_CELL + Math.trunc(F(F(F(budget) * w[k]) / sum));
    }
    var pickMelodyInCache = seq.melodyMode === 0 && !capturedMelody;
    var pickRhythmInCache = seq.rhythmMode === 0 && !capturedRhythm;
    var pitchState = newPitchState();
    var anchorDegree = 0, anchorOctave = 0, anchorLegato = false, anchorSlide = false, anchorAudible = true;
    var total, maxFit, tails, r, slot, cand, k;

    for (var i = 0; i < size; i++) {
        var ev = seq.steps[i];
        var stepRng = new Random(cellSeed(seed, i));
        var cellDur, isTail = false;
        var burstFit = burstIsFit(burstHoldForCell(seq.burst, seq.burstHold, seed, i));

        if (clusterRemaining > 0) {
            cellDur = clusterDurs[Math.min(clusterIdx, MAX_BURST - 1)];
            clusterIdx++; clusterRemaining--; isTail = true;
        } else if (pickRhythmInCache) {
            cellDur = lutTicks(pickDuration(seq, stepRng));
            if (i === 0) prevDur = cellDur;
            if (stepRng.nextRange(100) < seq.burst) {
                total = clamp(pickFromLutTriangular(BURST_COUNT_LUT, seq.burstCount, stepRng), 2, MAX_BURST + 1);
                if (burstFit) {
                    maxFit = Math.floor(prevDur / MIN_CLUSTER_CELL);
                    if (maxFit >= 2) {
                        if (total > maxFit) total = maxFit;
                        buildFitCluster(prevDur, total, curveFromKnob(seq.burstRate));
                        cellDur = clusterDurs[0]; clusterIdx = 1; clusterRemaining = total - 1;
                    }
                } else {
                    slot = pickBurstSpacingSlot(seq.burstRate, stepRng);
                    cand = Math.floor(prevDur / BURST_SPACING_LUT[slot]);
                    if (cand >= MIN_CLUSTER_CELL) {
                        for (k = 0; k < total; k++) clusterDurs[k] = cand;
                        clusterIdx = 1; clusterRemaining = total - 1; cellDur = cand;
                    }
                }
            }
        } else {
            cellDur = lutTicks(ev.durationIndex);
            if (i === 0) prevDur = cellDur;
            if (ev.burstTails > 0) {
                tails = clamp(ev.burstTails, 1, MAX_BURST);
                total = tails + 1;
                if (burstFit) {
                    maxFit = Math.floor(prevDur / MIN_CLUSTER_CELL);
                    if (maxFit >= 2) {
                        if (total > maxFit) total = maxFit;
                        buildFitCluster(prevDur, total, curveFromKnob(seq.burstRate));
                        cellDur = clusterDurs[0]; clusterIdx = 1; clusterRemaining = total - 1;
                    }
                } else {
                    slot = clamp(ev.burstRate, 0, BURST_SPACING_LUT.length - 1);
                    cand = Math.floor(prevDur / BURST_SPACING_LUT[slot]);
                    if (cand >= MIN_CLUSTER_CELL) {
                        for (k = 0; k < total; k++) clusterDurs[k] = cand;
                        clusterIdx = 1; clusterRemaining = tails; cellDur = cand;
                    }
                }
            }
        }

        var slotDegree = 0, slotOctave = 0, notes = Math.max(1, N), degAbs;
        if (pickMelodyInCache) {
            degAbs = generateDegree(seq, N, pitchState, new Random(cellSeed(seq.melodySeed, i)));
            slotDegree = (degAbs % notes) & 0x7f;
            slotOctave = Math.min(15, idiv(degAbs, notes)) & 0xf;
        }
        var cellDegree, cellOctave;
        if (isTail) {
            if (burstIsRoll(burstHoldForCell(seq.burst, seq.burstHold, seq.melodySeed, i))) {
                var tailRng = new Random(u32(cellSeed(seq.melodySeed, i) ^ 0x7A11C0DE));
                degAbs = generateDegree(seq, N, newPitchState(), tailRng);
                cellDegree = (degAbs % notes) & 0x7f;
                cellOctave = Math.min(15, idiv(degAbs, notes)) & 0xf;
            } else { cellDegree = anchorDegree; cellOctave = anchorOctave; }
        } else if (pickMelodyInCache) {
            cellDegree = anchorDegree = slotDegree;
            cellOctave = anchorOctave = slotOctave;
        } else {
            cellDegree = anchorDegree = ev.degree & 0x7f;
            cellOctave = anchorOctave = Math.min(15, ev.octave) & 0xf;
        }

        // gate length as 64ths of the cell, triangular around 50%
        var pct = 50, spread = clamp(seq.gateLength, 0, 100);
        if (spread > 0) {
            var tri = idiv(stepRng.nextRange(101) + stepRng.nextRange(101), 2);
            pct = 50 + idiv((tri - 50) * spread, 100);
        }
        pct = clamp(pct, 10, 100);
        var gateFrac = Math.min(63, idiv(pct * 64, 100));

        var cellLegato, cellSlide;
        if (isTail) { cellLegato = anchorLegato; cellSlide = anchorSlide; }
        else {
            var leg = clamp(seq.legatoProb, 0, 100);
            cellLegato = leg > 0 && stepRng.nextRange(100) < leg;
            var sl = clamp(seq.slide, 0, 100);
            var slideRng = new Random(u32(cellSeed(seq.melodySeed, i) ^ 0x511DE51D));
            cellSlide = sl > 0 && slideRng.nextRange(100) < sl;
            anchorLegato = cellLegato; anchorSlide = cellSlide;
        }

        var audible;
        if (isTail) audible = anchorAudible;
        else if (pickRhythmInCache) {
            var restKnob = clamp(seq.rest, 0, 100);
            var restRng = new Random(u32(cellSeed(seq.rhythmSeed, i) ^ 0x5E57DEAD));
            audible = !(restKnob > 0 && restRng.nextRange(100) < restKnob);
            anchorAudible = audible;
        } else { audible = !ev.rest; anchorAudible = audible; }

        if (cellDur > MAX_CELL_DURATION) cellDur = MAX_CELL_DURATION;
        cache.cells[i] = { dur: cellDur & 0xfff, degree: cellDegree, octave: cellOctave,
            gateLen: gateFrac, audible: audible, slide: cellSlide, legato: cellLegato };
        cache.count++;
        cycleTicks += cellDur;
        prevDur = cellDur;
    }
    cache.cycleTicks = cycleTicks;
    return cache.count;
}

// ---------------------------------------------------------------------
// ENGINE
// ---------------------------------------------------------------------
function patienceLambda(p) { return F(Math.pow(2, F(F(F(clamp(p, 0, 100)) * F(7)) / F(99)))); }
function poissonCdf(k, lambda) {
    if (k < 0) return 0;
    if (lambda <= 0) return 1;
    if (k > 200) k = 200;
    var term = F(Math.exp(-lambda)), sum = term;
    for (var i = 1; i <= k; i++) { term = F(term * F(lambda / F(i))); sum = F(sum + term); }
    return sum > 1 ? 1 : sum;
}
function patienceProbability(loops, p) { return p >= 100 ? 0 : poissonCdf(loops, patienceLambda(p)); }

var STOCHASTIC_RANGES = {
    rhythmMode: [0, 1], melodyMode: [0, 1], complexity: [0, 100], contour: [-100, 100],
    repeatProb: [0, 100], noteDuration: [0, 7], variation: [0, 100], rest: [0, 100],
    slide: [0, 100], burstRate: [0, 100], burstCount: [0, 100], burstHold: [0, 3],
    sleep: [0, 100], mutate: [0, 100], jump: [0, 100], range: [0, 100],
    degreeRotation: [-32, 32], maskRotation: [-32, 32], patienceMelody: [0, 100],
    patienceRhythm: [0, 100], legatoProb: [0, 100], marblesSpread: [0, 100],
    marblesBias: [0, 100], maskMelody: [0, 100], tiltMelody: [0, 100], maskRhythm: [0, 100],
    gateLength: [0, 100], tiltRhythm: [0, 50 * 2], burst: [0, 100], feel: [0, 100],
    rotate: [-64, 64], size: [2, 64], first: [0, 63], divisor: [1, 768], clockMult: [50, 150],
    scale: [0, 13], rootNote: [0, 11], scaleRotate: [0, 31], octave: [-10, 10],
    transpose: [-100, 100], cvUpdate: [0, 1], evolve: [0, 1]
};
// knobs baked into the cache: moving one rebuilds it on the next note
var SHAPING_KNOBS = ["noteDuration", "variation", "burst", "gateLength", "complexity", "contour",
    "marblesBias", "marblesSpread", "burstCount", "burstRate", "burstHold", "range"];

function StochasticSequence() {
    this.rhythmMode = 1; this.melodyMode = 1;     // 0 = Loop, 1 = Live
    this.complexity = 50; this.contour = 0; this.repeatProb = 0; this.noteDuration = 5;
    this.variation = 16; this.rest = 0; this.slide = 0; this.burstRate = 50; this.burstCount = 0;
    this.burstHold = HOLD_OVER; this.sleep = 0; this.mutate = 0; this.jump = 0; this.range = 50;
    this.degreeRotation = 0; this.maskRotation = 0; this.patienceMelody = 100; this.patienceRhythm = 100;
    this.legatoProb = 0; this.marblesSpread = 50; this.marblesBias = 50; this.maskMelody = 100;
    this.tiltMelody = 0; this.maskRhythm = 100; this.gateLength = 0; this.tiltRhythm = 50;
    this.burst = 0; this.feel = 50; this.rotate = 0; this.size = 32; this.first = 0;
    this.divisor = 12; this.clockMult = 100; this.scale = 1; this.rootNote = 0; this.scaleRotate = 0;
    this.octave = 0; this.transpose = 0; this.cvUpdate = 0;
    this.evolve = 0;   // device option: 0 = every play starts from the saved pattern
    this.degreeTickets = []; for (var i = 0; i < USER_SCALE_SIZE; i++) this.degreeTickets.push(0);
    this.durationTickets = [0, 0, 0, 0, 0, 0, 0, 0];
    this.steps = []; for (i = 0; i < STEP_COUNT; i++) this.steps.push(new StepContent());
    this.rhythmValid = false; this.melodyValid = false;
    this.rhythmSeed = 0; this.melodySeed = 0;
}

function StochasticEngine(host) {
    this.host = host;
    this.p = new StochasticSequence();
    this._capturedR = false; this._capturedM = false;
    this._undoR = null; this._undoM = null;
    this._shapingSnapshot = null;
    this._cache = { count: 0, cycleTicks: 0, cells: [] };
    this.saved = null;       // the pattern a fresh play starts from
    this.reset();
}

StochasticEngine.prototype.scaleView = function () { return new ScaleView(this.p.scale, this.p.scaleRotate); };
StochasticEngine.prototype.N = function () { return this.scaleView().notesPerOctave(); };
StochasticEngine.prototype.last = function () { return this.p.size - 1; };

StochasticEngine.prototype.effectiveDivisor = function () {
    var cm = F(this.p.clockMult * F(0.01));
    var d = this.p.divisor * 4;
    return Math.max(1, Math.round(F(d / cm)));
};

StochasticEngine.prototype.setParam = function (name, value) {
    var p = this.p, r = STOCHASTIC_RANGES[name];
    if (r) {
        value = clamp(Math.round(value), r[0], r[1]);
        if (name === "first") value = clamp(value, 0, p.size - 1);
        p[name] = value;
        if (name === "size") p.first = clamp(p.first, 0, p.size - 1);
        if (name === "size" || name === "first") this.syncWindowEdit();
        return true;
    }
    var m = /^degreeTicket(\d+)$/.exec(name);
    if (m) { p.degreeTickets[clamp(+m[1], 0, 31)] = clamp(Math.round(value), -1, 100); this.refreshStepCache(); return true; }
    m = /^durationTicket(\d+)$/.exec(name);
    if (m) { p.durationTickets[clamp(+m[1], 0, 7)] = clamp(Math.round(value), -1, 100); this.refreshStepCache(); return true; }
    return false;
};
// scenario lines from the firmware test harness ("degreeTicket 3 50")
StochasticEngine.prototype.setRefParam = function (t) {
    if (t[0] === "degreeTicket" || t[0] === "durationTicket") return this.setParam(t[0] + t[1], +t[2]);
    if (t[0] === "rhythmMode" || t[0] === "melodyMode") return this.setParam(t[0], +t[1]);
    return this.setParam(t[0], +t[1]);
};
StochasticEngine.prototype.refInit = function (renew) { if (renew) this.refreshLoopSources(); };

StochasticEngine.prototype.refreshStepCache = function () {
    rebuildStepCache(this._cache, this.p, this.effectiveDivisor(), this.p.rhythmSeed, this.N(),
        this._capturedR, this._capturedM);
    if (this.host.cacheChanged) this.host.cacheChanged();
};

StochasticEngine.prototype.reset = function () {
    // a fresh play starts from the saved pattern unless Evolve is on
    if (this.saved && !this.p.evolve) this.restoreSaved();
    this._currentStep = this.p.first;
    this._sleepRemaining = 0;
    this._loopCycleCount = 0; this._loopCycleCountMelody = 0;
    this._lastAppliedSize = 0;
    this._lastDegree = -1;
    this._pitchState = newPitchState();
    this._jumpRegister = 0;
    this._eventElapsed = 0; this._eventDuration = 0;
    this._gateOutput = false; this._slideActive = false;
    this._cvOutput = 0; this._cvOutputTarget = 0;
    this._lastStepContent = new StepContent(); this._lastStepContentValid = false; this._lastStepIndex = 0;
    this._gateQueue = new SortedQueue(16);
    this._cvQueue = new SortedQueue(16);
    this._rng = new Random(0x12345678);
    this._refreshPending = false;
    this._prevGate = false; this._prevCv = 0;
    this.refreshStepCache();
};

StochasticEngine.prototype.stop = function () {
    this._gateQueue.clear(); this._cvQueue.clear();
    this._gateOutput = false; this._prevGate = false;
};

// Every bar line the firmware re-selects the pattern (Engine.cpp song advance ->
// changePattern): Repeat forgets the last event and the cache is rebuilt.
// The module also drops pending gate-offs here, which holds a note over the
// bar line; the device keeps them (quirks = true reproduces the module exactly).
StochasticEngine.prototype.barBoundary = function () {
    if (this.quirks) { this._gateQueue.clear(); this._cvQueue.clear(); }
    this._lastStepContentValid = false;
    this.refreshStepCache();
};

// reset every N bars (firmware resetMeasure): back to First, counts as a cycle end
StochasticEngine.prototype.measureReset = function () {
    var preempted = this._currentStep !== this.p.first;
    this._currentStep = this.p.first;
    this._eventElapsed = 0; this._eventDuration = 0;
    if (preempted) this.rollCycleEndHooks();
};

StochasticEngine.prototype.tick = function (tick) {
    var divisor = this.effectiveDivisor();
    this._eventElapsed++;
    if (this._eventDuration === 0 || this._eventElapsed >= this._eventDuration) {
        this._eventElapsed = 0;
        if (this._sleepRemaining > 0) this._sleepRemaining--;
        else this.triggerStep(tick, divisor);
    }
    var updated = false, ev;
    while (!this._gateQueue.empty() && tick >= this._gateQueue.front().tick) {
        this._gateOutput = this._gateQueue.pop().gate;
    }
    while (!this._cvQueue.empty() && tick >= this._cvQueue.front().tick) {
        ev = this._cvQueue.pop();
        this._cvOutputTarget = ev.cv;
        this._slideActive = ev.slide;
        updated = true;
    }
    // the hardware only sees the state at the end of the tick; turn that into gate events
    if (this._gateOutput && !this._prevGate) this.host.gate(true, this._cvOutputTarget, 100, this._slideActive);
    else if (!this._gateOutput && this._prevGate) this.host.gate(false);
    else if (this._gateOutput && this._cvOutputTarget !== this._prevCv) this.host.gate(true, this._cvOutputTarget, 100, this._slideActive);
    this._prevGate = this._gateOutput;
    this._prevCv = this._cvOutputTarget;
    return updated;
};

// slide time is a CV feature; with MIDI the output simply follows the target
StochasticEngine.prototype.update = function () { this._cvOutput = this._cvOutputTarget; };

StochasticEngine.prototype.ensureLoopSources = function () {
    var p = this.p, regen = false;
    if (p.rhythmMode === 0 && !p.rhythmValid) { generateRhythm(p, this._rng.next()); regen = true; }
    if (p.melodyMode === 0 && !p.melodyValid) { generateMelody(p, this.N(), this._rng.next()); this._jumpRegister = 0; regen = true; }
    if (regen) this.refreshStepCache();
};

StochasticEngine.prototype.triggerStep = function (tick, divisor) {
    var p = this.p, i;
    // shaping knobs moved? rebuild the cache before reading it
    var cur = SHAPING_KNOBS.map(function (k) { return p[k]; }).join(",");
    if (cur !== this._shapingSnapshot) { this._refreshPending = true; this._shapingSnapshot = cur; }
    if (this._refreshPending) { this.refreshStepCache(); this._refreshPending = false; }

    var scale = this.scaleView(), N = scale.notesPerOctave();
    this.ensureLoopSources();

    if (this._currentStep < p.first || this._currentStep > this.last()) {
        this._currentStep = p.first;
        this._gateQueue.clear(); this._cvQueue.clear();
        this._gateOutput = false;
    }
    var stepIndex = this._currentStep;
    if (p.rotate !== 0) {
        var win = this.last() - p.first + 1;
        if (win > 0) {
            var off = (this._currentStep - p.first + p.rotate) % win;
            if (off < 0) off += win;
            stepIndex = p.first + off;
        }
    }

    var event = p.steps[stepIndex];
    var evl = new StepContent();
    var useRepeat = p.repeatProb > 0 && this._lastStepContentValid && this._rng.nextRange(100) < p.repeatProb;
    if (useRepeat) {
        evl = this._lastStepContent.copy();
    } else {
        if (p.rhythmMode === 1) {
            var rh = generateRhythmEvent(p, this._rng);
            evl.mergeRhythmFrom(rh);
            p.steps[stepIndex].mergeRhythmFrom(rh); this._refreshPending = true;   // live shadow
        } else evl.mergeRhythmFrom(event);
        if (p.melodyMode === 1) {
            var me = generateMelodyEvent(p, N, this._pitchState, this._rng);
            evl.mergeMelodyFrom(me);
            this._lastDegree = this._pitchState.lastDegree;
            p.steps[stepIndex].mergeMelodyFrom(me); this._refreshPending = true;
        } else {
            evl.mergeMelodyFrom(event);
            this._lastDegree = evl.degree + evl.octave * N;
        }
        this._lastStepContent = evl.copy();
        this._lastStepContentValid = true;
        this._lastStepIndex = stepIndex;
    }

    var pDurIdx = evl.durationIndex, pDegree = evl.degree, pOctave = evl.octave;
    var pRest = evl.rest, pSlide = evl.slide, pLegato = evl.legato;
    var cellDur = 0, gateFrac = 32;
    if (!useRepeat && stepIndex < this._cache.count) {
        var cell = this._cache.cells[stepIndex];
        cellDur = cell.dur || 1;
        gateFrac = cell.gateLen;
        pDegree = cell.degree; pOctave = cell.octave;
        pRest = !cell.audible; pSlide = cell.slide; pLegato = cell.legato;
        this._lastStepContent.legato = pLegato;
        this._lastStepContent.slide = pSlide;
    }
    var mult;
    if (cellDur > 0) mult = cellDur;
    else {
        var fr = durationFraction(pDurIdx);
        mult = Math.max(1, Math.floor(divisor * fr[0] / fr[1]));
    }
    mult = Math.max(1, applyFeelScale(mult, computeFeelScaleQ16(p.feel, this._cache.cycleTicks, CONFIG_PPQN)));
    var durationTicks = mult;
    this._eventDuration = durationTicks;

    var note = pDegree + (pOctave + this._jumpRegister + p.octave) * N + p.transpose;
    var finalCv = F(F(scale.noteToVolts(note)) + F(p.rootNote * F(1 / 12)));

    // Mask/Tilt Rhythm (Loop rhythm only): thin out long or short notes
    var maskRPass = true;
    if (p.rhythmMode === 0 && p.maskRhythm < 100) {
        var psize = Math.max(1, p.size), denom = psize > 1 ? psize - 1 : 1;
        var rank = useRepeat ? evl.densityRank : p.steps[stepIndex].densityRank;
        var rankMilli = Math.min(1000, Math.floor(rank * 1000 / denom));
        var salt = cellSeed(p.rhythmSeed, stepIndex);
        var saltMilli = Math.floor(((salt >>> 24) * 1000) / 255);
        var tiltSigned = p.tiltRhythm - 50;
        var tiltMag = Math.abs(tiltSigned) * 2;
        var eff = Math.floor((tiltMag * rankMilli + (100 - tiltMag) * saltMilli) / 100);
        var maskMilli = p.maskRhythm * 10;
        maskRPass = tiltSigned <= 0 ? eff < maskMilli : (1000 - eff) < maskMilli;
    }
    // Mask/Tilt Melody (Loop melody only): keep tonal (or anti-tonal) degrees
    var maskMPass = true;
    if (p.melodyMode === 0 && (p.maskMelody < 100 || p.tiltMelody > 0)) {
        var NN = N > 0 ? N : 1;
        var dio = pDegree % NN; if (dio < 0) dio += NN;
        var cen = pitchCentrality(dio, NN);
        var mT = idiv(CENTRALITY_MAX * (100 - p.maskMelody), 100);
        var tT = idiv(CENTRALITY_MAX * (100 - p.tiltMelody), 100);
        maskMPass = cen >= mT || (p.tiltMelody > 0 && (CENTRALITY_MAX - cen) >= tT);
    }
    var isRest = pRest || !maskRPass || !maskMPass || !evl.rhythmValid || !evl.melodyValid;

    var bursts = [];
    if (useRepeat && !isRest) {
        bursts = evaluateBurst(p, evl, N, note, durationTicks, this._rng, p.melodySeed, this._lastStepIndex);
    }

    if (!isRest) {
        this._cvQueue.push({ tick: tick, cv: finalCv, slide: pSlide });
        this._gateQueue.push({ tick: tick, gate: true });
        if (!pLegato && bursts.length === 0) {
            var gateLen;
            if (useRepeat) gateLen = pickGateLength(durationTicks, p.gateLength, this._rng);
            else gateLen = clamp(Math.floor(durationTicks * gateFrac / 64), MIN_AUDIBLE_GATE, durationTicks);
            if (gateLen > durationTicks) gateLen = durationTicks;
            this._gateQueue.push({ tick: tick + gateLen, gate: false });
        }
        for (i = 0; i < bursts.length; i++) {
            var b = bursts[i];
            var childTick = tick + b.tickOffset;
            var lowTick = childTick > tick + 2 ? childTick - 2 : tick;
            var childNote = b.note;
            if (b.roll) childNote += (this._jumpRegister + p.octave) * N + p.transpose;
            var childCv = F(F(scale.noteToVolts(childNote)) + F(p.rootNote * F(1 / 12)));
            this._gateQueue.push({ tick: lowTick, gate: false });
            this._cvQueue.push({ tick: childTick, cv: childCv, slide: pSlide });
            this._gateQueue.push({ tick: childTick, gate: true });
            this._gateQueue.push({ tick: childTick + b.gateTicks, gate: false });
        }
    } else {
        this._gateQueue.push({ tick: tick, gate: false });
        if (p.cvUpdate === 1) this._cvQueue.push({ tick: tick, cv: finalCv, slide: false });
    }
    this.host.stepInfo(stepIndex, p.size, p.first, isRest ? -1 : voltsToMidi(finalCv), durationTicks);

    this._currentStep++;
    if (this._currentStep > this.last()) {
        this._currentStep = p.first;
        this.rollCycleEndHooks();
        if (p.size !== this._lastAppliedSize) { this.refreshStepCache(); this._lastAppliedSize = p.size; }
    }
};

function pickGateLength(durationTicks, spread, rng) {
    var pct = 50;
    if (spread > 0) {
        var tri = idiv(rng.nextRange(101) + rng.nextRange(101), 2);
        pct = 50 + idiv((tri - 50) * spread, 100);
    }
    pct = clamp(pct, 10, 100);
    var t = Math.floor(durationTicks * pct / 100);
    if (t < MIN_AUDIBLE_GATE) t = MIN_AUDIBLE_GATE;
    if (t > durationTicks) t = durationTicks;
    return t;
}

StochasticEngine.prototype.rollPatience = function () {
    var p = this.p;
    if (p.rhythmMode === 0 && p.patienceRhythm < 100) {
        this._loopCycleCount = (this._loopCycleCount + 1) & 0xffff;
        var pR = patienceProbability(this._loopCycleCount, p.patienceRhythm);
        if (this._rng.nextRange(10000) < u32(Math.trunc(F(pR * F(10000))))) { p.rhythmValid = false; this._loopCycleCount = 0; }
    } else if (p.patienceRhythm >= 100) this._loopCycleCount = 0;
    if (p.melodyMode === 0 && p.patienceMelody < 100) {
        this._loopCycleCountMelody = (this._loopCycleCountMelody + 1) & 0xffff;
        var pM = patienceProbability(this._loopCycleCountMelody, p.patienceMelody);
        if (this._rng.nextRange(10000) < u32(Math.trunc(F(pM * F(10000))))) { p.melodyValid = false; this._loopCycleCountMelody = 0; }
    } else if (p.patienceMelody >= 100) this._loopCycleCountMelody = 0;
};

// end of a pattern cycle: Jump (octave walk), Sleep, Patience, Mutate
StochasticEngine.prototype.rollCycleEndHooks = function () {
    var p = this.p;
    if (p.jump > 0 && this._rng.nextRange(100) < p.jump) {
        var dir = this._rng.nextRange(2) === 0 ? -1 : 1;
        if (this._jumpRegister + dir > 2) dir = -1;
        else if (this._jumpRegister + dir < -2) dir = 1;
        this._jumpRegister += dir;
    }
    if (p.sleep > 0) this._sleepRemaining = idiv(p.sleep * 4 + 5, 10);
    this.rollPatience();
    if (p.mutate > 0 && this._rng.nextRange(100) < p.mutate) {
        var rE = p.rhythmMode === 0 && p.rhythmValid, mE = p.melodyMode === 0 && p.melodyValid;
        var self = this;
        var doR = function () { mutateRhythmOne(p, self._rng); };
        var doM = function () { mutateMelodyOne(p, self.N(), self._rng); };
        if (rE && mE) { if (this._rng.nextRange(2) === 0) doR(); else doM(); }
        else if (rE) doR();
        else if (mE) doM();
        this.refreshStepCache();
    }
};

StochasticEngine.prototype.syncWindowEdit = function () {
    var first = this.p.first, last = Math.max(first, this.last());
    if (this._currentStep < first || this._currentStep > last) {
        this._currentStep = first;
        this._eventElapsed = this._eventDuration;
    }
    this._gateQueue.clear(); this._cvQueue.clear();
    this._gateOutput = false;
    this.refreshStepCache();
};

// ---- user actions (the buttons on the device) ----
StochasticEngine.prototype.refreshLoopSources = function () {
    generateRhythm(this.p, this._rng.next());
    generateMelody(this.p, this.N(), this._rng.next());
    this._loopCycleCount = 0; this._loopCycleCountMelody = 0; this._jumpRegister = 0;
    this.refreshStepCache();
    this.save();
};
StochasticEngine.prototype.newRhythm = function () {
    if (this.p.rhythmMode === 1) {                // Live: capture what just played
        this.p.rhythmValid = true; this.p.rhythmMode = 0; this._capturedR = true;
        this._loopCycleCount = 0;
        if (this.host.notify) this.host.notify("rhythmMode", 0);
    } else {                                      // Loop: brand new rhythm (undo-able)
        this._undoR = this.p.rhythmSeed;
        generateRhythm(this.p, this._rng.next());
        this._capturedR = false; this._loopCycleCount = 0;
    }
    this.refreshStepCache();
    this.save();
};
StochasticEngine.prototype.newMelody = function () {
    if (this.p.melodyMode === 1) {
        this.p.melodyValid = true; this.p.melodyMode = 0; this._capturedM = true;
        this._loopCycleCountMelody = 0; this._jumpRegister = 0;
        if (this.host.notify) this.host.notify("melodyMode", 0);
    } else {
        this._undoM = this.p.melodySeed;
        generateMelody(this.p, this.N(), this._rng.next());
        this._capturedM = false; this._loopCycleCountMelody = 0; this._jumpRegister = 0;
    }
    this.refreshStepCache();
    this.save();
};
StochasticEngine.prototype.undoRhythm = function () {
    if (this._undoR === null) return;
    generateRhythm(this.p, this._undoR); this._undoR = null;
    this._loopCycleCount = 0; this.refreshStepCache(); this.save();
};
StochasticEngine.prototype.undoMelody = function () {
    if (this._undoM === null) return;
    generateMelody(this.p, this.N(), this._undoM); this._undoM = null;
    this._loopCycleCountMelody = 0; this._jumpRegister = 0; this.refreshStepCache(); this.save();
};

// "saved" = the pattern a fresh play starts from (also what the Live set stores)
StochasticEngine.prototype.save = function () {
    var p = this.p;
    this.saved = {
        steps: p.steps.map(function (s) { return s.copy(); }),
        rhythmValid: p.rhythmValid, melodyValid: p.melodyValid,
        rhythmSeed: p.rhythmSeed, melodySeed: p.melodySeed,
        capturedR: this._capturedR, capturedM: this._capturedM
    };
    if (this.host.stateChanged) this.host.stateChanged();
};
StochasticEngine.prototype.restoreSaved = function () {
    var s = this.saved, p = this.p;
    p.steps = s.steps.map(function (x) { return x.copy(); });
    p.rhythmValid = s.rhythmValid; p.melodyValid = s.melodyValid;
    p.rhythmSeed = s.rhythmSeed; p.melodySeed = s.melodySeed;
    this._capturedR = s.capturedR; this._capturedM = s.capturedM;
};

// state for the Live set: version, flags, seeds, 64 steps x 11 fields
StochasticEngine.prototype.getState = function () {
    if (!this.saved) this.save();
    var s = this.saved;
    var out = [1, s.rhythmValid ? 1 : 0, s.melodyValid ? 1 : 0, s.capturedR ? 1 : 0, s.capturedM ? 1 : 0,
        s.rhythmSeed >>> 16, s.rhythmSeed & 0xffff, s.melodySeed >>> 16, s.melodySeed & 0xffff];
    s.steps.forEach(function (e) {
        out.push(e.durationIndex, e.densityRank, e.burstTails, e.burstRate, e.rest ? 1 : 0,
            e.legato ? 1 : 0, e.slide ? 1 : 0, e.rhythmValid ? 1 : 0, e.degree, e.octave, e.melodyValid ? 1 : 0);
    });
    return out;
};
StochasticEngine.prototype.setState = function (a) {
    if (!a || a[0] !== 1 || a.length < 9 + STEP_COUNT * 11) return false;
    var p = this.p;
    p.rhythmValid = !!a[1]; p.melodyValid = !!a[2];
    this._capturedR = !!a[3]; this._capturedM = !!a[4];
    p.rhythmSeed = u32(a[5] * 65536 + a[6]); p.melodySeed = u32(a[7] * 65536 + a[8]);
    for (var i = 0; i < STEP_COUNT; i++) {
        var e = p.steps[i], o = 9 + i * 11;
        e.durationIndex = a[o]; e.densityRank = a[o + 1]; e.burstTails = a[o + 2]; e.burstRate = a[o + 3];
        e.rest = !!a[o + 4]; e.legato = !!a[o + 5]; e.slide = !!a[o + 6]; e.rhythmValid = !!a[o + 7];
        e.degree = a[o + 8]; e.octave = a[o + 9]; e.melodyValid = !!a[o + 10];
    }
    this.save();
    this.refreshStepCache();
    return true;
};
// a brand-new device starts with a rolled pattern (the module starts empty,
// which plays one silent-ish cycle of whole notes on the root)
StochasticEngine.prototype.initFresh = function () { this.refreshLoopSources(); };

// cells for the editor display: dur, degree, octave, audible per step
StochasticEngine.prototype.displayCells = function () {
    var out = ["cells", this.p.size, this.p.first];
    for (var i = 0; i < this._cache.count; i++) {
        var c = this._cache.cells[i];
        out.push(c.dur, c.degree + c.octave * this.N(), c.audible ? 1 : 0);
    }
    return out;
};

StochasticEngine.prototype.usesMsUpdate = false;
