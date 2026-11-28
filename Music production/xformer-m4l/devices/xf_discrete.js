// X|FORMER discrete for Max for Live - built file, edit the sources in m4l/src
var XF_DEVICE = "discrete";
var XF_PARAMS = ["input", "scanner", "gateLength", "clockMult", "rangeHigh", "rangeLow", "octave", "transpose", "clockSource", "divisor", "thresholdMode", "scale", "rootNote", "scaleRotate", "fixedVel", "resetBars", "loop", "playMode", "legato", "resetOnStart", "rstAny", "rstPitch"];

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


// ---------------- discrete.js ----------------
// =====================================================================
//  DISCRETE MAP engine (engine/DiscreteMapTrackEngine.cpp)
//
//  A voltage sweeps up and down a range (an internal saw or triangle
//  "ramp", or an outside input). 32 stages sit on that range, each with a
//  threshold, a direction (Rise, Fall, Both or Off) and a note. When the
//  sweep crosses a stage's threshold in its direction, that stage's note
//  plays. Moving thresholds around changes the rhythm; the notes are the
//  melody. The Scanner flips stage directions as it is swept across them.
// =====================================================================
var DM_STAGES = 32;
var DM_RISE = 0, DM_FALL = 1, DM_OFF = 2, DM_BOTH = 3;
var DM_RAMP_MIN = -5, DM_RAMP_MAX = 5;
var DM_SRC_SAW = 0, DM_SRC_TRIANGLE = 1, DM_SRC_EXTERNAL = 2;

function DmStage() { this.threshold = 0; this.direction = DM_OFF; this.noteIndex = 0; }
// Stage::cycleDirection: Rise -> Fall -> Off -> Both -> Rise
var DM_DIR_ORDER = [DM_RISE, DM_FALL, DM_OFF, DM_BOTH];
DmStage.prototype.cycleDirection = function () {
    this.direction = DM_DIR_ORDER[(DM_DIR_ORDER.indexOf(this.direction) + 1) % 4];
};

var DISCRETE_RANGES = {
    clockSource: [0, 2], syncMode: [0, 1], divisor: [1, 768], clockMult: [50, 150],
    gateLength: [0, 100], loop: [0, 1], resetMeasure: [0, 128], thresholdMode: [0, 1],
    scale: [0, 13], rootNote: [0, 11], scaleRotate: [0, 31], octave: [-10, 10],
    transpose: [-60, 60], rangeHigh: [-5, 5], rangeLow: [-5, 5], input: [-5, 5], scanner: [0, 34],
    cvUpdate: [0, 1], playMode: [0, 1]
};
var DISCRETE_FLOAT = { rangeHigh: true, rangeLow: true, input: true, scanner: true };

function DiscreteEngine(host) {
    this.host = host;
    this.p = {
        clockSource: 0, syncMode: 0, divisor: 192, clockMult: 100, gateLength: 0, loop: 1,
        resetMeasure: 8, thresholdMode: 0, scale: 1, rootNote: 0, scaleRotate: 0, octave: 0,
        transpose: 0, rangeHigh: 5, rangeLow: -5, input: 0, scanner: 0, cvUpdate: 0,
        playMode: 1     // 0 = Aligned (sweep locked to the Reset Measure bar grid), 1 = Free (module default)
    };
    this.stages = [];
    for (var i = 0; i < DM_STAGES; i++) this.stages.push(new DmStage());
    this.clearThresholds();
    this.measureTicks = 768;
    this._lengthTh = []; this._posTh = [];
    this.reset();
}

// the module's default "fret" layout: thresholds interleaved across 4 pages
DiscreteEngine.prototype.clearThresholds = function () {
    var step = Math.fround(200 / 31);
    for (var i = 0; i < DM_STAGES; i++) {
        var page = (i >> 3) + 1, button = i % 8;
        var g = button * 4 + (page - 1);
        var value = Math.fround(-100 + Math.fround(g * step));
        this.stages[i].threshold = clamp(Math.trunc(Math.fround(value + 0.5)), -100, 100);
    }
};

DiscreteEngine.prototype.setParam = function (name, value) {
    var r = DISCRETE_RANGES[name];
    if (!r) return false;
    value = DISCRETE_FLOAT[name] ? Math.fround(clamp(+value, r[0], r[1])) : clamp(Math.round(value), r[0], r[1]);
    this.p[name] = value;
    return true;
};

// stage fields: threshold -100..100, direction 0..3, noteIndex -63..63
DiscreteEngine.prototype.setStage = function (i, field, value) {
    if (i < 0 || i >= DM_STAGES) return false;
    var s = this.stages[i];
    value = Math.round(value);
    if (field === "threshold") s.threshold = clamp(value, -100, 100);
    else if (field === "direction") s.direction = clamp(value, 0, 3);
    else if (field === "noteIndex") s.noteIndex = clamp(value, -63, 63);
    else return false;
    this._thresholdsDirty = true;
    if (this.host.stateChanged) this.host.stateChanged();
    return true;
};

DiscreteEngine.prototype.setRefParam = function (t) {
    if (t[0] === "stage") return this.setStage(+t[1], t[2], +t[3]);
    if (t[0] === "slewTime" || t[0] === "pluck" || t[0] === "offset") return true;
    if (t[0] === "rangeHigh" || t[0] === "rangeLow") return this.setParam(t[0], +t[1] / 100);   // harness gives centivolts
    return this.setParam(t[0], +t[1]);
};

DiscreteEngine.prototype.rangeSpan = function () {
    var span = Math.fround(this.p.rangeHigh - this.p.rangeLow);
    if (Math.abs(span) < 0.01) return span >= 0 ? Math.fround(0.01) : Math.fround(-0.01);
    return span;
};

// transport start / Reset: also forget what was sounding
DiscreteEngine.prototype.reset = function () { this._prevGate = false; this._prevCv = 0; this._retrig = false; this._resetState(); };
// the module's own reset (Loop switched on, Reset Measure)
DiscreteEngine.prototype._resetState = function () {
    var p = this.p;
    this._rampPhase = 0; this._rampValue = DM_RAMP_MIN;
    this._prevInput = Math.fround(-5.001); this._currentInput = DM_RAMP_MIN;
    this._resetTickOffset = 0;
    this._prevLoop = !!p.loop;
    this._activeStage = -1;
    this._cvOutput = 0; this._targetCv = 0;
    this._gateTimer = 0;
    this._running = true;
    this._thresholdsDirty = true;
    this._lastScannerSegment = -1;
    this._prevRangeHigh = p.rangeHigh; this._prevRangeLow = p.rangeLow;
    this._prevThresholdMode = p.thresholdMode;
    this._sampledOctave = p.octave; this._sampledTranspose = p.transpose; this._sampledRoot = p.rootNote;
    this._extOnceArmed = false; this._extOnceDone = false;
    this._extMin = 0; this._extMax = 0;
};

DiscreteEngine.prototype.stop = function () { this._gateTimer = 0; this._emit(); };

DiscreteEngine.prototype.scaledDivisorTicks = function () {
    var cm = Math.fround(this.p.clockMult * Math.fround(0.01));
    var d = this.p.divisor * 4;
    return Math.max(1, lround(Math.fround(d / cm)));
};

DiscreteEngine.prototype.tick = function (tick) {
    var p = this.p, F = Math.fround;
    if (!this._prevLoop && p.loop) { this._resetState(); this._resetTickOffset = tick; }
    if (this._prevLoop && !p.loop) { this._extOnceArmed = false; this._extOnceDone = false; this._extMin = 0; this._extMax = 0; }
    this._prevLoop = !!p.loop;

    if (Math.abs(p.rangeHigh - this._prevRangeHigh) > 1e-6 || Math.abs(p.rangeLow - this._prevRangeLow) > 1e-6) {
        this._thresholdsDirty = true;
        this._prevRangeHigh = p.rangeHigh; this._prevRangeLow = p.rangeLow;
    }
    if (p.thresholdMode !== this._prevThresholdMode) { this._thresholdsDirty = true; this._prevThresholdMode = p.thresholdMode; }

    var rel = u32(tick - this._resetTickOffset);
    if (p.syncMode === 1) {
        var rd = p.resetMeasure * this.measureTicks;
        if (rd > 0 && rel % rd === 0) { this._resetState(); this._resetTickOffset = tick; rel = 0; }
    }

    // 1. the sweep (Aligned: locked to the bar, period = Divisor)
    if (p.clockSource !== DM_SRC_EXTERNAL) {
        var period = this.scaledDivisorTicks();
        var rd2 = p.resetMeasure * this.measureTicks;
        // Free counts from the last reset; Aligned also restarts the sweep every Reset Measure bars
        var align = (p.playMode === 0 && rd2 > 0) ? rel % rd2 : rel;
        if (this._running || p.loop) {
            this._rampPhase = F((align % period) / period);
            if (p.clockSource === DM_SRC_TRIANGLE) {
                var tri = this._rampPhase < 0.5 ? F(this._rampPhase * 2) : F(1 - F(F(this._rampPhase - 0.5) * 2));
                this._rampValue = F(DM_RAMP_MIN + F(tri * 10));
            } else this._rampValue = F(DM_RAMP_MIN + F(this._rampPhase * 10));
        }
        if (!p.loop && this._running && (rel % period) + 1 >= period) {
            this._running = false; this._rampPhase = 1; this._rampValue = DM_RAMP_MAX;
        }
        this._currentInput = this._rampValue;
    } else {
        this._currentInput = p.input;
    }

    var freeze = this.updateExternalOnce();

    // the Scanner: entering one of 32 zones flips that stage's direction
    var seg = Math.trunc(clamp(p.scanner, 0, 34));
    if (seg !== this._lastScannerSegment) {
        if (seg >= 1 && seg <= 32) { this.stages[seg - 1].cycleDirection(); if (this.host.stateChanged) this.host.stateChanged(); }
        this._lastScannerSegment = seg;
    }

    if (this._thresholdsDirty) {
        if (p.thresholdMode === 1) this.recalcLength(); else this.recalcPosition();
        this._thresholdsDirty = false;
    }

    var newStage = freeze ? this._activeStage : this.findActiveStage(this._currentInput, this._prevInput);
    var gateChanged = newStage !== this._activeStage;
    if (this._gateTimer > 0) this._gateTimer--;
    this._activeStage = newStage;

    if (gateChanged && this._activeStage >= 0) {
        if (p.cvUpdate === 0) { this._sampledOctave = p.octave; this._sampledTranspose = p.transpose; this._sampledRoot = p.rootNote; }
        var stepTicks = this.scaledDivisorTicks();
        this._gateTimer = p.gateLength === 0 ? 3 : Math.floor((stepTicks * p.gateLength) / 100);
        this._retrig = true;
    }

    if (this._activeStage >= 0 || p.cvUpdate === 1) {
        if (this._activeStage >= 0) this._targetCv = this.noteToVolts(this.stages[this._activeStage].noteIndex);
        else this._targetCv = 0;
        this._cvOutput = this._targetCv;
    }
    if (gateChanged && this._activeStage >= 0)
        this.host.stepInfo(this._activeStage, voltsToMidi(this._targetCv), this._currentInput);
    this._prevInput = this._currentInput;
    this._emit();
    return false;
};

DiscreteEngine.prototype.updateExternalOnce = function () {
    var p = this.p;
    if (!(p.clockSource === DM_SRC_EXTERNAL && !p.loop)) return false;
    var lo = Math.min(p.rangeLow, p.rangeHigh), hi = Math.max(p.rangeLow, p.rangeHigh);
    var span = Math.max(0.01, Math.abs(this.rangeSpan()));
    if (!this._extOnceDone) {
        if (this._extOnceArmed) {
            this._extMin = Math.min(this._extMin, this._currentInput);
            this._extMax = Math.max(this._extMax, this._currentInput);
            if (Math.fround((this._extMax - this._extMin) / span) >= Math.fround(0.9)) { this._extOnceArmed = false; this._extOnceDone = true; }
        } else {
            var tol = Math.fround(span * Math.fround(0.05));
            if (this._currentInput >= lo - tol && this._currentInput <= hi + tol) {
                this._extOnceArmed = true; this._extMin = this._extMax = this._currentInput;
            }
        }
    }
    return this._extOnceDone || !this._extOnceArmed;
};

DiscreteEngine.prototype.recalcPosition = function () {
    var F = Math.fround, minV = this.p.rangeLow, spanV = F(this.p.rangeHigh - minV);
    for (var i = 0; i < DM_STAGES; i++) {
        var norm = F((this.stages[i].threshold + 100) / F(200));
        this._posTh[i] = F(minV + F(norm * spanV));
    }
};
DiscreteEngine.prototype.recalcLength = function () {
    var F = Math.fround, p = this.p, total = 0, i;
    for (i = 0; i < DM_STAGES; i++) total += this.stages[i].threshold + 100;
    var span = F(p.rangeHigh - p.rangeLow);
    if (total === 0) {
        for (i = 0; i < DM_STAGES; i++) this._lengthTh[i] = F(p.rangeLow + F(F(F(i + 1) / F(DM_STAGES)) * span));
        return;
    }
    var v = p.rangeLow;
    for (i = 0; i < DM_STAGES; i++) {
        var prop = F((this.stages[i].threshold + 100) / F(total));
        v = F(v + F(prop * span));
        this._lengthTh[i] = v;
    }
};
DiscreteEngine.prototype.threshold = function (i) { return this.p.thresholdMode === 1 ? this._lengthTh[i] : this._posTh[i]; };

// first stage whose threshold the sweep crossed (in that stage's direction) wins
DiscreteEngine.prototype.findActiveStage = function (input, prev) {
    for (var i = 0; i < DM_STAGES; i++) {
        var s = this.stages[i], th = this.threshold(i), crossed = false;
        if (s.direction === DM_RISE) crossed = prev < th && input >= th;
        else if (s.direction === DM_FALL) crossed = prev > th && input <= th;
        else if (s.direction === DM_BOTH) crossed = (prev < th && input >= th) || (prev > th && input <= th);
        if (crossed) return i;
    }
    if (this._activeStage >= 0 && this.stages[this._activeStage].direction === DM_OFF) return -1;
    return this._activeStage;
};

DiscreteEngine.prototype.scaleView = function () { return new ScaleView(this.p.scale, this.p.scaleRotate); };
DiscreteEngine.prototype.noteToVolts = function (noteIndex) {
    var s = this.scaleView(), p = this.p, sampled = p.cvUpdate === 0;
    var oct = sampled ? this._sampledOctave : p.octave, tr = sampled ? this._sampledTranspose : p.transpose;
    var root = sampled ? this._sampledRoot : p.rootNote;
    var shift = oct * s.notesPerOctave() + tr;
    return Math.fround(Math.fround(s.noteToVolts(noteIndex + shift)) + Math.fround(root * Math.fround(1 / 12)));
};

DiscreteEngine.prototype.update = function () {};

// gate = timer running on an active stage; every new stage is a new note
DiscreteEngine.prototype._emit = function () {
    var g = this._gateTimer > 0 && this._activeStage >= 0, cv = this._cvOutput;
    if (g && this._retrig && this._prevGate) { this.host.gate(false); this._prevGate = false; }
    this._retrig = false;
    if (g && !this._prevGate) this.host.gate(true, cv, 100, false);
    else if (!g && this._prevGate) this.host.gate(false);
    else if (g && cv !== this._prevCv) this.host.gate(true, cv, 100, false);
    this._prevGate = g; this._prevCv = cv;
    this._gateOutput = g;
};

// ---- state for the Live set: 32 stages x (threshold, direction, note) ----
DiscreteEngine.prototype.getState = function () {
    var out = [1];
    this.stages.forEach(function (s) { out.push(s.threshold, s.direction, s.noteIndex); });
    return out;
};
DiscreteEngine.prototype.setState = function (a) {
    if (!a || a[0] !== 1 || a.length < 1 + DM_STAGES * 3) return false;
    for (var i = 0; i < DM_STAGES; i++) {
        var s = this.stages[i], o = 1 + i * 3;
        s.threshold = clamp(a[o] | 0, -100, 100); s.direction = clamp(a[o + 1] | 0, 0, 3); s.noteIndex = clamp(a[o + 2] | 0, -63, 63);
    }
    this._thresholdsDirty = true;
    return true;
};
// a brand-new device: 8 stages on rising edges spread across the sweep, a scale run
DiscreteEngine.prototype.initFresh = function () {
    var notes = [0, 2, 4, 1, 5, 3, 7, 4];
    for (var i = 0; i < DM_STAGES; i++) {
        var s = this.stages[i];
        s.direction = DM_OFF; s.noteIndex = 0;
        if (i < 8) { s.threshold = -100 + i * 25; s.direction = DM_RISE; s.noteIndex = notes[i]; }
    }
    this._thresholdsDirty = true;
};

DiscreteEngine.prototype.usesMsUpdate = false;

// Reset Bars on the device = the module's Sync: Reset Measure
DiscreteEngine.prototype.ownsResetMeasure = true;
DiscreteEngine.prototype.setResetBars = function (n) {
    this.p.resetMeasure = clamp(n | 0, 0, 128);
    this.p.syncMode = n > 0 ? 1 : 0;
};

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
