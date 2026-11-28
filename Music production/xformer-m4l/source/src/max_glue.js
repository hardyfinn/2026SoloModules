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
