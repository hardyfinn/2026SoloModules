// =====================================================================
//  X|FORMER display + editor (one file for all seven devices)
//  [jsui] arguments:  <device> <view>
//      device = algo | stochastic | note | indexed | discrete | phaseflux | fractal
//      view   = main (small strip on the device) | editor (pop-out window)
//  Receives the engine's display messages (step, play, cells, layer ...)
//  and sends edits back as "edit ..." messages. Plain ES5 for [jsui].
// =====================================================================
mgraphics.init();
mgraphics.relative_coords = 0;
mgraphics.autofill = 0;
inlets = 1;
outlets = 1;

var DEVICE = jsarguments.length > 1 ? String(jsarguments[1]) : "algo";
var VIEW = jsarguments.length > 2 ? String(jsarguments[2]) : "main";
var EDITOR = VIEW === "editor";

// ---- colours (dark, close to Live's default theme) ----
var C_BG = [0.11, 0.11, 0.11, 1], C_GRID = [0.2, 0.2, 0.2, 1], C_DIM = [0.32, 0.32, 0.32, 1];
var C_TEXT = [0.8, 0.8, 0.8, 1], C_NOTE = [0.37, 0.83, 0.78, 1], C_ACCENT = [1, 0.6, 0.24, 1];
var C_HEAD = [1, 0.6, 0.24, 1], C_LOOP = [0.18, 0.24, 0.24, 1], C_OFF = [0.85, 0.3, 0.3, 1];
var NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
function noteName(m) { return m < 0 ? "-" : NOTE_NAMES[m % 12] + (Math.floor(m / 12) - 2); }

// ---- what we know ----
var playing = false;
var head = -1;               // current step
var lastNote = -1;
var algoName = "";
var hist = [];               // algo: last note per step [midi, vel, accent]
var loopLen = 16;
var roll = [];               // recent notes for the editor's roll [midi, vel]
var cells = [], cellSize = 0, cellFirst = 0;     // stochastic
var tickets = null, ticketN = 7;                 // stochastic editor
var layer = { field: "", page: 0, min: 0, max: 1, first: 0, last: 15, pages: 1, v: [] };   // step editors
var gates = [];              // step devices: which steps sound (PhaseFlux: pulses per stage)
var noteInfo = { pulse: 0, acc: 0 };
var info = [];               // the newer devices: the rest of the last "step" message
var rec = { pos: -1, on: 0 };   // Fractal: where the next cell is recorded

function W() { return mgraphics.size[0]; }
function H() { return mgraphics.size[1]; }
function col(c, a) { mgraphics.set_source_rgba(c[0], c[1], c[2], a === undefined ? c[3] : a); }
function rect(x, y, w, h, c, a) { col(c, a); mgraphics.rectangle(x, y, w, h); mgraphics.fill(); }
function frame(x, y, w, h, c) { col(c); mgraphics.set_line_width(1); mgraphics.rectangle(x + 0.5, y + 0.5, w - 1, h - 1); mgraphics.stroke(); }
function text(s, x, y, c, size) {
    col(c || C_TEXT);
    mgraphics.select_font_face("Arial");
    mgraphics.set_font_size(size || 10);
    mgraphics.move_to(x, y);
    mgraphics.show_text(String(s));
}
function textW(s) { return mgraphics.text_measure(String(s))[0]; }

// =====================================================================
//  incoming messages
// =====================================================================
function step() {
    var a = arrayfromargs(arguments);
    playing = true;
    if (DEVICE === "algo") {
        // step <index> <loopLength> <midi|-1> <vel> <accent>
        loopLen = a[1] > 0 ? Math.min(a[1], 64) : 16;
        head = a[0] % loopLen;
        hist[head] = [a[2], a[3], a[4]];
        if (a[2] >= 0) lastNote = a[2];
    } else if (DEVICE === "stochastic") {
        // step <index> <size> <first> <midi|-1> <durationTicks>
        head = a[0];
        if (a[3] >= 0) lastNote = a[3];
    } else if (DEVICE === "note") {
        // step <index> <first> <last> <midi|-1> <pulse> <accumulator>
        head = a[0];
        layer.first = a[1]; layer.last = a[2];
        if (a[3] >= 0) lastNote = a[3];
        noteInfo.pulse = a[4]; noteInfo.acc = a[5];
    } else {
        // indexed:   step <index> <steps> <first> <midi|-1> <duration>
        // discrete:  step <stage> <midi|-1> <input>
        // phaseflux: step <stage> <slot> <pulses> <accumulator>
        // fractal:   step <cell> <segment> <midi>
        head = a[0];
        info = a.slice(1);
        var m = DEVICE === "indexed" ? a[3] : (DEVICE === "phaseflux" ? -1 : a[DEVICE === "fractal" ? 2 : 1]);
        if (m >= 0) lastNote = m;
    }
    mgraphics.redraw();
}
function play(pitch, vel) {
    if (DEVICE === "phaseflux" && vel > 0) lastNote = pitch;
    roll.push([pitch, vel]);
    if (roll.length > 128) roll.shift();
    if (EDITOR) mgraphics.redraw();
}
function stop() { playing = false; head = -1; mgraphics.redraw(); }
function algo(index, name) { algoName = name; hist = []; mgraphics.redraw(); }
function anything() {
    var a = arrayfromargs(arguments);
    switch (messagename) {
    case "cells":
        // cells <size> <first> (dur pitch audible) * n
        cellSize = a[0]; cellFirst = a[1]; cells = [];
        for (var i = 2; i + 2 < a.length; i += 3) cells.push([a[i], a[i + 1], a[i + 2]]);
        break;
    case "tickets":
        ticketN = a[0]; tickets = a.slice(1);
        break;
    case "layer":
        // layer <field> <page> <min> <max> <first> <last> <pages> v0..v15
        layer = { field: String(a[0]), page: a[1], min: a[2], max: a[3], first: a[4], last: a[5], pages: a[6], v: a.slice(7) };
        break;
    case "rec":
        rec = { pos: a[0], on: a[1] };
        break;
    case "gates":
        gates = a;
        break;
    default:
        return;
    }
    mgraphics.redraw();
}

// =====================================================================
//  drawing
// =====================================================================
function paint() {
    var w = W(), h = H();
    rect(0, 0, w, h, C_BG);
    if (DEVICE === "algo") paintAlgo(w, h);
    else if (DEVICE === "stochastic") paintStochastic(w, h);
    else paintNote(w, h);      // every step / stage / cell device
}

// note-height helper shared by the views
function pitchRange(list) {
    var lo = 127, hi = 0;
    for (var i = 0; i < list.length; i++) if (list[i] >= 0) { lo = Math.min(lo, list[i]); hi = Math.max(hi, list[i]); }
    if (lo > hi) { lo = 48; hi = 72; }
    if (hi - lo < 12) { var c = (hi + lo) / 2; lo = c - 6; hi = c + 6; }
    return [lo, hi];
}

function paintAlgo(w, h) {
    var top = 16, gh = EDITOR ? Math.floor(h * 0.55) - top : h - top - 2;
    text(algoName || "Algo", 4, 11, C_TEXT, 10);
    var nn = noteName(lastNote);
    text(nn, w - textW(nn) - 4, 11, C_NOTE, 10);
    var ps = [];
    for (var i = 0; i < loopLen; i++) ps.push(hist[i] ? hist[i][0] : -1);
    var r = pitchRange(ps);
    var cw = w / loopLen;
    for (i = 0; i < loopLen; i++) {
        var x = Math.floor(i * cw), cwi = Math.max(1, Math.floor((i + 1) * cw) - x - 1);
        rect(x, top, cwi, gh, i === head ? C_LOOP : C_GRID);
        var e = hist[i];
        if (e && e[0] >= 0) {
            var y = top + gh - 3 - (e[0] - r[0]) / (r[1] - r[0]) * (gh - 6);
            rect(x, y, cwi, 3, e[2] ? C_ACCENT : C_NOTE, 0.35 + 0.65 * (e[1] / 127));
            rect(x, y + 3, cwi, top + gh - y - 3, e[2] ? C_ACCENT : C_NOTE, 0.12);
        } else if (e) {
            rect(x + cwi / 2 - 1, top + gh - 4, 2, 2, C_DIM);
        }
        if (i === head) rect(x, top + gh - 2, cwi, 2, C_HEAD);
    }
    if (EDITOR) paintRoll(0, top + gh + 8, w, h - top - gh - 10);
}

// the editor's scrolling picture of the last 128 notes
function paintRoll(x0, y0, w, h) {
    rect(x0, y0, w, h, [0.08, 0.08, 0.08, 1]);
    text("last notes", x0 + 4, y0 + 11, C_DIM, 9);
    var ps = [];
    for (var i = 0; i < roll.length; i++) ps.push(roll[i][0]);
    var r = pitchRange(ps), n = 128, cw = w / n;
    for (i = 0; i < roll.length; i++) {
        var x = x0 + (n - roll.length + i) * cw;
        var y = y0 + h - 4 - (roll[i][0] - r[0]) / (r[1] - r[0]) * (h - 8);
        rect(x, y, Math.max(1, cw - 1), 3, C_NOTE, 0.3 + 0.7 * roll[i][1] / 127);
    }
}

// ---- Stochastic: the cycle as boxes, width = duration, height = pitch ----
var DUR_NAMES = ["8x", "4x", "3x", "2x", "4/3", "1x", "2/3", "1/2"];
function paintStochastic(w, h) {
    var top = 16, gh = EDITOR ? Math.floor(h * 0.5) - top : h - top - 2;
    text("Stochastic  " + cellSize + " steps", 4, 11, C_TEXT, 10);
    var nn = noteName(lastNote);
    text(nn, w - textW(nn) - 4, 11, C_NOTE, 10);
    var total = 0, i, lo = 1e9, hi = -1e9;
    for (i = 0; i < cells.length; i++) {
        total += cells[i][0];
        if (cells[i][2]) { lo = Math.min(lo, cells[i][1]); hi = Math.max(hi, cells[i][1]); }
    }
    if (lo > hi) { lo = 0; hi = 7; }
    if (hi - lo < 7) hi = lo + 7;
    rect(0, top, w, gh, C_GRID);
    var x = 0;
    for (i = 0; i < cells.length; i++) {
        var cw = total > 0 ? cells[i][0] / total * w : 0;
        var inWin = i >= cellFirst;
        if (i === head) rect(x, top, cw, gh, C_LOOP);
        if (cells[i][2]) {
            var y = top + gh - 4 - (cells[i][1] - lo) / (hi - lo) * (gh - 8);
            rect(x + 0.5, y, Math.max(1, cw - 1), 4, i === head ? C_HEAD : C_NOTE, inWin ? 1 : 0.25);
        } else {
            rect(x + 0.5, top + gh - 2, Math.max(1, cw - 1), 1, C_DIM, inWin ? 1 : 0.4);
        }
        col(C_BG); mgraphics.set_line_width(1);
        mgraphics.move_to(Math.floor(x) + 0.5, top); mgraphics.line_to(Math.floor(x) + 0.5, top + gh); mgraphics.stroke();
        x += cw;
    }
    if (EDITOR) paintTickets(0, top + gh + 8, w, h - top - gh - 10);
}

// ticket editor: degree weights (left) and duration weights (right)
// bar height = weight 0..100, the strip under a bar = off (-1, never played)
var ticketLayout = null;
function paintTickets(x0, y0, w, h) {
    ticketLayout = null;
    if (!tickets) return;
    var degW = Math.floor(w * 0.62), durX = x0 + degW + 12, durW = w - degW - 12;
    text("Degree tickets (drag; bottom strip = off)", x0 + 2, y0 + 10, C_DIM, 9);
    text("Duration tickets", durX + 2, y0 + 10, C_DIM, 9);
    var by = y0 + 16, bh = h - 16 - 24;
    ticketLayout = { by: by, bh: bh, deg: [x0, degW, ticketN], dur: [durX, durW, 8] };
    drawTicketRow(x0, by, degW, bh, tickets.slice(0, ticketN), function (i) { return String(i + 1); });
    drawTicketRow(durX, by, durW, bh, tickets.slice(32, 40), function (i) { return DUR_NAMES[i]; });
}
function drawTicketRow(x0, by, w, bh, vals, label) {
    var n = vals.length, cw = w / n;
    for (var i = 0; i < n; i++) {
        var x = x0 + i * cw, v = vals[i];
        rect(x + 1, by, cw - 2, bh, C_GRID);
        if (v < 0) rect(x + 1, by + bh + 2, cw - 2, 6, C_OFF);
        else {
            var hh = (v === 0 ? 1 : v) / 100 * bh;
            rect(x + 1, by + bh - hh, cw - 2, hh, v === 0 ? C_DIM : C_NOTE);
            rect(x + 1, by + bh + 2, cw - 2, 6, C_GRID);
        }
        var s = label(i);
        text(s, x + cw / 2 - textW(s) / 2, by + bh + 18, C_TEXT, 9);
    }
}

// ---- step devices: 16-column grid of one field + the overview strip ----
var NOTE_LAYERS = [
    ["gate", "Gate"], ["note", "Note"], ["length", "Length"], ["slide", "Slide"],
    ["gateProbability", "Gate %"], ["gateOffset", "Offset"], ["retrigger", "Ratchet"], ["retriggerProbability", "Ratch %"],
    ["pulseCount", "Pulses"], ["gateMode", "Gate mode"], ["accumulator", "Accum"], ["condition", "Condition"],
    ["lengthVariationRange", "Len var"], ["lengthVariationProbability", "Len var %"],
    ["noteVariationRange", "Note var"], ["noteVariationProbability", "Note var %"]
];
var DEVICE_LAYERS = {
    note: NOTE_LAYERS,
    indexed: [["duration", "Duration"], ["gateLength", "Gate"], ["noteIndex", "Note"], ["slide", "Slide"], ["groupMask", "Groups"]],
    discrete: [["direction", "Trigger"], ["noteIndex", "Note"], ["threshold", "Threshold"]],
    phaseflux: [["pulseCount", "Pulses"], ["length", "Length"], ["gateLength", "Gate"], ["skip", "Skip"],
        ["basePitch", "Base pitch"], ["pitchCurve", "Pitch curve"], ["pitchDirection", "Pitch dir"], ["pitchRange", "Pitch range"],
        ["pitchWarp", "Pitch warp"], ["pitchResponse", "Pitch resp"], ["pitchFlipH", "Pitch flip H"], ["pitchFlipV", "Pitch flip V"],
        ["temporalCurve", "Time curve"], ["temporalWarp", "Time warp"], ["temporalResponse", "Time resp"], ["phaseShift", "Phase shift"],
        ["temporalFlipH", "Time flip H"], ["temporalFlipV", "Time flip V"], ["temporalRepeat", "Time repeat"], ["temporalWindow", "Time window"],
        ["pitchRepeat", "Pitch repeat"], ["pitchWindow", "Pitch window"], ["mask", "Mask"], ["maskShift", "Mask shift"],
        ["maskMelody", "Melody mask"], ["tiltMelody", "Melody tilt"], ["accumulatorStep", "Note acc"], ["accumulatorTrigger", "Note acc on"],
        ["pulseAccumStep", "Pulse acc"], ["pulseAccumTrigger", "Pulse acc on"]],
    fractal: [["pitch", "Pitch"], ["gate", "Gate length"], ["onset", "Onset (Feel)"]]
};
var LAYERS = DEVICE_LAYERS[DEVICE] || [];
var TAB_ROWS = Math.ceil(LAYERS.length / 8);
// the device's step range, by name
var DEVICE_NAME = { note: "Note", indexed: "Indexed", discrete: "Discrete", phaseflux: "PhaseFlux", fractal: "Fractal" };
var DUR_TEXT = { 768: "1 bar", 576: "1/2.", 384: "1/2", 288: "1/4.", 256: "1/2T", 192: "1/4", 144: "1/8.", 128: "1/4T",
    96: "1/8", 72: "1/16.", 64: "1/8T", 48: "1/16", 36: "1/32.", 32: "1/16T", 24: "1/32", 12: "1/64" };
var PF_TEXT = {
    pitchRange: ["1/2 oct", "1 oct", "2 oct", "3 oct"], pitchDirection: ["Up", "Down", "Bipolar"],
    pitchCurve: ["Ramp", "Bell", "Triangle", "Bounce"], temporalCurve: ["Even", "Bell", "Bounce"],
    mask: ["Off", "1in2", "1in3", "1in4", "2in3", "2in4", "3in4", "1in8"],
    accumulatorTrigger: ["Stage", "Pulse"], pulseAccumTrigger: ["Stage", "Pulse"],
    temporalRepeat: ["x1", "x2", "x3", "x5"], pitchRepeat: ["x1", "x2", "x3", "x5"],
    temporalWindow: ["Off", "Focus70", "Focus50", "Polar70", "Polar50"], pitchWindow: ["Off", "Focus70", "Focus50", "Polar70", "Polar50"]
};
function newValueText(field, v) {
    if (DEVICE === "indexed") {
        if (field === "duration" || field === "gateLength") return v === 0 ? "0" : (DUR_TEXT[v] || String(v));
        if (field === "slide") return v ? "on" : "";
        if (field === "groupMask") { var g = ""; for (var b = 0; b < 4; b++) if (v & (1 << b)) g += "ABCD"[b]; return g || "-"; }
    }
    if (DEVICE === "discrete" && field === "direction") return ["Rise", "Fall", "Off", "Both"][v];
    if (DEVICE === "phaseflux") {
        if (PF_TEXT[field]) return PF_TEXT[field][v] || String(v);
        if (/Flip|skip/.test(field)) return v ? "on" : "";
        if (field === "gateLength" || field === "maskMelody" || field === "tiltMelody") return v + "%";
        if (field === "phaseShift") return v ? v + "/8" : "0";
        if (field === "pulseCount" || field === "length" || field === "maskShift") return String(v);
    }
    if (DEVICE === "fractal") {
        if (field === "gate") return v === 0 ? "rest" : (v === 15 ? "tie" : (v === 1 ? "trig" : Math.round(v / 15 * 100) + "%"));
        if (field === "onset") return v + "/16";
        if (field === "pitch") return noteName(60 + v);
    }
    return (v > 0 ? "+" : "") + v;
}
var GATE_MODES = ["All", "First", "Hold", "1st+Last"];
var CONDITIONS = (function () {
    var off = [0, 1, 0, 1, 2, 0, 1, 2, 3, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 6, 0, 1, 2, 3, 4, 5, 6, 7];
    var base = [2, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8];
    var l = ["Off", "Fill", "!Fill", "Pre", "!Pre", "First", "!First"];
    for (var inv = 0; inv < 2; inv++) for (var i = 0; i < 35; i++) l.push((inv ? "!" : "") + (off[i] + 1) + ":" + base[i]);
    return l;
})();
function valueText(field, v) {
    if (DEVICE !== "note") return newValueText(field, v);
    switch (field) {
    case "gate": case "slide": return v ? "on" : "";
    case "length": return v === 0 ? "trig" : Math.round((v + 1) * 12.5) + "%";
    case "gateProbability": case "retriggerProbability": case "lengthVariationProbability": case "noteVariationProbability":
        return Math.round((v + 1) * 12.5) + "%";
    case "gateOffset": return Math.round(v * 12.5) + "%";
    case "retrigger": return (v + 1) + "x";
    case "pulseCount": return String(v + 1);
    case "gateMode": return GATE_MODES[v] || v;
    case "condition": return CONDITIONS[v] || v;
    default: return (v > 0 ? "+" : "") + v;
    }
}
var noteLayout = null;
function paintNote(w, h) {
    if (!EDITOR) { if (DEVICE === "phaseflux") paintFluxMain(w, h); else paintNoteMain(w, h); return; }
    var tabsH = TAB_ROWS * 19 + 2, ovH = 22, i, pages = Math.max(1, layer.pages | 0);
    // layer tabs (rows of 8)
    var tw = w / 8;
    for (i = 0; i < LAYERS.length; i++) {
        var tx = (i % 8) * tw, ty = Math.floor(i / 8) * 19;
        var sel = LAYERS[i][0] === layer.field;
        rect(tx + 1, ty + 1, tw - 2, 17, sel ? C_NOTE : C_GRID);
        text(LAYERS[i][1], tx + 6, ty + 13, sel ? C_BG : C_TEXT, 10);
    }
    // page buttons
    var pw = 34, py = h - ovH - 22;
    if (pages > 1) {
        for (i = 0; i < pages; i++) {
            rect(i * (pw + 2), py, pw, 18, i === layer.page ? C_NOTE : C_GRID);
            var pl = String(i * 16 + 1) + "-" + String(i * 16 + 16);
            text(pl, i * (pw + 2) + pw / 2 - textW(pl) / 2, py + 13, i === layer.page ? C_BG : C_TEXT, 9);
        }
        text("shift-click a page to copy this page there", pages * (pw + 2) + 8, py + 13, C_DIM, 9);
    }
    var acc = statusText();
    text(acc, w - textW(acc) - 4, py + 13, C_TEXT, 10);
    // the grid
    var gy = tabsH + 4, gh = py - gy - 6, cw = w / 16;
    noteLayout = { gy: gy, gh: gh, cw: cw, py: py, pw: pw, tw: tw, tabsH: tabsH, pages: pages };
    var lo = layer.min, hi = layer.max, zero = lo < 0 ? 0 : lo;
    for (i = 0; i < 16; i++) {
        var idx = layer.page * 16 + i, x = i * cw;
        var inLoop = idx >= layer.first && idx <= layer.last;
        rect(x + 1, gy, cw - 2, gh, inLoop ? C_LOOP : C_GRID);
        if (idx === head) rect(x + 1, gy + gh - 3, cw - 2, 3, C_HEAD);
        if (DEVICE === "fractal" && idx === rec.pos) rect(x + 1, gy, cw - 2, 3, C_OFF, rec.on ? 1 : 0.35);
        var v = layer.v[i];
        if (v === undefined) continue;
        if (hi - lo <= 1) {
            if (v) rect(x + 4, gy + 4, cw - 8, gh - 8, C_NOTE);
        } else {
            var yv = gy + gh - (v - lo) / (hi - lo) * gh, yz = gy + gh - (zero - lo) / (hi - lo) * gh;
            rect(x + 3, Math.min(yv, yz), cw - 6, Math.max(2, Math.abs(yz - yv)), C_NOTE, 0.85);
        }
        var s = valueText(layer.field, v);
        text(s, x + cw / 2 - textW(s) / 2, gy + 14, C_TEXT, 10);
        if (i % 4 === 0) text(String(idx + 1), x + 3, gy + gh - 6, C_DIM, 9);
    }
    paintOverview(0, h - ovH, w, ovH);
}
// one line for the whole pattern; the bright cells sound
function paintOverview(x0, y0, w, h) {
    var n = Math.max(1, gates.length), cw = w / n;
    for (var i = 0; i < n; i++) {
        var inLoop = i >= layer.first && i <= layer.last;
        var c = gates[i] ? C_NOTE : (inLoop ? C_LOOP : C_GRID);
        rect(x0 + i * cw + 0.5, y0 + 2, cw - 1, h - 4, c, gates[i] && !inLoop ? 0.35 : 1);
        if (i === head) rect(x0 + i * cw, y0, cw, 2, C_HEAD);
        if (DEVICE === "fractal" && i === rec.pos) rect(x0 + i * cw, y0 + h - 2, cw, 2, C_OFF, rec.on ? 1 : 0.35);
    }
}
function statusText() {
    var nn = noteName(lastNote);
    if (DEVICE === "note") return "pulse " + noteInfo.pulse + "  acc " + noteInfo.acc + "  " + nn;
    if (DEVICE === "indexed") return (head >= 0 ? "step " + (head + 1) : "") + "  " + nn;
    if (DEVICE === "discrete") return (head >= 0 ? "stage " + (head + 1) : "") + "  " + nn;
    if (DEVICE === "phaseflux") return (head >= 0 ? "stage " + (head + 1) + "  pulses " + info[1] + "  acc " + info[2] : "") + "  " + nn;
    if (DEVICE === "fractal") return (rec.on ? "REC " + (rec.pos + 1) + "  " : "") + (info[0] ? "branch " + info[0] : "trunk") + "  " + nn;
    return nn;
}
function paintNoteMain(w, h) {
    text(DEVICE_NAME[DEVICE] + "  " + (layer.first + 1) + "-" + (layer.last + 1), 4, 11, C_TEXT, 10);
    var s = DEVICE === "note" ? "acc " + noteInfo.acc + "  " + noteName(lastNote) : statusText();
    text(s, w - textW(s) - 4, 11, rec.on ? C_OFF : C_NOTE, 10);
    // rows of 16 steps
    var n = Math.max(16, gates.length), rows = Math.ceil(n / 16);
    var top = 16, rh = (h - top - 2) / rows, cw = w / 16;
    for (var i = 0; i < n; i++) {
        var x = (i % 16) * cw, y = top + Math.floor(i / 16) * rh;
        var inLoop = i >= layer.first && i <= layer.last;
        var c = gates[i] ? C_NOTE : (inLoop ? C_LOOP : C_GRID);
        rect(x + 1, y + 1, cw - 2, rh - 2, c, gates[i] && !inLoop ? 0.3 : 1);
        if (i === head) frame(x, y, cw, rh, C_HEAD);
        if (DEVICE === "fractal" && i === rec.pos && rec.on) rect(x + 1, y + rh - 3, cw - 2, 2, C_OFF);
    }
}
// PhaseFlux on the device: the 4x4 grid with each stage's pulse count
function paintFluxMain(w, h) {
    text("PhaseFlux", 4, 11, C_TEXT, 10);
    var s = statusText();
    text(s, w - textW(s) - 4, 11, C_NOTE, 10);
    var top = 16, ch = (h - top - 2) / 4, cw = w / 4;
    for (var i = 0; i < 16; i++) {
        var x = (i % 4) * cw, y = top + Math.floor(i / 4) * ch;
        var inLoop = i >= layer.first && i <= layer.last, v = gates[i] | 0;
        rect(x + 1, y + 1, cw - 2, ch - 2, v ? C_NOTE : (inLoop ? C_LOOP : C_GRID), v ? (inLoop ? 0.25 + v / 21 : 0.15) : 1);
        if (v) text(String(v), x + cw / 2 - 3, y + ch / 2 + 4, C_TEXT, 10);
        if (i === head) frame(x, y, cw, ch, C_HEAD);
    }
}

// =====================================================================
//  mouse
// =====================================================================
var drag = null;
function onclick(x, y, but, cmd, shift) {
    if (!EDITOR) { outlet(0, "open"); return; }
    drag = null;
    if (LAYERS.length && noteLayout) {
        var L = noteLayout;
        if (y < L.tabsH) {
            var t = Math.floor(y / 19) * 8 + Math.floor(x / L.tw);
            if (LAYERS[t]) outlet(0, "edit", "layer", LAYERS[t][0]);
            return;
        }
        if (y >= L.py && y < L.py + 18 && L.pages > 1 && x < L.pages * (L.pw + 2)) {
            var pg = Math.floor(x / (L.pw + 2));
            if (shift) outlet(0, "edit", "copypage", layer.page, pg);
            else outlet(0, "edit", "page", pg);
            return;
        }
        if (y >= L.gy && y < L.gy + L.gh) {
            var i = clampi(Math.floor(x / L.cw), 0, 15);
            if (layer.max - layer.min <= 1) {
                // switches: click toggles, dragging paints the same value
                var nv = layer.v[i] ? 0 : 1;
                drag = { kind: "paint", v: nv, last: i };
                setStep(i, nv);
            } else {
                // Indexed durations move in 1/64 notes; shift-drag for single ticks
                drag = { kind: "value", grid: DEVICE === "indexed" && !shift && /duration|gateLength/.test(layer.field) ? 12 : 1 };
                ondrag(x, y, 1);
            }
        }
        return;
    }
    if (DEVICE === "stochastic" && ticketLayout) { drag = { kind: "ticket" }; ondrag(x, y, 1); }
}
function ondrag(x, y, but) {
    if (!but || !drag) { drag = null; return; }
    if (LAYERS.length && noteLayout) {
        var L = noteLayout, i = clampi(Math.floor(x / L.cw), 0, 15);
        if (drag.kind === "paint") {
            if (i !== drag.last) { drag.last = i; setStep(i, drag.v); }
        } else {
            var f = 1 - (y - L.gy) / L.gh;
            var v = Math.round(layer.min + clampf(f, 0, 1) * (layer.max - layer.min));
            if (drag.grid > 1) v = Math.round(v / drag.grid) * drag.grid;
            if (v !== layer.v[i]) setStep(i, v);
        }
        return;
    }
    if (drag.kind === "ticket") {
        var T = ticketLayout;
        var row = x >= T.dur[0] ? T.dur : T.deg, kind = row === T.dur ? "dur" : "deg";
        var j = clampi(Math.floor((x - row[0]) / (row[1] / row[2])), 0, row[2] - 1);
        var val = y > T.by + T.bh ? -1 : Math.round(clampf(1 - (y - T.by) / T.bh, 0, 1) * 100);
        var cur = tickets[(kind === "dur" ? 32 : 0) + j];
        if (val !== cur) {
            tickets[(kind === "dur" ? 32 : 0) + j] = val;
            outlet(0, "edit", "ticket", kind, j, val);
            mgraphics.redraw();
        }
    }
}
function setStep(i, v) {
    layer.v[i] = v;
    outlet(0, "edit", "step", layer.page * 16 + i, v);
    mgraphics.redraw();
}
function clampi(v, lo, hi) { return v < lo ? lo : (v > hi ? hi : v); }
function clampf(v, lo, hi) { return v < lo ? lo : (v > hi ? hi : v); }
function onresize() { mgraphics.redraw(); }
// ask the device for everything when this view appears
function loadbang() { outlet(0, "getdisplay"); }
