// X|FORMER fractal for Max for Live - built file, edit the sources in m4l/src
var XF_DEVICE = "fractal";
var XF_PARAMS = ["record", "lock", "ornRate", "ornIntensity", "branchCount", "path", "branchSeed", "rotate", "octave", "transpose", "orderMode", "divisor", "cadence", "scale", "rootNote", "loopFirst", "loopLast", "recordFirst", "recordLast", "recordSkip", "ornFirst", "ornLast", "trackDelay", "branchPool", "clockMult", "scaleRotate", "fixedVel", "resetBars", "fidelity", "recordMode", "quantize", "thru", "legato", "resetOnStart", "rstAny", "rstPitch"];

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


// ---------------- fractal.js ----------------
// =====================================================================
//  FRACTAL engine (engine/FractalTrackEngine.cpp, model/FractalSequence.h,
//  model/FractalTrack.h)
//
//  A phrase looper that grows. It listens to a source (on the module another
//  track; here the MIDI notes coming into the device), sums up every section
//  (one Divisor long) into one cell - pitch, how long the gate was held and
//  where in the section it started - and writes the cells into a "trunk" of
//  up to 128 cells. The trunk loops between Loop First and Loop Last.
//  Branches are transformed copies of the trunk (transposed, reversed,
//  inverted, rotated, squeezed, stretched, thinned) played after it, and
//  ornaments add little flourishes (turns, runs, trills...) to gated cells.
//
//  The device splits the module's tick in two: capture runs at the playhead,
//  in real time, because that is when the MIDI notes arrive; replay runs in
//  the host's lookahead like every other engine. The test harness runs both
//  in one tick, in the module's order (inlineSource).
// =====================================================================
var FRACTAL_CELLS = 128;
var FR_OP_TRANSPOSE = 0, FR_OP_REVERSE = 1, FR_OP_INVERSE = 2, FR_OP_RETINVERSE = 3,
    FR_OP_ROTATE = 4, FR_OP_COMPRESS = 5, FR_OP_EXPAND = 6, FR_OP_GATETHIN = 7;
var FR_OP_NAMES = ["Transpose", "Reverse", "Inverse", "Retrograde Inv.", "Rotate", "Compress", "Expand", "Gate Thin"];
var FR_ORN_NAMES = ["Anticipation", "Suspension", "Syncopation", "Octave Up", "Fifth Up",
    "Half-Turn Toward", "Half-Turn Away", "Run Toward", "Run Away", "Turn", "Arp Toward", "Arp Away",
    "Mordent Up", "Mordent Down", "Trill"];
var FR_ORDER_NAMES = ["Forward", "Reverse", "Pendulum", "Random", "Converge", "Diverge", "Page"];
var FR_RNG_SEED = 0x6f7261;

var FRACTAL_RANGES = {
    divisor: [1, 768], clockMult: [50, 150], resetMeasure: [0, 128],
    loopFirst: [0, 127], loopLast: [0, 127], rotate: [-64, 64], orderMode: [0, 6],
    recordSkip: [0, 15], recordFirst: [0, 127], recordLast: [0, 127], ornFirst: [0, 127], ornLast: [0, 127],
    recordMode: [0, 1], cadence: [0, 1], fidelity: [0, 1], record: [0, 1],
    branchCount: [0, 7], path: [0, 255], branchSeed: [0, 65535], branchPool: [0, 255],
    ornRate: [0, 100], ornIntensity: [0, 100], scale: [0, 13], rootNote: [0, 11], scaleRotate: [0, 31],
    lock: [0, 1], octave: [-10, 10], transpose: [-100, 100], quantize: [0, 1], trackDelay: [0, 16]
};

// ---- cell codec: bit 15 valid, bits 11-14 gate length (0 rest, 1 trig,
// 2-14 proportional, 15 tie), bits 0-10 signed pitch, 1024 = 60 semitones ----
function frEncode(semi, gateLen, valid) {
    var c = clamp(lround(Math.fround(Math.fround(semi * 1024) / 60)), -1024, 1023);
    var cell = (c & 0x7ff) | ((clamp(gateLen, 0, 15) & 0xf) << 11);
    if (valid) cell |= 0x8000;
    return cell;
}
function frDecode(cell) {
    var c = cell & 0x7ff;
    if (c & 0x400) c -= 0x800;
    return { semi: Math.fround(Math.fround(c * 60) / 1024), gateLen: (cell >> 11) & 0xf, valid: (cell & 0x8000) !== 0 };
}
function frGateLen(high, total) {
    if (high <= 0 || total <= 0) return 0;
    var f = Math.fround(high / total);
    if (f >= Math.fround(0.97)) return 15;
    if (f <= Math.fround(0.18)) return 1;
    return clamp(lround(Math.fround(f * 14)), 2, 14);
}
function frOnset(onsetTick, total) {
    if (onsetTick < 0 || total <= 0) return 0;
    return clamp(Math.trunc(onsetTick * 16 / total), 0, 15);
}
// C++ std::round for floats (halves away from zero)
function frRound(x) { return x < 0 ? -Math.floor(-x + 0.5) : Math.floor(x + 0.5); }

// the module's scale, in float, with its volts -> degree snap
function frScaleVolts(base, note) {
    var n = base.length, oct = floorDiv(note, n);
    return Math.fround(oct + Math.fround(base[note - oct * n] * Math.fround(1 / 1536)));
}
function frScaleFromVolts(base, volts) {
    volts = Math.fround(volts + Math.fround(0.01));
    var oct = Math.floor(volts), frac = Math.fround(volts - oct), idx = -1;
    for (var i = 0; i < base.length; i++) {
        if (frac < Math.fround(base[i] * Math.fround(1 / 1536))) break;
        idx = i;
    }
    if (idx === -1) { idx = base.length - 1; oct--; }
    return oct * base.length + idx;
}
function FrScale(index, rotate) {
    this.base = SCALES[clamp(index | 0, 0, SCALES.length - 1)].notes;
    var n = this.base.length;
    this.rotate = (((rotate | 0) % n) + n) % n;
}
FrScale.prototype.noteToVolts = function (note) {
    if (this.rotate === 0) return frScaleVolts(this.base, note);
    return Math.fround(frScaleVolts(this.base, note + this.rotate) - frScaleVolts(this.base, this.rotate));
};
FrScale.prototype.noteFromVolts = function (v) {
    if (this.rotate === 0) return frScaleFromVolts(this.base, v);
    return frScaleFromVolts(this.base, Math.fround(v + frScaleVolts(this.base, this.rotate))) - this.rotate;
};

// the read order inside the loop for Converge / Diverge / Page
function frOrderMap(mode, step, len) {
    if (len <= 0) return 0;
    var s = ((step % len) + len) % len;
    if (mode === 4) { var half = Math.trunc(s / 2); return s % 2 === 0 ? half : len - 1 - half; }
    if (mode === 5) {
        var c = Math.trunc((len - 1) / 2), k = Math.trunc((s + 1) / 2);
        return s % 2 === 0 ? clamp(c - k, 0, len - 1) : clamp(c + k, 0, len - 1);
    }
    if (mode === 6) {
        var pages = Math.trunc((len + 3) / 4);
        return clamp((s % pages) * 4 + Math.trunc(s / pages), 0, len - 1);
    }
    return s;
}
// Forward, Reverse, Pendulum, Random -> run modes; the rest walk forward and are remapped
function frRunMode(mode) { return mode === 1 ? 1 : (mode === 2 ? 2 : (mode === 3 ? 4 : 0)); }

// which segment the n-th part of the walk plays: the trunk, then branches whose
// Path bit is 0 going out, then branches whose bit is 1 coming back
function frRouteOf(path, n, r) {
    if (r === 0) return 0;
    var seen = 0, b;
    for (b = 1; b <= n; b++) if (!((path >> (b - 1)) & 1) && ++seen === r) return b;
    for (b = n; b >= 1; b--) if (((path >> (b - 1)) & 1) && ++seen === r) return b;
    return 0;
}

function FractalEngine(host) {
    this.host = host;
    this.p = {
        divisor: 12, clockMult: 100, resetMeasure: 0, loopFirst: 0, loopLast: 15, rotate: 0, orderMode: 0,
        recordSkip: 0, recordFirst: 0, recordLast: 15, ornFirst: 0, ornLast: 15,
        recordMode: 0, cadence: 1, fidelity: 1, record: 0,
        branchCount: 0, path: 0, branchSeed: 0, branchPool: 0xff, ornRate: 0, ornIntensity: 0,
        scale: 1, rootNote: 0, scaleRotate: 0, lock: 0, octave: 0, transpose: 0, quantize: 0, trackDelay: 0
    };
    this.trunk = []; this.onset = [];
    for (var i = 0; i < FRACTAL_CELLS; i++) { this.trunk.push(0); this.onset.push(0); }
    this.branches = [];
    for (i = 0; i < 7; i++) this.branches.push({ kind: 0, t: 0, k: 0, fx100: 100, n: 2 });
    this.measureTicks = 768;
    this.rngSeed = FR_RNG_SEED;
    this.inlineSource = null;     // test harness: { gate(), cv() } read in the module's order
    this.input = { gate: false, cv: 0 };   // device: the MIDI notes held right now
    this._rng = new Random(this.rngSeed);
    this.reset();
}

// test harness: the module's constructor runs self-checks that draw from the
// ornament dice before the track ever plays; replay those draws
FractalEngine.prototype.refInit = function () {
    this._rng = new Random(this.rngSeed);
    var i;
    for (i = 0; i < 52; i++) this._rng.nextRange(100);          // locked rolls: no ornament pick
    for (i = 0; i < 200; i++) {                                   // until a roll gives a multi-note ornament
        this._rng.nextRange(100);
        var o = this._rng.nextRange(15);
        if (o !== 1 && o !== 2) break;
    }
};

FractalEngine.prototype.setParam = function (name, value) {
    var r = FRACTAL_RANGES[name];
    if (!r) return false;
    this.p[name] = clamp(Math.round(value), r[0], r[1]);
    return true;
};
FractalEngine.prototype.setRefParam = function (t) {
    if (t[0] === "sourceA") return true;
    return this.setParam(t[0], +t[1]);
};
// Reset Bars on the device = the module's Reset Measure
FractalEngine.prototype.ownsResetMeasure = true;

// transport start / Reset: back to the start of the loop. Unlike the module,
// the recorded trunk stays (it lives in the Live set); "Clear" erases it.
FractalEngine.prototype.reset = function () {
    var p = this.p;
    // the device rolls the same ornaments every time the song plays from the start
    if (!this.quirks) this._rng = new Random(this.rngSeed);
    this.recordPos = p.recordFirst;
    this._skipLeft = 0;
    this._wasArmed = false;
    this.readPos = p.loopFirst;
    this.segment = 0;
    this._globalPos = 0;
    this._order = new SequenceState();
    this._branchHash = -1;
    this._gate = false; this._cv = 0;
    this._gateEv = []; this._cvEv = []; this._gateHead = 0; this._cvHead = 0;
    this._delay = [];
    this.lastOrnament = -1; this._lastDir = 1;
    this.queuedSegment = -1; this.queuedOrnament = -1;
    this._resetObservation();
    this._obsLastCv = 0;
    this._eventHasRef = false; this._eventRefSemi = 0; this._eventInhibit = 0;
    this._capTick = 0;
    this._prevGate = false; this._prevCv = 0; this._gateOutput = false; this._cvOutput = 0;
};
FractalEngine.prototype.clearTrunk = function () {
    for (var i = 0; i < FRACTAL_CELLS; i++) { this.trunk[i] = 0; this.onset[i] = 0; }
    this.recordPos = this.p.recordFirst; this._skipLeft = 0; this._wasArmed = false;
    this.readPos = this.p.loopFirst; this._globalPos = 0; this._order = new SequenceState();
    this._gate = false; this._gateEv = []; this._cvEv = []; this._gateHead = 0; this._cvHead = 0; this._delay = [];
    this.lastOrnament = -1;
    this._resetObservation();
    this._eventHasRef = false; this._eventInhibit = 0;
    if (this.host.stateChanged) this.host.stateChanged();
};
FractalEngine.prototype.stop = function () {
    this._gate = false;
    this._gateEv = []; this._cvEv = []; this._gateHead = 0; this._cvHead = 0; this._delay = [];
    this._emit();
};

FractalEngine.prototype.effectiveDivisor = function () {
    var cm = Math.fround(this.p.clockMult * Math.fround(0.01));
    if (cm <= 0) cm = 1;
    return Math.max(1, lround(Math.fround(this.p.divisor * 4 / cm)));
};
FractalEngine.prototype._relTick = function (tick) {
    var rd = this.p.resetMeasure * this.measureTicks;
    return { rel: rd === 0 ? tick : tick % rd, atReset: rd > 0 && tick % rd === 0 };
};

// ---- one module tick (test harness): capture and replay together ----
FractalEngine.prototype.tick = function (tick) {
    var div = this.effectiveDivisor(), r = this._relTick(tick);
    if (this.inlineSource) {
        if (r.atReset) this.recordPos = this.p.recordFirst;
        if (r.rel % div === 0) this._captureBoundary();
        this._replayTick(tick, div, r);
        this._observe(this.inlineSource.gate(), this.inlineSource.cv(), div);
    } else this._replayTick(tick, div, r);
    this._drain(tick);
    this._emit();
    return false;
};
FractalEngine.prototype._replayTick = function (tick, div, r) {
    if (r.atReset) { this.readPos = this.p.loopFirst; this._globalPos = 0; this._order = new SequenceState(); }
    if (r.rel % div !== 0) return;
    if (this.queuedSegment >= 0) {
        var n = clamp(this.p.branchCount, 0, 7);
        for (var i = 0; i <= n; i++) {
            if (frRouteOf(this.p.path, n, i) === this.queuedSegment) {
                this._globalPos = i * this.loopLen(); this._order = new SequenceState(); break;
            }
        }
        this.queuedSegment = -1;
    }
    this.replaySection(tick, div);
};

// ---- the device's capture side: called at the playhead with the held notes ----
FractalEngine.prototype.captureTick = function (tick, gate, cv) {
    var div = this.effectiveDivisor(), r = this._relTick(tick);
    if (r.atReset) this.recordPos = this.p.recordFirst;
    if (r.rel % div === 0) this._captureBoundary();
    this._observe(gate, cv, div);
};
FractalEngine.prototype.capturing = function () { return !!this.p.record && !this.p.lock; };

FractalEngine.prototype._captureBoundary = function () {
    var armed = !!this.p.record;
    if (armed && !this._wasArmed) { this._skipLeft = 0; this._resetObservation(); }
    this._wasArmed = armed;
    if (this.capturing() && this.p.cadence === 0) this.captureSection();
    this._resetObservation();
};
FractalEngine.prototype._resetObservation = function () {
    this._obsTicks = 0; this._obsHigh = 0; this._obsOnset = -1; this._obsEdgeCv = 0;
    this._obsAny = false; this._obsPrevGate = false;
};
// one tick of listening: gate time, first onset and its pitch; Event cadence
// writes a cell whenever the incoming pitch moves by about a semitone
FractalEngine.prototype._observe = function (gate, cv, div) {
    if (!this.capturing()) return;
    var semi = Math.fround(cv * 12);
    this._obsLastCv = cv;
    if (gate) {
        this._obsHigh = (this._obsHigh + 1) & 0xffff;
        this._obsAny = true;
        if (!this._obsPrevGate && this._obsOnset < 0) { this._obsOnset = this._obsTicks; this._obsEdgeCv = cv; }
    }
    if (this._eventInhibit > 0) this._eventInhibit--;
    if (this.p.cadence === 1) {
        if (!this._eventHasRef) {
            if (gate) { this._eventRefSemi = frRound(semi); this._eventHasRef = true; }
        } else if (Math.abs(Math.fround(semi - this._eventRefSemi)) >= Math.fround(1 - Math.fround(1 / 3)) && this._eventInhibit === 0) {
            this._eventRefSemi = frRound(semi);
            this._eventInhibit = Math.max(1, Math.trunc(div / 8));
            this.commitCell(semi, gate ? 8 : 0, 0);
            this.advanceRecordPos();
        }
    }
    this._obsPrevGate = gate;
    this._obsTicks = (this._obsTicks + 1) & 0xffff;
};
FractalEngine.prototype.captureSection = function () {
    var p = this.p;
    if (this._skipLeft > 0) { this._skipLeft--; return; }
    var feel = p.fidelity === 1, total = Math.max(1, this._obsTicks);
    var gl = frGateLen(this._obsHigh, total);
    var cv = (feel && this._obsAny) ? this._obsEdgeCv : this._obsLastCv;
    var onset = (feel && this._obsOnset >= 0) ? frOnset(this._obsOnset, total) : 0;
    // Latch keeps what was there when the section was silent (overdub)
    if (!(p.recordMode === 1 && gl === 0)) this.commitCell(Math.fround(cv * 12), gl, onset);
    this.advanceRecordPos();
    this._skipLeft = p.recordSkip;
};
FractalEngine.prototype.commitCell = function (semi, gl, onset) {
    this.trunk[this.recordPos] = frEncode(semi, gl, true);
    this.onset[this.recordPos] = onset & 0xf;
    if (this.host.cellWritten) this.host.cellWritten(this.recordPos);
    if (this.host.stateChanged) this.host.stateChanged();
};
FractalEngine.prototype.advanceRecordPos = function () {
    var first = this.p.recordFirst, last = Math.max(first, this.p.recordLast);
    if (this.recordPos < first || this.recordPos > last) { this.recordPos = first; return; }
    this.recordPos = this.recordPos >= last ? first : this.recordPos + 1;
};

// ---- the loop and its branches ----
FractalEngine.prototype.loopLen = function () {
    return Math.max(this.p.loopFirst, this.p.loopLast) - this.p.loopFirst + 1;
};
FractalEngine.prototype.trunkIndex = function (within) {
    var len = this.loopLen(), s = ((within % len) + len) % len;
    if (this.p.rotate !== 0) s = (s + (this.p.rotate % len) + len) % len;
    return clamp(this.p.loopFirst + s, 0, FRACTAL_CELLS - 1);
};
FractalEngine.prototype.inversionCenter = function () {
    var c = this.trunk[clamp(this.p.loopFirst, 0, FRACTAL_CELLS - 1)];
    return (c & 0x8000) ? frDecode(c).semi : 0;
};
FractalEngine.prototype._branchSignature = function () {
    return u32(this.p.branchSeed * 131 + this.p.branchPool * 17 + this.loopLen()) & 0x7fffffff;
};
// one transform per branch, drawn from the Pool with the Seed (same seed, same chain)
FractalEngine.prototype.rebuildBranches = function () {
    var p = this.p, len = this.loopLen(), elig = [];
    for (var k = 0; k < 8; k++) if ((p.branchPool >> k) & 1) elig.push(k);
    if (!elig.length) elig.push(FR_OP_TRANSPOSE);
    var rng = new Random(p.branchSeed), n = clamp(p.branchCount, 0, 7);
    for (var s = 1; s <= n; s++) {
        var b = this.branches[s - 1];
        b.kind = elig[rng.nextRange(elig.length)];
        b.t = 0; b.k = 0; b.fx100 = 100; b.n = 2;
        if (b.kind === FR_OP_TRANSPOSE) { var mag = [5, 7, 12][rng.nextRange(3)]; b.t = (rng.nextBinary() ? -1 : 1) * mag; }
        else if (b.kind === FR_OP_ROTATE) b.k = len > 1 ? 1 + rng.nextRange(len - 1) : 0;
        else if (b.kind === FR_OP_COMPRESS) b.fx100 = [50, 67][rng.nextRange(2)];
        else if (b.kind === FR_OP_EXPAND) b.fx100 = [150, 200][rng.nextRange(2)];
        else if (b.kind === FR_OP_GATETHIN) b.n = 2 + rng.nextRange(3);
    }
    this._branchHash = this._branchSignature();
    this._branchCountCache = n;
};
FractalEngine.prototype.segmentCell = function (seg, pos) {
    var len = this.loopLen(), F = Math.fround;
    pos = ((pos % len) + len) % len;
    if (seg === 0) return frDecode(this.trunk[this.trunkIndex(pos)]);
    var tf = this.branches[seg - 1], c;
    if (tf.kind === FR_OP_REVERSE) return this.segmentCell(seg - 1, len - 1 - pos);
    if (tf.kind === FR_OP_RETINVERSE) {
        c = this.segmentCell(seg - 1, len - 1 - pos);
        c.semi = F(F(2 * this.inversionCenter()) - c.semi);
        return c;
    }
    if (tf.kind === FR_OP_ROTATE) return this.segmentCell(seg - 1, pos + tf.k);
    c = this.segmentCell(seg - 1, pos);
    if (tf.kind === FR_OP_TRANSPOSE) c.semi = F(c.semi + tf.t);
    else if (tf.kind === FR_OP_INVERSE) c.semi = F(F(2 * this.inversionCenter()) - c.semi);
    else if (tf.kind === FR_OP_COMPRESS || tf.kind === FR_OP_EXPAND) {
        var ctr = this.inversionCenter();
        c.semi = F(ctr + frRound(F(F(F(c.semi - ctr) * tf.fx100) * F(0.01))));
    } else if (tf.kind === FR_OP_GATETHIN && (pos + 1) % tf.n === 0) c.gateLen = 0;
    return c;
};

FractalEngine.prototype.replaySection = function (tick, div) {
    var p = this.p;
    if (this._branchSignature() !== this._branchHash || clamp(p.branchCount, 0, 7) !== this._branchCountCache) this.rebuildBranches();
    var n = clamp(p.branchCount, 0, 7), len = this.loopLen(), total = len * (1 + n);
    var gp = this._globalPos % total;
    var seg = frRouteOf(p.path, n, clamp(Math.trunc(gp / len), 0, n));
    var rm = frRunMode(p.orderMode);
    this._order.advanceFree(rm, 0, len - 1, this._rng);
    var within = frOrderMap(p.orderMode, this._order.step, len);
    var next = new SequenceState();
    next.step = this._order.step; next.prevStep = this._order.prevStep;
    next.direction = this._order.direction; next.iteration = this._order.iteration;
    next.advanceFree(rm, 0, len - 1, this._rng);
    var nextWithin = frOrderMap(p.orderMode, next.step, len);

    var cell = this.segmentCell(seg, within), nextCell = this.segmentCell(seg, nextWithin);
    var readIdx = this.trunkIndex(within);
    var gated = cell.valid && cell.gateLen > 0;
    var inZone = readIdx >= p.ornFirst && readIdx <= p.ornLast;
    var ornOk = gated && (inZone || this.queuedOrnament >= 0);
    var onsetTicks = p.fidelity === 1 ? Math.trunc(this.onset[readIdx] * div / 16) : 0;
    this.segment = seg;
    if (p.trackDelay <= 0) {
        this.readPos = readIdx;
        this.scheduleSection(u32(tick + onsetTicks), div, cell.semi, nextCell.semi, cell.valid ? cell.gateLen : 0, ornOk);
    } else {
        // Track Delay: play what was read Delay sections ago (a canon with itself)
        this._drainDelay(tick, div);
        if (this._delay.length < 31) this._delay.push({ cv: clamp(lround(Math.fround(cell.semi * 256)), -32768, 32767),
            gateLen: (cell.valid ? cell.gateLen : 0) & 0xf, orn: ornOk, left: clamp(p.trackDelay - 1, 0, 15), readPos: readIdx & 0x7f });
    }
    this._globalPos = ((gp + 1) % total) & 0xffff;
};
FractalEngine.prototype._drainDelay = function (tick, div) {
    var pending = this._delay.splice(0, this._delay.length);
    for (var i = 0; i < pending.length; i++) {
        var e = pending[i];
        if (e.left === 0) {
            var semi = Math.fround(e.cv * Math.fround(1 / 256));
            this.readPos = e.readPos;
            this.scheduleSection(tick, div, semi, semi, e.gateLen, e.orn);
        } else { e.left--; this._delay.push(e); }
    }
};

// ---- notes and ornaments ----
FractalEngine.prototype.scale = function () { return new FrScale(this.p.scale, this.p.scaleRotate); };
FractalEngine.prototype.quantizeSemi = function (semi) {
    if (!this.p.quantize) return semi;
    var s = this.scale();
    return Math.fround(s.noteToVolts(s.noteFromVolts(Math.fround(semi * Math.fround(1 / 12)))) * 12);
};
FractalEngine.prototype.stepDegrees = function (semi, steps) {
    var s = this.scale();
    return Math.fround(s.noteToVolts(s.noteFromVolts(Math.fround(semi * Math.fround(1 / 12))) + steps) * 12);
};
FractalEngine.prototype.playbackCv = function (semi) {
    var F = Math.fround;
    var cv = F(semi * F(1 / 12));
    cv = F(cv + this.p.octave);
    return F(cv + F(this.p.transpose * F(1 / 12)));
};
// 2-step ornaments always, 4-step from Intensity 40, the trill from 75
FractalEngine.prototype.rollOrnament = function (rate, intensity) {
    if (this.queuedOrnament >= 0) { var q = this.queuedOrnament; this.queuedOrnament = -1; this.lastOrnament = q; return q; }
    if (rate <= 0) return -1;
    if (this._rng.nextRange(100) >= rate) return -1;
    if (this.p.lock) return this.lastOrnament;
    var pool = 7 + (intensity >= 40 ? 7 : 0) + (intensity >= 75 ? 1 : 0);
    var o = this._rng.nextRange(pool);
    this.lastOrnament = o;
    return o;
};
FractalEngine.prototype._pushCv = function (at, cv) { if (this._cvEv.length < 10) this._cvEv.push({ t: at, cv: cv }); };
FractalEngine.prototype._pushGate = function (at, g) { if (this._gateEv.length < 18) this._gateEv.push({ t: at, g: g }); };
FractalEngine.prototype.scheduleSection = function (tick, div, main, nextSemi, gateLen, ornOk) {
    var self = this;
    this._gateEv = []; this._cvEv = []; this._gateHead = 0; this._cvHead = 0;
    main = this.quantizeSemi(main);
    nextSemi = this.quantizeSemi(nextSemi);
    // (times are 32-bit unsigned like the module's, so a too-short note wraps to "already over")
    function note(semi, at, dur, gap) {
        if (gap === undefined) gap = 1;
        at = u32(at); dur = u32(dur);
        self._pushCv(at, self.playbackCv(semi));
        self._pushGate(at, true);
        self._pushGate(u32(at + (dur > gap ? dur - gap : 1)), false);
    }
    if (gateLen <= 0) { this._pushGate(tick, false); return; }
    var full = Math.max(1, Math.trunc(div * gateLen / 15)), e = Math.max(1, Math.trunc(div / 8));
    var name = ornOk ? this.rollOrnament(this.p.ornRate, this.p.ornIntensity) : -1;
    if (name < 0) { note(main, tick, full, 0); return; }
    this.host.ornament && this.host.ornament(name);
    var dir, ant;
    if (this.p.lock) { dir = this._lastDir; ant = this.stepDegrees(main, dir); }
    else { dir = nextSemi >= main ? 1 : -1; this._lastDir = dir; ant = nextSemi; }
    function grace(semi) { note(main, tick, u32(full - e)); note(semi, tick + u32(full - e), e); }
    var s, k;
    switch (name) {
    case 0: note(ant, tick, e); note(main, tick + e, u32(full - e)); break;          // Anticipation
    case 1: note(main, tick, full); break;                                             // Suspension
    case 2: note(main, tick + e, u32(full - e)); break;                                // Syncopation
    case 3: grace(Math.fround(main + 12)); break;                                      // Octave up
    case 4: grace(this.stepDegrees(main, 4)); break;                                   // Fifth up
    case 5: grace(this.stepDegrees(main, dir)); break;                                 // Half-turn toward
    case 6: grace(this.stepDegrees(main, -dir)); break;                                // Half-turn away
    case 9:                                                                            // Turn
        note(main, tick, e); note(this.stepDegrees(main, 1), tick + e, e);
        note(this.stepDegrees(main, -1), tick + 2 * e, e); note(main, tick + 3 * e, u32(full - 3 * e)); break;
    case 12: case 13:                                                                  // Mordents
        var d = name === 12 ? 1 : -1;
        note(main, tick, e); note(this.stepDegrees(main, d), tick + e, e); note(main, tick + 2 * e, u32(full - 2 * e)); break;
    case 7: case 8:                                                                    // Runs
        var rd = name === 7 ? dir : -dir;
        s = Math.max(1, Math.trunc(div / 5));
        note(main, tick, s);
        for (k = 1; k <= 4; k++) note(this.stepDegrees(main, rd * k), tick + k * s, s);
        break;
    case 10: case 11:                                                                  // Arpeggios
        var ad = name === 10 ? dir : -dir, degs = [0, 2, 4];
        s = Math.max(1, Math.trunc(div / 4));
        for (k = 0; k < 3; k++) note(this.stepDegrees(main, ad * degs[k]), tick + k * s, s);
        break;
    case 14:                                                                           // Trill
        var up = this.stepDegrees(main, 1);
        s = Math.max(1, Math.trunc(div / 8));
        for (k = 0; k < 8; k++) note((k & 1) ? up : main, tick + k * s, s);
        break;
    default: note(main, tick, full, 0);
    }
};
FractalEngine.prototype._drain = function (tick) {
    while (this._cvHead < this._cvEv.length && tick >= this._cvEv[this._cvHead].t) { this._cv = this._cvEv[this._cvHead].cv; this._cvHead++; }
    var fired = false;
    while (this._gateHead < this._gateEv.length && tick >= this._gateEv[this._gateHead].t) {
        this._gate = this._gateEv[this._gateHead].g; this._gateHead++; fired = true;
    }
    if (fired && this._gate) {
        var midi = clamp(60 + Math.round(this._cv * 12), 0, 127);
        this.host.stepInfo(this.readPos, this.segment, midi);
    }
};

FractalEngine.prototype.update = function () { this._cvOutput = this._cv; };
FractalEngine.prototype._emit = function () {
    var g = this._gate, cv = this._cv;
    if (g && !this._prevGate) this.host.gate(true, cv, 100, false);
    else if (!g && this._prevGate) this.host.gate(false);
    else if (g && cv !== this._prevCv) this.host.gate(true, cv, 100, false);
    this._prevGate = g; this._prevCv = cv;
    this._gateOutput = g; this._cvOutput = cv;
};

// ---- state for the Live set: the trunk and its onsets ----
FractalEngine.prototype.getState = function () {
    return [1].concat(this.trunk, this.onset);
};
FractalEngine.prototype.setState = function (a) {
    if (!a || a[0] !== 1 || a.length < 1 + 2 * FRACTAL_CELLS) return false;
    for (var i = 0; i < FRACTAL_CELLS; i++) { this.trunk[i] = (a[1 + i] | 0) & 0xffff; this.onset[i] = (a[1 + FRACTAL_CELLS + i] | 0) & 0xf; }
    return true;
};
// a brand-new device: a 16-cell phrase to play with before you record your own
FractalEngine.prototype.initFresh = function () {
    var notes = [0, 3, 7, 10, 12, 10, 7, 5, 3, 5, 7, 3, 0, -2, 0, 7];
    var gates = [8, 4, 8, 4, 12, 0, 8, 4, 8, 4, 8, 0, 15, 4, 8, 4];
    for (var i = 0; i < FRACTAL_CELLS; i++) { this.trunk[i] = 0; this.onset[i] = 0; }
    for (i = 0; i < 16; i++) this.trunk[i] = frEncode(notes[i], gates[i], true);
};

FractalEngine.prototype.usesMsUpdate = false;
FractalEngine.prototype.roundPitch = true;

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
