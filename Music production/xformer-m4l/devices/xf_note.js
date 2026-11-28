// X|FORMER note for Max for Live - built file, edit the sources in m4l/src
var XF_DEVICE = "note";
var XF_PARAMS = ["fill", "firstStep", "lastStep", "rotate", "octave", "gateProbabilityBias", "lengthBias", "noteProbabilityBias", "transpose", "runMode", "divisor", "scale", "rootNote", "retriggerProbabilityBias", "scaleRotate", "clockMult", "accMin", "accMax", "accStep", "fixedVel", "resetBars", "accEnabled", "accMode", "accDirection", "accOrder", "accTrigger", "legato", "resetOnStart", "rstAny", "rstPitch"];

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


// ---------------- note.js ----------------
// =====================================================================
//  NOTE ENGINE  (port of engine/NoteTrackEngine.cpp, Linear mode)
//  A Metropolix-style step sequencer: every step has its own gate,
//  probability, length, retrigger, note, variation, condition, plus
//  PULSE COUNT (how many clock pulses the step lasts), GATE MODE (which of
//  those pulses fire) and the ACCUMULATOR (a counter that transposes the
//  sequence a little further each time chosen steps play).
// =====================================================================

var NOTE_STEP_COUNT = 64;
var CONDITION_LOOP = 7, CONDITION_NOTLOOP = 42, CONDITION_LAST = 77;
var COND_OFFSET = [0, 1, 0, 1, 2, 0, 1, 2, 3, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 0, 1, 2, 3, 4, 5, 6, 7];
var COND_BASE = [2, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8];

// condition names for the editor: Off, Fill, !Fill, Pre, !Pre, First, !First, 1:2 ... !8:8
var CONDITION_NAMES = ["Off", "Fill", "!Fill", "Pre", "!Pre", "First", "!First"];
(function () {
    for (var inv = 0; inv < 2; inv++)
        for (var i = 0; i < 35; i++) CONDITION_NAMES.push((inv ? "!" : "") + (COND_OFFSET[i] + 1) + ":" + COND_BASE[i]);
})();

// per-step fields: [name, min, max, default]
var STEP_FIELDS = [
    ["gate", 0, 1, 0], ["gateProbability", 0, 7, 7], ["gateOffset", 0, 7, 0], ["slide", 0, 1, 0],
    ["retrigger", 0, 3, 0], ["retriggerProbability", 0, 7, 7], ["length", 0, 7, 3],
    ["lengthVariationRange", -7, 7, 0], ["lengthVariationProbability", 0, 7, 7],
    ["note", -63, 63, 0], ["noteVariationRange", -63, 63, 0], ["noteVariationProbability", 0, 7, 7],
    ["condition", 0, CONDITION_LAST - 1, 0], ["accumulator", -7, 7, 0], ["pulseCount", 0, 7, 0],
    ["gateMode", 0, 3, 0]
];
var STEP_FIELD_INDEX = {};
STEP_FIELDS.forEach(function (f, i) { STEP_FIELD_INDEX[f[0]] = i; });

function NoteStep() { var s = this; STEP_FIELDS.forEach(function (f) { s[f[0]] = f[3]; }); }
NoteStep.prototype.isAccumulatorTrigger = function () { return this.accumulator !== 0; };

// ---------------------------------------------------------------------
// Accumulator (model/Accumulator.cpp + engine/AccumulatorOps.cpp)
// ---------------------------------------------------------------------
var ACC_UP = 0, ACC_DOWN = 1, ACC_FREEZE = 2;
var ACC_WRAP = 0, ACC_PENDULUM = 1, ACC_RANDOM = 2, ACC_HOLD = 3;
var ACC_STEP = 0, ACC_GATE = 1, ACC_RETRIGGER = 2;
var accRandom = new Random(0);     // AccumulatorOps' own generator (Random order)

function Accumulator() {
    this.mode = 1; this.direction = ACC_UP; this.order = ACC_WRAP; this.enabled = true;
    this.triggerMode = ACC_STEP; this.minValue = 0; this.maxValue = 7; this.stepValue = 1;
    this.reset();
}
Accumulator.prototype.reset = function () {
    this.currentValue = this.minValue; this.pendulumDirection = 1; this.hasStarted = false;
};
Accumulator.prototype.clone = function () {
    var a = new Accumulator();
    for (var k in this) if (this.hasOwnProperty(k)) a[k] = this[k];
    return a;
};
Accumulator.prototype.tick = function () {
    if (!this.enabled) return;
    if (!this.hasStarted) { this.hasStarted = true; return; }   // the first tick only arms it
    var dir = this.direction === ACC_UP ? 1 : (this.direction === ACC_DOWN ? -1 : 0);
    var v = this.currentValue, mn = this.minValue, mx = this.maxValue, mag = this.stepValue & 0xff;
    if (dir === 0) return;
    switch (this.order) {
    case ACC_WRAP:
        v += dir > 0 ? mag : -mag;
        var range = mx - mn + 1;
        if (range > 0) v = mn + (((v - mn) % range) + range) % range;
        break;
    case ACC_PENDULUM:
        v += mag * this.pendulumDirection;
        if (v >= mx) { v = mx; this.pendulumDirection = -this.pendulumDirection; }
        else if (v <= mn) { v = mn; this.pendulumDirection = -this.pendulumDirection; }
        break;
    case ACC_RANDOM:
        v = mn === mx ? mn : mn + accRandom.nextRange(mx - mn + 1);
        break;
    case ACC_HOLD:
        if (dir > 0) { v += mag; if (v >= mx) v = mx; }
        else { v -= mag; if (v <= mn) v = mn; }
        break;
    }
    this.currentValue = (v << 16) >> 16;   // int16
};
// a step-value override: negative flips direction, then one tick
Accumulator.prototype.tickOverride = function (stepValue) {
    if (stepValue === 1) { this.tick(); return; }
    if (stepValue === 0) return;
    var sv = this.stepValue, sd = this.direction;
    if (stepValue < 0) { this.direction = sd === ACC_UP ? ACC_DOWN : ACC_UP; this.stepValue = -stepValue; }
    else this.stepValue = stepValue;
    this.tick();
    this.stepValue = sv; this.direction = sd;
};

// ---------------------------------------------------------------------
// ENGINE
// ---------------------------------------------------------------------
var NOTE_RANGES = {
    firstStep: [0, 63], lastStep: [0, 63], runMode: [0, 5], divisor: [1, 768], clockMult: [50, 150],
    resetMeasure: [0, 128], scale: [0, 13], rootNote: [0, 11], scaleRotate: [0, 31],
    octave: [-10, 10], transpose: [-100, 100], rotate: [-64, 64],
    gateProbabilityBias: [-8, 8], retriggerProbabilityBias: [-8, 8], lengthBias: [-8, 8],
    noteProbabilityBias: [-8, 8], cvUpdate: [0, 1]
};
var ACC_PARAMS = { accEnabled: "enabled", accMode: "mode", accDirection: "direction", accOrder: "order",
    accTrigger: "triggerMode", accMin: "minValue", accMax: "maxValue", accStep: "stepValue" };
var ACC_RANGES = { accEnabled: [0, 1], accMode: [0, 1], accDirection: [0, 2], accOrder: [0, 3],
    accTrigger: [0, 2], accMin: [-100, 100], accMax: [-100, 100], accStep: [0, 100] };

function NoteEngine(host) {
    this.host = host;
    this.p = {
        firstStep: 0, lastStep: 15, runMode: 0, divisor: 12, clockMult: 100, resetMeasure: 0,
        scale: 1, rootNote: 0, scaleRotate: 0, octave: 0, transpose: 0, rotate: 0,
        gateProbabilityBias: 0, retriggerProbabilityBias: 0, lengthBias: 0, noteProbabilityBias: 0,
        cvUpdate: 0
    };
    this.steps = [];
    for (var i = 0; i < NOTE_STEP_COUNT; i++) this.steps.push(new NoteStep());
    this.acc = new Accumulator();
    this.measureTicks = 768;          // one 4/4 bar at 192 PPQN (the host keeps this in sync)
    this._rng = new Random(0);
    this._ms = 0;
    this.reset();
}

NoteEngine.prototype.setParam = function (name, value) {
    var p = this.p, r;
    if (name === "fill") { this.fill = !!value; return true; }   // the Fill button (Fill / !Fill conditions)
    value = Math.round(value);
    if ((r = NOTE_RANGES[name])) {
        value = clamp(value, r[0], r[1]);
        if (name === "firstStep") value = clamp(value, 0, p.lastStep);
        if (name === "lastStep") value = clamp(value, p.firstStep, 63);
        p[name] = value;
        return true;
    }
    if ((r = ACC_RANGES[name])) {
        value = clamp(value, r[0], r[1]);
        this.acc[ACC_PARAMS[name]] = name === "accEnabled" ? !!value : value;
        if (name === "accMin") this.acc.reset();
        return true;
    }
    return false;
};

// editor / harness: "step <index> <field> <value>"
NoteEngine.prototype.setStep = function (i, field, value) {
    var f = STEP_FIELDS[STEP_FIELD_INDEX[field]];
    if (!f || i < 0 || i >= NOTE_STEP_COUNT) return false;
    var v = clamp(Math.round(value), f[1], f[2]);
    if (field === "gateOffset") v = Math.max(0, v);
    this.steps[i][field] = v;
    if (this.host.stateChanged) this.host.stateChanged();
    return true;
};
NoteEngine.prototype.setRefParam = function (t) {
    if (t[0] === "step") return this.setStep(+t[1], t[2], +t[3]);
    if (t[0] === "playMode" || t[0] === "slideTime") return true;   // CV-only / always Aligned here
    return this.setParam(t[0], +t[1]);
};

NoteEngine.prototype.effectiveDivisor = function () {
    var cm = Math.fround(this.p.clockMult * Math.fround(0.01));
    return Math.max(1, Math.round(Math.fround((this.p.divisor * 4) / cm)));
};

// full reset (transport start / Reset): also restarts the random generators,
// so the same song position always plays the same variation
NoteEngine.prototype.reset = function () {
    this._rng = new Random(0);
    accRandom = new Random(0);
    this.softReset();
};
// the module's own reset (Reset Measure): state only, randomness carries on
NoteEngine.prototype.softReset = function () {
    this._state = new SequenceState();
    this._currentStep = -1;
    this._prevCondition = false;
    this._pulseCounter = 0;
    this._gateOutput = false;
    this._trigGateActive = false;
    this._cvOutput = 0; this._cvOutputTarget = 0;
    this._slideActive = false;
    this._gateQueue = new SortedQueue(16);
    this._cvQueue = new SortedQueue(16);
    this.acc.reset();
};

NoteEngine.prototype.stop = function () {
    this._gateQueue.clear(); this._cvQueue.clear();
    this._gateOutput = false;
    this._emit();
};

// bar line: the module re-selects the pattern and (with its spread-retrigger
// build flag) drops queued gates. quirks = true reproduces that exactly; the
// device keeps them so a note never hangs over the bar line.
NoteEngine.prototype.barBoundary = function () {
    if (this.quirks) { this._gateQueue.clear(); this._cvQueue.clear(); }
};

NoteEngine.prototype.scaleView = function () { return new ScaleView(this.p.scale, this.p.scaleRotate); };

NoteEngine.prototype.evalStepNote = function (step, bias, scale) {
    var p = this.p;
    var note = step.note + p.octave * scale.notesPerOctave() + p.transpose;
    if (this.acc.enabled) {
        if (this.acc.mode === 1 || step.isAccumulatorTrigger()) note += this.acc.currentValue;
    }
    var prob = clamp(step.noteVariationProbability + bias, -1, 7);
    if (this._rng.nextRange(8) <= prob) {
        var offset = step.noteVariationRange === 0 ? 0 : this._rng.nextRange(Math.abs(step.noteVariationRange) + 1);
        if (step.noteVariationRange < 0) offset = -offset;
        note = clamp(note + offset, -63, 63);
    }
    return Math.fround(Math.fround(scale.noteToVolts(note)) + Math.fround(p.rootNote * Math.fround(1 / 12)));
};

NoteEngine.prototype.evalCondition = function (step, iteration, fill, prev) {
    var c = step.condition;
    switch (c) {
    case 0: return true;
    case 1: prev.v = fill; return prev.v;
    case 2: prev.v = !fill; return prev.v;
    case 3: return prev.v;
    case 4: return !prev.v;
    case 5: prev.v = iteration === 0; return prev.v;
    case 6: prev.v = iteration !== 0; return prev.v;
    }
    if (c >= CONDITION_LOOP && c < CONDITION_LAST) {
        var inv = c >= CONDITION_NOTLOOP, idx = c - (inv ? CONDITION_NOTLOOP : CONDITION_LOOP);
        prev.v = iteration % COND_BASE[idx] === COND_OFFSET[idx];
        if (inv) prev.v = !prev.v;
        return prev.v;
    }
    return true;
};

NoteEngine.prototype.tick = function (tick) {
    var p = this.p;
    var divisor = this.effectiveDivisor();
    var resetDivisor = p.resetMeasure * this.measureTicks;
    var relativeTick = resetDivisor === 0 ? tick : tick % resetDivisor;
    if (relativeTick === 0) this.softReset();

    if (relativeTick % divisor === 0) {
        if (this._state.step < 0) {
            this._state.advanceFree(p.runMode, p.firstStep, p.lastStep, this._rng);
            this._pulseCounter = 0;
        } else {
            // a step lasts (Pulse Count + 1) clock pulses before the sequence moves on
            var pc = this.steps[rotateStep(this._state.step, p.firstStep, p.lastStep, p.rotate)].pulseCount;
            if (this._pulseCounter > pc) {
                this._pulseCounter = 0;
                this._state.advanceAligned(idiv(relativeTick, divisor), p.runMode, p.firstStep, p.lastStep, this._rng);
            }
        }
        this._pulseCounter++;
        this.triggerStep(tick, divisor);
    }

    var updated = false, ev;
    while (!this._gateQueue.empty() && tick >= this._gateQueue.front().tick) {
        ev = this._gateQueue.pop();
        this._gateOutput = ev.gate;
        if (ev.gate && ev.trig) { this._trigGateActive = true; this._trigGateOff = this._ms + 6; }
        if (ev.accTick && this.acc.enabled && this.acc.triggerMode === ACC_RETRIGGER) {
            this.acc.tick();
            if (ev.gate && ev.cv !== 0) { this._cvOutputTarget = ev.cv; this._slideActive = false; updated = true; }
        }
    }
    while (!this._cvQueue.empty() && tick >= this._cvQueue.front().tick) {
        ev = this._cvQueue.pop();
        this._cvOutputTarget = ev.cv;
        this._slideActive = ev.slide;
        updated = true;
    }
    this._emit();
    return updated;
};

// once per millisecond: ends the fixed 6 ms trigger of Length-0 steps
NoteEngine.prototype.update = function () {
    if (this._trigGateActive && this._ms >= this._trigGateOff) {
        this._gateOutput = false;
        this._trigGateActive = false;
        this._emit();
    }
    this._cvOutput = this._cvOutputTarget;
    this._ms++;
};

// turn the gate/pitch state into gate events for the MIDI converter
NoteEngine.prototype._emit = function () {
    if (this._gateOutput && !this._prevGate) this.host.gate(true, this._cvOutputTarget, 100, this._slideActive);
    else if (!this._gateOutput && this._prevGate) this.host.gate(false);
    else if (this._gateOutput && this._cvOutputTarget !== this._prevCv) this.host.gate(true, this._cvOutputTarget, 100, this._slideActive);
    this._prevGate = this._gateOutput;
    this._prevCv = this._cvOutputTarget;
};

NoteEngine.prototype.triggerStep = function (tick, divisor) {
    var p = this.p, acc = this.acc, rng = this._rng;
    this._currentStep = rotateStep(this._state.step, p.firstStep, p.lastStep, p.rotate);
    var step = this.steps[this._currentStep];
    var prev = { v: this._prevCondition };
    var scale = this.scaleView();

    // accumulator, Step trigger: once per step (first pulse only)
    if (step.isAccumulatorTrigger() && this._pulseCounter === 1) {
        if (step.condition !== 0) {
            var tmp = { v: this._prevCondition };
            if (!this.evalCondition(step, this._state.iteration, !!this.fill, tmp)) {
                this.host.stepInfo(this._currentStep, p.firstStep, p.lastStep, -1, this._pulseCounter, acc.currentValue);
                return;
            }
        }
        if (acc.enabled && acc.triggerMode === ACC_STEP) acc.tickOverride(step.accumulator);
    }

    var gateOffset = Math.floor(divisor * step.gateOffset / 8);
    var stepGate = step.gate && rng.nextRange(8) <= clamp(step.gateProbability + p.gateProbabilityBias, -1, 7);
    if (stepGate) {
        stepGate = this.evalCondition(step, this._state.iteration, !!this.fill, prev);
        this._prevCondition = prev.v;
    }

    if (stepGate) {
        var fire, gm = step.gateMode, pc = step.pulseCount;
        if (gm === 1 || gm === 2) fire = this._pulseCounter === 1;          // First / Hold
        else if (gm === 3) fire = this._pulseCounter === 1 || this._pulseCounter === pc + 1;  // First+Last
        else fire = true;                                                  // All

        if (fire) {
            if (step.isAccumulatorTrigger() && acc.enabled && acc.triggerMode === ACC_GATE) acc.tickOverride(step.accumulator);

            if (step.length === 0) {
                // Length 0 = a short fixed trigger
                this._gateQueue.pushReplace({ tick: tick + gateOffset, gate: true, accTick: false, cv: 0, trig: true });
            } else {
                var length = clamp(step.length + p.lengthBias, 0, 7) + 1;
                if (rng.nextRange(8) <= step.lengthVariationProbability) {
                    var off = step.lengthVariationRange === 0 ? 0 : rng.nextRange(Math.abs(step.lengthVariationRange) + 1);
                    if (step.lengthVariationRange < 0) off = -off;
                    length = clamp(length + off, 0, 8);
                }
                var stepLength = Math.floor(divisor * length / 8);
                if (gm === 2) stepLength = divisor * (pc + 1);               // Hold: one gate over all pulses

                var retrig = rng.nextRange(8) <= clamp(step.retriggerProbability + p.retriggerProbabilityBias, -1, 7) ? step.retrigger + 1 : 1;
                var shouldTickAcc = step.isAccumulatorTrigger() && acc.enabled && acc.triggerMode === ACC_RETRIGGER;
                var cv;
                if (retrig > 1) {
                    var retrigLength = Math.floor(divisor / retrig), retrigOffset = 0, idx = 0, baseCv = 0;
                    if (shouldTickAcc) baseCv = this.evalStepNote(step, p.noteProbabilityBias, scale);
                    while (retrig-- > 0 && retrigOffset <= stepLength) {
                        cv = 0;
                        if (shouldTickAcc) {
                            var t = acc.clone();
                            for (var k = 0; k < idx; k++) t.tick();
                            cv = Math.fround(baseCv + Math.fround(scale.noteToVolts(t.currentValue)));
                        }
                        this._gateQueue.pushReplace({ tick: tick + gateOffset + retrigOffset, gate: true, accTick: shouldTickAcc, cv: cv, trig: false });
                        this._gateQueue.pushReplace({ tick: tick + gateOffset + retrigOffset + (retrigLength >> 1), gate: false, accTick: false, cv: 0, trig: false });
                        retrigOffset += retrigLength;
                        idx++;
                    }
                } else {
                    cv = 0;
                    if (shouldTickAcc) {
                        cv = this.evalStepNote(step, p.noteProbabilityBias, scale);
                        cv = Math.fround(cv + Math.fround(scale.noteToVolts(acc.currentValue)));
                    }
                    this._gateQueue.pushReplace({ tick: tick + gateOffset, gate: true, accTick: shouldTickAcc, cv: cv, trig: false });
                    this._gateQueue.pushReplace({ tick: tick + gateOffset + stepLength, gate: false, accTick: false, cv: 0, trig: false });
                }
            }
        }
    }

    var midi = -1;
    if (stepGate || p.cvUpdate === 1) {
        var note = this.evalStepNote(step, p.noteProbabilityBias, scale);
        this._cvQueue.push({ tick: tick + gateOffset, cv: note, slide: step.slide === 1 });
        if (stepGate) midi = voltsToMidi(note);
    }
    this.host.stepInfo(this._currentStep, p.firstStep, p.lastStep, midi, this._pulseCounter, acc.currentValue);
};

// ---- state for the Live set: 64 steps x 16 fields ----
NoteEngine.prototype.getState = function () {
    var out = [1];
    this.steps.forEach(function (s) { STEP_FIELDS.forEach(function (f) { out.push(s[f[0]]); }); });
    return out;
};
NoteEngine.prototype.setState = function (a) {
    if (!a || a[0] !== 1 || a.length < 1 + NOTE_STEP_COUNT * STEP_FIELDS.length) return false;
    var o = 1;
    for (var i = 0; i < NOTE_STEP_COUNT; i++)
        for (var j = 0; j < STEP_FIELDS.length; j++) {
            var f = STEP_FIELDS[j];
            this.steps[i][f[0]] = clamp(a[o++] | 0, f[1], f[2]);
        }
    return true;
};
// a brand-new device: a simple 8-step line so it makes sound straight away
NoteEngine.prototype.initFresh = function () {
    var notes = [0, 2, 4, 7, 9, 7, 4, 2];
    for (var i = 0; i < 16; i++) { this.steps[i].gate = (i % 2 === 0) ? 1 : 0; this.steps[i].note = notes[(i >> 1) % 8]; }
    this.p.lastStep = 15;
};
// one layer for the editor grid: "layer <field> v0 v1 ... v15"
NoteEngine.prototype.displayLayer = function (field, count) {
    var out = ["layer", field];
    for (var i = 0; i < (count || 16); i++) out.push(this.steps[i][field]);
    return out;
};

NoteEngine.prototype.usesMsUpdate = true;
NoteEngine.prototype.ownsResetMeasure = true;

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
