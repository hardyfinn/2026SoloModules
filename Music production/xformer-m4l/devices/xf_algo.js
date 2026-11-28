// X|FORMER algo for Max for Live - built file, edit the sources in m4l/src
var XF_DEVICE = "algo";
var XF_PARAMS = ["flow", "ornament", "power", "gateLength", "glide", "trill", "octave", "transpose", "algorithm", "loopLength", "divisor", "scale", "rootNote", "start", "stepTrill", "skew", "rotate", "scaleRotate", "clockMult", "gateOffset", "fixedVel", "resetBars", "maskParameter", "timeMode", "maskProgression", "velMode", "legato", "resetOnStart", "rstAny", "rstPitch"];

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


// ---------------- tuesday.js ----------------
// =====================================================================
//  ALGO ENGINE  (port of engine/TuesdayTrackEngine.cpp)
//  16 generative algorithms driven by Flow / Ornament / Power.
//  Everything below mirrors the firmware line by line; the comments
//  explain what each part does musically.
// =====================================================================

var LOOP_LENGTHS = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    19, 21, 24, 32, 35, 42, 48, 56, 64, 95, 96, 127, 128];
var MASK_VALUES = [2, 3, 5, 11, 19, 31, 43, 61, 89, 131, 197, 277, 409, 599];
var ALGO_NAMES = ["Test", "TriTrance", "Stomper", "Markov", "Chip1", "Chip2", "Wobble",
    "ScaleWlk", "Window", "Minimal", "Ganz", "Blake", "Aphex", "Autech", "StepWave", "Turing"];

// Music-Thing style Turing register (engine/generators/TuringRegister.h)
function TuringRegister() { this.reg = 0xffffffff; this.length = 16; }
TuringRegister.prototype.seed = function (v) { v = u32(v); this.reg = v ? v : 0xffffffff; };
TuringRegister.prototype.setLength = function (rng, len) {
    len = len & 0xff;
    if (len < 3) len = 3; else if (len > 32) len = 32;
    if (len > this.length) this.reg = u32(this.reg | ((rng.next() & 1) << this.length));
    this.length = len;
};
TuringRegister.prototype.clock = function (rng, probability) {
    var sr = this.reg;
    if (probability >= 255 || rng.nextRange(255) < probability) sr = u32(sr ^ 1);
    var msb = u32(1 << (this.length - 1));
    if (sr & 1) sr = u32((sr >>> 1) | msb); else sr = u32((sr >>> 1) & ~msb);
    if (!sr) sr = u32(1 << (this.length - 1));
    this.reg = sr;
    var mask = this.length >= 32 ? 0xffffffff : u32((1 << this.length) - 1);
    return u32(sr & mask);
};

function TuesdayEngine(host) {
    this.host = host;
    // ---- the knobs (same names, ranges and defaults as the firmware sequence) ----
    this.p = {
        algorithm: 1, flow: 4, ornament: 4, power: 12, start: 0, loopLength: 0,
        glide: 50, trill: 50, stepTrill: 0, skew: 0, octave: 0, transpose: 0,
        divisor: 12, clockMult: 100, scale: 1, rootNote: 0, scaleRotate: 0,
        gateLength: 50, gateOffset: 0, maskParameter: 0, timeMode: 0,
        maskProgression: 0, rotate: 0
    };
    this._rng = new Random(0);
    this._extraRng = new Random(0);
    this._uiRng = new Random(0);
    this._turingSR = new TuringRegister();
    this.reset();
}

// allowed range of every knob (the firmware setters clamp to these)
var TUESDAY_RANGES = {
    algorithm: [0, 15], flow: [0, 16], ornament: [0, 16], power: [0, 16], start: [0, 16],
    loopLength: [0, 29], glide: [0, 100], trill: [0, 100], stepTrill: [0, 100], skew: [-8, 8],
    octave: [-10, 10], transpose: [-11, 11], divisor: [1, 768], clockMult: [50, 150],
    scale: [0, 13], rootNote: [0, 11], scaleRotate: [0, 31], gateLength: [0, 100],
    gateOffset: [0, 100], maskParameter: [0, 15], timeMode: [0, 3], maskProgression: [0, 3],
    rotate: [-127, 127]
};

TuesdayEngine.prototype.setParam = function (name, value) {
    if (!(name in this.p)) return false;
    var r = TUESDAY_RANGES[name];
    this.p[name] = clamp(Math.round(value), r[0], r[1]);
    return true;
};

// --- helpers that mirror the TuesdaySequence getters ---
TuesdayEngine.prototype.actualLoopLength = function () {
    var i = this.p.loopLength;
    return (i >= 0 && i < 30) ? LOOP_LENGTHS[i] : 16;
};
TuesdayEngine.prototype.rotateValue = function () {
    var len = this.actualLoopLength();
    if (len > 0) return clamp(this.p.rotate, -(len - 1), len - 1);
    return 0;
};
TuesdayEngine.prototype.effectiveDivisor = function () {
    var clockMult = Math.fround(this.p.clockMult * Math.fround(0.01));
    var d = this.p.divisor * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
    return Math.max(1, Math.round(Math.fround(d / clockMult)));
};

// ---------------------------------------------------------------------
// initAlgorithm: seed the two random generators from Flow (structure) and
// Ornament (detail) and set up the chosen algorithm's starting state.
// ---------------------------------------------------------------------
TuesdayEngine.prototype.initAlgorithm = function () {
    var p = this.p;
    var flow = p.flow, ornament = p.ornament, algorithm = p.algorithm;
    var flowSeed = u32((flow - 1) << 4);
    var ornamentSeed = u32((ornament - 1) << 4);
    var extraSeed = u32(ornamentSeed + 0x9e3779b9);
    var S = this.s;
    var i, j;

    this._uiRng = new Random(flow * 37 + ornament * 101);
    this._cachedFlow = flow;
    this._cachedOrnament = ornament;
    this._cachedAlgorithm = algorithm;
    this._cachedLoopLength = p.loopLength;
    this._cachedPrimeParam = -1;

    switch (algorithm) {
    case 1: // TRITRANCE
        this._rng = new Random(flowSeed);
        this._extraRng = new Random(extraSeed);
        S.b1 = this._rng.next() & 0x7;
        S.b2 = this._rng.next() & 0x7;
        S.b3 = this._extraRng.next() & 0x15;
        if (S.b3 >= 7) S.b3 -= 7; else S.b3 = 0;
        S.b3 -= 4;
        break;
    case 2: // STOMPER
        this._rng = new Random(flowSeed);
        this._extraRng = new Random(extraSeed);
        S.mode = (this._extraRng.next() % 7) * 2;
        S.countDown = 0;
        S.lowNote = this._rng.next() % 3;
        S.lastNote = S.lowNote;
        S.lastOctave = 0;
        S.highNote = [this._rng.next() % 7, this._rng.next() % 5];
        break;
    case 3: // MARKOV
        this._rng = new Random(flowSeed);
        S.history1 = this._rng.next() & 0x7;
        S.history3 = this._rng.next() & 0x7;
        S.matrix = [];
        for (i = 0; i < 8; i++) {
            S.matrix.push([]);
            for (j = 0; j < 8; j++) S.matrix[i].push([this._rng.next() % 8, this._rng.next() % 8]);
        }
        break;
    case 4: // CHIPARP 1
        this._rng = new Random(flowSeed);
        S.chordSeed = this._rng.next();
        S.rngSeed = S.chordSeed;
        S.base = this._rng.next() % 3;
        S.dir = (this._rng.next() >>> 7) % 2;
        break;
    case 5: // CHIPARP 2
        this._rng = new Random(flowSeed);
        S.rngSeed = this._rng.next();
        S.chordScaler = (this._rng.next() % 3) + 2;
        S.offset = this._rng.next() % 5;
        S.len = ((this._rng.next() & 0x3) + 1) * 2;
        S.timeMult = this._rng.nextBinary() ? (this._rng.nextBinary() ? 1 : 0) : 0;
        S.deadTime = 0;
        S.idx = 0;
        S.dir = this._rng.nextBinary() ? (this._rng.nextBinary() ? 1 : 0) : 0;
        S.chordLen = 3 + (flow >> 2);
        break;
    case 6: // WOBBLE
        this._rng = new Random(flowSeed);
        this._extraRng = new Random(extraSeed);
        S.phase = 0; S.phaseSpeed = 0x08000000;
        S.phase2 = 0; S.lastWasHigh = 0; S.phaseSpeed2 = 0x02000000;
        break;
    case 7: // SCALEWALKER
        this._rng = new Random(flowSeed);
        this._extraRng = new Random(extraSeed);
        S.pos = 0;
        break;
    case 8: // WINDOW
        this._rng = new Random(flowSeed);
        this._extraRng = new Random(extraSeed);
        S.slowPhase = u32(this._rng.next() << 16);
        S.fastPhase = u32(this._rng.next() << 16);
        S.noteMemory = this._rng.next() & 0x7;
        S.noteHistory = this._rng.next() & 0x7;
        S.ghostThreshold = this._rng.next() & 0x1f;
        S.phaseRatio = 3 + (this._rng.next() & 0x3);
        break;
    case 9: // MINIMAL
        this._rng = new Random(flowSeed);
        this._extraRng = new Random(extraSeed);
        S.burstLength = 2 + (this._rng.next() % 7);
        S.silenceLength = 4 + (flow % 13);
        S.clickDensity = (ornament * 16) & 0xff;
        S.mode = 0;
        S.silenceTimer = S.silenceLength;
        S.burstTimer = 0;
        S.noteIndex = 0;
        break;
    case 10: // GANZ
        this._rng = new Random(flowSeed);
        this._extraRng = new Random(extraSeed);
        S.phaseA = u32(this._rng.next() << 16);
        S.phaseB = u32(this._rng.next() << 16);
        S.phaseC = u32(this._rng.next() << 16);
        S.noteHistory = [this._rng.next() % 7, this._rng.next() % 7, this._rng.next() % 7];
        S.selectMode = this._rng.next() % 4;
        S.skipDecimator = flow >> 2;
        S.phraseSkipCount = 0;
        S.velocitySample = 128 + (this._extraRng.next() & 0x7F);
        break;
    case 11: // BLAKE
        this._rng = new Random(flowSeed);
        this._extraRng = new Random(extraSeed);
        S.motif = [];
        for (i = 0; i < 4; i++) S.motif.push(this._rng.next() % 7);
        S.breathPhase = u32(this._rng.next() << 16);
        S.breathPattern = (flow >> 2) % 4;
        S.breathCycleLength = 4 + ((ornament >> 2) % 4);
        S.subBassCountdown = 0;
        break;
    case 12: // APHEX
        this._rng = new Random(flowSeed);
        S.track1 = []; S.track2 = []; S.track3 = [];
        for (i = 0; i < 4; ++i) S.track1.push(this._rng.next() % 12);
        for (i = 0; i < 3; ++i) S.track2.push(this._rng.next() % 3);
        for (i = 0; i < 5; ++i) S.track3.push((this._rng.next() % 8 === 0) ? (this._rng.next() % 5) : 0);
        S.pos1 = (ornament * 1) % 4;
        S.pos2 = (ornament * 2) % 3;
        S.pos3 = (ornament * 3) % 5;
        break;
    case 13: // AUTECHRE
        this._rng = new Random(flowSeed);
        // Port note: the firmware draws the rule order from whatever state the
        // Ornament generator was left in. Seeding it here makes the pattern
        // repeat exactly every time Live starts, and lets Ornament pick the rules.
        this._extraRng = new Random(extraSeed);
        this.initAutechrePattern();
        break;
    case 14: // STEPWAVE
        this._rng = new Random(flowSeed);
        this._extraRng = new Random(extraSeed);
        S.direction = 0;
        S.step_count = 3 + (this._rng.next() % 5);
        S.current_step = 0;
        S.chromatic_offset = 0;
        S.is_stepped = true;
        break;
    case 15: // TURING
        this._rng = new Random(flowSeed);
        this._extraRng = new Random(extraSeed);
        this._turingSR.seed(flowSeed);
        this._turingSR.setLength(this._rng, 2 + flow * 2);
        break;
    default: // 0 = TEST
        S.mode = (flow - 1) >> 3;
        S.sweepSpeed = (flow - 1) & 0x3;
        S.accent = (ornament - 1) >> 3;
        S.velocity = ((ornament - 1) << 4) & 0xff;
        S.note = 0;
        this._rng = new Random(flowSeed);
        this._extraRng = new Random(extraSeed);
        break;
    }
};

TuesdayEngine.prototype.initAutechrePattern = function () {
    var S = this.s, i;
    S.pattern = [];
    for (i = 0; i < 8; ++i) {
        var r = this._rng.next() % 4;
        var v = (r === 0) ? 12 : (r === 1 ? 24 : 0);
        if (this._rng.nextBinary()) v += (this._rng.next() % 5) * 2;
        S.pattern.push(v);
    }
    S.rule_timer = 8 + (this.p.flow * 4);
    var tempRng = new Random(this._extraRng.next());
    S.rule_sequence = [];
    for (i = 0; i < 8; ++i) S.rule_sequence.push(tempRng.next() % 5);
    S.rule_index = 0;
};

// ---------------------------------------------------------------------
// reset: back to the start of the phrase (transport start, Reset button)
// ---------------------------------------------------------------------
TuesdayEngine.prototype.reset = function () {
    this._cachedFlow = -1; this._cachedOrnament = -1; this._cachedLoopLength = -1;
    this._stepIndex = 0;
    this._displayStep = -1;
    this._gateTicks = 0;
    this._coolDown = 0; this._coolDownMax = 0;
    this._microCoolDown = 0; this._microCoolDownMax = 0;
    this._gateOffset = 0;
    this._slide = 0;
    this._cvTarget = 0; this._cvCurrent = 0; this._cvDelta = 0; this._slideCountDown = 0;
    this._microGateQueue = new SortedQueue(32);
    this._retriggerArmed = false; this._retriggerCount = 0;
    this._retriggerPeriod = 0; this._retriggerLength = 0;
    this._trillCvTarget = 0; this._ratchetInterval = 0;
    this._gateOutput = false; this._cvOutput = 0;
    this._primeMaskCounter = 0; this._primeMaskState = 1;
    this._cachedPrimeParam = -1; this._cachedTimeMode = -1;
    this._timeModeStartTick = 0; this._currentMaskArrayIndex = 0;
    this._cachedMaskProgression = -1;
    this._lastVelocity = 100;
    this.s = {};
    this.initAlgorithm();
};

TuesdayEngine.prototype.stop = function () {
    this._microGateQueue.clear();
    this._gateOutput = false;
    this._retriggerArmed = false; this._retriggerCount = 0; this._retriggerTimer = 0;
    this._gateTicks = 0;
};

// Reseed: a fresh random variation with the same knob settings
TuesdayEngine.prototype.reseed = function () {
    var S = this.s, p = this.p, i, j;
    this._stepIndex = 0;
    this._coolDown = 0;
    this._rng = new Random(this._rng.next());
    this._extraRng = new Random(this._extraRng.next());
    switch (p.algorithm) {
    case 0: S.note = 0; break;
    case 1:
        S.b1 = this._rng.next() & 0x7; S.b2 = this._rng.next() & 0x7;
        S.b3 = this._extraRng.next() & 0x15;
        if (S.b3 >= 7) S.b3 -= 7; else S.b3 = 0;
        S.b3 -= 4;
        break;
    case 2:
        S.mode = (this._extraRng.next() % 7) * 2; S.countDown = 0;
        S.lowNote = this._rng.next() % 3; S.lastNote = S.lowNote; S.lastOctave = 0;
        S.highNote = [this._rng.next() % 7, this._rng.next() % 5];
        break;
    case 3:
        S.history1 = this._rng.next() & 0x7; S.history3 = this._rng.next() & 0x7;
        for (i = 0; i < 8; i++) for (j = 0; j < 8; j++) S.matrix[i][j] = [this._rng.next() % 8, this._rng.next() % 8];
        break;
    case 4:
        S.chordSeed = this._rng.next(); S.rngSeed = S.chordSeed;
        S.base = this._rng.next() % 3; S.dir = (this._rng.next() >>> 7) % 2;
        break;
    case 5:
        S.rngSeed = this._rng.next(); S.chordScaler = (this._rng.next() % 3) + 2;
        S.offset = this._rng.next() % 5; S.len = ((this._rng.next() & 0x3) + 1) * 2;
        S.timeMult = this._rng.nextBinary() ? (this._rng.nextBinary() ? 1 : 0) : 0;
        S.deadTime = 0; S.idx = 0;
        S.dir = this._rng.nextBinary() ? (this._rng.nextBinary() ? 1 : 0) : 0;
        S.chordLen = 3 + (p.flow >> 2);
        break;
    case 6:
        S.phase = 0; S.phaseSpeed = 0x08000000; S.phase2 = 0; S.lastWasHigh = 0; S.phaseSpeed2 = 0x02000000;
        break;
    case 7: S.pos = 0; break;
    case 8:
        S.slowPhase = u32(this._rng.next() << 16); S.fastPhase = u32(this._rng.next() << 16);
        S.noteMemory = this._rng.next() & 0x7; S.noteHistory = this._rng.next() & 0x7;
        S.ghostThreshold = this._rng.next() & 0x1f; S.phaseRatio = 3 + (this._rng.next() & 0x3);
        break;
    case 9:
        S.burstLength = 2 + (this._rng.next() % 7); S.silenceLength = 4 + (this._cachedFlow % 13);
        S.clickDensity = (this._cachedOrnament * 16) & 0xff; S.mode = 0;
        S.silenceTimer = S.silenceLength; S.burstTimer = 0; S.noteIndex = 0;
        break;
    case 10:
        S.phaseA = u32(this._rng.next() << 16); S.phaseB = u32(this._rng.next() << 16); S.phaseC = u32(this._rng.next() << 16);
        S.noteHistory = [this._rng.next() % 7, this._rng.next() % 7, this._rng.next() % 7];
        S.selectMode = this._rng.next() % 4; S.skipDecimator = this._cachedFlow >> 2;
        S.phraseSkipCount = 0; S.velocitySample = 128 + (this._extraRng.next() & 0x7F);
        break;
    case 11:
        for (i = 0; i < 4; i++) S.motif[i] = this._rng.next() % 7;
        S.breathPhase = u32(this._rng.next() << 16); S.breathPattern = (this._cachedFlow >> 2) % 4;
        S.breathCycleLength = 4 + ((this._cachedOrnament >> 2) % 4); S.subBassCountdown = 0;
        break;
    case 12:
        for (i = 0; i < 4; ++i) S.track1[i] = this._rng.next() % 12;
        for (i = 0; i < 3; ++i) S.track2[i] = this._rng.next() % 3;
        for (i = 0; i < 5; ++i) S.track3[i] = (this._rng.next() % 8 === 0) ? (this._rng.next() % 5) : 0;
        break;
    case 13: this.initAutechrePattern(); break;
    case 14:
        S.direction = 0; S.step_count = 3 + (this._rng.next() % 5);
        S.current_step = 0; S.chromatic_offset = 0; S.is_stepped = true;
        break;
    case 15: this._turingSR.seed(this._rng.next()); break;
    }
};

// Shared per-step timing context (calculateContext)
TuesdayEngine.prototype.calculateContext = function () {
    var p = this.p;
    var ctx = {};
    ctx.divisor = this.effectiveDivisor();
    ctx.stepsPerBeat = ctx.divisor > 0 ? Math.floor(CONFIG_PPQN / ctx.divisor) : 0;
    ctx.isBeatStart = ctx.stepsPerBeat > 0 && (this._stepIndex % ctx.stepsPerBeat) === 0;
    var stepTicks = ctx.divisor;
    ctx.tpb = Math.max(1, Math.floor((192 + Math.floor(stepTicks / 2)) / stepTicks));
    ctx.loopLength = this.actualLoopLength();
    ctx.effectiveLoopLength = ctx.loopLength > 0 ? ctx.loopLength : 32;
    var rotate = ctx.loopLength > 0 ? this.rotateValue() : 0;
    ctx.rotatedStep = ctx.loopLength > 0 ? ((this._stepIndex + rotate + ctx.loopLength) % ctx.loopLength) : this._stepIndex;
    ctx.ornament = p.ornament;
    ctx.subdivisions = 4;
    if (ctx.ornament >= 5 && ctx.ornament <= 8) ctx.subdivisions = 3;
    else if (ctx.ornament >= 9 && ctx.ornament <= 12) ctx.subdivisions = 5;
    else if (ctx.ornament >= 13) ctx.subdivisions = 7;
    return ctx;
};

function newResult() {
    return { note: 0, octave: 0, velocity: 0, accent: false, slide: false, gateRatio: 75,
        gateOffset: 0, trillCount: 1, beatSpread: 0, polyCount: 0,
        noteOffsets: [0, 0, 0, 0, 0, 0, 0, 0], isSpatial: true };
}

// ---------------------------------------------------------------------
// The 16 algorithms. Each returns one abstract step: scale degree,
// octave, velocity, accent, slide, gate ratio, timing offset, trills.
// ---------------------------------------------------------------------
TuesdayEngine.prototype.generateTest = function (ctx) {
    var S = this.s, r = newResult();
    r.velocity = 255; r.gateRatio = 75;
    if (this._rng.nextRange(100) < this.p.glide) r.slide = true;
    if (S.mode === 0) { r.octave = this._stepIndex % 5; r.note = 0; }
    else { r.octave = 0; r.note = S.note; S.note = (S.note + 1) % 12; }
    r.velocity = S.velocity;
    return r;
};

TuesdayEngine.prototype.generateTritrance = function (ctx) {
    var S = this.s, r = newResult(), rng = this._rng, x = this._extraRng;
    r.velocity = 255;
    var gateLenRnd = rng.nextRange(100);
    if (gateLenRnd < 40) r.gateRatio = 50 + rng.nextRange(4) * 12;
    else if (gateLenRnd < 70) r.gateRatio = 100 + rng.nextRange(4) * 25;
    else r.gateRatio = 200 + rng.nextRange(9) * 25;
    if (rng.nextRange(100) < this.p.glide) r.slide = true;
    var phase = (this._stepIndex + S.b2) % 3;
    switch (phase) {
    case 0:
        if (x.nextBinary() && x.nextBinary()) {
            S.b3 = x.next() & 0x15;
            if (S.b3 >= 7) S.b3 -= 7; else S.b3 = 0;
            S.b3 -= 4;
        }
        r.octave = 0; r.note = S.b3 + 4;
        r.gateOffset = clamp(10 - rng.nextRange(10), 0, 100);
        break;
    case 1:
        r.octave = 1; r.note = S.b3 + 4;
        if (rng.nextBinary()) S.b2 = rng.next() & 0x7;
        r.gateOffset = clamp(25 + rng.nextRange(10), 0, 100);
        break;
    case 2:
        r.octave = 2; r.note = S.b1; r.velocity = 255; r.accent = true;
        if (rng.nextBinary()) S.b1 = (rng.next() >>> 5) & 0x7;
        r.gateOffset = clamp(40 + rng.nextRange(10), 0, 100);
        r.trillCount = 3;
        r.noteOffsets[0] = -2; r.noteOffsets[1] = -1; r.noteOffsets[2] = 0;
        break;
    }
    if (!r.accent) r.velocity = Math.floor(rng.nextRange(256) / 2);
    return r;
};

TuesdayEngine.prototype.generateMarkov = function (ctx) {
    var S = this.s, r = newResult(), rng = this._rng;
    r.velocity = 255; r.gateRatio = 75;
    var idx = rng.nextBinary() ? 1 : 0;
    var newNote = S.matrix[S.history1][S.history3][idx];
    S.history1 = S.history3; S.history3 = newNote;
    r.note = newNote;
    r.octave = rng.nextBinary() ? 1 : 0;
    if (rng.nextRange(100) < 50) r.gateRatio = 100 + rng.nextRange(4) * 25; else r.gateRatio = 75;
    if (rng.nextBinary() && rng.nextBinary()) r.slide = true;
    r.accent = true;
    r.velocity = Math.floor(rng.nextRange(256) / 2) + 40;
    if (rng.nextBinary()) r.gateOffset = clamp(10 - rng.nextRange(10), 0, 100);
    else r.gateOffset = clamp(15 + rng.nextRange(10), 0, 100);
    if (rng.nextRange(256) < 20) {
        r.trillCount = 2; r.noteOffsets[0] = 0; r.noteOffsets[1] = 0; r.gateRatio = 50;
    }
    return r;
};

TuesdayEngine.prototype.generateWobble = function (ctx) {
    var S = this.s, r = newResult(), rng = this._rng;
    r.velocity = 255;
    S.phaseSpeed = Math.floor(0xFFFFFFFF / Math.max(1, ctx.effectiveLoopLength));
    S.phaseSpeed2 = Math.floor(0xCFFFFFFF / Math.max(1, Math.floor(ctx.effectiveLoopLength / 4)));
    S.phase = u32(S.phase + S.phaseSpeed);
    S.phase2 = u32(S.phase2 + S.phaseSpeed2);
    var rawPhase;
    if (rng.nextRange(256) >= this.p.ornament * 16) {
        rawPhase = (S.phase2 >>> 27) & 0x1F;
        r.note = rawPhase % 7;
        r.octave = 1 + Math.floor(rawPhase / 7);
        if (S.lastWasHigh === 0) { if (rng.nextRange(256) >= 200) r.slide = true; }
        S.lastWasHigh = 1;
    } else {
        rawPhase = (S.phase >>> 27) & 0x1F;
        r.note = rawPhase % 7;
        r.octave = Math.floor(rawPhase / 7);
        if (S.lastWasHigh === 1) { if (rng.nextRange(256) >= 200) r.slide = true; }
        S.lastWasHigh = 0;
    }
    r.velocity = Math.floor(this._extraRng.nextRange(256) / 4);
    if (rng.nextRange(256) >= 50) r.accent = true;
    r.gateOffset = clamp((u32(S.phase + S.phase2) >>> 28) % 30, 0, 100);
    return r;
};

TuesdayEngine.prototype.generateStomper = function (ctx) {
    var S = this.s, r = newResult(), rng = this._rng, x = this._extraRng;
    r.velocity = 255; r.gateRatio = 75;
    var accentoffs = 0, veloffset = 0;
    if (S.countDown > 0) {
        r.gateRatio = S.countDown * 25;
        r.velocity = 0;
        S.countDown--;
        r.note = S.lastNote; r.octave = S.lastOctave;
    } else {
        if (S.mode >= 14) S.mode = (x.next() % 7) * 2;
        S.lowNote = rng.next() % 3;
        S.highNote[0] = rng.next() % 7;
        S.highNote[1] = rng.next() % 5;
        veloffset = 100;
        var maxticklen = 2;
        switch (S.mode) {
        case 10:
            r.octave = 1; r.note = S.highNote[rng.next() % 2]; S.mode++; break;
        case 11:
            r.octave = 0; r.note = S.lowNote; r.slide = true;
            if (x.nextBinary()) S.countDown = x.next() % maxticklen;
            S.mode = 14; break;
        case 12:
            r.octave = 0; r.note = S.lowNote; S.mode++; break;
        case 13:
            r.octave = 1; r.note = S.highNote[rng.next() % 2]; r.slide = true;
            if (x.nextBinary()) S.countDown = x.next() % maxticklen;
            S.mode = 14; break;
        case 4: case 5:
            accentoffs = 100; r.octave = 0; r.note = S.lowNote;
            if (x.nextBinary()) S.countDown = x.next() % maxticklen;
            S.mode = 14; break;
        case 0: case 1:
            r.octave = S.lastOctave; r.note = S.lastNote; veloffset = 0;
            if (x.nextBinary()) S.countDown = x.next() % maxticklen;
            S.mode = 14; break;
        case 6: case 7: case 8: case 9: case 2: case 3:
            if (S.mode === 6 || S.mode === 7 || S.mode === 9 || S.mode === 2) accentoffs = 100;
            if (S.mode === 8 || S.mode === 3) {
                r.octave = 1; r.note = S.highNote[rng.next() % 2];
                if (S.mode === 8) S.mode++; else if (x.nextBinary()) S.countDown = x.next() % maxticklen;
                if (S.mode === 3) S.mode = 14;
            } else {
                r.octave = 0; r.note = S.lowNote;
                if (x.nextBinary()) S.countDown = x.next() % maxticklen;
                S.mode = 14;
            }
            break;
        }
        S.lastNote = r.note; S.lastOctave = r.octave;
        r.velocity = Math.floor(x.nextRange(256) / 4) + veloffset;
        if (rng.nextRange(256) >= 50 + accentoffs) r.accent = true;
        r.beatSpread = 18;
    }
    return r;
};

TuesdayEngine.prototype.generateChipArp1 = function (ctx) {
    var S = this.s, r = newResult(), rng = this._rng;
    r.velocity = 255; r.gateRatio = 75;
    var chordpos = this._stepIndex % ctx.tpb;
    if (chordpos === 0) {
        r.trillCount = 3; r.noteOffsets[0] = 0; r.noteOffsets[1] = 2; r.noteOffsets[2] = 4;
    }
    if (chordpos === 0) {
        r.accent = true;
        S.rngSeed = S.chordSeed;
        if (rng.nextRange(256) >= 0x20) S.base = rng.next() % 3;
        if (rng.nextRange(256) >= 0xF0) S.dir = (rng.next() >>> 7) % 2;
    }
    if (S.dir === 1) chordpos = ctx.tpb - chordpos - 1;
    var chipRng = new Random(S.rngSeed);
    if (chipRng.nextRange(256) >= 0x20) r.accent = true;
    if (chipRng.nextRange(256) >= 0x80) r.slide = true;
    if (chipRng.nextRange(256) >= 0xD0) {
        r.velocity = 0;
    } else {
        r.note = chordpos * 2 + S.base;
        r.octave = 0;
        var rnd = (chipRng.next() >>> 7) % 3;
        r.gateRatio = (4 + 6 * rnd) * 16;
    }
    var extraVel = ctx.rotatedStep === 0 ? 127 : 0;
    r.velocity = Math.floor(rng.nextRange(256) / 2) + extraVel;
    r.gateOffset = clamp(this._stepIndex % 7, 0, 100);
    return r;
};

TuesdayEngine.prototype.generateChipArp2 = function (ctx) {
    var S = this.s, r = newResult(), rng = this._rng, i;
    r.velocity = 255; r.gateRatio = 75; r.octave = 0;
    if (S.deadTime > 0) {
        S.deadTime--;
        r.velocity = 0;
    } else {
        var deadTimeAdd = 0;
        if (S.idx === S.chordLen) {
            S.idx = 0;
            S.len = (S.len - 1) & 0xff;
            r.accent = true;
            r.trillCount = 4;
            for (i = 0; i < 4; i++) r.noteOffsets[i] = i * S.chordScaler;
            if (rng.nextRange(256) >= 200) deadTimeAdd = 1 + (rng.next() % 3);
            if (S.len === 0) {
                S.rngSeed = rng.next();
                S.chordScaler = (rng.next() % 3) + 2;
                S.offset = rng.next() % 5;
                S.len = ((rng.next() & 0x3) + 1) * 2;
                if (rng.nextBinary()) S.dir = rng.nextBinary() ? 1 : 0;
            }
        }
        var scaleidx = S.idx % S.chordLen;
        if (S.dir) scaleidx = S.chordLen - scaleidx - 1;
        r.note = scaleidx * S.chordScaler + S.offset;
        S.idx = (S.idx + 1) & 0xff;
        S.deadTime = S.timeMult;
        if (S.deadTime > 0) r.gateRatio = (1 + S.timeMult) * 100;
        S.deadTime = (S.deadTime + deadTimeAdd) & 0xff;
    }
    var chip2Rng = new Random(S.rngSeed);
    r.velocity = chip2Rng.nextRange(256);
    return r;
};

TuesdayEngine.prototype.generateAphex = function (ctx) {
    var S = this.s, r = newResult();
    r.velocity = 255;
    var modifierStep = this._stepIndex;
    if (ctx.subdivisions !== 4) modifierStep = Math.floor((this._stepIndex * ctx.subdivisions) / 4);
    var pos1 = ctx.rotatedStep % 4;
    var pos2 = modifierStep % 3;
    var pos3 = ctx.rotatedStep % 5;
    r.note = S.track1[pos1]; r.octave = 0; r.gateRatio = 75; r.velocity = 180;
    var modifier = S.track2[pos2];
    if (modifier === 1) { r.gateRatio = 25; r.velocity = 80; r.beatSpread = 60; }
    else if (modifier === 2) { r.gateRatio = 100; r.slide = true; }
    var bass = S.track3[pos3];
    if (bass > 0) { r.note = bass; r.octave = -1; r.gateRatio = 90; r.velocity = 255; r.accent = true; }
    var polyCollision = ((pos1 * 7) ^ (pos2 * 5) ^ (pos3 * 3)) % 12;
    if (polyCollision > 9) { r.beatSpread = 100; r.velocity = 255; }
    var polyRhythmFactor = (pos1 + pos2 + pos3) % 12;
    r.gateOffset = clamp(polyRhythmFactor * 8, 0, 100);
    return r;
};

// int8 cast for the Autechre pattern (values stay small, kept for safety)
function i8(v) { v = v & 0xff; return v > 127 ? v - 256 : v; }
function cmod(a, b) { return a % b; }   // C remainder (sign follows a), same as JS %

TuesdayEngine.prototype.generateAutechre = function (ctx) {
    var S = this.s, r = newResult(), i, t;
    r.velocity = 255;
    var flow = this.p.flow;
    if (ctx.isBeatStart && ctx.subdivisions !== 4) {
        r.beatSpread = 20 + flow * 5;
        r.polyCount = ctx.subdivisions;
        r.isSpatial = true;
        for (i = 0; i < ctx.subdivisions && i < 8; i++) {
            var patternIdx = (ctx.rotatedStep + i) % 8;
            var patternNote = cmod(S.pattern[patternIdx], 12);
            var baseNote = cmod(S.pattern[ctx.rotatedStep % 8], 12);
            r.noteOffsets[i] = i8(patternNote - baseNote);
        }
    }
    var patternVal = S.pattern[ctx.rotatedStep % 8];
    r.note = cmod(patternVal, 12);
    r.octave = Math.trunc(patternVal / 12);
    r.gateOffset = clamp(this._rng.nextRange(11), 0, 100);
    S.rule_timer--;
    r.velocity = 160;
    r.gateRatio = 75;
    if (S.rule_timer <= 0) {
        var rule = S.rule_sequence[S.rule_index];
        var intensity = Math.floor(this.p.power / 2);
        if (r.velocity > 0) { r.velocity = 255; r.accent = true; }
        var P = S.pattern;
        if (rule === 0) { t = P[7]; for (i = 7; i > 0; --i) P[i] = P[i - 1]; P[0] = t; }
        else if (rule === 1) { for (i = 0; i < 4; ++i) { t = P[i]; P[i] = P[7 - i]; P[7 - i] = t; } }
        else if (rule === 2) { for (i = 0; i < 8; ++i) { var o = Math.trunc(P[i] / 12); var n = cmod(P[i], 12); n = cmod(6 - (n - 6) + 12, 12); P[i] = i8(n + o * 12); } }
        else if (rule === 3) { for (i = 0; i < 8; i += 2) { t = P[i]; P[i] = P[i + 1]; P[i + 1] = t; } }
        else if (rule === 4) { for (i = 0; i < 8; ++i) { var o2 = Math.trunc(P[i] / 12); var n2 = cmod(cmod(P[i], 12) + intensity, 12); P[i] = i8(n2 + o2 * 12); } }
        S.rule_timer = 8 + this._cachedFlow * 4;
        S.rule_index = (S.rule_index + 1) % 8;
    }
    return r;
};

TuesdayEngine.prototype.generateStepwave = function (ctx) {
    var S = this.s, r = newResult(), rng = this._rng, i;
    r.velocity = 255;
    var flow = this.p.flow, ornament = this.p.ornament, dp;
    if (ctx.isBeatStart && ctx.subdivisions !== 4) {
        r.beatSpread = 20 + flow * 5;
        r.polyCount = ctx.subdivisions;
        r.isSpatial = ornament >= 9;
        dp = clamp((16 - flow) * 6, 10, 90);
        S.direction = rng.nextRange(100) < dp ? -1 : 1;
        S.step_count = ctx.subdivisions;
        for (i = 0; i < ctx.subdivisions && i < 8; i++) r.noteOffsets[i] = i * S.direction;
        r.slide = true;
    }
    dp = clamp((16 - flow) * 6, 10, 90);
    var dir = rng.nextRange(100) < dp ? -1 : 1;
    var stepSize = 2 + (this._stepIndex % 2);
    r.note = (this._stepIndex * dir * stepSize) % 7;
    if (r.note < 0) r.note += 7;
    r.velocity = 200;
    if (rng.nextRange(100) < 20 + flow * 3) { r.octave = 2; r.velocity = 255; r.accent = true; }
    else r.octave = 0;
    if (rng.nextRange(100) < this.p.glide) { r.slide = true; r.gateRatio = 100; }
    else r.gateRatio = 75;
    r.gateOffset = clamp(rng.nextRange(7), 0, 100);
    return r;
};

TuesdayEngine.prototype.generateScalewalker = function (ctx) {
    var S = this.s, r = newResult(), p = this.p, i;
    r.velocity = 255;
    var direction = (p.flow <= 7) ? -1 : 1;
    r.note = S.pos; r.octave = 0;
    r.velocity = (100 + p.power * 10) & 0xff;
    r.gateRatio = 75;
    var shouldFirePolyGate = false;
    if (ctx.subdivisions !== 4) {
        var beatPosition = this._stepIndex % ctx.stepsPerBeat;
        var gateIndex = Math.floor((beatPosition * ctx.subdivisions) / ctx.stepsPerBeat);
        var nextGateIndex = Math.floor(((beatPosition + 1) * ctx.subdivisions) / ctx.stepsPerBeat);
        if (gateIndex !== nextGateIndex) {
            shouldFirePolyGate = true;
            r.polyCount = 1; r.beatSpread = 100; r.isSpatial = true; r.slide = true;
            r.noteOffsets[0] = gateIndex * direction;
        }
    }
    var stepTrillCount = 1;
    if (p.stepTrill > 0) stepTrillCount = clamp(1 + Math.floor((p.stepTrill * 3) / 100), 1, 4);
    if (shouldFirePolyGate) {
        if (stepTrillCount > 1) {
            var baseOffset = r.noteOffsets[0];
            r.polyCount = stepTrillCount; r.beatSpread = 0; r.isSpatial = false;
            for (i = 0; i < stepTrillCount && i < 8; i++) r.noteOffsets[i] = baseOffset + i * direction;
            r.slide = p.glide > 50;
        }
    } else if (stepTrillCount > 1) {
        r.polyCount = stepTrillCount; r.beatSpread = 0; r.isSpatial = false;
        for (i = 0; i < stepTrillCount && i < 8; i++) r.noteOffsets[i] = i * direction;
        r.slide = p.glide > 50;
    } else if (ctx.subdivisions === 4) {
        r.polyCount = 0; r.beatSpread = 0; r.isSpatial = false; r.noteOffsets[0] = 0;
    } else {
        r.polyCount = 0; r.velocity = 0;
    }
    S.pos = (S.pos + direction + 7) % 7;
    return r;
};

TuesdayEngine.prototype.generateMinimal = function (ctx) {
    var S = this.s, r = newResult(), p = this.p, x = this._extraRng;
    r.velocity = 0;
    if (S.mode === 0) {
        if (S.silenceTimer > 0) S.silenceTimer--;
        if (S.silenceTimer === 0) {
            S.mode = 1; S.burstTimer = S.burstLength;
            S.burstLength = 2 + (this._rng.next() % 7);
        }
    } else {
        if (S.burstTimer > 0) S.burstTimer--;
        if (S.burstTimer === 0) {
            S.mode = 0; S.silenceTimer = S.silenceLength;
            S.silenceLength = 4 + (p.flow % 13);
        }
    }
    if (S.mode === 0) {
        r.velocity = 0; r.note = 0; r.octave = 0; r.gateRatio = 25;
    } else {
        var shouldPlay = true;
        if (S.clickDensity > 0) shouldPlay = (this._rng.next() & 0xFF) < S.clickDensity;
        if (shouldPlay) {
            r.note = S.noteIndex % 7;
            r.octave = Math.floor(S.noteIndex / 7) % 3;
            r.velocity = 128 + (x.next() & 0x7F);
            r.gateRatio = clamp(25 + Math.floor(p.gateLength / 4), 25, 50);
            var glideProb = Math.floor(p.glide / 5);
            var trillProb = Math.floor(p.stepTrill / 10);
            r.slide = (x.next() % 100) < glideProb;
            if ((x.next() % 100) < trillProb) r.trillCount = 2 + (x.next() % 3);
            else r.trillCount = 1;
            r.gateOffset = x.next() % 6;
            S.noteIndex = (S.noteIndex + 1) % 21;
        } else {
            r.velocity = 0; r.note = 0; r.octave = 0; r.gateRatio = 25;
        }
    }
    return r;
};

TuesdayEngine.prototype.generateWindow = function (ctx) {
    var S = this.s, r = newResult(), x = this._extraRng;
    r.velocity = 0;
    var slowIncrement = ctx.effectiveLoopLength > 0 ? Math.floor(0xFFFFFFFF / ctx.effectiveLoopLength) : 0x08000000;
    var fastDivisor = Math.max(1, Math.floor(ctx.effectiveLoopLength / S.phaseRatio));
    var fastIncrement = ctx.effectiveLoopLength > 0 ? Math.floor(0xFFFFFFFF / fastDivisor) : 0x20000000;
    var oldSlowPhase = S.slowPhase;
    S.slowPhase = u32(S.slowPhase + slowIncrement);
    S.fastPhase = u32(S.fastPhase + fastIncrement);
    if (this._stepIndex % 96 === 0 && this._stepIndex > 0) S.phaseRatio = 3 + (this._rng.next() & 0x3);
    if (this._stepIndex % 64 === 0 && this._stepIndex > 0) S.ghostThreshold = this._rng.next() & 0x1f;
    var isAccent = S.slowPhase < oldSlowPhase;
    var isGhost = ((S.fastPhase >>> 27) & 0x1F) > S.ghostThreshold;
    var voiceA = (S.fastPhase >>> 28) & 1;
    if (isAccent) {
        r.note = S.noteMemory;
        r.octave = 2 + (x.next() % 2);
        r.velocity = 100 + (x.next() & 0x3F);
        r.gateRatio = 150 + (x.next() % 26);
        r.slide = true;
    } else if (isGhost) {
        r.note = (S.fastPhase >>> 25) & 0x7;
        r.octave = 0;
        r.velocity = 5 + (x.next() & 0x1F);
        r.gateRatio = 50 + (x.next() % 26);
        r.slide = false;
    } else if (voiceA) {
        var markovRng = new Random(u32(x.next() + S.noteHistory + S.noteMemory));
        var step = (markovRng.next() % 3) - 1;
        var newNote = (S.noteMemory + step + 7) % 7;
        S.noteHistory = S.noteMemory; S.noteMemory = newNote;
        r.note = newNote; r.octave = 0;
        r.velocity = 40 + (x.next() % 86);
        r.gateRatio = 75;
        r.slide = (x.next() % 100) < 25;
    } else {
        r.note = (S.fastPhase >>> 25) & 0x7;
        r.octave = 0;
        r.velocity = 40 + (x.next() % 86);
        r.gateRatio = 75;
        r.slide = false;
    }
    return r;
};

TuesdayEngine.prototype.generateBlake = function (ctx) {
    var S = this.s, r = newResult(), x = this._extraRng;
    r.velocity = 0;
    var breathIncrement = ctx.effectiveLoopLength > 0 ? Math.floor(0xFFFFFFFF / (ctx.effectiveLoopLength * 8)) : 0x02000000;
    S.breathPhase = u32(S.breathPhase + breathIncrement);
    if (this._stepIndex % 96 === 0 && this._stepIndex > 0) S.breathPattern = this._rng.next() % 4;
    if (this._stepIndex % 64 === 0 && this._stepIndex > 0) S.breathCycleLength = 4 + (this._rng.next() % 4);
    var breathPos = this._stepIndex % S.breathCycleLength;
    var REAL = 0, GHOST = 1, WHISPER = 2, art = REAL;
    switch (S.breathPattern) {
    case 0: art = breathPos === 3 ? GHOST : REAL; break;
    case 1: art = breathPos === 1 ? WHISPER : (breathPos === 3 ? GHOST : REAL); break;
    case 2: art = breathPos === 0 ? REAL : (breathPos === 2 ? WHISPER : GHOST); break;
    case 3: art = breathPos < 2 ? REAL : (breathPos === 2 ? GHOST : WHISPER); break;
    }
    r.note = S.motif[this._stepIndex % 4];
    r.octave = 0;
    if (S.subBassCountdown > 0) {
        S.subBassCountdown--;
        r.octave = -1; r.note = 0; r.velocity = 255; r.accent = true; r.gateRatio = 100;
        r.slide = S.subBassCountdown === 0;
        return r;
    }
    if (ctx.isBeatStart) {
        var dropChance = this.p.power * 6;
        if ((this._rng.next() % 100) < dropChance) S.subBassCountdown = 2 + (this._rng.next() % 3);
    }
    switch (art) {
    case REAL:
        var breathBias = (S.breathPhase >>> 24) & 0xFF;
        r.velocity = 128 + Math.floor(breathBias / 2);
        r.gateRatio = 75;
        if ((x.next() % 100) < 25) r.octave = 1;
        break;
    case GHOST:
        r.octave = 2; r.velocity = 20 + (x.next() & 0x0F); r.gateRatio = 40;
        break;
    case WHISPER:
        r.velocity = 0; r.gateRatio = 25;
        break;
    }
    r.gateOffset = x.next() % 5;
    return r;
};

TuesdayEngine.prototype.generateGanz = function (ctx) {
    var S = this.s, r = newResult(), x = this._extraRng;
    r.velocity = 0;
    var incrementA = ctx.effectiveLoopLength > 0 ? Math.floor(0xFFFFFFFF / ctx.effectiveLoopLength) : 0x08000000;
    var oldPhaseA = S.phaseA;
    S.phaseA = u32(S.phaseA + incrementA);
    S.phaseB = u32(S.phaseB + u32(Math.imul(incrementA, 5)));
    S.phaseC = u32(S.phaseC + u32(Math.imul(incrementA, 7)));
    if (this._stepIndex % 96 === 0 && this._stepIndex > 0) S.selectMode = this._rng.next() % 4;
    var tupletPos = (S.phaseB >>> 27) % 5;
    if (!(tupletPos <= 2)) { r.velocity = 0; r.gateRatio = 25; return r; }
    if (S.phraseSkipCount > 0) { S.phraseSkipCount--; r.velocity = 0; return r; }
    if ((S.phaseC >>> 28) < S.skipDecimator * 2) {
        S.phraseSkipCount = 1 + (this._rng.next() % 4);
        r.velocity = 0;
        return r;
    }
    var selectedNote = 0;
    switch (S.selectMode) {
    case 0: selectedNote = (S.noteHistory[0] + ((x.next() % 3) - 1) + 7) % 7; break;
    case 1: selectedNote = (S.phaseA >>> 25) % 7; break;
    case 2: selectedNote = S.noteHistory[this._stepIndex % 3]; break;
    case 3: selectedNote = (((S.phaseA >>> 24) ^ (S.phaseC >>> 24)) & 0xFF) % 7; break;
    }
    S.noteHistory[2] = S.noteHistory[1];
    S.noteHistory[1] = S.noteHistory[0];
    S.noteHistory[0] = selectedNote;
    r.note = selectedNote;
    r.octave = (S.phaseC >>> 29) % 3;
    if (this._stepIndex % 8 === 0) S.velocitySample = 64 + (x.next() & 0xBF);
    r.velocity = S.velocitySample;
    if (S.phaseA < oldPhaseA) {
        r.accent = true; r.velocity = 255; r.gateRatio = 125; r.slide = true;
    } else {
        r.gateRatio = 60 + tupletPos * 15;
        r.slide = (x.next() % 100) < this.p.ornament * 6;
    }
    r.gateOffset = ((S.phaseB >>> 26) & 0x1F) % 20;
    return r;
};

TuesdayEngine.prototype.generateTuring = function (ctx) {
    var p = this.p, r = newResult();
    r.velocity = 255;
    r.gateRatio = clamp(50 + p.gateLength * 3, 50, 400);
    if (this._rng.nextRange(100) < p.glide) r.slide = true;
    var prob = Math.floor((p.power * 255) / 16) & 0xff;
    var v = this._turingSR.clock(this._rng, prob);
    var range = 1 + p.ornament;
    var idx = v % range;
    r.note = idx % 12;
    r.octave = Math.floor(idx / 12);
    return r;
};

TuesdayEngine.prototype.generateStep = function () {
    var ctx = this.calculateContext();
    switch (this.p.algorithm) {
    case 1: return this.generateTritrance(ctx);
    case 2: return this.generateStomper(ctx);
    case 3: return this.generateMarkov(ctx);
    case 4: return this.generateChipArp1(ctx);
    case 5: return this.generateChipArp2(ctx);
    case 6: return this.generateWobble(ctx);
    case 7: return this.generateScalewalker(ctx);
    case 8: return this.generateWindow(ctx);
    case 9: return this.generateMinimal(ctx);
    case 10: return this.generateGanz(ctx);
    case 11: return this.generateBlake(ctx);
    case 12: return this.generateAphex(ctx);
    case 13: return this.generateAutechre(ctx);
    case 14: return this.generateStepwave(ctx);
    case 15: return this.generateTuring(ctx);
    default: return this.generateTest(ctx);
    }
};

// Scale degree + octave -> 1V/oct (scaleToVolts)
TuesdayEngine.prototype.scaleToVolts = function (noteIndex, octave) {
    var p = this.p;
    var scale = new ScaleView(p.scale, p.scaleRotate);
    var totalDegree = noteIndex + octave * scale.notesPerOctave() + p.transpose;
    return scale.noteToVolts(totalDegree) + p.rootNote / 12 + p.octave;
};

// MIDI velocity from the algorithm's 0-255 velocity (accents play full)
function tuesdayVelocity(v, accent) {
    if (accent) return 127;
    return clamp(Math.round((v & 0xff) * 127 / 255), 1, 127);
}

// ---------------------------------------------------------------------
// tick: called 192 times per quarter note. Handles start delay, masks,
// finite loops, the micro-gate queue, then generates a step on every
// divisor boundary and shapes it (power/density, gate length, trills).
// ---------------------------------------------------------------------
TuesdayEngine.prototype.tick = function (tick) {
    var p = this.p, host = this.host, i;

    if (this._cachedAlgorithm !== p.algorithm || this._cachedFlow !== p.flow ||
        this._cachedOrnament !== p.ornament || this._cachedLoopLength !== p.loopLength) {
        this.initAlgorithm();
    }

    var divisor = this.effectiveDivisor();

    // start delay, extended by the mask's prime value
    var extendedStartTicks = p.start * divisor;
    var maskParameter = p.maskParameter;
    if (maskParameter > 0 && maskParameter < 15) extendedStartTicks += MASK_VALUES[(maskParameter - 1) % 14];
    if (tick < extendedStartTicks) { this.setGate(false); return; }
    tick -= extendedStartTicks;

    // prime-number mask: hides ticks from the algorithm to carve rests/polyrhythms
    var tickAllowed = true;
    var maskParam = p.maskParameter, timeMode = p.timeMode, maskProgression = p.maskProgression;
    if (this._cachedPrimeParam !== maskParam || this._cachedTimeMode !== timeMode || this._cachedMaskProgression !== maskProgression) {
        this._primeMaskCounter = 0;
        this._primeMaskState = 1;
        this._currentMaskArrayIndex = (maskParam > 0 && maskParam < 15) ? (maskParam - 1) % 14 : 0;
        this._cachedPrimeParam = maskParam;
        this._cachedTimeMode = timeMode;
        this._cachedMaskProgression = maskProgression;
        this._timeModeStartTick = tick;
    }
    if (maskParam === 0) {
        tickAllowed = true;
    } else if (maskParam === 15) {
        tickAllowed = false;
    } else {
        var effectiveParam = MASK_VALUES[this._currentMaskArrayIndex % 14];
        var inc = maskProgression === 1 ? 1 : (maskProgression === 2 ? 5 : 7);
        if (timeMode === 0) {
            this._primeMaskCounter++;
            if (this._primeMaskCounter >= effectiveParam) {
                this._primeMaskState = 1 - this._primeMaskState;
                if (maskProgression > 0) this._currentMaskArrayIndex = (this._currentMaskArrayIndex + inc) % 14;
                this._primeMaskCounter = 0;
            }
            tickAllowed = this._primeMaskState === 1;
        } else {
            var q = 192, targetDuration;
            if (timeMode === 1) targetDuration = q - effectiveParam;
            else if (timeMode === 2) targetDuration = Math.floor((q * 3) / 2) - effectiveParam;
            else if (timeMode === 3) targetDuration = q * 3 - effectiveParam;
            else targetDuration = effectiveParam;
            this._primeMaskCounter++;
            if (this._primeMaskCounter <= effectiveParam) tickAllowed = false;
            else if (this._primeMaskCounter <= effectiveParam + targetDuration) tickAllowed = true;
            else {
                if (maskProgression > 0) this._currentMaskArrayIndex = (this._currentMaskArrayIndex + inc) % 14;
                this._primeMaskCounter = 0;
                tickAllowed = false;
            }
        }
    }
    if (!tickAllowed) {
        this.setGate(false);
        this._cvOutput = this._cvCurrent;
        return;
    }

    // finite loop: restart the seeds every N steps so the phrase repeats
    var loopLength = this.actualLoopLength();
    var resetDivisor = loopLength > 0 ? loopLength * divisor : 0;
    var relativeTick = resetDivisor === 0 ? tick : tick % resetDivisor;
    if (resetDivisor > 0 && relativeTick === 0) {
        this.initAlgorithm();
        this._stepIndex = 0;
    }
    var stepTrigger = (relativeTick % divisor) === 0;

    // play the scheduled micro gates (notes, trills, polyrhythm hits)
    while (!this._microGateQueue.empty() && tick >= this._microGateQueue.front().tick) {
        var ev = this._microGateQueue.pop();
        if (ev.gate) this._cvOutput = ev.cvTarget;
        this.setGate(ev.gate, ev.cvTarget, ev.velocity, ev.slide);
    }

    if (this._gateTicks > 0) {
        this._gateTicks--;
        if (this._gateTicks === 0) this.setGate(false);
    }

    if (this._slideCountDown > 0) {
        this._cvCurrent += this._cvDelta;
        this._slideCountDown--;
        if (this._slideCountDown === 0) this._cvCurrent = this._cvTarget;
        this._cvOutput = this._cvCurrent;
    }

    if (!stepTrigger) return;

    this._stepIndex = Math.floor(relativeTick / divisor);
    this._displayStep = this._stepIndex;

    // 1. GENERATE
    var result = this.generateStep();

    // timing offset: knob 0% = straight, 50% = algorithm's own feel, 100% = doubled
    this._gateOffset = clamp(Math.floor((result.gateOffset * p.gateOffset * 2) / 100), 0, 100);

    // trills requested by the algorithm fire with Step Trill probability
    if (result.trillCount > 1 && p.stepTrill > 0) {
        if (this._uiRng.nextRange(100) < p.stepTrill) {
            result.polyCount = result.trillCount;
            result.isSpatial = false;
            result.beatSpread = 0;
        }
    }

    // 2. POWER = density. Skew bends it across the loop.
    var power = p.power;
    var skew = p.skew;
    if (loopLength > 0 && skew !== 0) {
        var currentPos = this._stepIndex % loopLength;
        var offset = -skew + Math.trunc((2 * skew * currentPos) / Math.max(1, loopLength - 1));
        power = Math.max(0, Math.min(16, power + offset));
    }
    var baseCooldown = 17 - power;
    if (baseCooldown < 1) baseCooldown = 1;
    this._coolDownMax = baseCooldown;
    this._microCoolDownMax = baseCooldown;
    if (this._coolDown > 0) { this._coolDown--; if (this._coolDown > this._coolDownMax) this._coolDown = this._coolDownMax; }
    if (this._microCoolDown > 0) { this._microCoolDown--; if (this._microCoolDown > this._microCoolDownMax) this._microCoolDown = this._microCoolDownMax; }

    var velocity = result.velocity & 0xff;
    var midiVel = tuesdayVelocity(velocity, result.accent);
    this._retriggerArmed = false;

    if (result.polyCount > 0) {
        // micro-sequencing: N gates inside the step, each with its own pitch
        var tupleN = result.polyCount;
        var spacing = Math.floor(divisor / tupleN);
        var baseGateLen = Math.floor((spacing * result.gateRatio) / 100);
        var gateLen = Math.floor((baseGateLen * p.gateLength) / 50);
        if (gateLen < 1) gateLen = 1;
        var gateOffsetTicks = Math.floor((divisor * this._gateOffset) / 100);
        var baseTick = tick + gateOffsetTicks;
        var stepAllowed = false;
        if (velocity === 0 || power === 0) stepAllowed = false;
        else if (result.accent) stepAllowed = true;
        else if (this._coolDown === 0) { stepAllowed = true; this._coolDown = this._coolDownMax; }
        else if (Math.floor(velocity / 16) >= this._coolDown) { stepAllowed = true; this._coolDown = this._coolDownMax; }
        if (stepAllowed) {
            var velDensity = Math.floor(velocity / 16);
            for (i = 0; i < tupleN; i++) {
                var microAllowed = false;
                if (this._microCoolDown === 0) { microAllowed = true; this._microCoolDown = this._microCoolDownMax; }
                else if (velDensity >= this._microCoolDown * 2) { microAllowed = true; this._microCoolDown = this._microCoolDownMax; }
                if (microAllowed) {
                    var volts = this.scaleToVolts(result.note + result.noteOffsets[i], result.octave);
                    var at = baseTick + i * spacing;
                    this._microGateQueue.push({ tick: at, gate: true, cvTarget: volts, velocity: midiVel, slide: result.slide && p.glide > 0 });
                    this._microGateQueue.push({ tick: at + gateLen, gate: false, cvTarget: volts });
                }
            }
        }
        this._retriggerArmed = true;
    } else if (result.beatSpread > 0) {
        // old-style ratchet: the gate fires twice more, a whole tone up
        var chance = Math.floor((result.beatSpread * p.trill) / 100);
        if (this._uiRng.nextRange(100) < chance) {
            this._retriggerArmed = true;
            this._retriggerCount = 2;
            this._retriggerPeriod = Math.floor(divisor / 3);
            this._retriggerLength = Math.floor(this._retriggerPeriod / 2);
            this._ratchetInterval = 0;
            this._retriggerVelocity = midiVel;
        }
    }

    // glide amount for slides
    this._slide = 0;
    if (result.slide && p.glide > 0) this._slide = 1 + Math.floor(p.glide / 30);

    // density gate: accents always play, otherwise cooldown or loud velocity
    var densityGate = false, accentActive = false;
    if (velocity === 0 || power === 0) densityGate = false;
    else if (result.accent) { densityGate = true; accentActive = true; }
    else if (this._coolDown === 0) densityGate = true;
    else if (Math.floor(velocity / 16) >= this._coolDown) densityGate = true;

    var noteVolts = this.scaleToVolts(result.note, result.octave);
    if (densityGate) {
        this._coolDown = this._coolDownMax;
        var gOffTicks = Math.floor((divisor * this._gateOffset) / 100);
        var baseLen = Math.floor((divisor * result.gateRatio) / 100);
        var gateLengthTicks = Math.floor((baseLen * p.gateLength) / 50);
        if (gateLengthTicks < 1) gateLengthTicks = 1;
        if (accentActive) gateLengthTicks = Math.floor((gateLengthTicks * 3) / 2);
        if (!this._retriggerArmed) {
            this._microGateQueue.push({ tick: tick + gOffTicks, gate: true, cvTarget: noteVolts, velocity: midiVel, slide: this._slide > 0 });
            this._microGateQueue.push({ tick: tick + gOffTicks + gateLengthTicks, gate: false, cvTarget: noteVolts });
        }
        if (this._slide > 0) {
            this._cvTarget = noteVolts;
            var slideTicks = this._slide * 12;
            this._cvDelta = (this._cvTarget - this._cvCurrent) / slideTicks;
            this._slideCountDown = slideTicks;
        } else {
            this._cvCurrent = noteVolts;
            this._cvOutput = noteVolts;
            this._slideCountDown = 0;
        }
        if (this._retriggerArmed) {
            this._gateTicks = this._retriggerLength;
            this._trillCvTarget = noteVolts;
        }
    } else if (velocity > 0) {
        // Free CV mode: pitch follows even on rests (no MIDI effect)
        this._cvCurrent = noteVolts;
        this._cvOutput = noteVolts;
    }

    host.stepInfo(this._stepIndex, loopLength, densityGate ? voltsToMidi(noteVolts) : -1, midiVel, result.accent);
    return true;   // a step happened (the firmware then runs one extra update, see host)
};

// update: runs once per millisecond, like the firmware's engine loop.
// It drives glides and the old-style ratchets (Trill).
TuesdayEngine.prototype.update = function () {
    if (this._slideCountDown > 0) {
        this._slideCountDown--;
        this._cvCurrent += this._cvDelta;
        if (!this._retriggerArmed) this._cvOutput = this._cvCurrent;
    }
    if (this._retriggerArmed) {
        if (this._gateOutput) {
            if (this._gateTicks > 0) this._gateTicks--;
            else {
                this.setGate(false);
                this._gateTicks = this._retriggerPeriod - this._retriggerLength;
            }
        } else {
            if (this._gateTicks > 0) this._gateTicks--;
            else if (this._retriggerCount > 0) {
                this._retriggerCount--;
                this._gateTicks = this._retriggerLength;
                this._cvOutput += (this._ratchetInterval !== 0 ? this._ratchetInterval : 2) / 12;
                this.setGate(true, this._cvOutput, this._retriggerVelocity || 100, false);
            } else {
                this._retriggerArmed = false;
            }
        }
    }
};

// all gate changes go through here so the host can turn them into MIDI notes
TuesdayEngine.prototype.setGate = function (on, cv, velocity, slide) {
    this._gateOutput = on;
    if (on) this.host.gate(true, cv, velocity, slide);
    else this.host.gate(false);
};

TuesdayEngine.prototype.usesMsUpdate = true;

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
