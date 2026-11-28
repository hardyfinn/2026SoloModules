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
