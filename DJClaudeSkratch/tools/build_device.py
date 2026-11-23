"""Builds "DJ Claude Skratch.amxd" (and .maxpat) from src/. The device is generated, never hand-edited.

    python3 tools/build_device.py

Writes: DJ Claude Skratch.amxd, DJ Claude Skratch.maxpat, build/skratch_core.genexpr,
        ref/core_sim.js, and copies of the scripts next to the device (they are also embedded).
"""
import base64, json, os, struct, shutil, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, 'ref'))
import genexpr

NAME = 'DJ Claude Skratch'
VERSION = '1.0.0'
DEVICE_W = 1150
DEVICE_H = 169
APP = {"major": 9, "minor": 0, "revision": 5, "architecture": "x64", "modernui": 1}
BUFS = '---skr_rec ---skr_cut ---skr_q ---skr_f ---skr_stat'
WORDS = ['baby', 'forward', 'stab', 'chirp', 'flare1', 'flare2', 'crab', 'tear',
         'scribble', 'transform', 'drag', 'tip', 'lazer', 'release', 'echo', 'reverse']
LABEL = ['BABY', 'FORWARD', 'STAB', 'CHIRP', '1-FLARE', '2-FLARE', 'CRAB', 'TEAR',
         'SCRIBBLE', 'TRANSFRM', 'DRAG', 'TIP', 'LAZER', 'RELEASE', 'ECHO', 'REVERSE']
LONG = ['Baby', 'Forward', 'Stab', 'Chirp', '1-Click Flare', '2-Click Flare', 'Crab', 'Tear',
        'Scribble', 'Transform', 'Drag', 'Tip', 'Lazer', 'Release', 'Echo', 'Reverse']
CELLS = ['rest', 'baby8', 'baby16', 'pickup', 'baby-rest', 'fwd-fwd', 'fwd-baby', 'stabs', 'stab-off', 'chirp',
         'chirp-baby', 'flare1', 'flare2', 'crab', 'tear', 'tear-trip', 'scribble', 'tips', 'lazer', 'transform',
         'transform-trip', 'drag', 'echo', 'release']
ROUTINE_DEFAULT = ['tear-trip', 'tear-trip', 'chirp', 'fwd-fwd', 'tear-trip', 'tear-trip', 'chirp', 'fwd-fwd',
                   'fwd-baby', 'stabs', 'rest', 'baby16', 'pickup', 'stabs', 'rest', 'baby16']

boxes, lines, params = [], [], {}
_n = [0]


def nid():
    _n[0] += 1
    return 'obj-%d' % _n[0]


def box(maxclass, rect, text=None, nin=1, nout=0, otype=None, pres=None, **kw):
    b = {"id": nid(), "maxclass": maxclass, "numinlets": nin, "numoutlets": nout,
         "patching_rect": [float(v) for v in rect]}
    if otype is not None:
        b["outlettype"] = otype
    if text is not None:
        b["text"] = text
    if pres is not None:
        b["presentation"] = 1
        b["presentation_rect"] = [float(v) for v in pres]
    b.update(kw)
    boxes.append({"box": b})
    return b["id"]


def obj(text, rect, nin, nout, otype=None, **kw):
    return box("newobj", rect, text=text, nin=nin, nout=nout,
               otype=otype if otype is not None else [""] * nout, **kw)


def builtin_b64():
    """The built-in record: the synthesized test sound from ref/sounds.py, 16-bit mono."""
    from sounds import record
    x, _ = record()
    x = x / np.abs(x).max() * 0.89
    pcm = np.round(x * 32767).astype('<i2').tobytes()
    return base64.b64encode(pcm).decode('ascii')


def script_text(name):
    code = open(os.path.join(ROOT, 'src', name)).read()
    if name == 'skratch.js':
        assert "/*BUILTIN*/''" in code
        code = code.replace("/*BUILTIN*/''", "'" + builtin_b64() + "'")
    return code


def embedded(name):
    """The script is stored inside the device, so Live never has to find a .js file."""
    return {"text": script_text(name), "filename": "none", "flags": 0, "embed": 1, "autowatch": 1}


def line(src, so, dst, di):
    lines.append({"patchline": {"source": [src, so], "destination": [dst, di]}})


def valueof(long, short, ptype, **kw):
    v = {"parameter_longname": long, "parameter_shortname": short, "parameter_type": ptype}
    v.update(kw)
    return {"valueof": v}


def param(bid, long, short):
    params[bid] = [long, short, 0]


# ----------------------------------------------------------------- core objects
midi = obj("notein", [20, 20, 50, 22], 1, 3, ["int", "int", "int"])
pk = obj("pack 0 0", [20, 50, 60, 22], 2, 1)
pn = obj("prepend note", [20, 80, 80, 22], 1, 1)
line(midi, 0, pk, 0); line(midi, 1, pk, 1); line(pk, 0, pn, 0)
pph = obj("plugphasor~", [120, 20, 80, 22], 1, 1, ["signal"])
code = genexpr.expand()
open(os.path.join(ROOT, 'build', 'skratch_core.genexpr'), 'w').write(code)
open(os.path.join(ROOT, 'ref', 'core_sim.js'), 'w').write(genexpr.to_js(code))
gen_patcher = {
    "fileversion": 1, "appversion": APP, "classnamespace": "dsp.gen",
    "rect": [100.0, 100.0, 900.0, 700.0], "bglocked": 0, "openinpresentation": 0,
    "default_fontsize": 12.0, "default_fontface": 0, "default_fontname": "Arial",
    "gridonopen": 1, "gridsize": [15.0, 15.0], "gridsnaponopen": 1, "objectsnaponopen": 1,
    "statusbarvisible": 2, "toolbarvisible": 1, "boxanimatetime": 200,
    "enablehscroll": 1, "enablevscroll": 1, "devicewidth": 0.0, "description": "", "digest": "",
    "tags": "", "style": "", "subpatcher_template": "",
    "boxes": [
        {"box": {"id": "obj-1", "maxclass": "newobj", "numinlets": 0, "numoutlets": 1, "outlettype": [""], "patching_rect": [20.0, 20.0, 30.0, 22.0], "text": "in 1"}},
        {"box": {"id": "obj-4", "maxclass": "codebox", "numinlets": 1, "numoutlets": 2, "outlettype": ["", ""],
                 "patching_rect": [20.0, 60.0, 840.0, 560.0], "fontface": 0, "fontname": "Menlo", "fontsize": 11.0, "code": code}},
        {"box": {"id": "obj-5", "maxclass": "newobj", "numinlets": 1, "numoutlets": 0, "patching_rect": [20.0, 640.0, 40.0, 22.0], "text": "out 1"}},
        {"box": {"id": "obj-6", "maxclass": "newobj", "numinlets": 1, "numoutlets": 0, "patching_rect": [120.0, 640.0, 40.0, 22.0], "text": "out 2"}},
    ],
    "lines": [
        {"patchline": {"source": ["obj-1", 0], "destination": ["obj-4", 0]}},
        {"patchline": {"source": ["obj-4", 0], "destination": ["obj-5", 0]}},
        {"patchline": {"source": ["obj-4", 1], "destination": ["obj-6", 0]}},
    ],
}
gen = obj("gen~", [120, 120, 200, 22], 1, 2, ["signal", "signal"], patcher=gen_patcher)
pout = obj("plugout~", [120, 200, 60, 22], 2, 2, ["signal", "signal"])
line(pph, 0, gen, 0)
line(gen, 0, pout, 0); line(gen, 1, pout, 1)

# buffers, named per device instance with ---; sizes in ms (cut: 20 s of record at 4 x)
y = 260
for name, args in (("skr_rec", "20000 2"), ("skr_cut", "80000 2"), ("skr_q", "100"), ("skr_f", "100"), ("skr_stat", "10")):
    obj("buffer~ ---%s %s" % (name, args), [20, y, 180, 22], 1, 2, ["float", "bang"])
    y += 28

# bind gen~'s Buffers to this device's buffers
lb = obj("loadbang", [260, 20, 60, 22], 1, 1, ["bang"])
bind = box("message", [260, 50, 330, 22], text="cut ---skr_cut, q ---skr_q, fq ---skr_f, st ---skr_stat", nin=2, nout=1, otype=[""])
line(lb, 0, bind, 0); line(bind, 0, gen, 0)

# the planner
js = obj("v8", [400, 120, 300, 22], 1, 2, ["", ""], textfile=embedded("skratch.js"),
         parameter_enable=1,
         saved_attribute_attributes=valueof(NAME + " State", "State", 3, parameter_invisible=1))
param(js, NAME + " State", "State")
line(js, 0, gen, 0)
line(pn, 0, js, 0)
ltd = obj("live.thisdevice", [400, 20, 90, 22], 1, 3, ["bang", "int", "int"])
initm = box("message", [400, 50, 330, 22], text="names " + BUFS + ", init", nin=2, nout=1, otype=[""])
line(ltd, 0, initm, 0); line(initm, 0, js, 0)

# js outlet 1 -> panel
UIROUTE = ["status", "strip", "cue", "timing", "weight", "slop", "risk"]
route = obj("route " + " ".join(UIROUTE), [400, 170, 420, 22], 1, len(UIROUTE) + 1)
line(js, 1, route, 0)
ROUTE_OUT = {k: i for i, k in enumerate(UIROUTE)}

# ----------------------------------------------------------------- panel
px = [700]
FONT = {"fontname": "Ableton Sans Medium", "fontsize": 9.0}
FONTS = {"fontname": "Ableton Sans Medium", "fontsize": 8.0}


def wire(bid, name, outlet=0):
    pre = obj("prepend " + name, [px[0], 600, 90, 22], 2, 1)
    line(bid, outlet, pre, 0)
    line(pre, 0, js, 0)
    px[0] += 100
    return pre


GREY, WHITE = [0.78, 0.78, 0.8, 1.0], [1.0, 1.0, 1.0, 1.0]
TEXTCOL = [GREY]


def label(text, pres, small=False, color=None):
    return box("comment", [px[0], 560, 80, 18], text=text, nin=1, nout=0, pres=pres,
               textcolor=color or TEXTCOL[0], **(FONTS if small else FONT))


def dial(name, long, short, pres, lo, hi, init, unit=1, ptype=0, **kw):
    v = valueof(long, short, ptype, parameter_mmin=float(lo), parameter_mmax=float(hi),
                parameter_initial=[init], parameter_initial_enable=1, parameter_unitstyle=unit)
    b = box("live.dial", [px[0], 500, 41, 48], nin=1, nout=2, otype=["", "float"], pres=pres,
            parameter_enable=1, saved_attribute_attributes=v, varname=long,
            **dict({"textcolor": TEXTCOL[0]} if TEXTCOL[0] is WHITE else {}, **kw))
    param(b, long, short)
    wire(b, name)
    return b


def menu(name, long, short, pres, items, init, patch_only=False):
    v = valueof(long, short, 2, parameter_enum=items, parameter_initial=[init], parameter_initial_enable=1,
                parameter_mmax=len(items) - 1)
    b = box("live.menu", [px[0], 500, 70, 15], nin=1, nout=3, otype=["", "", "float"], pres=pres,
            parameter_enable=1, saved_attribute_attributes=v, varname=long, **FONT)
    param(b, long, short)
    wire(b, name)
    return b


def toggle(name, long, text, texton, pres, init=0, momentary=False, small=False):
    v = valueof(long, text, 2, parameter_enum=["off", "on"], parameter_initial=[init],
                parameter_initial_enable=1, parameter_mmax=1)
    b = box("live.text", [px[0], 500, 60, 20], nin=1, nout=2, otype=["", ""], pres=pres,
            parameter_enable=1, saved_attribute_attributes=v, varname=long, text=text, texton=texton,
            mode=0 if momentary else 1, **(FONTS if small else FONT))
    param(b, long, text)
    wire(b, name)
    return b


def numbox(name, long, short, pres, lo, hi, init, unit=0, ptype=1):
    v = valueof(long, short, ptype, parameter_mmin=float(lo), parameter_mmax=float(hi),
                parameter_initial=[init], parameter_initial_enable=1, parameter_unitstyle=unit)
    b = box("live.numbox", [px[0], 500, 44, 15], nin=1, nout=2, otype=["", "float"], pres=pres,
            parameter_enable=1, saved_attribute_attributes=v, varname=long, **FONT)
    param(b, long, short)
    wire(b, name)
    return b


def panel_bg(pres, color):
    return box("panel", [px[0], 640, 50, 20], nin=1, nout=0, pres=pres, bgcolor=color, background=1,
               border=0, rounded=4)


SEC = [0.17, 0.17, 0.18, 1.0]
SECTIONS = [(2, 300), (304, 396), (702, 112), (816, 136), (954, 140), (1096, 52)]
for x, w in SECTIONS:
    panel_bg([x, 2, w, 165], SEC)

# RECORD
label("RECORD", [6, 4, 80, 14])
strip = box("v8ui", [px[0], 420, 300, 120], nin=1, nout=1, otype=[""], pres=[6, 18, 292, 92],
            filename="none", embed=1, textfile=embedded("skratch_strip.js"), parameter_enable=0)
line(route, ROUTE_OUT["strip"], strip, 0)
wire(strip, "strip")
drop = box("live.drop", [px[0], 500, 84, 44], nin=1, nout=1, otype=[""], pres=[6, 114, 76, 48])
wire(drop, "drop")
label("CUE", [88, 114, 30, 14])
cue = numbox("cue", "Cue Point", "Cue", [88, 128, 36, 15], 1, 8, 1)
line(route, ROUTE_OUT["cue"], cue, 0)
toggle("clear", "Built-In Record", "BUILT-IN", "BUILT-IN", [88, 147, 54, 15], momentary=True)
stat = box("comment", [px[0], 560, 160, 30], text="", nin=1, nout=0, pres=[146, 114, 152, 22],
           textcolor=[1.0, 0.75, 0.3, 1.0], linecount=2, **FONTS)
line(route, ROUTE_OUT["status"], stat, 0)
label("CLICK A MARK TO CUE IT, DRAG TO MOVE. DOUBLE-CLICK TO ADD OR REMOVE.", [146, 138, 152, 26], small=True)

# WORDS
label("WORDS", [308, 4, 80, 14])
for i, w in enumerate(WORDS):
    r, c = divmod(i, 8)
    x, yy = 308 + c * 49, 20 + r * 40
    toggle("btn_" + w, LONG[i], LABEL[i], LABEL[i], [x, yy, 47, 20], momentary=True)
    toggle("on_" + w, "Word On " + LONG[i], "ON", "ON", [x + 12, yy + 23, 23, 11], init=1, small=True)
label("SPEED", [308, 104, 40, 14])
menu("speed", "Speed", "Speed", [308, 118, 46, 15], ["1/8", "1/16", "1/16T", "1/32"], 1)
label("QUANT", [360, 104, 40, 14])
menu("quant", "Quantise", "Quant", [360, 118, 46, 15], ["1/16", "1/8", "1/4", "OFF"], 0)
label("KEYS C1 TO D#2 PLAY THE WORDS, HELD TO REPEAT. C3 TO G3 PICK CUE POINTS 1 TO 8. "
      "THE SMALL SWITCHES SAY WHICH WORDS ROUTINE AND INSTINCT MAY USE.", [414, 102, 282, 40], small=True)

# PLAY
label("PLAY", [706, 4, 60, 14])
v = valueof("Mode", "Mode", 2, parameter_enum=["HANDS", "ROUTINE", "INSTINCT"], parameter_initial=[0],
            parameter_initial_enable=1, parameter_mmax=2)
mode = box("live.tab", [px[0], 500, 70, 48], nin=1, nout=3, otype=["", "", "float"], pres=[706, 18, 64, 48],
           parameter_enable=1, saved_attribute_attributes=v, varname="Mode", num_lines_patching=3,
           num_lines_presentation=3, **FONT)
param(mode, "Mode", "Mode")
wire(mode, "mode")
openr = box("live.text", [px[0], 500, 60, 20], nin=1, nout=2, otype=["", ""], pres=[706, 72, 104, 18],
            text="ROUTINE...", texton="ROUTINE...", mode=0, parameter_enable=0, **FONT)
px[0] += 80
pc = obj("pcontrol", [px[0], 540, 60, 22], 1, 1)
openmsg = box("message", [px[0], 510, 40, 22], text="open", nin=2, nout=1, otype=[""])
line(openr, 0, openmsg, 0); line(openmsg, 0, pc, 0)
label("PHRASE", [706, 96, 60, 14])
label("ON", [706, 110, 20, 14], small=True)
menu("phon", "Phrase On", "Ph On", [722, 110, 40, 15], ["1 BAR", "2 BARS", "4 BARS", "8 BARS"], 2)
label("OFF", [706, 128, 20, 14], small=True)
menu("phoff", "Phrase Off", "Ph Off", [722, 128, 40, 15], ["0", "1 BAR", "2 BARS", "4 BARS", "8 BARS"], 0)
label("BARS OF SCRATCHING, THEN OF REST", [766, 108, 46, 40], small=True)

# INSTINCT
label("INSTINCT", [820, 4, 80, 14])
for i, (nm, long, init) in enumerate((("density", "Density", 50), ("skill", "Skill", 75), ("motif", "Motif", 60),
                                      ("release", "Release Ending", 60), ("funk", "Funk", 50), ("walk", "Walk", 0))):
    r, c = divmod(i, 3)
    dial(nm, long, long.split()[0].upper(), [820 + c * 44, 18 + r * 74, 41, 48], 0, 100, init, unit=5)
label("RELEASE: HOW OFTEN A PHRASE ENDS BY PLAYING THE SOUND. WALK: MOVES BETWEEN CUES.", [820, 140, 130, 26], small=True)

# HANDS: white text, as Doubles
TEXTCOL[0] = WHITE
label("HANDS", [958, 4, 60, 14])
dial("hands", "Hands", "HANDS", [958, 18, 44, 48], 0, 100, 50, unit=5)
dials = {}
for i, (nm, long) in enumerate((("timing", "Timing"), ("weight", "Weight"))):
    dials[nm] = dial(nm, long, long.upper(), [1004 + i * 44, 18, 41, 48], 0, 100, 50, unit=5)
for i, (nm, long) in enumerate((("slop", "Slop"), ("risk", "Risk"))):
    dials[nm] = dial(nm, long, long.upper(), [958 + i * 44, 92, 41, 48], 0, 100, 50, unit=5)
for nm in ("timing", "weight", "slop", "risk"):
    line(route, ROUTE_OUT[nm], dials[nm], 0)
label("HANDS MOVES ALL FOUR. 0 IS A MACHINE, 50 A TIGHT DJ.", [1046, 92, 46, 70], small=True, color=WHITE)

# OUT
label("OUT", [1100, 4, 40, 14])
dial("level", "Level", "LEVEL", [1100, 18, 41, 48], -70, 6, 0, unit=4)
TEXTCOL[0] = GREY
label("v" + VERSION, [1100, 150, 44, 14], small=True)

# ROUTINE window: 16 one-beat slots, each a cell and a cue point
sub_boxes, sub_lines = [], []
sub_boxes.append({"box": {"id": "r-in", "maxclass": "inlet", "numinlets": 0, "numoutlets": 1, "outlettype": [""], "patching_rect": [10.0, 10.0, 30.0, 30.0]}})
sub_boxes.append({"box": {"id": "r-out", "maxclass": "outlet", "numinlets": 1, "numoutlets": 0, "patching_rect": [10.0, 380.0, 30.0, 30.0]}})
sub_boxes.append({"box": {"id": "r-title", "maxclass": "comment", "numinlets": 1, "numoutlets": 0,
                          "patching_rect": [60.0, 10.0, 640.0, 34.0], "linecount": 2,
                          "text": "DJ CLAUDE SKRATCH ROUTINE: 16 slots of one beat, four bars, looping from the bar line where MODE is set to ROUTINE. "
                                  "A two-beat cell takes the next slot too. CUE: SAME keeps the cue point, 1 to 8 moves to it first."}})
for i in range(16):
    r, c = divmod(i, 8)
    x, yy = 60 + c * 82, 60 + r * 150
    mid, cid, pid, qid, lid = "r-m%d" % i, "r-c%d" % i, "r-p%d" % i, "r-q%d" % i, "r-l%d" % i
    long = "Slot %d" % (i + 1)
    sub_boxes.append({"box": {"id": lid, "maxclass": "comment", "numinlets": 1, "numoutlets": 0,
                              "patching_rect": [float(x), float(yy), 78.0, 18.0], "text": "BEAT %d" % (i + 1)}})
    sub_boxes.append({"box": {"id": mid, "maxclass": "live.menu", "numinlets": 1, "numoutlets": 3,
                              "outlettype": ["", "", "float"], "patching_rect": [float(x), float(yy + 20), 78.0, 15.0],
                              "parameter_enable": 1, "varname": long,
                              "saved_attribute_attributes": valueof(long, "S%d" % (i + 1), 2, parameter_enum=[c_.upper() for c_ in CELLS],
                                                                    parameter_initial=[CELLS.index(ROUTINE_DEFAULT[i])], parameter_initial_enable=1,
                                                                    parameter_mmax=len(CELLS) - 1)}})
    sub_boxes.append({"box": {"id": cid, "maxclass": "live.menu", "numinlets": 1, "numoutlets": 3,
                              "outlettype": ["", "", "float"], "patching_rect": [float(x), float(yy + 40), 78.0, 15.0],
                              "parameter_enable": 1, "varname": long + " Cue",
                              "saved_attribute_attributes": valueof(long + " Cue", "C%d" % (i + 1), 2,
                                                                    parameter_enum=["CUE SAME"] + ["CUE %d" % k for k in range(1, 9)],
                                                                    parameter_initial=[0], parameter_initial_enable=1, parameter_mmax=8)}})
    sub_boxes.append({"box": {"id": pid, "maxclass": "newobj", "numinlets": 2, "numoutlets": 1, "outlettype": [""],
                              "patching_rect": [float(x), float(yy + 70), 78.0, 22.0], "text": "prepend slot%d" % (i + 1)}})
    sub_boxes.append({"box": {"id": qid, "maxclass": "newobj", "numinlets": 2, "numoutlets": 1, "outlettype": [""],
                              "patching_rect": [float(x), float(yy + 100), 78.0, 22.0], "text": "prepend scue%d" % (i + 1)}})
    sub_lines.append({"patchline": {"source": [mid, 0], "destination": [pid, 0]}})
    sub_lines.append({"patchline": {"source": [cid, 0], "destination": [qid, 0]}})
    sub_lines.append({"patchline": {"source": [pid, 0], "destination": ["r-out", 0]}})
    sub_lines.append({"patchline": {"source": [qid, 0], "destination": ["r-out", 0]}})
    params[mid] = [long, "S%d" % (i + 1), 0]
    params[cid] = [long + " Cue", "C%d" % (i + 1), 0]
sub = {"fileversion": 1, "appversion": APP, "classnamespace": "box", "rect": [200.0, 200.0, 760.0, 420.0],
       "openinpresentation": 0, "default_fontsize": 10.0, "default_fontname": "Arial Bold",
       "gridsize": [8.0, 8.0], "boxes": sub_boxes, "lines": sub_lines,
       "description": "", "digest": "", "tags": "", "style": "", "subpatcher_template": ""}
rout = obj("p routine", [px[0], 580, 80, 22], 1, 1, patcher=sub)
line(pc, 0, rout, 0)
line(rout, 0, js, 0)

# ----------------------------------------------------------------- patcher
patcher = {
    "fileversion": 1, "appversion": APP, "classnamespace": "box",
    "rect": [40.0, 80.0, 1400.0, 760.0], "openrect": [0.0, 0.0, float(DEVICE_W), float(DEVICE_H)],
    "bglocked": 0, "openinpresentation": 1, "default_fontsize": 10.0, "default_fontface": 0,
    "default_fontname": "Arial Bold", "gridonopen": 1, "gridsize": [8.0, 8.0], "gridsnaponopen": 1,
    "objectsnaponopen": 1, "statusbarvisible": 2, "toolbarvisible": 1, "boxanimatetime": 500,
    "enablehscroll": 1, "enablevscroll": 1, "devicewidth": float(DEVICE_W),
    "description": "DJ Claude Skratch: a modelled record hand and fader hand scratch one sound in time with Live.",
    "digest": "", "tags": "", "style": "", "subpatcher_template": "",
    "boxes": boxes, "lines": lines,
    "parameters": dict(params, **{"inherited_shortname": 1}),
    "dependency_cache": [],
    "latency": 0, "is_mpe": 0, "minimum_live_version": "", "minimum_max_version": "", "platform_compatibility": 0,
    "autosave": 0,
}
AMXDTYPE = struct.unpack('>I', b'iiii')[0]          # instrument
device = {"patcher": dict(patcher, project={
    "version": 1, "creationdate": 3590052786, "modificationdate": 3590052786,
    "viewrect": [0.0, 0.0, 300.0, 500.0], "autoorganize": 1, "hideprojectwindow": 1, "showdependencies": 1,
    "autolocalize": 0, "contents": {"patchers": {}}, "layout": {}, "searchpath": {}, "detailsvisible": 0,
    "amxdtype": AMXDTYPE, "readonly": 0, "devpathtype": 0, "devpath": ".", "sortmode": 0, "viewmode": 0,
    "includepackages": 0})}


# ----------------------------------------------------------------- lint
def lint(p, where='top'):
    ids = {}
    errs = []
    for b in p["boxes"]:
        bb = b["box"]
        if bb["id"] in ids:
            errs.append('%s: duplicate id %s' % (where, bb["id"]))
        ids[bb["id"]] = bb
        if "patcher" in bb:
            errs += lint(bb["patcher"], where + '/' + bb.get("text", bb["id"]))
    for l in p["lines"]:
        s, si = l["patchline"]["source"]
        d, di = l["patchline"]["destination"]
        if s not in ids or d not in ids:
            errs.append('%s: line to missing box %s -> %s' % (where, s, d))
            continue
        if si >= ids[s]["numoutlets"]:
            errs.append('%s: %s has no outlet %d' % (where, ids[s].get("text", ids[s]["maxclass"]), si))
        if di >= ids[d]["numinlets"]:
            errs.append('%s: %s has no inlet %d' % (where, ids[d].get("text", ids[d]["maxclass"]), di))
    return errs


errs = lint(patcher)
longs = {}
for k, v in params.items():
    if v[0] in longs:
        errs.append('duplicate parameter name %s' % v[0])
    longs[v[0]] = k
for b in boxes:
    r = b["box"].get("presentation_rect")
    if r and (r[1] + r[3] > DEVICE_H or r[0] + r[2] > DEVICE_W):
        errs.append('outside the device view: %s %s' % (b["box"].get("text", b["box"]["maxclass"]), r))
# no function definitions in the codebox (Doubles fix round 1), and every Param the planner sends exists
import re
if any(m not in ('if', 'for', 'while') for m in re.findall(r'^\s*(\w+)\s*\([^)]*\)\s*\{', code, re.M)):
    errs.append('codebox has a function definition')
gparams = set(re.findall(r'^Param (\w+)\(', code, re.M))
sent = set(re.findall(r"HOST\.gen\('(\w+)'", open(os.path.join(ROOT, 'src', 'skratch.js')).read()))
sent |= {'lb0a', 'lb1a', 'lb2a', 'la1a', 'la2a', 'lb0b', 'lb1b', 'lb2b', 'la1b', 'la2b', 'lb0c', 'lb1c', 'lb2c', 'la1c', 'la2c'}
sent = {s for s in sent if not s.endswith(("0'", "1'"))}
missing = sorted(n for n in sent if n not in gparams and not n.startswith(('lb', 'la')))
if missing:
    errs.append('planner sends Params gen~ does not have: %s' % missing)
# the routing names the planner uses for the panel
for k in re.findall(r"HOST\.ui\('(\w+)'", open(os.path.join(ROOT, 'src', 'skratch.js')).read()):
    if k not in UIROUTE:
        errs.append('planner sends %s to the panel but nothing routes it' % k)
if errs:
    print('\n'.join(errs))
    sys.exit(1)

# ----------------------------------------------------------------- write
body = json.dumps(device, indent='\t').encode('utf8') + b'\n\x00'
amxd = b'ampf' + struct.pack('<I', 4) + b'iiii' + b'meta' + struct.pack('<I', 4) + struct.pack('<I', 0) \
    + b'ptch' + struct.pack('<I', len(body)) + body
open(os.path.join(ROOT, NAME + '.amxd'), 'wb').write(amxd)
open(os.path.join(ROOT, NAME + '.maxpat'), 'w').write(json.dumps(device, indent='\t') + '\n')
for f in ('skratch.js', 'skratch_strip.js'):
    open(os.path.join(ROOT, f), 'w').write(script_text(f))
print('%s.amxd: %d bytes, %d boxes, %d lines, %d parameters' % (NAME, len(amxd), len(boxes), len(lines), len(params)))
