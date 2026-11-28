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
