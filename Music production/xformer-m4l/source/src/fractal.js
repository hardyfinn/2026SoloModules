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
