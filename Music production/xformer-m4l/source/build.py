#!/usr/bin/env python3
"""Builds the seven X|FORMER Max for Live MIDI devices.

For each device it writes
    XF <Name>.amxd      the device (MIDI Effect), patch generated below
    xf_<name>.js        the engine + timing host + Max glue, for [v8]
and once
    xf_display.js       the display / editor for [jsui]

Patch overview (same for all devices):

  live.thisdevice -> path live_set -> live.path -> live.observer is_playing -> "playing"
  metro 5 ticks -> transport -> ticks / 5 = slot (1/96 note)
        slot -> [t i i i]: 1. coll lookup -> zl iter 2 -> midiformat -> midiflush -> midiout
                           2. remove that slot from the coll
                           3. "pos <slot>" -> v8 (engine runs ahead, fills the coll)
  knobs -> prepend p <name> -> s ---xfp -> v8
  v8 -> coll / midiformat (urgent notes) / midiflush / display / pattr (pattern store)
  midiin -> midiparse -> "midinote" -> v8 (reset by note)
  jsui (strip) click -> pcontrol -> [p editor] pop-out window
"""
import json, os, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "src")
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "dist")

DIVISOR_NAMES = ["1/64", "1/32", "1/16T", "1/16", "1/8T", "1/16.", "1/8", "1/4T", "1/8.", "1/4", "1/2", "1 bar", "2 bars", "4 bars"]
SCALE_NAMES = ["Chromatic", "Major", "Harm. Minor", "Major Blues", "Minor Blues", "Major Pent.", "Minor Pent.",
               "Folk", "Japanese", "Gamelan", "Gypsy", "Arabian", "Flamenco", "Whole Tone"]
ROOTS = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
ALGOS = ["Test", "TriTrance", "Stomper", "Markov", "Chip1", "Chip2", "Wobble", "ScaleWalk",
         "Window", "Minimal", "Ganz", "Blake", "Aphex", "Autechre", "StepWave", "Turing"]
LOOPS = ["Inf"] + [str(v) for v in [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                                       19, 21, 24, 32, 35, 42, 48, 56, 64, 95, 96, 127, 128]]
MASKS = ["All"] + [str(v) for v in [2, 3, 5, 11, 19, 31, 43, 61, 89, 131, 197, 277, 409, 599]] + ["None"]
DURS = ["8x", "4x", "3x", "2x", "4/3x", "1x", "2/3x", "1/2x"]
# Indexed step groups: 0 = all steps, 1-15 = groups A-D as bits, 16 = steps in no group
GROUPS = ["All"] + ["".join("ABCD"[b] for b in range(4) if i & (1 << b)) for i in range(1, 16)] + ["No group"]


# ---------------------------------------------------------------------------
#  Parameter tables.  (kind, name, label, args..., default)
#    dial  name label min max default [unit]       unit: "%", "st", "x" ... or None
#    menu  name label items default
#    tab   name label items default
#    numb  name label min max default style        style 8 = MIDI note
#    btn   action label                             a button that sends "act <action>"
#    tog   name label default                       on/off button
# ---------------------------------------------------------------------------
COMMON_EDITOR = [
    ("dial", "fixedVel", "Velocity", 1, 127, 100, None),
    ("dial", "resetBars", "Reset Bars", 0, 32, 0, None),
]
COMMON_TABS = [
    ("tab", "legato", "Slides", ["Gap", "Legato"], 1),
    ("tab", "resetOnStart", "Play From", ["Here", "Song"], 1),
    ("tab", "rstAny", "Reset by Note", ["Off", "Any", "Pitch"], 0),
    ("numb", "rstPitch", "Pitch", 0, 127, 36, 8),
]

DEVICES = {
    "algo": {
        "title": "XF Algo",
        "main_dials": [
            ("dial", "flow", "Flow", 0, 16, 4, None), ("dial", "ornament", "Ornament", 0, 16, 4, None),
            ("dial", "power", "Power", 0, 16, 12, None), ("dial", "gateLength", "Gate", 0, 100, 50, "%"),
            ("dial", "glide", "Glide", 0, 100, 50, "%"), ("dial", "trill", "Trill", 0, 100, 50, "%"),
            ("dial", "octave", "Octave", -10, 10, 0, None), ("dial", "transpose", "Transp", -11, 11, 0, "st"),
        ],
        "main_menus": [
            ("menu", "algorithm", "Algorithm", ALGOS, 1), ("menu", "loopLength", "Loop", LOOPS, 0),
            ("menu", "divisor", "Rate", DIVISOR_NAMES, 3), ("menu", "scale", "Scale", SCALE_NAMES, 1),
            ("menu", "rootNote", "Root", ROOTS, 0),
        ],
        "main_buttons": [("btn", "reset", "Reset"), ("btn", "reseed", "Reseed")],
        "editor_dials": [
            ("dial", "start", "Start", 0, 16, 0, None), ("dial", "stepTrill", "Step Trill", 0, 100, 0, "%"),
            ("dial", "skew", "Skew", -8, 8, 0, None), ("dial", "rotate", "Rotate", -64, 64, 0, None),
            ("dial", "scaleRotate", "Mode", 0, 11, 0, None), ("dial", "clockMult", "Clock Mult", 50, 150, 100, "%"),
            ("dial", "gateOffset", "Gate Offs", 0, 100, 0, "%"),
        ] + COMMON_EDITOR,
        "editor_tabs": [
            ("menu", "maskParameter", "Mask", MASKS, 0),
            ("tab", "timeMode", "Mask Time", ["Free", "Qrt", "1.5Q", "3Qrt"], 0),
            ("tab", "maskProgression", "Mask Step", ["None", "+1", "+5", "+7"], 0),
            ("tab", "velMode", "Vel Source", ["Algo", "Fixed"], 0),
        ] + COMMON_TABS,
        "editor_buttons": [],
    },
    "stochastic": {
        "title": "XF Stochastic",
        "main_dials": [
            ("dial", "complexity", "Complexity", 0, 100, 50, "%"), ("dial", "contour", "Contour", -100, 100, 0, None),
            ("dial", "range", "Range", 0, 100, 50, "%"), ("dial", "variation", "Variation", 0, 100, 16, "%"),
            ("dial", "rest", "Rest", 0, 100, 0, "%"), ("dial", "gateLength", "Gate", 0, 100, 0, "%"),
            ("dial", "burst", "Burst", 0, 100, 0, "%"), ("dial", "slide", "Slide", 0, 100, 0, "%"),
        ],
        "main_menus": [
            ("tab", "rhythmMode", "Rhythm", ["Loop", "Live"], 1), ("tab", "melodyMode", "Melody", ["Loop", "Live"], 1),
            ("menu", "noteDuration", "Length", DURS, 5), ("menu", "divisor", "Rate", DIVISOR_NAMES, 3),
            ("menu", "scale", "Scale", SCALE_NAMES, 1), ("menu", "rootNote", "Root", ROOTS, 0),
        ],
        "main_buttons": [("btn", "reset", "Reset"), ("btn", "newRhythm", "New Rhythm"), ("btn", "newMelody", "New Melody")],
        "editor_dials": [
            ("dial", "repeatProb", "Repeat", 0, 100, 0, "%"), ("dial", "legatoProb", "Legato", 0, 100, 0, "%"),
            ("dial", "burstRate", "Burst Rate", 0, 100, 50, "%"), ("dial", "burstCount", "Burst Cnt", 0, 100, 0, "%"),
            ("dial", "sleep", "Sleep", 0, 100, 0, "%"), ("dial", "mutate", "Mutate", 0, 100, 0, "%"),
            ("dial", "jump", "Jump", 0, 100, 0, "%"), ("dial", "patienceRhythm", "Patience R", 0, 100, 100, "%"),
            ("dial", "patienceMelody", "Patience M", 0, 100, 100, "%"), ("dial", "marblesSpread", "Spread", 0, 100, 50, "%"),
            ("dial", "marblesBias", "Bias", 0, 100, 50, "%"), ("dial", "maskRhythm", "Mask R", 0, 100, 100, "%"),
            ("dial", "tiltRhythm", "Tilt R", 0, 100, 50, "%"), ("dial", "maskMelody", "Mask M", 0, 100, 100, "%"),
            ("dial", "tiltMelody", "Tilt M", 0, 100, 0, "%"), ("dial", "feel", "Feel", 0, 100, 50, "%"),
            ("dial", "degreeRotation", "Deg Rot", -32, 32, 0, None), ("dial", "maskRotation", "Mask Rot", -32, 32, 0, None),
            ("dial", "rotate", "Rotate", -64, 64, 0, None), ("dial", "size", "Steps", 2, 64, 32, None),
            ("dial", "first", "First", 0, 63, 0, None), ("dial", "octave", "Octave", -10, 10, 0, None),
            ("dial", "transpose", "Transp", -48, 48, 0, None), ("dial", "scaleRotate", "Mode", 0, 11, 0, None),
            ("dial", "clockMult", "Clock Mult", 50, 150, 100, "%"),
        ] + COMMON_EDITOR,
        "editor_tabs": [
            ("tab", "burstHold", "Burst Mode", ["Hold fit", "Hold over", "Roll fit", "Roll over"], 1),
            ("tab", "evolve", "Evolve", ["Off", "On"], 0),
        ] + COMMON_TABS,
        "editor_buttons": [("btn", "undoRhythm", "Undo Rhythm"), ("btn", "undoMelody", "Undo Melody"),
                           ("btn", "newBoth", "New Both")],
    },
    "note": {
        "title": "XF Note",
        "main_dials": [
            ("dial", "firstStep", "First", 1, 64, 1, None), ("dial", "lastStep", "Last", 1, 64, 16, None),
            ("dial", "rotate", "Rotate", -64, 64, 0, None), ("dial", "octave", "Octave", -10, 10, 0, None),
            ("dial", "gateProbabilityBias", "Gate Bias", -8, 8, 0, None), ("dial", "lengthBias", "Len Bias", -8, 8, 0, None),
            ("dial", "noteProbabilityBias", "Note Bias", -8, 8, 0, None), ("dial", "transpose", "Transp", -48, 48, 0, None),
        ],
        "main_menus": [
            ("menu", "runMode", "Run", ["Forward", "Backward", "Pendulum", "PingPong", "Random", "Random Walk"], 0),
            ("menu", "divisor", "Rate", DIVISOR_NAMES, 3), ("menu", "scale", "Scale", SCALE_NAMES, 1),
            ("menu", "rootNote", "Root", ROOTS, 0),
        ],
        "main_buttons": [("btn", "reset", "Reset"), ("tog", "fill", "Fill", 0)],
        "editor_dials": [
            ("dial", "retriggerProbabilityBias", "Ratch Bias", -8, 8, 0, None),
            ("dial", "scaleRotate", "Mode", 0, 11, 0, None), ("dial", "clockMult", "Clock Mult", 50, 150, 100, "%"),
            ("dial", "accMin", "Acc Min", -100, 100, 0, None), ("dial", "accMax", "Acc Max", -100, 100, 7, None),
            ("dial", "accStep", "Acc Step", 0, 100, 1, None),
        ] + COMMON_EDITOR,
        "editor_tabs": [
            ("tab", "accEnabled", "Accumulator", ["Off", "On"], 1),
            ("tab", "accMode", "Acc Mode", ["Stage", "Track"], 1),
            ("tab", "accDirection", "Acc Dir", ["Up", "Down", "Freeze"], 0),
            ("tab", "accOrder", "Acc Order", ["Wrap", "Pendulum", "Random", "Hold"], 0),
            ("tab", "accTrigger", "Acc Trigger", ["Step", "Gate", "Ratchet"], 0),
        ] + COMMON_TABS,
        "editor_buttons": [("btn", "clearSteps", "Clear Steps"), ("btn", "initSteps", "Init Steps")],
    },
    "indexed": {
        "title": "XF Indexed",
        "main_dials": [
            ("dial", "activeLength", "Steps", 1, 48, 8, None), ("dial", "firstStep", "First", 1, 48, 1, None),
            ("dial", "modA", "Mod A", -100, 100, 0, "%"), ("dial", "modB", "Mod B", -100, 100, 0, "%"),
            ("dial", "octave", "Octave", -10, 10, 0, None), ("dial", "transpose", "Transp", -48, 48, 0, "st"),
            ("dial", "stretch", "Stretch", 1, 16, 1, None), ("dial", "clockMult", "Clock Mult", 50, 150, 100, "%"),
        ],
        "main_menus": [
            ("menu", "runMode", "Run", ["Forward", "Backward", "Pendulum", "PingPong", "Random", "Random Walk"], 0),
            ("tab", "loop", "Play", ["Once", "Loop"], 1),
            ("menu", "scale", "Scale", SCALE_NAMES, 1), ("menu", "rootNote", "Root", ROOTS, 0),
        ],
        "main_buttons": [("btn", "reset", "Reset")],
        "editor_dials": [
            ("dial", "routeAAmount", "A Amount", -200, 200, 100, "%"), ("dial", "routeBAmount", "B Amount", -200, 200, 100, "%"),
            ("dial", "scaleRotate", "Mode", 0, 11, 0, None),
        ] + COMMON_EDITOR,
        "editor_tabs": [
            ("tab", "routeASource", "Route A from", ["Off", "Mod A", "Mod B"], 0),
            ("tab", "routeATarget", "Route A to", ["Duration", "Gate", "Note"], 0),
            ("menu", "routeAGroups", "Route A steps", GROUPS, 0),
            ("tab", "routeBSource", "Route B from", ["Off", "Mod A", "Mod B"], 0),
            ("tab", "routeBTarget", "Route B to", ["Duration", "Gate", "Note"], 0),
            ("menu", "routeBGroups", "Route B steps", GROUPS, 0),
            ("tab", "routeCombine", "Combine", ["A then B", "Mux", "Min", "Max"], 0),
        ] + COMMON_TABS,
        "editor_buttons": [("btn", "clearSteps", "Clear Steps"), ("btn", "initSteps", "Init Steps")],
    },
    "discrete": {
        "title": "XF Discrete",
        "main_dials": [
            ("dial", "input", "Input", -100, 100, 0, "%"), ("dial", "scanner", "Scanner", 0, 34, 0, None),
            ("dial", "gateLength", "Gate", 0, 100, 0, "%"), ("dial", "clockMult", "Clock Mult", 50, 150, 100, "%"),
            ("dial", "rangeHigh", "Above", -100, 100, 100, "%"), ("dial", "rangeLow", "Below", -100, 100, -100, "%"),
            ("dial", "octave", "Octave", -10, 10, 0, None), ("dial", "transpose", "Transp", -48, 48, 0, "st"),
        ],
        "main_menus": [
            ("menu", "clockSource", "Source", ["Saw", "Triangle", "Input"], 0),
            ("menu", "divisor", "Sweep", DIVISOR_NAMES, 11),
            ("tab", "thresholdMode", "Spacing", ["Position", "Length"], 0),
            ("menu", "scale", "Scale", SCALE_NAMES, 1), ("menu", "rootNote", "Root", ROOTS, 0),
        ],
        "main_buttons": [("btn", "reset", "Reset")],
        "editor_dials": [("dial", "scaleRotate", "Mode", 0, 11, 0, None)] + COMMON_EDITOR,
        "editor_tabs": [
            ("tab", "loop", "Play", ["Once", "Loop"], 1),
            ("tab", "playMode", "Sweep timing", ["Bar-aligned", "Free"], 1),
        ] + COMMON_TABS,
        "editor_buttons": [("btn", "clearSteps", "Clear Stages"), ("btn", "initSteps", "Init Stages")],
    },
    "phaseflux": {
        "title": "XF PhaseFlux",
        "main_dials": [
            ("dial", "pulseNudge", "Pulses +-", -15, 15, 0, None), ("dial", "lenNudge", "Length +-", -16, 16, 0, None),
            ("dial", "warpNudge", "Warp", -64, 64, 0, None), ("dial", "responseNudge", "Response", -64, 64, 0, None),
            ("dial", "cyclePhaseWarp", "Cycle Warp", -64, 64, 0, None), ("dial", "globalPhase", "Phase", 0, 100, 0, "%"),
            ("dial", "octave", "Octave", -10, 10, 0, None), ("dial", "transpose", "Transp", -48, 48, 0, "st"),
        ],
        "main_menus": [
            ("menu", "traversal", "Path", ["Snake", "Rows", "SnakeUp", "Cols", "ColSnake", "Spiral", "Inward", "Diag"], 0),
            ("menu", "divisor", "Rate", DIVISOR_NAMES, 3),
            ("tab", "pitchMode", "Pitch Mode", ["Per stage", "Global"], 0),
            ("menu", "scale", "Scale", SCALE_NAMES, 1), ("menu", "rootNote", "Root", ROOTS, 0),
        ],
        "main_buttons": [("btn", "reset", "Reset")],
        "editor_dials": [
            ("dial", "firstStage", "First", 1, 16, 1, None), ("dial", "lastStage", "Last", 1, 16, 16, None),
            ("dial", "clockMult", "Clock Mult", 50, 150, 100, "%"), ("dial", "scaleRotate", "Mode", 0, 11, 0, None),
            ("dial", "noteAccPos", "NAcc Max", 0, 64, 28, None), ("dial", "noteAccNeg", "NAcc Min", 0, 64, 28, None),
            ("dial", "noteAccReset", "NAcc Reset", 0, 15, 0, None), ("dial", "pulseAccPos", "PAcc Max", 0, 16, 16, None),
            ("dial", "pulseAccNeg", "PAcc Min", 0, 16, 16, None), ("dial", "pulseAccReset", "PAcc Reset", 0, 15, 0, None),
        ] + COMMON_EDITOR,
        "editor_tabs": [
            ("tab", "cycleLength", "Cycle", ["Adaptive", "Fixed"], 0),
            ("menu", "pitchRate", "Pitch Rate", ["1:7", "1:5", "1:3", "2:5", "3:5", "5:7", "7:9", "11:12", "16:17", "1:1",
                                                 "18:17", "13:12", "11:9", "9:7", "7:5", "5:3", "9:5"], 9),
            ("tab", "noteAccScope", "Note acc", ["Per stage", "Shared"], 0),
            ("tab", "noteAccOrder", "Note acc order", ["Wrap", "Pendulum", "Hold", "Reset"], 0),
            ("tab", "noteAccPolarity", "Note acc range", ["Uni", "Bi"], 0),
            ("tab", "pulseAccScope", "Pulse acc", ["Per stage", "Shared"], 0),
            ("tab", "pulseAccOrder", "Pulse acc order", ["Wrap", "Pendulum", "Hold", "Reset"], 0),
            ("tab", "pulseAccPolarity", "Pulse acc range", ["Uni", "Bi"], 0),
        ] + COMMON_TABS,
        "editor_buttons": [("btn", "clearSteps", "Clear Stages"), ("btn", "initSteps", "Init Stages")],
    },
    "fractal": {
        "title": "XF Fractal",
        "main_dials": [
            ("dial", "ornRate", "Ornaments", 0, 100, 0, "%"), ("dial", "ornIntensity", "Orn Depth", 0, 100, 0, "%"),
            ("dial", "branchCount", "Branches", 0, 7, 0, None), ("dial", "path", "Path", 0, 255, 0, None),
            ("dial", "branchSeed", "Seed", 0, 255, 0, None), ("dial", "rotate", "Rotate", -64, 64, 0, None),
            ("dial", "octave", "Octave", -10, 10, 0, None), ("dial", "transpose", "Transp", -48, 48, 0, "st"),
        ],
        "main_menus": [
            ("menu", "orderMode", "Order", ["Forward", "Reverse", "Pendulum", "Random", "Converge", "Diverge", "Page"], 0),
            ("menu", "divisor", "Rate", DIVISOR_NAMES, 3),
            ("tab", "cadence", "Capture", ["Section", "Event"], 1),
            ("menu", "scale", "Scale", SCALE_NAMES, 1), ("menu", "rootNote", "Root", ROOTS, 0),
        ],
        "main_buttons": [("tog", "record", "Record", 0), ("tog", "lock", "Lock", 0), ("btn", "clear", "Clear")],
        "editor_dials": [
            ("dial", "loopFirst", "Loop First", 1, 128, 1, None), ("dial", "loopLast", "Loop Last", 1, 128, 16, None),
            ("dial", "recordFirst", "Rec First", 1, 128, 1, None), ("dial", "recordLast", "Rec Last", 1, 128, 16, None),
            ("dial", "recordSkip", "Rec Skip", 0, 15, 0, None), ("dial", "ornFirst", "Orn First", 1, 128, 1, None),
            ("dial", "ornLast", "Orn Last", 1, 128, 16, None), ("dial", "trackDelay", "Delay", 0, 16, 0, None),
            ("dial", "branchPool", "Pool", 0, 255, 255, None), ("dial", "clockMult", "Clock Mult", 50, 150, 100, "%"),
            ("dial", "scaleRotate", "Mode", 0, 11, 0, None),
        ] + COMMON_EDITOR,
        "editor_tabs": [
            ("tab", "fidelity", "Timing", ["Quantized", "Feel"], 1),
            ("tab", "recordMode", "Record mode", ["Replace", "Latch"], 0),
            ("tab", "quantize", "Main note", ["As played", "To scale"], 0),
            ("tab", "thru", "MIDI Thru", ["Off", "On"], 0),
        ] + COMMON_TABS,
        "editor_buttons": [("btn", "reset", "Reset"), ("btn", "initSteps", "Demo Phrase")],
    },
}


# ---------------------------------------------------------------------------
#  Patch writer
# ---------------------------------------------------------------------------
class Patch:
    def __init__(self):
        self.boxes, self.lines, self.n = [], [], 0

    def add(self, box):
        self.n += 1
        box["id"] = "obj-%d" % self.n
        self.boxes.append({"box": box})
        return box["id"]

    def obj(self, text, x, y, ins=1, outs=1, types=None, w=None, **extra):
        b = {"maxclass": "newobj", "text": text, "numinlets": ins, "numoutlets": outs,
             "outlettype": types if types is not None else [""] * outs,
             "patching_rect": [x, y, w or max(30, 7 * len(text) + 12), 22.0]}
        b.update(extra)
        return self.add(b)

    def msg(self, text, x, y):
        return self.add({"maxclass": "message", "text": text, "numinlets": 2, "numoutlets": 1,
                         "outlettype": [""], "patching_rect": [x, y, max(30, 7 * len(text) + 14), 22.0]})

    def comment(self, text, x, y, w=300, pres=None, size=None):
        b = {"maxclass": "comment", "text": text, "numinlets": 1, "numoutlets": 0,
             "patching_rect": [x, y, w, 20.0]}
        if pres:
            b.update({"presentation": 1, "presentation_rect": pres, "textcolor": [0.75, 0.75, 0.75, 1.0]})
        if size:
            b["fontsize"] = size
        return self.add(b)

    def connect(self, a, ao, b, bi=0):
        self.lines.append({"patchline": {"source": [a, ao], "destination": [b, bi]}})


def valueof(longname, short, ptype, extra):
    v = {"parameter_longname": longname, "parameter_shortname": short, "parameter_type": ptype}
    v.update(extra)
    return {"valueof": v}


def live_control(P, ctl, x, y, pres, prefix):
    """Creates one live.* control and the [prepend p name] after it.
    Returns (control id, output id to send on)."""
    kind, name = ctl[0], ctl[1]
    long = prefix + ctl[2]
    if kind == "dial":
        _, _, label, lo, hi, dflt, unit = ctl
        extra = {"parameter_mmin": lo, "parameter_mmax": hi, "parameter_initial_enable": 1,
                 "parameter_initial": [dflt], "parameter_unitstyle": 0}
        if unit:
            extra["parameter_unitstyle"] = {"%": 5, "st": 7}[unit]
        b = {"maxclass": "live.dial", "varname": name, "numinlets": 1, "numoutlets": 2,
             "outlettype": ["", "float"], "patching_rect": [x, y, 44, 48],
             "presentation": 1, "presentation_rect": pres, "parameter_enable": 1,
             "saved_attribute_attributes": valueof(long, label, 1, extra)}
    elif kind in ("menu", "tab"):
        _, _, label, items, dflt = ctl
        extra = {"parameter_enum": items, "parameter_mmax": len(items) - 1, "parameter_initial_enable": 1,
                 "parameter_initial": [dflt]}
        b = {"maxclass": "live.menu" if kind == "menu" else "live.tab", "varname": name, "numinlets": 1,
             "numoutlets": 3, "outlettype": ["", "", "float"], "patching_rect": [x, y, 100, 15],
             "presentation": 1, "presentation_rect": pres, "parameter_enable": 1,
             "saved_attribute_attributes": valueof(long, label, 2, extra)}
        if kind == "tab":
            b.update({"num_lines_patching": 1, "num_lines_presentation": 1})
            b["saved_attribute_attributes"]["valueof"]["parameter_unitstyle"] = 9
    elif kind == "numb":
        _, _, label, lo, hi, dflt, style = ctl
        extra = {"parameter_mmin": lo, "parameter_mmax": hi, "parameter_initial_enable": 1,
                 "parameter_initial": [dflt], "parameter_unitstyle": style}
        b = {"maxclass": "live.numbox", "varname": name, "numinlets": 1, "numoutlets": 2,
             "outlettype": ["", "float"], "patching_rect": [x, y, 44, 15],
             "presentation": 1, "presentation_rect": pres, "parameter_enable": 1,
             "saved_attribute_attributes": valueof(long, label, 1, extra)}
    elif kind == "tog":
        _, _, label, dflt = ctl
        extra = {"parameter_enum": ["off", "on"], "parameter_mmax": 1, "parameter_initial_enable": 1,
                 "parameter_initial": [dflt]}
        b = {"maxclass": "live.text", "varname": name, "text": label, "texton": label, "mode": 1,
             "numinlets": 1, "numoutlets": 2, "outlettype": ["", ""], "patching_rect": [x, y, 44, 15],
             "presentation": 1, "presentation_rect": pres, "parameter_enable": 1,
             "saved_attribute_attributes": valueof(long, label, 2, extra)}
    else:
        raise ValueError(kind)
    cid = P.add(b)
    pre = P.obj("prepend p " + name, x, y + 60 if kind == "dial" else y + 26)
    P.connect(cid, 0, pre, 0)
    return cid, pre


def action_button(P, action, label, x, y, pres, prefix, mappable=True):
    """live.text in button mode -> sel 1 -> "act <action>"."""
    b = {"maxclass": "live.text", "varname": "btn_" + action, "text": label, "mode": 0,
         "numinlets": 1, "numoutlets": 2, "outlettype": ["", ""], "patching_rect": [x, y, 60, 15],
         "presentation": 1, "presentation_rect": pres, "parameter_enable": 1 if mappable else 0}
    if mappable:
        b["saved_attribute_attributes"] = valueof(prefix + label, label, 2,
                                                  {"parameter_enum": ["off", "on"], "parameter_mmax": 1,
                                                   "parameter_invisible": 1})
    cid = P.add(b)
    # a button may send 1 or bang on a click (and 0 on release): take the first two
    sel = P.obj("route bang 1", x, y + 26, 1, 3)
    m = P.msg("act " + action, x, y + 52)
    P.connect(cid, 0, sel, 0)
    P.connect(sel, 0, m, 0)
    P.connect(sel, 1, m, 0)
    return cid, m


def label(P, text, pres, x, y):
    return P.add({"maxclass": "live.comment", "text": text, "numinlets": 1, "numoutlets": 0,
                  "patching_rect": [x, y, 80, 18], "presentation": 1, "presentation_rect": pres,
                  "textjustification": 0, "fontsize": 9.0})


def patcher_dict(P, rect, presentation=True, extra=None):
    d = {"fileversion": 1,
         "appversion": {"major": 9, "minor": 0, "revision": 0, "architecture": "x64", "modernui": 1},
         "classnamespace": "box", "rect": rect, "bglocked": 0,
         "openinpresentation": 1 if presentation else 0,
         "default_fontsize": 10.0, "default_fontface": 0, "default_fontname": "Arial Bold",
         "gridonopen": 1, "gridsize": [15.0, 15.0], "gridsnaponopen": 1, "objectsnaponopen": 1,
         "statusbarvisible": 2, "toolbarvisible": 1, "lefttoolbarpinned": 0, "toptoolbarpinned": 0,
         "righttoolbarpinned": 0, "bottomtoolbarpinned": 0, "toolbars_unpinned_last_save": 0,
         "tallnewobj": 0, "boxanimatetime": 200, "enablehscroll": 1, "enablevscroll": 1,
         "devicewidth": 0.0, "description": "", "digest": "", "tags": "", "style": "",
         "subpatcher_template": "", "assistshowspatchername": 0,
         "boxes": P.boxes, "lines": P.lines}
    if extra:
        d.update(extra)
    return d


def jsui(P, device, view, x, y, w, h, pres):
    return P.add({"maxclass": "jsui", "filename": "xf_display.js", "jsarguments": [device, view],
                  "numinlets": 1, "numoutlets": 1, "outlettype": [""], "parameter_enable": 0,
                  "patching_rect": [x, y, w, h], "presentation": 1, "presentation_rect": pres,
                  "border": 0})


# ---------------------------------------------------------------------------
#  Editor (pop-out window, a [p editor] subpatcher)
# ---------------------------------------------------------------------------
EDITOR_W, EDITOR_H = 840, 520


def build_editor(key, cfg, prefix):
    P = Patch()
    P.add({"maxclass": "panel", "numinlets": 1, "numoutlets": 0, "patching_rect": [0, 0, 60, 30],
           "presentation": 1, "presentation_rect": [0, 0, EDITOR_W, EDITOR_H],
           "bgcolor": [0.14, 0.14, 0.14, 1.0], "background": 1, "mode": 0, "border": 0})
    P.obj("inlet", 10, 10, 0, 1)        # pcontrol talks to the window through this box
    send = P.obj("s ---xfp", 600, 700)
    recv = P.obj("r ---xfd", 600, 40, 1, 1)
    gh = 300 if key != "algo" else 280
    view = jsui(P, key, "editor", 600, 80, 400, 200, [10, 10, EDITOR_W - 20, gh])
    P.connect(recv, 0, view, 0)
    P.connect(view, 0, send, 0)
    names = []
    # dials, 14 per row
    y0 = gh + 20
    px = 10
    for i, ctl in enumerate(cfg["editor_dials"]):
        col, row = i % 14, i // 14
        pres = [10 + col * 58, y0 + row * 58, 44, 48]
        cid, pre = live_control(P, ctl, 20 + i * 70, 400 + row * 100, pres, prefix)
        P.connect(pre, 0, send, 0)
        names.append(ctl[1])
    rows = (len(cfg["editor_dials"]) + 13) // 14
    # tabs / menus / numbox with labels, 4 per row
    ty = y0 + rows * 58 + 4
    for i, ctl in enumerate(cfg["editor_tabs"]):
        col, row = i % 5, i // 5
        x, y = 10 + col * 164, ty + row * 34
        label(P, ctl[2], [x, y, 150, 14], 20 + i * 110, 650)
        w = 50 if ctl[0] == "numb" else 150
        cid, pre = live_control(P, ctl, 20 + i * 110, 600, [x, y + 14, w, 16], prefix)
        P.connect(pre, 0, send, 0)
        names.append(ctl[1])
    trows = (len(cfg["editor_tabs"]) + 4) // 5
    by = ty + trows * 34 + 4
    for i, ctl in enumerate(cfg["editor_buttons"]):
        cid, m = action_button(P, ctl[1], ctl[2], 20 + i * 90, 760, [10 + i * 98, by, 90, 18], prefix)
        P.connect(m, 0, send, 0)
    height = max(by + 30, ty + trows * 34 + 10)
    P.boxes[0]["box"]["presentation_rect"][3] = height      # background panel fills the window
    # the window is dark, outside Live's theme: labels and values are white, but drop-down
    # menus and option buttons sit on light grey (or orange when selected), so they get black
    WHITE = [0.95, 0.95, 0.95, 1.0]
    BLACK = [0.0, 0.0, 0.0, 1.0]
    for b in P.boxes:
        b = b["box"]
        if b["maxclass"] in ("live.menu", "live.tab"):
            b["textcolor"] = BLACK
            if b["maxclass"] == "live.tab":
                b["activetextcolor"] = BLACK
        elif b["maxclass"] in ("live.comment", "live.dial", "live.numbox", "live.text", "comment"):
            b["textcolor"] = WHITE
    P.comment("X|FORMER " + cfg["title"][3:] + " editor", 10, 40, 300)
    return P, names, height


# ---------------------------------------------------------------------------
#  Main device patch
# ---------------------------------------------------------------------------
def build_device(key, cfg):
    P = Patch()
    prefix = ""      # Live shows these names in automation lanes
    names = []

    # --- engine ---
    js = P.obj("v8 xf_%s.js" % key, 300, 420, 1, 5, ["", "", "", "", ""], w=120,
               saved_object_attributes={"parameter_enable": 0})
    recv = P.obj("r ---xfp", 300, 380)
    P.connect(recv, 0, js, 0)
    send = P.obj("s ---xfp", 40, 900)

    # --- clock ---
    P.comment("clock: 1 slot = 5 ticks = 1/96 note, only while Live plays", 40, 20, 340)
    metro = P.obj("metro 5 ticks @active 1 @quantize 5 ticks", 40, 50, 2, 1, ["bang"], w=230)
    trans = P.obj("transport", 40, 85, 2, 9, ["int", "int", "float", "float", "float", "", "int", "float", ""])
    P.connect(metro, 0, trans, 0)
    toslot = P.obj("expr int($f1 / 5. + 0.5)", 40, 120, 1, 1, w=150)
    P.connect(trans, 7, toslot, 0)
    tii = P.obj("t i i i", 40, 155, 1, 3, ["int", "int", "int"])
    P.connect(toslot, 0, tii, 0)
    coll = P.obj("coll", 160, 230, 1, 4, ["", "", "", ""], saved_object_attributes={"embed": 0, "precision": 6})
    P.connect(tii, 2, coll, 0)                       # 1. play this slot
    rem = P.obj("prepend remove", 120, 195)
    P.connect(tii, 1, rem, 0)                        # 2. forget it
    P.connect(rem, 0, coll, 0)
    pos = P.obj("prepend pos", 40, 195)
    P.connect(tii, 0, pos, 0)                        # 3. tell the engine
    P.connect(pos, 0, js, 0)
    iter2 = P.obj("zl iter 2", 160, 265, 2, 2)
    P.connect(coll, 0, iter2, 0)
    mf = P.obj("midiformat", 160, 300, 7, 2, ["int", ""])
    P.connect(iter2, 0, mf, 0)
    flush = P.obj("midiflush", 160, 335, 1, 1, ["int"])
    P.connect(mf, 0, flush, 0)
    mout = P.obj("midiout", 160, 370, 1, 0, [])
    P.connect(flush, 0, mout, 0)
    tempo = P.obj("change 0.", 300, 120, 1, 3, ["", "int", "int"])
    P.connect(trans, 4, tempo, 0)
    ptempo = P.obj("prepend tempo", 300, 155)
    P.connect(tempo, 0, ptempo, 0)
    P.connect(ptempo, 0, js, 0)
    tsig = P.obj("zl change", 420, 120, 2, 2)
    P.connect(trans, 5, tsig, 0)
    ptsig = P.obj("prepend timesig", 420, 155)
    P.connect(tsig, 0, ptsig, 0)
    P.connect(ptsig, 0, js, 0)

    # --- engine outputs ---
    P.connect(js, 0, coll, 0)                        # scheduled notes
    P.connect(js, 1, mf, 0)                          # notes due right now
    P.connect(js, 2, flush, 0)                       # stop: end hanging notes
    dsend = P.obj("s ---xfd", 420, 470)
    P.connect(js, 3, dsend, 0)
    pattr = P.obj("pattr xfstate", 540, 470, 1, 3, ["", "", ""], varname="xfstate",
                  saved_object_attributes={"parameter_enable": 1, "parameter_mappable": 0},
                  saved_attribute_attributes=valueof("xfstate", "xfstate", 3, {"parameter_invisible": 1}))
    P.connect(js, 4, pattr, 0)
    pst = P.obj("prepend xfstate", 540, 505)
    P.connect(pattr, 0, pst, 0)
    P.connect(pst, 0, js, 0)

    # --- transport state + load ---
    dev = P.obj("live.thisdevice", 560, 20, 1, 3, ["bang", "int", "int"])
    tbb = P.obj("t b b b", 560, 55, 1, 3, ["bang", "bang", "bang"])
    P.connect(dev, 0, tbb, 0)
    mpath = P.msg("path live_set", 640, 90)
    P.connect(tbb, 2, mpath, 0)
    lpath = P.obj("live.path", 640, 125, 1, 3, ["", "", ""])
    P.connect(mpath, 0, lpath, 0)
    mprop = P.msg("property is_playing", 560, 125)
    P.connect(tbb, 1, mprop, 0)
    obs = P.obj("live.observer", 560, 160, 2, 2, ["", ""], saved_object_attributes={"_persistence": 1})
    P.connect(mprop, 0, obs, 0)
    P.connect(lpath, 0, obs, 1)
    pplay = P.obj("prepend playing", 560, 195)
    P.connect(obs, 0, pplay, 0)
    P.connect(pplay, 0, js, 0)
    mload = P.msg("loaded", 700, 90)
    P.connect(tbb, 0, mload, 0)
    P.connect(mload, 0, js, 0)

    # --- MIDI in (for Reset by note) ---
    midin = P.obj("midiin", 800, 20, 1, 1, ["int"])
    mparse = P.obj("midiparse", 800, 55, 1, 8, ["", "", "", "int", "int", "", "int", ""])
    P.connect(midin, 0, mparse, 0)
    pnote = P.obj("prepend midinote", 800, 90)
    P.connect(mparse, 0, pnote, 0)
    P.connect(pnote, 0, js, 0)

    # --- display strip ---
    # layout: [display 240 wide] [dials 4 x 2] [menus column]
    view = jsui(P, key, "main", 40, 560, 240, 110, [4, 4, 236, 116])
    drecv = P.obj("r ---xfd", 40, 530)
    P.connect(drecv, 0, view, 0)
    route = P.obj("route open", 40, 690, 2, 2)
    P.connect(view, 0, route, 0)
    mopen = P.msg("open", 40, 720)
    P.connect(route, 0, mopen, 0)
    P.connect(route, 1, send, 0)
    pctl = P.obj("pcontrol", 40, 760)
    P.connect(mopen, 0, pctl, 0)

    # buttons under the display
    btns = cfg["main_buttons"] + [("btn", "editor", "Editor")]
    bw = (236 - 4 * (len(btns) - 1)) / len(btns)
    for i, ctl in enumerate(btns):
        pres = [4 + i * (bw + 4), 126, bw, 18]
        if ctl[1] == "editor":
            b = {"maxclass": "live.text", "varname": "btn_editor", "text": "Editor", "mode": 0,
                 "numinlets": 1, "numoutlets": 2, "outlettype": ["", ""], "patching_rect": [140, 690, 60, 15],
                 "presentation": 1, "presentation_rect": pres, "parameter_enable": 0}
            eid = P.add(b)
            sel = P.obj("route bang 1", 140, 715, 1, 3)
            P.connect(eid, 0, sel, 0)
            P.connect(sel, 0, mopen, 0)
            P.connect(sel, 1, mopen, 0)
        elif ctl[0] == "tog":
            cid, pre = live_control(P, ctl, 220 + i * 90, 690, pres, prefix)
            P.connect(pre, 0, send, 0)
            names.append(ctl[1])
        else:
            cid, m = action_button(P, ctl[1], ctl[2], 220 + i * 90, 690, pres, prefix)
            P.connect(m, 0, send, 0)
    P.comment("click the display or Editor for the big window", 0, 0, 10,
              pres=[4, 148, 236, 16], size=9.0)

    # dials 4 x 2
    for i, ctl in enumerate(cfg["main_dials"]):
        col, row = i % 4, i // 4
        pres = [248 + col * 50, 4 + row * 58, 44, 48]
        cid, pre = live_control(P, ctl, 300 + i * 60, 600 + row * 120, pres, prefix)
        P.connect(pre, 0, send, 0)
        names.append(ctl[1])
    # menus / tabs with labels, one column
    mx = 248 + 4 * 50 + 6
    n = len(cfg["main_menus"])
    step = min(28, 165 / n)
    for i, ctl in enumerate(cfg["main_menus"]):
        y = 2 + i * step
        label(P, ctl[2], [mx, y, 96, 12], 800 + i * 110, 600)
        cid, pre = live_control(P, ctl, 800 + i * 110, 630, [mx, y + 12, 96, 15], prefix)
        P.connect(pre, 0, send, 0)
        names.append(ctl[1])
    width = mx + 100
    longs = [b["box"]["saved_attribute_attributes"]["valueof"]["parameter_longname"]
             for b in P.boxes if "saved_attribute_attributes" in b["box"]]

    # --- pop-out editor ---
    E, enames, eh = build_editor(key, cfg, prefix)
    names += enames
    longs += [b["box"]["saved_attribute_attributes"]["valueof"]["parameter_longname"]
              for b in E.boxes if "saved_attribute_attributes" in b["box"]]
    dup = set(l for l in longs if longs.count(l) > 1)
    assert not dup, "duplicate parameter names: %s" % dup
    assert len(set(names)) == len(names), "duplicate scripting names"
    ed = P.obj("p editor", 40, 800, 1, 0, [])
    ebox = P.boxes[-1]["box"]
    ebox["patcher"] = patcher_dict(E, [100.0, 100.0, float(EDITOR_W), float(eh)],
                                   extra={"toolbarvisible": 0, "statusbarvisible": 0,
                                          "enablehscroll": 0, "enablevscroll": 0,
                                          "title": cfg["title"] + " editor"})
    ebox["saved_object_attributes"] = {"description": "", "digest": "", "globalpatchername": "", "tags": ""}
    P.connect(pctl, 0, ed, 0)

    P.comment("X|FORMER %s for Live. Engines: xf_%s.js (v8), display: xf_display.js (jsui)." % (cfg["title"][3:], key),
              40, 960, 500)
    return P, names, width


def amxd_bytes(patcher):
    body = json.dumps({"patcher": patcher}, indent=1).encode("utf-8") + b"\n\x00"
    out = b"ampf" + struct.pack("<I", 4) + b"mmmm"
    out += b"meta" + struct.pack("<I", 4) + b"\x00\x00\x00\x00"
    out += b"ptch" + struct.pack("<I", len(body)) + body
    return out


ENGINE_FILES = {"algo": "tuesday.js", "stochastic": "stochastic.js", "note": "note.js", "indexed": "indexed.js",
                "discrete": "discrete.js", "phaseflux": "phaseflux.js", "fractal": "fractal.js"}


def build_js(key, names):
    parts = ["// X|FORMER %s for Max for Live - built file, edit the sources in m4l/src\n" % key,
             'var XF_DEVICE = "%s";\n' % key,
             "var XF_PARAMS = %s;\n" % json.dumps(names)]
    for f in ["common.js", ENGINE_FILES[key], "host.js", "max_glue.js"]:
        parts.append("\n// ---------------- %s ----------------\n" % f)
        parts.append(open(os.path.join(SRC, f)).read())
    return "".join(parts)


def main():
    os.makedirs(OUT, exist_ok=True)
    for key, cfg in DEVICES.items():
        P, names, width = build_device(key, cfg)
        patcher = patcher_dict(P, [60.0, 80.0, 1100.0, 1000.0],
                               extra={"devicewidth": float(width),
                                      "dependency_cache": [
                                          {"name": "xf_%s.js" % key, "bootpath": ".", "patcherrelativepath": ".",
                                           "type": "TEXT", "implicit": 1},
                                          {"name": "xf_display.js", "bootpath": ".", "patcherrelativepath": ".",
                                           "type": "TEXT", "implicit": 1}],
                                      "latency": 0, "is_mpe": 0, "minimum_live_version": "",
                                      "minimum_max_version": "", "platform_compatibility": 0,
                                      "autosave": 0})
        with open(os.path.join(OUT, cfg["title"] + ".amxd"), "wb") as f:
            f.write(amxd_bytes(patcher))
        with open(os.path.join(OUT, "xf_%s.js" % key), "w") as f:
            f.write(build_js(key, names))
        # the same patch as a plain .maxpat, handy for reading in Max
        with open(os.path.join(OUT, cfg["title"] + ".maxpat"), "w") as f:
            json.dump({"patcher": patcher}, f, indent=1)
        print("built", cfg["title"], "params:", len(names), "width:", width)
    with open(os.path.join(OUT, "xf_display.js"), "w") as f:
        f.write(open(os.path.join(SRC, "xf_display.js")).read())


if __name__ == "__main__":
    main()
