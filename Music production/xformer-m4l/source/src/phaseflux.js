// =====================================================================
//  PHASEFLUX engine (engine/PhaseFluxTrackEngine.cpp, model/PhaseFluxMath.cpp,
//  model/PhaseFluxSequence.h)
//
//  16 stages on a 4x4 grid, walked in one of 8 paths (snake, rows,
//  spiral...). Each stage lasts Length x Divisor and fires a burst of
//  0-16 pulses inside that time. Two curves shape each burst:
//    - the temporal curve decides WHEN the pulses fall (even, bunched in
//      the middle, bouncing towards the end...),
//    - the pitch curve decides WHICH note each pulse plays (a ramp, a bell,
//      a triangle...), over a range of 1/2 to 3 octaves above, below or
//      around the stage's base pitch.
//  Warp and Response bend both curves, Repeat and Window fold them, Mask
//  thins the pulses, and two accumulators let pitch and pulse count drift
//  from one pass to the next.
// =====================================================================
var PF_STAGES = 16;
var PF_MAX_PULSES = 16;
var PF_MIN_PULSE_GATE = 6;

// the 8 walks across the 4x4 grid: slot -> stage
var PF_TRAVERSALS = [
    [0, 1, 2, 3, 7, 6, 5, 4, 8, 9, 10, 11, 15, 14, 13, 12],
    [12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3],
    [12, 13, 14, 15, 11, 10, 9, 8, 4, 5, 6, 7, 3, 2, 1, 0],
    [15, 11, 7, 3, 14, 10, 6, 2, 13, 9, 5, 1, 12, 8, 4, 0],
    [0, 4, 8, 12, 13, 9, 5, 1, 2, 6, 10, 14, 15, 11, 7, 3],
    [12, 13, 14, 15, 11, 7, 3, 2, 1, 0, 4, 8, 9, 10, 6, 5],
    [9, 5, 6, 10, 8, 4, 0, 1, 2, 3, 7, 11, 15, 14, 13, 12],
    [12, 8, 13, 4, 9, 14, 0, 5, 10, 15, 1, 6, 11, 2, 7, 3]
];
var PF_TRAVERSAL_NAMES = ["Snake", "Rows", "SnakeUp", "Cols", "ColSnake", "Spiral", "Inward", "Diag"];
// Mask: a set bit mutes that pulse. Off, 1in2, 1in3, 1in4, 2in3, 2in4, 3in4, 1in8
var PF_MASKS = [0x00, 0xAA, 0x49, 0x11, 0xB6, 0x55, 0x77, 0x01];
var PF_MASK_NAMES = ["Off", "1in2", "1in3", "1in4", "2in3", "2in4", "3in4", "1in8"];
// Pitch Rate (Global pitch mode): how fast the pitch curve runs against the stages
var PF_RATES = [[1, 7], [1, 5], [1, 3], [2, 5], [3, 5], [5, 7], [7, 9], [11, 12], [16, 17], [1, 1],
    [18, 17], [13, 12], [11, 9], [9, 7], [7, 5], [5, 3], [9, 5]];
var PF_RATE_NAMES = PF_RATES.map(function (r) { return r[0] + ":" + r[1]; });
var PF_REPEAT = [1, 2, 3, 5];

// per-stage fields: [name, min, max, default]
var PF_FIELDS = [
    ["pulseCount", 0, 16, 0], ["length", 1, 127, 4], ["skip", 0, 1, 0], ["gateLength", 0, 100, 50],
    ["basePitch", -64, 63, 0], ["pitchRange", 0, 3, 1], ["pitchDirection", 0, 2, 0], ["pitchCurve", 0, 3, 0],
    ["pitchFlipV", 0, 1, 0], ["pitchFlipH", 0, 1, 0], ["pitchWarp", -64, 63, 0], ["pitchResponse", -64, 63, 0],
    ["temporalCurve", 0, 2, 0], ["temporalFlipV", 0, 1, 0], ["temporalFlipH", 0, 1, 0],
    ["temporalWarp", -64, 63, 0], ["temporalResponse", -64, 63, 0],
    ["maskMelody", 0, 100, 100], ["tiltMelody", 0, 100, 0], ["phaseShift", 0, 7, 0], ["mask", 0, 7, 0],
    ["maskShift", 0, 7, 0], ["accumulatorStep", -16, 15, 0], ["pulseAccumStep", -8, 7, 0],
    ["accumulatorTrigger", 0, 1, 0], ["pulseAccumTrigger", 0, 1, 0],
    ["temporalRepeat", 0, 3, 0], ["pitchRepeat", 0, 3, 0], ["temporalWindow", 0, 4, 0], ["pitchWindow", 0, 4, 0]
];
var PF_FIELD_INDEX = {};
PF_FIELDS.forEach(function (f, i) { PF_FIELD_INDEX[f[0]] = i; });

function PfStage() { this.clear(); }
PfStage.prototype.clear = function () {
    for (var i = 0; i < PF_FIELDS.length; i++) this[PF_FIELDS[i][0]] = PF_FIELDS[i][3];
};

var PHASEFLUX_RANGES = {
    divisor: [1, 768], clockMult: [50, 150], cycleLength: [0, 1], traversal: [0, 7], pitchMode: [0, 1],
    pitchRate: [0, 16], globalPhase: [0, 100], warpNudge: [-64, 64], responseNudge: [-64, 64],
    cyclePhaseWarp: [-64, 64], pulseNudge: [-15, 15], lenNudge: [-64, 64], resetMeasure: [0, 128],
    firstStage: [0, 15], lastStage: [0, 15], scale: [0, 13], rootNote: [0, 11], scaleRotate: [0, 31],
    octave: [-10, 10], transpose: [-60, 60],
    noteAccScope: [0, 1], noteAccOrder: [0, 3], noteAccPolarity: [0, 1], noteAccReset: [0, 15],
    noteAccPos: [0, 255], noteAccNeg: [0, 255],
    pulseAccScope: [0, 1], pulseAccOrder: [0, 3], pulseAccPolarity: [0, 1], pulseAccReset: [0, 15],
    pulseAccPos: [0, 255], pulseAccNeg: [0, 255]
};

// ---- the curve maths (float, like the module) ----
var PF_TWO_PI = Math.fround(2 * Math.fround(3.1415926536));
function pfClamp01(v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
function pfPowerBend(z, knob) {
    var p = Math.fround(knob / 64);
    var e = Math.fround(Math.fround(1 - p) / Math.fround(1 + p));
    return Math.fround(Math.pow(z, e));
}
function pfBend(x, knob) { return knob === 0 ? x : pfPowerBend(pfClamp01(x), knob); }
// temporal 0 Linear, 1 Bell, 2 Bounce; pitch 0 Ramp, 1 Bell, 2 Triangle, 3 Bounce
function pfCurve(kind, x) {
    var F = Math.fround;
    switch (kind) {
    case "bell": return F(0.5 - F(0.5 * F(Math.cos(F(x * PF_TWO_PI)))));
    case "triangle": return F((x < 0.5 ? x : F(1 - x)) * 2);
    case "bounce":
        if (!(x < 1)) return 0;
        var y = F(F(x * 3) % 1);
        return F(F(1 - y) * F(1 - y));
    }
    return x;
}
var PF_TEMPORAL_CURVES = ["ramp", "bell", "bounce"];
var PF_PITCH_CURVES = ["ramp", "bell", "triangle", "bounce"];
var PF_WIN_LO = [0, Math.fround(0.15), Math.fround(0.25), Math.fround(0.35), Math.fround(0.25)];
var PF_WIN_HI = [1, Math.fround(0.85), Math.fround(0.75), Math.fround(0.65), Math.fround(0.75)];
// Window: 1-2 keep the middle (Focus 70/50), 3-4 keep the outsides (Polarize 70/50)
function pfInWindow(phi, w) {
    if (w === 1 || w === 2) return phi >= PF_WIN_LO[w] && phi <= PF_WIN_HI[w];
    if (w === 3 || w === 4) return phi <= PF_WIN_LO[w] || phi >= PF_WIN_HI[w];
    return true;
}
function pfHoldWindow(phi, w) {
    if (w === 1 || w === 2) return phi < PF_WIN_LO[w] ? PF_WIN_LO[w] : (phi > PF_WIN_HI[w] ? PF_WIN_HI[w] : phi);
    if (w === 3 || w === 4) {
        if (phi > PF_WIN_LO[w] && phi < PF_WIN_HI[w]) return phi < 0.5 ? PF_WIN_LO[w] : PF_WIN_HI[w];
    }
    return phi;
}
// Warp -> Repeat -> Window (hold) -> FlipH -> curve -> Response -> FlipV
function pfPitchValue(phi, R, warp, flipH, curve, flipV, resp, win) {
    var F = Math.fround;
    var w = pfBend(phi, warp);
    var r = R > 1 ? F(F(w * R) - Math.floor(F(w * R))) : w;
    var h = pfHoldWindow(r, win);
    var c = pfCurve(curve, flipH ? F(1 - h) : h);
    var v = pfBend(c, resp);
    return flipV ? F(1 - v) : v;
}
// where pulse i of n sits: warp the even spread, then split it into R repeats
function pfTemporalAlloc(i, n, R, warp) {
    var F = Math.fround;
    var seed = n > 0 ? F(i / n) : 0;
    var scaled = F(pfBend(seed, warp) * R);
    var s = clamp(Math.floor(scaled), 0, R - 1);
    return [s, F(scaled - s)];
}
// how "tonal" a scale degree is (root, fifth, thirds...), 0..30
function pfCentrality(d, N) {
    if (N <= 1) return 0;
    var hw = Math.max(1, Math.trunc(N / 6));
    function k(target, weight) {
        var dist = Math.abs(d - target);
        if (N - dist < dist) dist = N - dist;
        var w = Math.trunc((hw - dist) * weight / hw);
        return w > 0 ? w : 0;
    }
    var b = k(0, 30);
    if (N >= 2) b += k(Math.trunc(N / 2), 20);
    if (N >= 3) b += k(Math.trunc(N / 3), 10) + k(Math.trunc(2 * N / 3), 10);
    if (N >= 4) b += k(Math.trunc(N / 4), 5) + k(Math.trunc(3 * N / 4), 5);
    return b;
}

// accumulator configs: scope 0 Local (per stage) / 1 Track (shared),
// order 0 Wrap / 1 Pendulum / 2 Hold / 3 Return-to-zero, polarity 0 Uni / 1 Bi
function pfAdvance(c, dirs, idx, cfg, step) {
    if (step === 0) return;
    var lo, hi;
    if (cfg.polarity === 0) { if (step > 0) { lo = 0; hi = cfg.pos; } else { lo = -cfg.neg; hi = 0; } }
    else { lo = -cfg.neg; hi = cfg.pos; }
    var d = step > 0 ? 1 : -1, mag = Math.abs(step), v = c[idx];
    switch (cfg.order) {
    case 0:
        v += d * mag;
        var range = hi - lo + 1;
        if (range > 0) v = lo + (((v - lo) % range) + range) % range;
        break;
    case 1:
        v += step * dirs[idx];
        if (v >= hi) { v = hi; dirs[idx] = -dirs[idx]; }
        else if (v <= lo) { v = lo; dirs[idx] = -dirs[idx]; }
        break;
    case 2:
        v += d * mag;
        if (d > 0 && v >= hi) v = hi;
        if (d < 0 && v <= lo) v = lo;
        break;
    case 3:
        v += d * mag;
        if (v > hi || v < lo) v = 0;
        break;
    }
    c[idx] = v;
}

function PhaseFluxEngine(host) {
    this.host = host;
    this.p = {
        divisor: 12, clockMult: 100, cycleLength: 0, traversal: 0, pitchMode: 0, pitchRate: 9,
        globalPhase: 0, warpNudge: 0, responseNudge: 0, cyclePhaseWarp: 0, pulseNudge: 0, lenNudge: 0,
        resetMeasure: 0, firstStage: 0, lastStage: 15, scale: 1, rootNote: 0, scaleRotate: 0,
        octave: 0, transpose: 0,
        noteAccScope: 0, noteAccOrder: 0, noteAccPolarity: 0, noteAccReset: 0, noteAccPos: 28, noteAccNeg: 28,
        pulseAccScope: 0, pulseAccOrder: 0, pulseAccPolarity: 0, pulseAccReset: 0, pulseAccPos: 16, pulseAccNeg: 16
    };
    this.stages = [];
    for (var i = 0; i < PF_STAGES; i++) {
        var s = new PfStage();
        if (i < 4) s.pulseCount = 4; else s.skip = 1;
        this.stages.push(s);
    }
    this.measureTicks = 768;
    this._layoutKey = "";
    this.reset();
}

PhaseFluxEngine.prototype.setParam = function (name, value) {
    var r = PHASEFLUX_RANGES[name], p = this.p;
    if (!r) return false;
    value = clamp(Math.round(value), r[0], r[1]);
    // the module keeps First <= Last
    if (name === "firstStage") value = Math.min(value, p.lastStage);
    if (name === "lastStage") value = Math.max(value, p.firstStage);
    p[name] = value;
    return true;
};
PhaseFluxEngine.prototype.setStage = function (i, field, value) {
    var f = PF_FIELDS[PF_FIELD_INDEX[field]];
    if (!f || i < 0 || i >= PF_STAGES) return false;
    this.stages[i][field] = clamp(Math.round(value), f[1], f[2]);
    if (this.host.stateChanged) this.host.stateChanged();
    return true;
};
PhaseFluxEngine.prototype.setRefParam = function (t) {
    if (t[0] === "stage") return this.setStage(+t[1], t[2], +t[3]);
    return this.setParam(t[0], +t[1]);
};
// Reset Bars on the device = the module's Reset Measure
PhaseFluxEngine.prototype.ownsResetMeasure = true;

PhaseFluxEngine.prototype._accCfg = function (pre) {
    var p = this.p;
    return { scope: p[pre + "Scope"], order: p[pre + "Order"], polarity: p[pre + "Polarity"],
        reset: p[pre + "Reset"], pos: p[pre + "Pos"], neg: p[pre + "Neg"] };
};
PhaseFluxEngine.prototype._clearAcc = function (which) {
    for (var i = 0; i < PF_STAGES; i++) {
        if (which !== "pulse") { this._noteAcc[i] = 0; this._noteDir[i] = 1; }
        if (which !== "note") { this._pulseAcc[i] = 0; this._pulseDir[i] = 1; }
    }
};

PhaseFluxEngine.prototype.reset = function () {
    this._resetTickOffset = 0;
    this._firstTick = true;
    this._layoutDirty = true;
    this._pulseFired = 0;
    this._slotIdx = 0; this._prevSlotIdx = -1; this._activeCell = 0;
    this._stagePhase = 0; this._pitchPhase = 0;
    this._gateState = false; this._gateTimer = 0;
    this._cvOutput = 0; this._cvOutputTarget = 0;
    this._schedule = [];
    this._noteAcc = []; this._noteDir = []; this._pulseAcc = []; this._pulseDir = [];
    this._clearAcc();
    this._cycleCount = 0; this._prevCycleTick = 0;
    this._cum = []; for (var i = 0; i <= PF_STAGES; i++) this._cum.push(0);
    this._cycleTicks = 0;
    this._prevGate = false; this._prevCv = 0; this._retrig = false; this._gateOutput = false;
};
PhaseFluxEngine.prototype.stop = function () {
    this._gateState = false; this._gateTimer = 0;
    this._emit();
};

PhaseFluxEngine.prototype._layoutSignature = function () {
    // (the module misses Traversal and Cycle Length changes until something else moves; the device doesn't)
    var p = this.p, k = [p.divisor, p.clockMult, p.lenNudge, p.firstStage, p.lastStage, p.traversal, p.cycleLength];
    for (var i = 0; i < PF_STAGES; i++) k.push(this.stages[i].length, this.stages[i].skip);
    return k.join(",");
};

// the stages' start times in walk order (computeCumulativeTicks)
PhaseFluxEngine.prototype.rebuildCumulativeTable = function () {
    var p = this.p, order = PF_TRAVERSALS[p.traversal], cum = this._cum;
    var unit = p.divisor * 4, fixed = p.cycleLength === 1;
    cum[0] = 0;
    for (var i = 0; i < PF_STAGES; i++) {
        var cell = order[i], s = this.stages[cell];
        var len = clamp(s.length + p.lenNudge, 0, 127);
        if (cell < p.firstStage || cell > p.lastStage) len = 0;
        if (!fixed && s.skip) len = 0;
        cum[i + 1] = cum[i] + Math.trunc(len * unit * 100 / p.clockMult);
    }
    var cycle = cum[PF_STAGES];
    if (cycle > 0) {
        // very short cycles are stretched to at least 1/32 bar
        var minCycle = Math.trunc(this.measureTicks / 32);
        if (minCycle > 0 && cycle < minCycle) {
            for (i = 1; i <= PF_STAGES; i++) cum[i] = Math.floor((cum[i] * minCycle + Math.trunc(cycle / 2)) / cycle);
            cycle = cum[PF_STAGES];
        }
    }
    this._cycleTicks = cycle;
    this._layoutKey = this._layoutSignature();
    this._gateTimer = 0; this._gateState = false;
    this._pulseFired = 0; this._schedule = [];
    this._prevSlotIdx = -1; this._prevCycleTick = 0;
    this._layoutDirty = false;
};

PhaseFluxEngine.prototype.scaleView = function () { return new ScaleView(this.p.scale, this.p.scaleRotate); };
PhaseFluxEngine.prototype.degreeToVolts = function (scale, degree) {
    return Math.fround(Math.fround(scale.noteToVolts(degree)) + Math.fround(this.p.rootNote * Math.fround(1 / 12)));
};

// the burst for the stage just entered (rebuildSchedule)
PhaseFluxEngine.prototype.rebuildSchedule = function (slotDur) {
    var F = Math.fround, p = this.p;
    this._schedule = [];
    if (slotDur <= 0) return;
    var stage = this.stages[this._activeCell];
    if (stage.skip) return;

    var pulseCfg = this._accCfg("pulseAcc"), noteCfg = this._accCfg("noteAcc");
    var pIdx = pulseCfg.scope === 0 ? this._activeCell : 0;
    var nIdx = noteCfg.scope === 0 ? this._activeCell : 0;
    var pulseCount = clamp(stage.pulseCount + p.pulseNudge + this._pulseAcc[pIdx], 0, PF_MAX_PULSES);

    var scale = this.scaleView(), N = scale.notesPerOctave();
    var tCurve = PF_TEMPORAL_CURVES[stage.temporalCurve] || "ramp";
    var maskByte = PF_MASKS[stage.mask];
    var tWarp = clamp(stage.temporalWarp + p.warpNudge, -64, 64);
    var tResp = clamp(stage.temporalResponse + p.responseNudge, -64, 64);

    var global = p.pitchMode === 1;
    var ps = global ? this.stages[0] : stage;
    var pCurve = PF_PITCH_CURVES[ps.pitchCurve];
    var pWarp = clamp(ps.pitchWarp + p.warpNudge, -64, 64);
    var pResp = clamp(ps.pitchResponse + p.responseNudge, -64, 64);
    var rate = PF_RATES[p.pitchRate];
    var advance = global ? F(F(F(rate[0]) / F(rate[1])) / slotDur) : 0;

    var rangeOct = [0.5, 1, 2, 3][stage.pitchRange];
    var rangeDeg = Math.max(1, Math.round(F(rangeOct * N)));
    var baseDegree = stage.basePitch + this._noteAcc[nIdx] + p.octave * N + p.transpose;
    var tR = PF_REPEAT[stage.temporalRepeat];
    var pR = global ? 1 : PF_REPEAT[stage.pitchRepeat];
    var pWin = global ? 0 : stage.pitchWindow;

    var cands = [];
    for (var i = 0; i < pulseCount; i++) {
        if ((maskByte >> ((i + stage.maskShift) & 7)) & 1) continue;
        var alloc = pfTemporalAlloc(i, pulseCount, tR, tWarp);
        if (!pfInWindow(alloc[1], stage.temporalWindow)) continue;
        var tl = pfBend(pfCurve(tCurve, alloc[1]), tResp);
        if (stage.temporalFlipV) tl = F(1 - tl);
        var tg = F(F(alloc[0] + tl) / tR);
        var ts = F(tg + F(stage.phaseShift * 0.125));
        ts = F(ts % 1);
        if (ts < 0) ts = F(ts + 1);
        var trig = F(ts * slotDur);

        var phi;
        if (global) { phi = F(this._pitchPhase + F(trig * advance)); phi = F(phi - Math.floor(phi)); }
        else phi = pulseCount === 1 ? 0 : ts;
        var pv = pfPitchValue(phi, pR, pWarp, !!ps.pitchFlipH, pCurve, !!ps.pitchFlipV, pResp, pWin);
        var off = 0;
        if (stage.pitchDirection === 0) off = F(pv * rangeDeg);
        else if (stage.pitchDirection === 1) off = F(-pv * rangeDeg);
        else off = F(F(pv - 0.5) * rangeDeg);
        var degree = baseDegree + lround(off);

        // Melody mask keeps the tonal degrees, Tilt the off-beat ones
        if (N > 0 && (stage.maskMelody < 100 || stage.tiltMelody > 0)) {
            var c = pfCentrality(((degree % N) + N) % N, N);
            var maskPass = c >= Math.trunc(30 * (100 - stage.maskMelody) / 100);
            var tiltPass = stage.tiltMelody > 0 && (30 - c) >= Math.trunc(30 * (100 - stage.tiltMelody) / 100);
            if (!maskPass && !tiltPass) continue;
        }
        cands.push({ t: trig, cv: this.degreeToVolts(scale, degree) });

        // Pulse-triggered accumulators move once per accepted pulse
        if (stage.accumulatorTrigger === 1 && stage.accumulatorStep !== 0)
            pfAdvance(this._noteAcc, this._noteDir, nIdx, noteCfg, stage.accumulatorStep);
        if (stage.pulseAccumTrigger === 1 && stage.pulseAccumStep !== 0)
            pfAdvance(this._pulseAcc, this._pulseDir, pIdx, pulseCfg, stage.pulseAccumStep);
    }
    // stable insertion sort by time
    for (i = 1; i < cands.length; i++) {
        var key = cands[i], j = i - 1;
        while (j >= 0 && cands[j].t > key.t) { cands[j + 1] = cands[j]; j--; }
        cands[j + 1] = key;
    }
    if (slotDur < PF_MIN_PULSE_GATE) return;

    var sched = this._schedule;
    for (var k = 0; k < cands.length; k++) {
        var t = Math.trunc(cands[k].t);
        var next = k + 1 < cands.length ? Math.trunc(cands[k + 1].t) : slotDur;
        var period = next - t;
        if (period <= 0) continue;
        var len = stage.gateLength;
        if (len === 0) continue;
        var g = Math.trunc(len * period / 100);
        if (len === 1 && g < PF_MIN_PULSE_GATE) g = PF_MIN_PULSE_GATE;
        if (g < PF_MIN_PULSE_GATE) continue;
        if (sched.length >= PF_MAX_PULSES) break;
        sched.push({ at: Math.max(0, t) & 0xffff, gate: g & 0xffff, cv: cands[k].cv });
    }
    // temporal Flip H mirrors the burst in time
    if (stage.temporalFlipH && sched.length) {
        for (k = 0; k < sched.length; k++) sched[k].at = Math.max(0, slotDur - (sched[k].at + sched[k].gate)) & 0xffff;
        for (i = 1; i < sched.length; i++) {
            key = sched[i]; j = i - 1;
            while (j >= 0 && sched[j].at > key.at) { sched[j + 1] = sched[j]; j--; }
            sched[j + 1] = key;
        }
    }
};

PhaseFluxEngine.prototype.tick = function (tick) {
    var F = Math.fround, p = this.p;
    if (this._firstTick) { this._resetTickOffset = tick; this._firstTick = false; }
    if (this._layoutDirty || this._layoutSignature() !== this._layoutKey) this.rebuildCumulativeTable();

    var cycle = this._cycleTicks;
    var rel = u32(tick - this._resetTickOffset);
    if (cycle > 0) {
        if (p.globalPhase > 0) {
            var gp = F(p.globalPhase / 100);
            rel = (rel + Math.trunc(F(gp * cycle))) % cycle;
        }
        if (p.cyclePhaseWarp !== 0) {
            var cp = F((rel % cycle) / cycle);
            rel = Math.trunc(F(pfBend(cp, p.cyclePhaseWarp) * cycle)) % cycle;
        }
    }
    // Reset Measure: back to the first stage, accumulators keep going
    if (p.resetMeasure > 0) {
        var rd = p.resetMeasure * this.measureTicks;
        if (rel % rd === 0 && rel !== 0) {
            this._resetTickOffset = tick; rel = 0;
            this._pulseFired = 0; this._gateTimer = 0; this._gateState = false;
            this._prevSlotIdx = -1; this._prevCycleTick = 0;
        }
    }
    if (cycle <= 0) {
        // every stage skipped: silence
        this._gateState = false; this._gateTimer = 0;
        this._emit();
        return false;
    }

    var order = PF_TRAVERSALS[p.traversal], cum = this._cum;
    var cycleTick = rel % cycle;
    var slot = 0;
    for (var i = 0; i < PF_STAGES; i++) if (cycleTick >= cum[i] && cycleTick < cum[i + 1]) { slot = i; break; }
    this._slotIdx = slot;
    this._activeCell = order[slot];
    var slotTicks = cum[slot + 1] - cum[slot];
    this._stagePhase = slotTicks > 0 ? F((cycleTick - cum[slot]) / slotTicks) : 0;

    // a new pass through the cycle
    if (cycleTick < this._prevCycleTick) {
        this._cycleCount++;
        this._pulseFired = 0;
        if (p.noteAccReset > 0 && this._cycleCount % p.noteAccReset === 0) this._clearAcc("note");
        if (p.pulseAccReset > 0 && this._cycleCount % p.pulseAccReset === 0) this._clearAcc("pulse");
    }
    this._prevCycleTick = cycleTick;

    if (this._slotIdx !== this._prevSlotIdx) {
        if (this._prevSlotIdx >= 0) {
            // Stage-triggered accumulators move when their stage ends
            var done = order[this._prevSlotIdx], st = this.stages[done];
            if (!st.skip) {
                var nc = this._accCfg("noteAcc"), pc = this._accCfg("pulseAcc");
                if (st.accumulatorTrigger === 0) pfAdvance(this._noteAcc, this._noteDir, nc.scope === 1 ? 0 : done, nc, st.accumulatorStep);
                if (st.pulseAccumTrigger === 0) pfAdvance(this._pulseAcc, this._pulseDir, pc.scope === 1 ? 0 : done, pc, st.pulseAccumStep);
            }
        }
        this._pulseFired = 0;
        this._prevSlotIdx = this._slotIdx;
        this.rebuildSchedule(slotTicks);
        var s = this.stages[this._activeCell];
        this.host.stepInfo(this._activeCell, this._slotIdx, s.skip ? 0 : this._schedule.length,
            this._noteAcc[this._accCfg("noteAcc").scope === 0 ? this._activeCell : 0]);
    }

    var pos = cycleTick - cum[this._slotIdx], sch = this._schedule;
    for (var k = 0; k < sch.length; k++) {
        if (this._pulseFired & (1 << k)) continue;
        if (pos < sch[k].at) continue;
        this._pulseFired |= (1 << k);
        if (this._gateState) this._retrig = true;   // a new pulse over a held one: new note
        this._cvOutputTarget = sch[k].cv;
        this._gateState = true;
        this._gateTimer = sch[k].gate;
    }
    if (this._gateTimer > 0) {
        this._gateTimer--;
        if (this._gateTimer === 0) this._gateState = false;
    }
    // the free-running pitch phase (Global pitch mode)
    if (slotTicks > 0) {
        var rate = PF_RATES[p.pitchRate];
        this._pitchPhase = F(this._pitchPhase + F(F(F(rate[0]) / F(rate[1])) / slotTicks));
        this._pitchPhase = F(this._pitchPhase - Math.floor(this._pitchPhase));
    }
    this._emit();
    return false;
};

PhaseFluxEngine.prototype.update = function () { this._cvOutput = this._cvOutputTarget; };

PhaseFluxEngine.prototype._emit = function () {
    var g = this._gateState, cv = this._cvOutputTarget;
    if (g && this._retrig && this._prevGate) { this.host.gate(false); this._prevGate = false; }
    this._retrig = false;
    if (g && !this._prevGate) this.host.gate(true, cv, 100, false);
    else if (!g && this._prevGate) this.host.gate(false);
    else if (g && cv !== this._prevCv) this.host.gate(true, cv, 100, false);
    this._prevGate = g; this._prevCv = cv;
    this._gateOutput = g;
};

// ---- state for the Live set: every stage field ----
PhaseFluxEngine.prototype.getState = function () {
    var out = [1];
    this.stages.forEach(function (s) { PF_FIELDS.forEach(function (f) { out.push(s[f[0]]); }); });
    return out;
};
PhaseFluxEngine.prototype.setState = function (a) {
    var n = PF_FIELDS.length;
    if (!a || a[0] !== 1 || a.length < 1 + PF_STAGES * n) return false;
    for (var i = 0; i < PF_STAGES; i++)
        for (var j = 0; j < n; j++) this.stages[i][PF_FIELDS[j][0]] = clamp(a[1 + i * n + j] | 0, PF_FIELDS[j][1], PF_FIELDS[j][2]);
    return true;
};
// a brand-new device: four bursts that climb, fall, bloom and bounce
PhaseFluxEngine.prototype.initFresh = function () {
    for (var i = 0; i < PF_STAGES; i++) { this.stages[i].clear(); if (i >= 4) this.stages[i].skip = 1; }
    var s = this.stages;
    s[0].pulseCount = 4; s[0].pitchCurve = 0; s[0].pitchDirection = 0;
    s[1].pulseCount = 3; s[1].pitchDirection = 1; s[1].basePitch = 7; s[1].temporalCurve = 1;
    s[2].pulseCount = 6; s[2].pitchCurve = 1; s[2].basePitch = 2; s[2].temporalCurve = 1; s[2].gateLength = 40;
    s[3].pulseCount = 5; s[3].pitchCurve = 3; s[3].pitchDirection = 2; s[3].basePitch = 4; s[3].temporalCurve = 2;
};

PhaseFluxEngine.prototype.usesMsUpdate = false;
