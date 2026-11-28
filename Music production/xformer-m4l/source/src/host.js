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
