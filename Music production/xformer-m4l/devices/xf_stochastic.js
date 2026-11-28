// X|FORMER stochastic for Max for Live - built file, edit the sources in m4l/src
var XF_DEVICE = "stochastic";
var XF_PARAMS = ["complexity", "contour", "range", "variation", "rest", "gateLength", "burst", "slide", "rhythmMode", "melodyMode", "noteDuration", "divisor", "scale", "rootNote", "repeatProb", "legatoProb", "burstRate", "burstCount", "sleep", "mutate", "jump", "patienceRhythm", "patienceMelody", "marblesSpread", "marblesBias", "maskRhythm", "tiltRhythm", "maskMelody", "tiltMelody", "feel", "degreeRotation", "maskRotation", "rotate", "size", "first", "octave", "transpose", "scaleRotate", "clockMult", "fixedVel", "resetBars", "burstHold", "evolve", "legato", "resetOnStart", "rstAny", "rstPitch"];

// ---------------- common.js ----------------
// =====================================================================
//  SHARED HELPERS (ported from the X|FORMER firmware)
//  Random  = core/utils/Random.h   (the exact same number generator, so a
//            given Flow/Ornament/seed gives the same pattern as the module)
//  Scales  = model/Scale.cpp + RotatedScale.h (12-tone scales only, MIDI
//            can't play the microtonal ones)
// =====================================================================

// (polyfills, only used if the device runs in the old [js] object instead of [v8])
if (!Math.fround) Math.fround = (function () { var f = new Float32Array(1); return function (x) { f[0] = x; return f[0]; }; })();
if (!Math.trunc) Math.trunc = function (x) { return x < 0 ? Math.ceil(x) : Math.floor(x); };
if (!Math.imul) Math.imul = function (a, b) { var al = a & 0xffff, bl = b & 0xffff; return (al * bl + ((((a >>> 16) * bl + al * (b >>> 16)) & 0xffff) << 16)) | 0; };
if (!Math.log2) Math.log2 = function (x) { return Math.log(x) / Math.LN2; };

function clamp(v, lo, hi) { return v < lo ? lo : (v > hi ? hi : v); }
function u32(v) { return v >>> 0; }
// C-style integer division (truncates toward zero, like the firmware)
function idiv(a, b) { return Math.trunc(a / b); }
// C lround(): halves round away from zero (Math.round rounds them up)
function lround(x) { return x < 0 ? -Math.floor(-x + 0.5) : Math.floor(x + 0.5); }
// floor-division for negative scale degrees (roundDownDivide in the firmware)
function floorDiv(a, b) { return Math.floor(a / b); }

// --- Linear congruential generator, identical to the firmware's Random ---
function Random(seed) { this.state = u32(seed || 0); }
Random.prototype.next = function () {
    this.state = u32(Math.imul(this.state, 1664525) + 1013904223);
    return this.state;
};
Random.prototype.nextBinary = function () { return this.next() < 0x80000000; };
Random.prototype.nextRange = function (range) {
    // next() / (0xffffffff / range), unsigned integer maths
    var div = Math.floor(0xffffffff / u32(range));
    return Math.floor(this.next() / div);
};
Random.prototype.nextFloat = function () {
    return (this.next() >>> 9) / 8388608;
};
Random.prototype.clone = function () { return new Random(this.state); };

// --- Keyed RNG (engine/KeyedRng.h): one deterministic seed per cell ---
function mix32(h) {
    h = u32(h);
    h = u32(h ^ (h >>> 16));
    h = u32(Math.imul(h, 0x85ebca6b));
    h = u32(h ^ (h >>> 13));
    h = u32(Math.imul(h, 0xc2b2ae35));
    h = u32(h ^ (h >>> 16));
    return h;
}
function cellSeed(baseSeed, cellIdx) {
    return mix32(u32(baseSeed) ^ u32(Math.imul(u32(cellIdx), 0x9e3779b9)));
}

// --- Scales. Notes are in 1/128 semitone units, 1536 per octave. ---
var SCALES = [
    { name: "Semitones",   notes: [0, 128, 256, 384, 512, 640, 768, 896, 1024, 1152, 1280, 1408] },
    { name: "Major",       notes: [0, 256, 512, 640, 896, 1152, 1408] },
    { name: "H.Minor",     notes: [0, 256, 384, 640, 896, 1024, 1408] },
    { name: "Major Blues", notes: [0, 384, 512, 896, 1152, 1280] },
    { name: "Minor Blues", notes: [0, 384, 640, 768, 896, 1280] },
    { name: "Major Pent.", notes: [0, 256, 512, 896, 1152] },
    { name: "Minor Pent.", notes: [0, 384, 640, 896, 1280] },
    { name: "Folk",        notes: [0, 128, 384, 512, 640, 896, 1024, 1280] },
    { name: "Japanese",    notes: [0, 128, 640, 896, 1024] },
    { name: "Gamelan",     notes: [0, 128, 384, 896, 1024] },
    { name: "Gypsy",       notes: [0, 256, 384, 768, 896, 1024, 1408] },
    { name: "Arabian",     notes: [0, 128, 512, 640, 896, 1024, 1408] },
    { name: "Flamenco",    notes: [0, 128, 512, 640, 896, 1024, 1280] },
    { name: "Whole Tone",  notes: [0, 256, 512, 768, 1024, 1280] }
];
var SCALE_NAMES = SCALES.map(function (s) { return s.name; });

// A scale view with rotation (Scale Rotate turns Major into the modes:
// 1 = Dorian, 2 = Phrygian, 3 = Lydian, 4 = Mixolydian, 5 = Aeolian, 6 = Locrian)
function ScaleView(index, rotate) {
    this.base = SCALES[clamp(index | 0, 0, SCALES.length - 1)].notes;
    var n = this.base.length;
    this.rotate = (((rotate | 0) % n) + n) % n;
}
ScaleView.prototype.notesPerOctave = function () { return this.base.length; };
ScaleView.prototype.isChromatic = function () { return true; };
ScaleView.prototype.baseVolts = function (note) {
    var n = this.base.length;
    var octave = floorDiv(note, n);
    var index = note - octave * n;
    return octave + this.base[index] / 1536;
};
ScaleView.prototype.noteToVolts = function (note) {
    if (this.rotate === 0) return this.baseVolts(note);
    return this.baseVolts(note + this.rotate) - this.baseVolts(this.rotate);
};

// 1V/oct -> MIDI note. Same mapping as the firmware's MIDI output (0V = note 60).
function voltsToMidi(cv) { return clamp(60 + Math.floor(cv * 12 + 0.01), 0, 127); }

// --- Sorted event queue (engine/SortedQueue.h), events kept in tick order ---
// capacity works like the firmware ring buffer: it holds capacity-1 events.
function SortedQueue(capacity) { this.items = []; this.max = (capacity || 16) - 1; }
SortedQueue.prototype.clear = function () { this.items.length = 0; };
SortedQueue.prototype.empty = function () { return this.items.length === 0; };
SortedQueue.prototype.front = function () { return this.items[0]; };
SortedQueue.prototype.pop = function () { return this.items.shift(); };
SortedQueue.prototype._insert = function (ev) {
    // insertion sort, a new event goes after existing events with the same tick
    var i = this.items.length;
    while (i > 0 && ev.tick < this.items[i - 1].tick) i--;
    this.items.splice(i, 0, ev);
    return i;
};
// push: when full, the oldest event is dropped first
SortedQueue.prototype.push = function (ev) {
    if (this.items.length >= this.max) this.items.shift();
    this._insert(ev);
};
// pushReplace: insert, then throw away every event scheduled after it
// (this is how a new step cuts off the previous step's pending gates)
SortedQueue.prototype.pushReplace = function (ev) {
    var i = this._insert(ev);
    this.items.length = i + 1;
};

// Divisor menu shared by all devices (values in 48 PPQN sequence ticks, like the firmware)
var DIVISORS = [
    { name: "1/64", v: 3 }, { name: "1/32", v: 6 }, { name: "1/16T", v: 8 },
    { name: "1/16", v: 12 }, { name: "1/8T", v: 16 }, { name: "1/16.", v: 18 },
    { name: "1/8", v: 24 }, { name: "1/4T", v: 32 }, { name: "1/8.", v: 36 },
    { name: "1/4", v: 48 }, { name: "1/2", v: 96 }, { name: "1 bar", v: 192 },
    { name: "2 bars", v: 384 }, { name: "4 bars", v: 768 }
];

var CONFIG_PPQN = 192;
var CONFIG_SEQUENCE_PPQN = 48;

// ---------------------------------------------------------------------
// Sequence state (engine/SequenceState.cpp): run modes
// ---------------------------------------------------------------------
function SequenceState() { this.reset(); }
SequenceState.prototype.reset = function () { this.step = -1; this.prevStep = -1; this.direction = 1; this.iteration = 0; };
function randomStep(first, last, rng) { return rng.nextRange(last - first + 1) + first; }
SequenceState.prototype.advanceFree = function (runMode, first, last, rng) {
    this.prevStep = this.step;
    if (this.step === -1) {
        if (runMode <= 3) { this.step = runMode === 1 ? last : first; this.direction = runMode === 1 ? -1 : 1; }
        else { this.step = randomStep(first, last, rng); this.direction = 1; }
        return;
    }
    this.step = clamp(this.step, first, last);
    switch (runMode) {
    case 0: this.direction = 1; if (this.step >= last) { this.step = first; this.iteration++; } else this.step++; break;
    case 1: this.direction = -1; if (this.step <= first) { this.step = last; this.iteration++; } else this.step--; break;
    case 2: case 3:
        if (this.direction > 0 && this.step >= last) this.direction = -1;
        else if (this.direction < 0 && this.step <= first) { this.direction = 1; this.iteration++; }
        if (runMode === 2) { if (this.step + this.direction >= first && this.step + this.direction <= last) this.step += this.direction; }
        else this.step += this.direction;
        break;
    case 4: this.direction = 1; this.step = randomStep(first, last, rng); break;
    case 5: this.randomWalk(first, last, rng); break;
    }
};
SequenceState.prototype.advanceAligned = function (abs, runMode, first, last, rng) {
    this.prevStep = this.step;
    var n = last - first + 1;
    switch (runMode) {
    case 0: this.direction = 1; this.step = first + abs % n; this.iteration = idiv(abs, n); break;
    case 1: this.direction = -1; this.step = last - abs % n; this.iteration = idiv(abs, n); break;
    case 2:
        this.iteration = idiv(abs, 2 * n); abs %= 2 * n;
        this.direction = abs < n ? 1 : -1;
        this.step = abs < n ? first + abs : last - (abs - n);
        break;
    case 3:
        // (2n-2 is 0 for a one-step window on the module, which would divide by zero)
        var period = Math.max(1, 2 * n - 2);
        this.iteration = idiv(abs, period); abs %= period;
        this.direction = abs < n ? 1 : -1;
        this.step = abs < n ? first + abs : last - (abs - n) - 1;
        break;
    case 4: this.direction = 1; this.step = first + rng.nextRange(n); break;
    case 5: this.randomWalk(first, last, rng); break;
    }
};
SequenceState.prototype.randomWalk = function (first, last, rng) {
    if (this.step === -1) { this.step = randomStep(first, last, rng); this.direction = 1; return; }
    if (rng.nextRange(2) === 0) { this.step = this.step === first ? last : this.step - 1; this.direction = -1; }
    else { this.step = this.step === last ? first : this.step + 1; this.direction = 1; }
};

function rotateStep(step, first, last, rotate) {
    var n = last - first + 1;
    step = first + (step - first + rotate) % n;
    return step + (step < 0 ? n : 0);
}


// ---------------- stochastic.js ----------------
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

// ---------------- host.js ----------------
// =====================================================================
//  HOST: the glue between Ableton's clock and an X|FORMER engine.
//
//  Why this exists: JavaScript in Max runs on the low-priority thread, so
//  anything it sends "right now" can arrive a few milliseconds late. To keep
//  timing tight, the engine runs a little AHEAD of the playhead and writes
//  its notes into a [coll] keyed by time slot. The patch reads that [coll]
//  from the high-priority clock ([metro] -> [transport]) and plays each slot
//  exactly on time. JavaScript never has to be punctual, only early.
//
//  Time units
//    engine tick = 1/192 of a quarter note (the firmware's CONFIG_PPQN)
//    slot        = 1/96 of a quarter note  = 5 Max ticks = 2 engine ticks
//
//  The engine object must provide: reset(), stop(), tick(t), setParam(n, v)
//  and may provide: update() (+ usesMsUpdate), getState()/setState(list),
//  edit(name, args), displayAll().
//  It talks back through host.gate(on, cv, vel, slide) and host.stepInfo(...).
// =====================================================================

var LOOKAHEAD = 32;      // slots generated ahead of the playhead (1/3 beat)
var MARGIN = 2;          // slots closer than this are sent immediately
var SLIDE_HOLD = 48;     // engine ticks a note-off waits in case the next note slides
var FF_LIMIT = 400000;   // longest silent fast-forward (engine ticks)

function Host(EngineCtor, out) {
    this.out = out;                 // { coll, now, flush, display, state }
    this.engine = new EngineCtor(this);
    // device-level settings (not part of the firmware engine)
    this.hp = {
        velMode: 0,         // 0 = Dynamic (engine velocity), 1 = Fixed
        fixedVel: 100,
        legato: 1,          // slides become overlapping (legato) notes
        resetNote: -2,      // -2 = notes don't reset, -1 = any note resets, else that pitch
        resetBars: 0,       // 0 = off, else reset every N bars
        resetOnStart: 1     // 1 = pattern restarts at song start / bar line on play
    };
    this.running = false;
    this.tempo = 120;
    this.timesig = [4, 4];
    this.reported = 0;              // last slot the clock reported
    this.lastPos = -1;
    this.pending = {};              // slot -> [pitch, vel, ...] already written to coll
    this.display = [];              // [slot, list] waiting to be shown
    this._resetConv();
    this.genA = 0;                  // next absolute engine tick to generate
    this.originA = 0;               // absolute tick where the engine's tick 0 sits
    this.msAcc = 0;
    this.muted = false;
    this.curA = 0;
}

// ---------------------------------------------------------------------
// Clock input. Called every slot by the patch: "pos <slot>"
// ---------------------------------------------------------------------
Host.prototype.pos = function (slot) {
    slot = slot | 0;
    this.reported = slot;
    if (!this.running) {
        this.running = true;
        this.relocate(slot, this.startOrigin(slot));
    } else if (slot > this.lastPos + 1 && slot <= this.lastPos + 4) {
        // the clock skipped a slot or two (busy computer): play what they held
        for (var s = this.lastPos + 1; s < slot; s++) {
            var l = this.pending[s];
            if (!l) continue;
            for (var i = 0; i < l.length; i += 2) this.out.now([l[i], l[i + 1]]);
            delete this.pending[s];
            this.out.coll(["remove", s]);
        }
    } else if (slot !== this.lastPos && slot !== this.lastPos + 1) {
        // Live jumped (loop brace, clicked in the timeline): stay position-locked
        this.relocate(slot, this.startOrigin(slot));
    }
    this.lastPos = slot;
    if (this.engine.captureTick) this.capture(slot);
    this.fill();
    this.showDisplay(slot);
};

// Fractal: listening happens at the playhead, because that is when the MIDI
// notes arrive (replay runs ahead like every engine). Two engine ticks per slot.
Host.prototype.capture = function (slot) {
    var endA = (slot + 1) * 2, inp = this.engine.input;
    if (this.capA === undefined || this.capA > endA || endA - this.capA > 10) this.capA = slot * 2;
    while (this.capA < endA) {
        if (this.capA >= this.originA) this.engine.captureTick(this.capA - this.originA, inp.gate, inp.cv);
        this.capA++;
    }
};

Host.prototype.stop = function () {
    this.running = false;
    this.lastPos = -1;
    this.out.coll(["clear"]);
    this.out.flush();
    this.pending = {};
    this.display = [];
    this._resetConv();
    this.engine.stop();
};

// slots per bar from the time signature (assumes it does not change mid-song)
Host.prototype.barSlots = function () {
    return Math.max(1, Math.round(96 * this.timesig[0] * 4 / this.timesig[1]));
};

// where the pattern's beginning sits for a playhead at `slot`
Host.prototype.startOrigin = function (slot) {
    if (!this.hp.resetOnStart) return slot;
    // only a hard bar reset makes the N-bar line a clean starting point; the
    // soft ones (Note, Stochastic) keep their dice rolling, so they replay from 0
    if (this.hp.resetBars > 0 && !this.engine.measureReset && !this.engine.ownsResetMeasure) {
        var period = this.barSlots() * this.hp.resetBars;
        return Math.floor(slot / period) * period;
    }
    return 0;
};

// Manual reset (button / MIDI note): restart on the nearest 1/16 note
Host.prototype.manualReset = function () {
    if (!this.running) { this.engine.reset(); return; }
    var s = this.reported;
    var origin = Math.round(s / 24) * 24;   // 24 slots = one 1/16 note
    this.relocate(s, origin);
    this.fill();
};

// Throw away everything scheduled, restart the engine at `originSlot`
// and silently catch it up to the playhead.
Host.prototype.relocate = function (slot, originSlot) {
    this.out.coll(["clear"]);
    this.out.flush();
    this.pending = {};
    this.display = [];
    this._resetConv();
    this.engine.reset();
    this.originA = originSlot * 2;
    var playA = slot * 2;
    this.capA = playA;
    if (playA - this.originA > FF_LIMIT) this.originA = playA;
    this.genA = this.originA;
    this.msAcc = 0;
    if (this.genA < playA) {
        this.muted = true;
        while (this.genA < playA) this.step();
        this.muted = false;
        this._resetConv();
    }
};

// generate engine ticks until we are LOOKAHEAD slots ahead
Host.prototype.fill = function () {
    var target = (this.reported + LOOKAHEAD) * 2;
    while (this.genA < target) this.step();
    this.flushHeldOff(false);
};

// one engine tick (plus the ms-rate updates that fall inside it)
Host.prototype.step = function () {
    var A = this.genA;
    if (A >= this.originA) {
        var rel = A - this.originA;
        this.curA = A;
        var barLine = A > this.originA && A % (this.barSlots() * 2) === 0;
        if (barLine && this.engine.barBoundary) this.engine.barBoundary();
        // reset every N bars (counted from the last start/reset). The Note engine
        // does this itself (its Reset Measure); Stochastic has its own soft
        // version; anything else gets a hard restart on the bar line.
        if (this.hp.resetBars > 0 && rel > 0 && !this.engine.ownsResetMeasure) {
            var period = this.barSlots() * 2 * this.hp.resetBars;
            if (rel % period === 0) {
                if (this.engine.measureReset) this.engine.measureReset();
                else {
                    this.endNote();
                    this.engine.reset();
                    this.originA = A;
                    rel = 0;
                }
            }
        }
        this.engine.tick(rel);
        if (this.engine.usesMsUpdate) {
            this.msAcc += 60000 / (Math.max(1, this.tempo) * CONFIG_PPQN);
            while (this.msAcc >= 1) { this.msAcc -= 1; this.engine.update(); }
        }
    }
    this.genA = A + 1;
};

// ---------------------------------------------------------------------
// Gate -> MIDI converter.
// The engine thinks in "gate high/low + pitch". MIDI needs note-on/off pairs.
//   - a new gate starts a note at the current pitch
//   - pitch changing while the gate stays high = legato: new note starts,
//     old note ends 2 ticks later (overlap, so a mono synth glides)
//   - a sliding note right after a gate-off stretches the old note into
//     the new one (only when "Legato" is on)
// ---------------------------------------------------------------------
Host.prototype._resetConv = function () {
    this.cur = null;        // sounding note {pitch, onSlot}
    this.held = null;       // ended note waiting to see if the next one slides
    this.offAt = {};        // pitch -> slot of a scheduled future note-off
};

Host.prototype.gate = function (on, cv, vel, slide) {
    var A = this.curA;
    if (on) {
        // (Fractal plays back recorded pitch, which its 11-bit cells keep to a few cents: round it)
        var pitch = this.engine.roundPitch ? clamp(60 + Math.round(cv * 12), 0, 127) : voltsToMidi(cv);
        var v = this.hp.velMode === 1 ? this.hp.fixedVel : clamp(vel | 0 || 100, 1, 127);
        if (this.cur) {
            // gate re-triggered while high
            if (this.cur.pitch === pitch) return;            // tie
            var old = this.cur;
            this.noteOn(pitch, v, A);
            this.noteOff(old, A + 2);
            return;
        }
        if (this.held) {
            var h = this.held;
            this.held = null;
            if (slide && this.hp.legato) {
                if (h.pitch === pitch) { this.cur = h; return; }   // slide to same note = tie
                this.noteOn(pitch, v, A);
                this.noteOff(h, A + 2);
                return;
            }
            this.noteOff(h, h.offA);
        }
        this.noteOn(pitch, v, A);
    } else {
        if (!this.cur) return;
        var n = this.cur;
        this.cur = null;
        n.offA = A;
        if (this.hp.legato) this.held = n;   // wait: the next note may slide out of it
        else this.noteOff(n, A);
    }
};

// close a note-off that waited too long (or was forced)
Host.prototype.flushHeldOff = function (force) {
    if (this.held && (force || this.genA - this.held.offA >= SLIDE_HOLD)) {
        this.noteOff(this.held, this.held.offA);
        this.held = null;
    }
};

// end whatever is sounding right now (used by bar resets)
Host.prototype.endNote = function () {
    if (this.cur) { this.noteOff(this.cur, this.curA); this.cur = null; }
    this.flushHeldOff(true);
};

Host.prototype.noteOn = function (pitch, vel, A) {
    var slot = A >> 1;
    // a previous note of the same pitch must end before this one starts
    if (this.offAt[pitch] !== undefined && this.offAt[pitch] >= slot) {
        this.removeEvent(this.offAt[pitch], pitch, 0);
        this.addEvent(slot, pitch, 0);
    }
    delete this.offAt[pitch];
    this.cur = { pitch: pitch, onSlot: slot, offA: 0 };
    this.addEvent(slot, pitch, vel);
    if (!this.muted) this.queueDisplay(slot, ["play", pitch, vel]);
};

Host.prototype.noteOff = function (n, A) {
    var slot = Math.max(A >> 1, n.onSlot + 1);   // never in the same slot as its note-on
    this.addEvent(slot, n.pitch, 0);
    this.offAt[n.pitch] = slot;
};

// write one MIDI pair into a slot (note-offs are kept in front of note-ons)
Host.prototype.addEvent = function (slot, pitch, vel) {
    if (this.muted) return;
    if (slot <= this.reported + MARGIN) {
        // too close to the playhead for the coll: play it now
        this.out.now([pitch, vel]);
        return;
    }
    var list = this.pending[slot] || [];
    if (vel === 0) list.unshift(pitch, 0); else list.push(pitch, vel);
    this.pending[slot] = list;
    this.out.coll([slot].concat(list));
};

Host.prototype.removeEvent = function (slot, pitch, vel) {
    var list = this.pending[slot];
    if (!list) return;
    for (var i = 0; i < list.length; i += 2) {
        if (list[i] === pitch && list[i + 1] === vel) { list.splice(i, 2); break; }
    }
    if (list.length) this.out.coll([slot].concat(list));
    else { delete this.pending[slot]; this.out.coll(["remove", slot]); }
};

// called by the patch after a slot was played, so memory doesn't grow
Host.prototype.played = function (slot) {
    delete this.pending[slot];
    for (var p in this.offAt) if (this.offAt[p] <= slot) delete this.offAt[p];
};

// ---------------------------------------------------------------------
// Display: engine step info is generated early, shown on time
// ---------------------------------------------------------------------
Host.prototype.stepInfo = function () {
    if (this.muted) return;
    var args = Array.prototype.slice.call(arguments);
    this.queueDisplay(this.curA >> 1, ["step"].concat(args));
};
Host.prototype.queueDisplay = function (slot, list) {
    this.display.push([slot, list]);
};
Host.prototype.showDisplay = function (slot) {
    var i = 0;
    while (i < this.display.length && this.display[i][0] <= slot) {
        this.out.display(this.display[i][1]);
        i++;
    }
    if (i) this.display.splice(0, i);
    this.played(slot - 1);
};

// ---------------------------------------------------------------------
// Parameters
// ---------------------------------------------------------------------
Host.prototype.param = function (name, value) {
    if (name in this.hp) {
        this.hp[name] = value | 0;
        if (name === "resetBars") this.syncMeasure();
        if (name === "legato" && !this.hp.legato) this.flushHeldOff(true);
        return;
    }
    if (this.engine.setParam(name, value) && this.engine.paramChanged) this.engine.paramChanged(name);
};

// keep the engine's own Reset Measure in step with the device setting
Host.prototype.syncMeasure = function () {
    if (!this.engine.ownsResetMeasure) return;
    this.engine.measureTicks = this.barSlots() * 2;
    if (this.engine.setResetBars) this.engine.setResetBars(this.hp.resetBars);
    else this.engine.setParam("resetMeasure", this.hp.resetBars);
};

Host.prototype.midiIn = function (pitch, vel) {
    if (vel <= 0 || this.hp.resetNote === -2) return;
    if (this.hp.resetNote < 0 || this.hp.resetNote === pitch) this.manualReset();
};

// ---------------- max_glue.js ----------------
// =====================================================================
//  MAX GLUE: turns patch messages into Host / engine calls.
//  (the top of the built file sets XF_DEVICE = "algo" | "stochastic" | "note" |
//   "indexed" | "discrete" | "phaseflux" | "fractal", and XF_PARAMS = the
//   scripting names of the device's knobs)
//
//  Inlet messages
//    pos <slot>              clock, one per 1/96 note while Live plays
//    playing <0|1>           Live's transport (from live.observer is_playing)
//    tempo <bpm>             timesig <num> <den>
//    midinote <pitch> <vel>  incoming MIDI (Reset by note; Fractal records it)
//    p <name> <value>        a knob / menu / switch moved
//    act <name>              a button was pressed (reset, reseed, newRhythm ...)
//    edit ...                the pop-out editor changed a step / stage / cell / ticket
//    getdisplay              a display asks for everything again
//    xfstate <list>          stored pattern coming back from the Live set (pattr)
//    loaded                  the device finished loading
//  Outlets
//    0 coll    1 MIDI now (pitch vel)    2 flush (bang)
//    3 display (to both jsui views)      4 state (to pattr)
//    5 notify (set a UI control without it sending back)
// =====================================================================
autowatch = 0;
inlets = 1;
outlets = 5;

// only the device's own engine is in the built file
var ENGINE = eval({ algo: "TuesdayEngine", stochastic: "StochasticEngine", note: "NoteEngine", indexed: "IndexedEngine",
    discrete: "DiscreteEngine", phaseflux: "PhaseFluxEngine", fractal: "FractalEngine" }[XF_DEVICE]);

var host = new Host(ENGINE, {
    coll: function (l) { outlet(0, l); },
    now: function (pair) { outlet(1, pair); },
    flush: function () { outlet(2, "bang"); },
    display: function (l) { outlet(3, l); },
    state: function (l) { outlet(4, l); }
});
var engine = host.engine;
var gotState = false;
var isLoaded = false;
var lastStateText = "";
var editorPage = 0;           // step editors: steps 1-16, 17-32 ...

// ---- engine call-backs -------------------------------------------------
// the engine changed a setting itself (New Rhythm switches Live -> Loop):
// move the knob too, with "set" so it doesn't send the value back
var controls = {};
host.notify = function (name, v) { if (controls[name]) controls[name].message("set", v); };
host.stateChanged = function () { if (isLoaded) saveTask.schedule(300); };
host.cacheChanged = function () { if (isLoaded) cellsTask.schedule(40); };

// collect the pattern (+ device extras) into one list for the Live set
function buildState() {
    var s = engine.getState ? engine.getState() : [];
    var extra = [];
    if (XF_DEVICE === "stochastic") extra = engine.p.degreeTickets.concat(engine.p.durationTickets);
    return [2, s.length].concat(s, extra);
}
var saveTask = new Task(function () {
    var st = buildState();
    lastStateText = st.join(" ");
    outlet(4, st);
});
var cellsTask = new Task(function () { if (engine.displayCells) outlet(3, engine.displayCells()); });
// Fractal: a recorded cell shows up in the editor (a few times a second at most)
var recTask = new Task(function () { sendLayer(); outlet(3, ["rec", engine.recordPos, engine.capturing() ? 1 : 0]); });
host.cellWritten = function () { if (isLoaded) recTask.schedule(60); };

// ---- the step editors: one adapter per device -------------------------------
//  count = steps, fields = [name, min, max], get/set one field, range = the
//  playing window [first, last], on = does a step sound (overview strip)
function stepFieldsOf(table) { return table.map(function (f) { return [f[0], f[1], f[2]]; }); }
var GRID = null;
if (XF_DEVICE === "note") GRID = {
    count: 64, fields: stepFieldsOf(STEP_FIELDS),
    get: function (i, f) { return engine.steps[i][f]; }, set: function (i, f, v) { engine.setStep(i, f, v); },
    range: function () { return [engine.p.firstStep, engine.p.lastStep]; },
    on: function (i) { return engine.steps[i].gate ? 1 : 0; }
};
if (XF_DEVICE === "indexed") GRID = {
    count: INDEXED_MAX_STEPS, fields: stepFieldsOf(INDEXED_FIELDS),
    get: function (i, f) { return engine.steps[i][f]; }, set: function (i, f, v) { engine.setStep(i, f, v); },
    range: function () { return [0, engine.p.activeLength - 1]; },
    on: function (i) { return i < engine.p.activeLength && engine.steps[i].gateLength > 0 ? 1 : 0; }
};
if (XF_DEVICE === "discrete") GRID = {
    count: DM_STAGES, fields: [["threshold", -100, 100], ["direction", 0, 3], ["noteIndex", -63, 63]],
    get: function (i, f) { return engine.stages[i][f]; }, set: function (i, f, v) { engine.setStage(i, f, v); },
    range: function () { return [0, DM_STAGES - 1]; },
    on: function (i) { return engine.stages[i].direction !== DM_OFF ? 1 : 0; }
};
if (XF_DEVICE === "phaseflux") GRID = {
    count: PF_STAGES, fields: stepFieldsOf(PF_FIELDS),
    get: function (i, f) { return engine.stages[i][f]; }, set: function (i, f, v) { engine.setStage(i, f, v); },
    range: function () { return [engine.p.firstStage, engine.p.lastStage]; },
    on: function (i) { var s = engine.stages[i]; return s.skip ? 0 : s.pulseCount; }
};
if (XF_DEVICE === "fractal") GRID = {
    count: FRACTAL_CELLS, fields: [["pitch", -60, 60], ["gate", 0, 15], ["onset", 0, 15]],
    get: function (i, f) {
        if (f === "onset") return engine.onset[i];
        var c = frDecode(engine.trunk[i]);
        if (f === "pitch") return c.valid ? Math.round(c.semi) : 0;
        return c.valid ? c.gateLen : 0;
    },
    set: function (i, f, v) {
        v = v | 0;
        if (f === "onset") engine.onset[i] = clamp(v, 0, 15);
        else {
            var c = frDecode(engine.trunk[i]);
            if (!c.valid) { c.semi = 0; c.gateLen = 8; }
            if (f === "pitch") c.semi = clamp(v, -60, 60); else c.gateLen = clamp(v, 0, 15);
            engine.trunk[i] = frEncode(c.semi, c.gateLen, true);
        }
        host.stateChanged();
    },
    range: function () { return [engine.p.loopFirst, Math.max(engine.p.loopFirst, engine.p.loopLast)]; },
    on: function (i) { var c = frDecode(engine.trunk[i]); return c.valid && c.gateLen > 0 ? 1 : 0; }
};
var editorLayer = GRID ? GRID.fields[0][0] : "";     // which field the editor shows
function gridField(name) {
    if (!GRID) return null;
    for (var i = 0; i < GRID.fields.length; i++) if (GRID.fields[i][0] === name) return GRID.fields[i];
    return null;
}
function gridPages() { return GRID ? Math.ceil(GRID.count / 16) : 1; }

// ---- clock ---------------------------------------------------------------
function pos(slot) { host.pos(slot); }
function playing(v) {
    if (!v) { host.stop(); outlet(3, "stop"); }
}
function tempo(bpm) { host.tempo = bpm > 0 ? bpm : 120; }
function timesig(num, den) {
    if (!(num > 0 && den > 0) || (num === host.timesig[0] && den === host.timesig[1])) return;
    host.timesig = [num, den];
    host.syncMeasure();
    if (host.running) host.relocate(host.reported, host.startOrigin(host.reported));
}
// Fractal records the notes coming in: the gate is "a key is down", the pitch
// the last key pressed (0 V = middle C, like the module's 1 V/oct)
var heldNotes = [];
function midinote(pitch, vel) {
    if (XF_DEVICE === "fractal") {
        var i = heldNotes.indexOf(pitch);
        if (i >= 0) heldNotes.splice(i, 1);
        if (vel > 0) heldNotes.push(pitch);
        engine.input.gate = heldNotes.length > 0;
        if (heldNotes.length) engine.input.cv = Math.fround((heldNotes[heldNotes.length - 1] - 60) / 12);
        if (thru) outlet(1, [pitch, vel]);
    }
    host.midiIn(pitch, vel);
}

// ---- parameters ------------------------------------------------------------
var resetAny = 0, resetPitch = 36;
if (XF_DEVICE !== "algo") host.hp.velMode = 1;   // only Algo makes its own velocities
// knobs shown 1-based (step 1 = the module's step 0)
var ONE_BASED = { firstStep: 1, lastStep: XF_DEVICE === "note", firstStage: 1, lastStage: 1, loopFirst: 1, loopLast: 1,
    recordFirst: 1, recordLast: 1, ornFirst: 1, ornLast: 1 };
// Discrete: knobs in % of the module's +-5 V
var VOLTS = { input: 1, rangeHigh: 1, rangeLow: 1 };
var thru = 0;

function p(name, value) {
    name = String(name);
    // knobs whose screen value differs from the engine value
    if (name === "divisor") value = DIVISORS[clamp(value | 0, 0, DIVISORS.length - 1)].v;
    if (name === "stretch") name = "divisor";          // Indexed: Divisor multiplies every duration
    if (ONE_BASED[name]) value = (value | 0) - 1;
    if (XF_DEVICE === "discrete" && VOLTS[name]) value = value * 0.05;
    if (name === "thru") { thru = value | 0; return; }
    if (name === "rstAny" || name === "rstPitch") {
        if (name === "rstAny") resetAny = value | 0; else resetPitch = value | 0;
        host.param("resetNote", resetAny === 0 ? -2 : (resetAny === 1 ? -1 : resetPitch));   // Off / Any / Pitch
        return;
    }
    host.param(name, value);
    // things the displays draw depend on these
    if (/^(firstStep|lastStep|activeLength|firstStage|lastStage|loopFirst|loopLast|record)$/.test(name)) sendLayer();
    if (XF_DEVICE === "fractal" && (name === "record" || name === "lock")) outlet(3, ["rec", engine.recordPos, engine.capturing() ? 1 : 0]);
    if (XF_DEVICE === "stochastic" && (name === "scale" || name === "scaleRotate")) sendTickets();
}

function act(name) {
    switch (String(name)) {
    case "reset": host.manualReset(); break;
    case "reseed": if (engine.reseed) engine.reseed(); break;
    case "newRhythm": engine.newRhythm(); break;
    case "newMelody": engine.newMelody(); break;
    case "undoRhythm": engine.undoRhythm(); break;
    case "undoMelody": engine.undoMelody(); break;
    case "newBoth": engine.newRhythm(); engine.newMelody(); break;
    case "clearSteps": clearSteps(); host.stateChanged(); sendLayer(); break;
    case "initSteps":
        if (XF_DEVICE === "note") for (var j = 0; j < 64; j++) engine.steps[j] = new NoteStep();
        if (engine.initFresh) engine.initFresh();
        host.stateChanged(); sendLayer();
        break;
    case "clear": if (engine.clearTrunk) { engine.clearTrunk(); sendLayer(); } break;
    }
}
// an empty pattern for each device
function clearSteps() {
    var i;
    if (XF_DEVICE === "note") for (i = 0; i < 64; i++) engine.steps[i] = new NoteStep();
    if (XF_DEVICE === "indexed")
        for (i = 0; i < INDEXED_MAX_STEPS; i++) { var s = engine.steps[i]; s.noteIndex = 0; s.slide = 0; s.groupMask = 0; s.setDuration(48); s.setGateLength(0); }
    if (XF_DEVICE === "discrete") for (i = 0; i < DM_STAGES; i++) { engine.stages[i].direction = DM_OFF; engine.stages[i].noteIndex = 0; }
    if (XF_DEVICE === "phaseflux") for (i = 0; i < PF_STAGES; i++) engine.stages[i].clear();
    if (XF_DEVICE === "fractal") engine.clearTrunk();
}

// ---- the pop-out editor ----------------------------------------------------
//  edit step <index> <value>       (Note: sets the shown layer of that step)
//  edit layer <field>              (Note: show another field)
//  edit page <0..3>
//  edit copypage <from> <to>
//  edit ticket deg|dur <index> <value>   (Stochastic)
function edit() {
    var a = arrayfromargs(arguments);
    var what = String(a[0]);
    if (what === "step" && GRID) {
        var idx = a[1] | 0;
        if (idx >= 0 && idx < GRID.count && gridField(editorLayer)) GRID.set(idx, editorLayer, a[2]);
        sendLayer();
    } else if (what === "layer") {
        if (gridField(String(a[1]))) editorLayer = String(a[1]);
        sendLayer();
    } else if (what === "page") {
        editorPage = clamp(a[1] | 0, 0, gridPages() - 1);
        sendLayer();
    } else if (what === "copypage" && GRID) {
        var from = clamp(a[1] | 0, 0, gridPages() - 1) * 16, to = clamp(a[2] | 0, 0, gridPages() - 1) * 16;
        var copy = [];
        for (var i = 0; i < 16 && from + i < GRID.count; i++)
            copy.push(GRID.fields.map(function (f) { return GRID.get(from + i, f[0]); }));
        for (i = 0; i < copy.length && to + i < GRID.count; i++)
            for (var k = 0; k < GRID.fields.length; k++) GRID.set(to + i, GRID.fields[k][0], copy[i][k]);
        host.stateChanged();
        sendLayer();
    } else if (what === "ticket" && XF_DEVICE === "stochastic") {
        var name = (String(a[1]) === "deg" ? "degreeTicket" : "durationTicket") + (a[2] | 0);
        engine.setParam(name, a[3]);
        host.stateChanged();
        sendTickets();
    }
}

// step editors: "layer <field> <page> <min> <max> <first> <last> <pages> v0 .. v15"
function sendLayer() {
    var f = gridField(editorLayer);
    if (!f) return;
    editorPage = clamp(editorPage, 0, gridPages() - 1);
    var r = GRID.range();
    var out = ["layer", editorLayer, editorPage, f[1], f[2], r[0], r[1], gridPages()];
    for (var i = 0; i < 16; i++) {
        var idx = editorPage * 16 + i;
        out.push(idx < GRID.count ? GRID.get(idx, editorLayer) : 0);
    }
    outlet(3, out);
    // which steps sound, for the overview strip (PhaseFlux: pulses per stage)
    var g = ["gates"];
    for (i = 0; i < GRID.count; i++) g.push(GRID.on(i));
    outlet(3, g);
}
// Stochastic editor: "tickets <notesPerOctave> d0..d31 u0..u7"
function sendTickets() {
    if (XF_DEVICE !== "stochastic") return;
    outlet(3, ["tickets", engine.N()].concat(engine.p.degreeTickets, engine.p.durationTickets));
}

function getdisplay() {
    if (GRID) sendLayer();
    if (XF_DEVICE === "fractal") outlet(3, ["rec", engine.recordPos, engine.capturing() ? 1 : 0]);
    if (XF_DEVICE === "stochastic") { sendTickets(); if (engine.displayCells) outlet(3, engine.displayCells()); }
    if (XF_DEVICE === "algo") outlet(3, ["algo", engine.p.algorithm, ALGO_NAMES[engine.p.algorithm]]);
}

// ---- saving with the Live set ----------------------------------------------
function xfstate() {
    var a = arrayfromargs(arguments);
    if (a.join(" ") === lastStateText) return;      // our own save coming back
    if (a[0] !== 2 || a.length < 2) return;
    var n = a[1] | 0, s = a.slice(2, 2 + n), extra = a.slice(2 + n);
    if (!engine.setState || !engine.setState(s)) return;
    if (XF_DEVICE === "stochastic" && extra.length >= 40) {
        for (var i = 0; i < 32; i++) engine.p.degreeTickets[i] = extra[i] | 0;
        for (i = 0; i < 8; i++) engine.p.durationTickets[i] = extra[32 + i] | 0;
        engine.refreshStepCache();
    }
    gotState = true;
    lastStateText = a.join(" ");
    if (isLoaded) getdisplay();
}

// read every knob once the device is up. Twice, because some limits depend on
// each other (First Step can't pass Last Step) and the order isn't known.
function readControls(patcher) {
    var wanted = {};
    for (var i = 0; i < XF_PARAMS.length; i++) wanted[XF_PARAMS[i]] = true;
    patcher.applydeep(function (obj) {
        if (obj.varname && wanted[obj.varname]) controls[obj.varname] = obj;
    });
    for (var pass = 0; pass < 2; pass++)
        for (var name in controls) {
            var v = controls[name].getvalueof();
            if (typeof v === "number" || (v && v.length !== undefined)) p(name, typeof v === "number" ? v : v[0]);
        }
}

function loaded() {
    if (isLoaded) return;
    readControls(this.patcher);
    isLoaded = true;
    if (!gotState && engine.initFresh) { engine.initFresh(); saveTask.schedule(100); }
    host.syncMeasure();
    getdisplay();
}

// anything else is a mistake in the patch: say so in the Max window
function anything() { post("xformer: unknown message " + messagename + "\n"); }
