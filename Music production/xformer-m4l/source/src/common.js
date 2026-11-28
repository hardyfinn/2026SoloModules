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

