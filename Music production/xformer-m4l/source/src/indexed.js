// =====================================================================
//  INDEXED engine (engine/IndexedTrackEngine.cpp, model/IndexedSequence.h)
//
//  A list of up to 48 steps. Every step has its own duration (in 1/192
//  notes, times the Divisor), its own gate length and a scale-note index.
//  Durations don't have to add up to a bar, so the pattern drifts against
//  the beat unless you want it to. Two modulation inputs (A and B) can
//  stretch durations, gates or transpose notes on chosen step groups.
// =====================================================================
var INDEXED_MAX_STEPS = 48;
var INDEXED_MAX_DURATION = 1023;
var INDEXED_GATE_MAX = 1023;

// per-step fields: [name, min, max, default]
var INDEXED_FIELDS = [
    ["duration", 0, 1023, 0], ["gateLength", 0, 1023, 0], ["noteIndex", -63, 63, 0],
    ["slide", 0, 1, 0], ["groupMask", 0, 15, 0]
];

function IndexedStep() { this.duration = 0; this.gateLength = 0; this.noteIndex = 0; this.slide = 0; this.groupMask = 0; }
// the model keeps the gate inside the duration whenever either changes
IndexedStep.prototype.setDuration = function (v) {
    this.duration = clamp(v | 0, 0, INDEXED_MAX_DURATION);
    this.setGateLength(this.gateLength);
};
IndexedStep.prototype.setGateLength = function (v) {
    v = clamp(v | 0, 0, INDEXED_GATE_MAX);
    if (this.duration === 0) v = 0;
    else if (v > this.duration) v = this.duration;
    this.gateLength = v;
};
IndexedStep.prototype.copy = function () {
    var s = new IndexedStep();
    s.duration = this.duration; s.gateLength = this.gateLength; s.noteIndex = this.noteIndex;
    s.slide = this.slide; s.groupMask = this.groupMask;
    return s;
};

// route: which inputs modulate what. targetGroups: 0 = all steps,
// 0x10 = steps in no group, else a bit mask of groups A-D
var IDX_TARGET_DURATION = 0, IDX_TARGET_GATE = 1, IDX_TARGET_NOTE = 2;
var IDX_COMBINE_ATOB = 0, IDX_COMBINE_MUX = 1, IDX_COMBINE_MIN = 2, IDX_COMBINE_MAX = 3;
function IndexedRoute() { this.targetGroups = 0; this.targetParam = IDX_TARGET_DURATION; this.amount = 100; this.source = 0; }

var INDEXED_RANGES = {
    divisor: [1, 768], clockMult: [50, 150], loop: [0, 1], runMode: [0, 5], activeLength: [1, 48],
    scale: [0, 13], rootNote: [0, 11], scaleRotate: [0, 31], firstStep: [0, 47],
    syncMode: [0, 1], resetMeasure: [0, 128], octave: [-10, 10], transpose: [-60, 60],
    routeCombine: [0, 3], modA: [-100, 100], modB: [-100, 100],
    routeASource: [0, 2], routeAGroups: [0, 0x1f], routeATarget: [0, 2], routeAAmount: [-200, 200],
    routeBSource: [0, 2], routeBGroups: [0, 0x1f], routeBTarget: [0, 2], routeBAmount: [-200, 200]
};

function IndexedEngine(host) {
    this.host = host;
    this.p = {
        divisor: 1, clockMult: 100, loop: 1, runMode: 0, activeLength: 5, scale: 1, rootNote: 0,
        scaleRotate: 0, firstStep: 0, syncMode: 0, resetMeasure: 0, octave: 0, transpose: 0,
        routeCombine: 0, modA: 0, modB: 0
    };
    this.routeA = new IndexedRoute();
    this.routeB = new IndexedRoute();
    this.steps = [];
    for (var i = 0; i < INDEXED_MAX_STEPS; i++) this.steps.push(new IndexedStep());
    // the module's empty pattern: 5 silent steps spread over one bar
    for (i = 0; i < 5; i++) this.steps[i].setDuration(i < 3 ? 154 : 153);
    this.measureTicks = 768;
    this._rng = new Random(0);
    this._tick = 0;
    this.reset();
}

IndexedEngine.prototype.setParam = function (name, value) {
    var r = INDEXED_RANGES[name], p = this.p;
    if (!r) return false;
    value = clamp(Math.round(value), r[0], r[1]);
    var m = /^route([AB])(Source|Groups|Target|Amount)$/.exec(name);
    if (m) {
        var route = m[1] === "A" ? this.routeA : this.routeB;
        var key = { Source: "source", Groups: "targetGroups", Target: "targetParam", Amount: "amount" }[m[2]];
        route[key] = value;
        return true;
    }
    if (name === "activeLength") {
        p.activeLength = value;
        if (p.firstStep >= value) p.firstStep = value - 1;
        return true;
    }
    if (name === "firstStep") value = clamp(value, 0, p.activeLength - 1);
    p[name] = value;
    return true;
};

IndexedEngine.prototype.setStep = function (i, field, value) {
    if (i < 0 || i >= INDEXED_MAX_STEPS) return false;
    var s = this.steps[i];
    value = Math.round(value);
    if (field === "duration") s.setDuration(value);
    else if (field === "gateLength") s.setGateLength(value);
    else if (field === "noteIndex") s.noteIndex = clamp(value, -63, 63);
    else if (field === "slide") s.slide = value ? 1 : 0;
    else if (field === "groupMask") s.groupMask = value & 15;
    else return false;
    if (this.host.stateChanged) this.host.stateChanged();
    return true;
};

// scenario lines from the firmware test harness
IndexedEngine.prototype.setRefParam = function (t) {
    if (t[0] === "step") return this.setStep(+t[1], t[2], +t[3]);
    if (t[0] === "playMode" || t[0] === "slideTime" || t[0] === "cvUpdate") return true;
    return this.setParam(t[0], +t[1]);
};

// the clock position: the module reads a wall-clock estimate, the device
// uses the exact tick (the test harness swaps in the module's estimate)
IndexedEngine.prototype.tickPos = function () {
    return this.host.tickPos ? this.host.tickPos(this._tick) : this._tick;
};

IndexedEngine.prototype.resetSequenceState = function () {
    var p = this.p;
    this._state = new SequenceState();
    this._stepsRemaining = Math.max(0, p.activeLength - 1);
    this._currentStepIndex = 0;
    this._stepTimer = 0;
    this._gateTimer = 0;
    this._effectiveStepDuration = 0;
    this._cvOutput = 0; this._cvOutputTarget = 0;
    this._running = true;
    this._slideActive = false;
    this._lastFreeTickPos = this.tickPos();
    if (p.activeLength > 0) {
        this._state.advanceFree(p.runMode, 0, p.activeLength - 1, this._rng);
        this._currentStepIndex = this._state.step;
    }
    this._pendingTrigger = true;
};

IndexedEngine.prototype.reset = function () {
    this._rng = new Random(0);
    this._tick = 0;
    this._prevGate = false; this._prevCv = 0; this._retrig = false;
    this.resetSequenceState();
};

IndexedEngine.prototype.stop = function () {
    this._gateTimer = 0;
    this._emit();
};

IndexedEngine.prototype.tick = function (tick) {
    var p = this.p;
    this._tick = tick;
    if (p.syncMode === 1) {
        var resetDivisor = p.resetMeasure * this.measureTicks;
        if (resetDivisor > 0 && tick % resetDivisor === 0) this.resetSequenceState();
    }
    if (!this._running) { this._emit(); return false; }

    if (this._pendingTrigger) { this.triggerStep(); this._pendingTrigger = false; }

    var stepDuration = this._effectiveStepDuration & 0xffff;
    var clockMult = Math.fround(p.clockMult * Math.fround(0.01));
    if (stepDuration === 0) {
        // zero-length steps: rush through them
        this.advanceStep();
        this.triggerStep();
        this._emit();
        return false;
    }
    var tickPos = this.tickPos();
    var delta = tickPos - this._lastFreeTickPos;
    if (delta < 0) delta = 0;
    this._lastFreeTickPos = tickPos;
    var scaled = delta * clockMult;
    if (this._effectiveStepDuration > 0) {
        var maxDelta = this._effectiveStepDuration * 2;
        if (scaled > maxDelta) scaled = maxDelta;
    }
    if (this._gateTimer > 0) {
        this._gateTimer -= scaled;
        if (this._gateTimer <= 0) this._gateTimer = 0;
    }
    this._stepTimer += scaled;
    if (this._stepTimer >= stepDuration) {
        this.advanceStep();
        this.triggerStep();
    }
    this._emit();
    return false;
};

IndexedEngine.prototype.update = function () { this._cvOutput = this._cvOutputTarget; };

IndexedEngine.prototype.advanceStep = function () {
    var p = this.p, n = p.activeLength;
    if (n <= 0) return;
    if (!p.loop && this._stepsRemaining <= 0) { this._running = false; this._stepTimer = 0; return; }
    var skip = 0;
    while (skip < n) {
        this._state.advanceFree(p.runMode, 0, n - 1, this._rng);
        this._currentStepIndex = this._state.step;
        if (!p.loop) this._stepsRemaining--;
        var eff = (this._currentStepIndex + p.firstStep) % n;
        if (this.steps[eff].duration > 0) break;
        skip++;
        if (!p.loop && this._stepsRemaining <= 0) { this._running = false; this._stepTimer = 0; return; }
    }
    this._stepTimer = 0;
};

function idxStepMatches(step, route) {
    var g = route.targetGroups;
    return g === 0 || (g === 0x10 && step.groupMask === 0) || (step.groupMask & g) !== 0;
}

// route modulation for one target (IndexedTrackEngine.cpp applyStepModulation)
IndexedEngine.prototype.applyMod = function (target, cvA, cvB, aActive, bActive, combine, scaleSize, v) {
    var F = Math.fround, rA = this.routeA, rB = this.routeB;
    function amountFor(r, cv) {
        if (target === IDX_TARGET_DURATION) return F(cv * F(r.amount * F(0.01)));
        if (target === IDX_TARGET_NOTE) return F(cv * F(F(r.amount * F(0.01)) * scaleSize));
        return F(cv * r.amount);
    }
    var modA = aActive && rA.targetParam === target ? amountFor(rA, cvA) : 0;
    var modB = bActive && rB.targetParam === target ? amountFor(rB, cvB) : 0;
    var fin;
    if (combine && aActive && bActive) {
        var mode = this.p.routeCombine;
        if (mode === IDX_COMBINE_MUX) fin = F(F(0.5) * F(modA + modB));
        else if (mode === IDX_COMBINE_MIN) fin = Math.min(modA, modB);
        else if (mode === IDX_COMBINE_MAX) fin = Math.max(modA, modB);
        else fin = modA;
    } else fin = F(modA + modB);
    if (fin === 0) return;
    if (target === IDX_TARGET_DURATION) {
        v.duration = clamp(lround(F(v.duration * F(1 + fin))), 0, INDEXED_MAX_DURATION);
    } else if (target === IDX_TARGET_GATE) {
        var t = clamp(v.gate + lround(fin), 0, INDEXED_GATE_MAX);
        if (v.duration > 0) t = Math.min(t, v.duration);
        v.gate = t;
    } else {
        v.note = clamp(Math.trunc(F(v.note + fin)), -63, 64);
    }
};

IndexedEngine.prototype.triggerStep = function () {
    var p = this.p;
    var eff = (this._currentStepIndex + p.firstStep) % p.activeLength;
    var step = this.steps[eff];
    var v = { duration: step.duration, gate: step.gateLength, note: step.noteIndex };

    var rA = this.routeA, rB = this.routeB;
    var aActive = rA.source !== 0 && idxStepMatches(step, rA);
    var bActive = rB.source !== 0 && idxStepMatches(step, rB);
    if (aActive || bActive) {
        var cvA = Math.fround(p.modA * Math.fround(0.01)), cvB = Math.fround(p.modB * Math.fround(0.01));
        var aVal = rA.source === 1 ? cvA : cvB, bVal = rB.source === 1 ? cvA : cvB;
        var size = this.scaleView().notesPerOctave();
        var combine = p.routeCombine !== IDX_COMBINE_ATOB && aActive && bActive && rA.targetParam === rB.targetParam;
        for (var target = 0; target < 3; target++) this.applyMod(target, aVal, bVal, aActive, bActive, combine, size, v);
    }
    if (v.duration === 0) v.gate = 0;
    else if (v.gate > v.duration) v.gate = v.duration;

    var effectiveDuration = u32(v.duration * p.divisor);
    var gateTicks = u32(v.gate * p.divisor);
    if (effectiveDuration > 0) gateTicks = Math.min(gateTicks, effectiveDuration);
    if (gateTicks > 0 && this._gateTimer > 0 && !step.slide) this._retrig = true;   // a new note, not a tie
    this._gateTimer = gateTicks;
    this._effectiveStepDuration = effectiveDuration;

    var midi = -1;
    if (gateTicks > 0) {
        this._cvOutputTarget = this.noteToVolts(v.note);
        this._slideActive = !!step.slide;
        midi = voltsToMidi(this._cvOutputTarget);
    } else this._slideActive = false;
    this.host.stepInfo(eff, p.activeLength, p.firstStep, midi, effectiveDuration);
};

IndexedEngine.prototype.scaleView = function () { return new ScaleView(this.p.scale, this.p.scaleRotate); };
IndexedEngine.prototype.noteToVolts = function (noteIndex) {
    var s = this.scaleView(), p = this.p;
    var shift = p.octave * s.notesPerOctave() + p.transpose;
    return Math.fround(Math.fround(s.noteToVolts(noteIndex + shift)) + Math.fround(p.rootNote * Math.fround(1 / 12)));
};

// gate/pitch edges -> host (a retriggered step ends the old note first)
IndexedEngine.prototype._emit = function () {
    var g = this._running && this._gateTimer > 0, cv = this._cvOutputTarget;
    if (g && this._retrig && this._prevGate) { this.host.gate(false); this._prevGate = false; }
    this._retrig = false;
    if (g && !this._prevGate) this.host.gate(true, cv, 100, this._slideActive);
    else if (!g && this._prevGate) this.host.gate(false);
    else if (g && cv !== this._prevCv) this.host.gate(true, cv, 100, this._slideActive);
    this._prevGate = g; this._prevCv = cv;
    this._gateOutput = g;
};

// ---- state for the Live set ----
IndexedEngine.prototype.getState = function () {
    var out = [1, this.routeA.source, this.routeA.targetGroups, this.routeA.targetParam, this.routeA.amount,
        this.routeB.source, this.routeB.targetGroups, this.routeB.targetParam, this.routeB.amount];
    this.steps.forEach(function (s) { out.push(s.duration, s.gateLength, s.noteIndex, s.slide, s.groupMask); });
    return out;
};
IndexedEngine.prototype.setState = function (a) {
    if (!a || a[0] !== 1 || a.length < 9 + INDEXED_MAX_STEPS * 5) return false;
    var rA = this.routeA, rB = this.routeB;
    rA.source = a[1] | 0; rA.targetGroups = a[2] | 0; rA.targetParam = a[3] | 0; rA.amount = a[4] | 0;
    rB.source = a[5] | 0; rB.targetGroups = a[6] | 0; rB.targetParam = a[7] | 0; rB.amount = a[8] | 0;
    for (var i = 0; i < INDEXED_MAX_STEPS; i++) {
        var s = this.steps[i], o = 9 + i * 5;
        s.duration = clamp(a[o] | 0, 0, INDEXED_MAX_DURATION);
        s.gateLength = 0; s.setGateLength(a[o + 1] | 0);
        s.noteIndex = clamp(a[o + 2] | 0, -63, 63); s.slide = a[o + 3] ? 1 : 0; s.groupMask = (a[o + 4] | 0) & 15;
    }
    return true;
};
// a brand-new device: 8 steps over one bar, a long-short rhythm with a small melody
IndexedEngine.prototype.initFresh = function () {
    var durs = [144, 48, 96, 96, 144, 48, 96, 96], notes = [0, 2, 4, 2, 5, 4, 2, 7];
    for (var i = 0; i < INDEXED_MAX_STEPS; i++) {
        var s = this.steps[i];
        s.duration = 0; s.gateLength = 0; s.noteIndex = 0; s.slide = 0; s.groupMask = 0;
        if (i < 8) { s.setDuration(durs[i]); s.setGateLength(Math.round(durs[i] * 0.6)); s.noteIndex = notes[i]; }
    }
    this.p.activeLength = 8;
    if (this.p.firstStep >= 8) this.p.firstStep = 0;
};

IndexedEngine.prototype.usesMsUpdate = false;

// Reset Bars on the device = the module's Sync: Reset Measure
IndexedEngine.prototype.ownsResetMeasure = true;
IndexedEngine.prototype.setResetBars = function (n) {
    this.p.resetMeasure = clamp(n | 0, 0, 128);
    this.p.syncMode = n > 0 ? 1 : 0;
};
