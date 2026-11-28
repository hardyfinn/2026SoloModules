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
