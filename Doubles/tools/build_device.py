"""Builds Doubles.amxd (and Doubles.maxpat) from src/. The device is generated, never hand-edited.

    python3 tools/build_device.py

Writes:  Doubles.amxd, Doubles.maxpat, doubles.js, doubles_strip.js (copied next to the device)
"""
import json, os, struct, shutil, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, '..')
sys.path.insert(0, HERE)
import genexpr

DEVICE_W = 1112
APP = {"major": 9, "minor": 0, "revision": 5, "architecture": "x64", "modernui": 1}
BUFS = '---dbl_rec ---dbl_q ---dbl_stat ---dbl_save ---dbl_load'

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


def embedded(name):
    """The script is stored inside the device, so Live never has to find a .js file."""
    code = open(os.path.join(ROOT, 'src', name)).read()
    return {"text": code, "filename": "none", "flags": 0, "embed": 1, "autowatch": 1}


def line(src, so, dst, di):
    lines.append({"patchline": {"source": [src, so], "destination": [dst, di]}})


def valueof(long, short, ptype, **kw):
    v = {"parameter_longname": long, "parameter_shortname": short, "parameter_type": ptype}
    v.update(kw)
    return {"valueof": v}


def param(bid, long, short):
    params[bid] = [long, short, 0]


# ----------------------------------------------------------------- core objects
pin = obj("plugin~", [20, 20, 60, 22], 2, 2, ["signal", "signal"])
pph = obj("plugphasor~", [100, 20, 80, 22], 1, 1, ["signal"])
code = genexpr.expand()
open(os.path.join(ROOT, 'build', 'doubles_core.genexpr'), 'w').write(code)
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
        {"box": {"id": "obj-2", "maxclass": "newobj", "numinlets": 0, "numoutlets": 1, "outlettype": [""], "patching_rect": [120.0, 20.0, 30.0, 22.0], "text": "in 2"}},
        {"box": {"id": "obj-3", "maxclass": "newobj", "numinlets": 0, "numoutlets": 1, "outlettype": [""], "patching_rect": [220.0, 20.0, 30.0, 22.0], "text": "in 3"}},
        {"box": {"id": "obj-4", "maxclass": "codebox", "numinlets": 3, "numoutlets": 2, "outlettype": ["", ""],
                 "patching_rect": [20.0, 60.0, 840.0, 560.0], "fontface": 0, "fontname": "Menlo", "fontsize": 11.0, "code": code}},
        {"box": {"id": "obj-5", "maxclass": "newobj", "numinlets": 1, "numoutlets": 0, "patching_rect": [20.0, 640.0, 40.0, 22.0], "text": "out 1"}},
        {"box": {"id": "obj-6", "maxclass": "newobj", "numinlets": 1, "numoutlets": 0, "patching_rect": [120.0, 640.0, 40.0, 22.0], "text": "out 2"}},
    ],
    "lines": [
        {"patchline": {"source": ["obj-1", 0], "destination": ["obj-4", 0]}},
        {"patchline": {"source": ["obj-2", 0], "destination": ["obj-4", 1]}},
        {"patchline": {"source": ["obj-3", 0], "destination": ["obj-4", 2]}},
        {"patchline": {"source": ["obj-4", 0], "destination": ["obj-5", 0]}},
        {"patchline": {"source": ["obj-4", 1], "destination": ["obj-6", 0]}},
    ],
}
gen = obj("gen~", [20, 120, 200, 22], 3, 2, ["signal", "signal"], patcher=gen_patcher)
pout = obj("plugout~", [20, 200, 60, 22], 2, 2, ["signal", "signal"])
line(pin, 0, gen, 0); line(pin, 1, gen, 1); line(pph, 0, gen, 2)
line(gen, 0, pout, 0); line(gen, 1, pout, 1)

# buffers, named per device instance with ---
y = 260
for name, args in (("dbl_rec", "100000 2"), ("dbl_q", "100"), ("dbl_stat", "10"), ("dbl_save", "10 2"), ("dbl_load", "10 2")):
    obj("buffer~ ---%s %s" % (name, args), [20, y, 180, 22], 1, 2, ["float", "bang"])
    y += 28

# bind gen~'s Buffers to this device's buffers
lb = obj("loadbang", [260, 20, 60, 22], 1, 1, ["bang"])
bind = box("message", [260, 50, 330, 22], text="rec ---dbl_rec, q ---dbl_q, st ---dbl_stat", nin=2, nout=1, otype=[""])
line(lb, 0, bind, 0); line(bind, 0, gen, 0)

# the logic
js = obj("v8", [400, 120, 300, 22], 1, 2, ["", ""], textfile=embedded("doubles.js"),
         parameter_enable=1,
         saved_attribute_attributes=valueof("Doubles State", "State", 3, parameter_invisible=1))
param(js, "Doubles State", "State")
line(js, 0, gen, 0)
ltd = obj("live.thisdevice", [400, 20, 90, 22], 1, 3, ["bang", "int", "int"])
initm = box("message", [400, 50, 330, 22], text="names " + BUFS + ", init", nin=2, nout=1, otype=[""])
line(ltd, 0, initm, 0); line(initm, 0, js, 0)

# js outlet 1 -> panel
UIROUTE = ["status", "record", "strip", "btn", "timing", "weight", "slop", "risk"]
route = obj("route " + " ".join(UIROUTE), [400, 170, 420, 22], 1, len(UIROUTE) + 1)
line(js, 1, route, 0)
ROUTE_OUT = {k: i for i, k in enumerate(UIROUTE)}
btnroute = obj("route loop double chase shuffle fill phase", [560, 200, 300, 22], 1, 7)
line(route, ROUTE_OUT["btn"], btnroute, 0)

# ----------------------------------------------------------------- panel
px = [700]   # patching x for panel objects
FONT = {"fontname": "Ableton Sans Medium", "fontsize": 9.0}


def wire(bid, name, outlet=0):
    pre = obj("prepend " + name, [px[0], 600, 90, 22], 2, 1)
    line(bid, outlet, pre, 0)
    line(pre, 0, js, 0)
    px[0] += 100
    return pre


GREY, WHITE = [0.78, 0.78, 0.8, 1.0], [1.0, 1.0, 1.0, 1.0]
TEXTCOL = [GREY]   # label and dial-name colour for the section being built


def label(text, pres, w=None):
    return box("comment", [px[0], 560, 80, 18], text=text, nin=1, nout=0, pres=pres,
               textcolor=TEXTCOL[0], **FONT)


def dial(name, long, short, pres, lo, hi, init, unit=1, ptype=0, **kw):
    v = valueof(long, short, ptype, parameter_mmin=float(lo), parameter_mmax=float(hi),
                parameter_initial=[init], parameter_initial_enable=1, parameter_unitstyle=unit)
    b = box("live.dial", [px[0], 500, 41, 48], nin=1, nout=2, otype=["", "float"], pres=pres,
            parameter_enable=1, saved_attribute_attributes=v, varname=long,
            **dict({"textcolor": TEXTCOL[0]} if TEXTCOL[0] is WHITE else {}, **kw))
    param(b, long, short)
    wire(b, name)
    return b


def menu(name, long, short, pres, items, init):
    v = valueof(long, short, 2, parameter_enum=items, parameter_initial=[init], parameter_initial_enable=1,
                parameter_mmax=len(items) - 1)
    b = box("live.menu", [px[0], 500, 70, 15], nin=1, nout=3, otype=["", "", "float"], pres=pres,
            parameter_enable=1, saved_attribute_attributes=v, varname=long, **FONT)
    param(b, long, short)
    wire(b, name)
    return b


def toggle(name, long, text, texton, pres, init=0, momentary=False):
    v = valueof(long, text, 2, parameter_enum=["off", "on"], parameter_initial=[init],
                parameter_initial_enable=1, parameter_mmax=1)
    b = box("live.text", [px[0], 500, 60, 20], nin=1, nout=2, otype=["", ""], pres=pres,
            parameter_enable=1, saved_attribute_attributes=v, varname=long, text=text, texton=texton,
            mode=0 if momentary else 1, **FONT)
    param(b, long, text)
    wire(b, name)
    return b


def button(name, long, short, pres):
    v = valueof(long, short, 2, parameter_enum=["off", "on"], parameter_mmax=1)
    b = box("live.button", [px[0], 500, 15, 15], nin=1, nout=1, otype=["bang"], pres=pres,
            parameter_enable=1, saved_attribute_attributes=v, varname=long)
    param(b, long, short)
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
# section backgrounds
for x, w in ((2, 92), (96, 302), (400, 254), (656, 102), (760, 180), (942, 114), (1058, 52)):
    panel_bg([x, 2, w, 165], SEC)

# RECORD
label("RECORD", [6, 4, 84, 14])
rec = toggle("record", "Record", "RECORD", "ARMED", [6, 20, 84, 20])
line(route, ROUTE_OUT["record"], rec, 0)
menu("length", "Length", "Length", [6, 44, 84, 15], ["1 BAR", "2 BARS", "4 BARS", "8 BARS", "16 BARS", "FREE"], 3)
toggle("clear", "Clear", "CLEAR", "CLEAR", [6, 63, 84, 15], momentary=True)
stat = box("comment", [px[0], 560, 84, 30], text="EMPTY", nin=1, nout=0, pres=[6, 82, 86, 28],
           textcolor=[1.0, 0.75, 0.3, 1.0], linecount=2, **FONT)
line(route, ROUTE_OUT["status"], stat, 0)
drop = box("live.drop", [px[0], 500, 84, 44], nin=1, nout=1, otype=[""], pres=[6, 114, 84, 48])
wire(drop, "drop")

# STRIP
strip = box("v8ui", [px[0], 420, 300, 120], nin=1, nout=1, otype=[""], pres=[100, 6, 294, 126],
            filename="none", embed=1, textfile=embedded("doubles_strip.js"), parameter_enable=0)
line(route, ROUTE_OUT["strip"], strip, 0)
wire(strip, "strip")
label("FILL KICK", [100, 136, 60, 14])
numbox("fillk", "Fill Kick", "Fill K", [100, 150, 44, 15], 0, 256, 0)
label("FILL SNARE", [160, 136, 64, 14])
numbox("fills", "Fill Snare", "Fill S", [160, 150, 44, 15], 0, 256, 0)
label("0 = FIND FROM EACH BAR.  CLICK A TICK TO REMOVE IT, SHIFT-CLICK TO ADD.", [226, 134, 170, 30])

# MOVES
label("MOVES", [404, 4, 80, 14])
held = {}
for i, (nm, lab) in enumerate((("loop", "LOOP"), ("double", "DOUBLE"), ("chase", "CHASE"),
                                ("shuffle", "SHUFFLE"), ("fill", "FILL"), ("phase", "PHASE"))):
    held[nm] = toggle(nm, lab.title(), lab, lab, [404 + i * 41, 20, 39, 20])
    line(btnroute, i, held[nm], 0)
for i, (nm, lab) in enumerate((("triple", "TRIPLE"), ("pause", "PAUSE"), ("spin", "SPIN"), ("drag", "DRAG"),
                                ("baby", "BABY"))):
    toggle(nm, lab.title(), lab, lab, [404 + i * 41, 44, 39, 20], momentary=True)
label("QUANT", [540, 106, 46, 14])
menu("quant", "Quantise", "Quant", [540, 120, 46, 15], ["1/16", "1/8", "1/4", "BAR"], 2)
for i, (nm, long, items, init) in enumerate((
        ("looplen", "Loop Length", ["1/16", "1/8", "1/4", "1/2", "BAR"], 2),
        ("doubledist", "Double Distance", ["1/16", "1/8", "1/4"], 1),
        ("chaseunit", "Chase Unit", ["1/8", "1/4"], 1),
        ("pauselen", "Pause Length", ["1/16", "1/8", "3/16"], 1),
        ("spinback", "Spin Back", ["1 BEAT", "2 BEATS", "1 BAR", "2 BARS"], 2))):
    x = 404 + i * 50
    label(["LOOP", "DOUBLE", "CHASE", "PAUSE", "SPIN"][i], [x, 70, 48, 14])
    menu(nm, long, long.split()[0], [x, 84, 46, 15], items, init)
label("SHUFFLE", [404, 106, 48, 14])
numbox("shuffle_amt", "Shuffle Amount", "Shuffle", [404, 120, 46, 15], 0, 60, 50, unit=5)
label("FILL PATTERN", [458, 106, 70, 14])
numbox("fillpat", "Fill Pattern", "Fill Pat", [458, 120, 46, 15], 1, 8, 1)
label("ON/OFF: LOOP TO PHASE.  ONE-SHOT: TRIPLE TO BABY.", [404, 140, 246, 24])

# PADS
label("PADS  A / B", [660, 4, 90, 14])
for i in range(8):
    r, c = divmod(i, 4)
    button("pad%d" % (i + 1), "Pad %d" % (i + 1), "Pad %d" % (i + 1), [662 + c * 24, 22 + r * 30, 20, 20])
label("A: KICK, SNARE, KICK 2, SNARE 2.  B: THE SAME ON THE OTHER COPY.", [660, 84, 96, 60])

# HANDS, PLAY and OUT: white text (Hardy, 2026-11-22)
TEXTCOL[0] = WHITE
label("HANDS", [764, 4, 60, 14])
hands = dial("hands", "Hands", "HANDS", [764, 18, 44, 48], 0, 100, 50, unit=5)
dials = {}
for i, (nm, long) in enumerate((("timing", "Timing"), ("weight", "Weight"))):
    dials[nm] = dial(nm, long, long.upper(), [810 + i * 42, 18, 41, 48], 0, 100, 50, unit=5)
for i, (nm, long) in enumerate((("slop", "Slop"), ("risk", "Risk"), ("copy", "Copy"))):
    dials[nm] = dial(nm, long, long.upper(), [764 + i * 44, 92, 41, 48], 0, 100, 50, unit=5)
for nm in ("timing", "weight", "slop", "risk"):
    line(route, ROUTE_OUT[nm], dials[nm], 0)
# baby scratches (1.1): not moved by the HANDS macro
dial("scratch", "Scratch", "SCRATCH", [894, 18, 41, 48], 0, 100, 30, unit=5)
label("OVER", [894, 92, 44, 14])
menu("scrover", "Scratch Over", "Over", [894, 106, 44, 15], ["GROOVE", "MIX", "SILENCE"], 1)
label("HANDS MOVES ALL FOUR.  COPY: WEAR ON DECK B.  SCRATCH: BABY SCRATCHES.", [764, 142, 174, 24])

# PLAY
label("PLAY", [946, 4, 60, 14])
v = valueof("Mode", "Mode", 2, parameter_enum=["HANDS-ON", "ROUTINE", "INSTINCT"], parameter_initial=[0],
            parameter_initial_enable=1, parameter_mmax=2)
mode = box("live.tab", [px[0], 500, 70, 48], nin=1, nout=3, otype=["", "", "float"], pres=[946, 18, 64, 48],
           parameter_enable=1, saved_attribute_attributes=v, varname="Mode", num_lines_patching=3,
           num_lines_presentation=3, **FONT)
param(mode, "Mode", "Mode")
wire(mode, "mode")
dial("density", "Density", "DENSITY", [1012, 18, 41, 48], 0, 100, 60, unit=5)
dial("funk", "Funk", "FUNK", [946, 72, 41, 48], 0, 100, 50, unit=5)
label("ARC", [992, 76, 40, 14])
menu("arc", "Arc", "Arc", [992, 90, 58, 15], ["8 BARS", "16 BARS", "32 BARS"], 1)
openr = box("live.text", [px[0], 500, 60, 20], nin=1, nout=2, otype=["", ""], pres=[946, 126, 106, 18],
            text="ROUTINE...", texton="ROUTINE...", mode=0, parameter_enable=0, **FONT)
px[0] += 80
pc = obj("pcontrol", [px[0], 540, 60, 22], 1, 1)
openmsg = box("message", [px[0], 510, 40, 22], text="open", nin=2, nout=1, otype=[""])
line(openr, 0, openmsg, 0); line(openmsg, 0, pc, 0)

# OUT
label("OUT", [1062, 4, 40, 14])
dial("straight", "Straight Level", "STRAIGHT", [1062, 18, 41, 48], 0, 100, 100, unit=5)
dial("level", "Level", "LEVEL", [1062, 72, 41, 48], -70, 6, 0, unit=4)
toggle("brake", "Brake", "BRAKE", "BRAKE", [1062, 126, 44, 18], init=1)

TEXTCOL[0] = GREY

# ROUTINE window: a subpatcher with the 16 slots
sub_boxes, sub_lines = [], []
SLOTS = ["STRAIGHT", "LOOP", "DOUBLE", "TRIPLE", "CHASE", "SHUFFLE", "PAUSE", "FILL 1", "FILL 2", "FILL 3",
         "FILL 4", "FILL 5", "FILL 6", "FILL 7", "FILL 8", "SPIN", "PHASE"]
DEFAULT = [0, 1, 0, 3, 2, 0, 4, 7, 0, 1, 5, 8, 2, 0, 6, 15]
sub_boxes.append({"box": {"id": "r-in", "maxclass": "inlet", "numinlets": 0, "numoutlets": 1, "outlettype": [""], "patching_rect": [10.0, 10.0, 30.0, 30.0]}})
sub_boxes.append({"box": {"id": "r-out", "maxclass": "outlet", "numinlets": 1, "numoutlets": 0, "patching_rect": [10.0, 330.0, 30.0, 30.0]}})
sub_boxes.append({"box": {"id": "r-title", "maxclass": "comment", "numinlets": 1, "numoutlets": 0,
                          "patching_rect": [60.0, 10.0, 520.0, 20.0],
                          "text": "DOUBLES ROUTINE: one move per slot. Set MODE to ROUTINE to play it. RESET starts it at the next bar."}})
for i in range(16):
    r, c = divmod(i, 8)
    x, yy = 60 + c * 75, 50 + r * 90
    mid, pid, lid = "r-m%d" % i, "r-p%d" % i, "r-l%d" % i
    long = "Slot %d" % (i + 1)
    sub_boxes.append({"box": {"id": lid, "maxclass": "comment", "numinlets": 1, "numoutlets": 0,
                              "patching_rect": [float(x), float(yy), 70.0, 18.0], "text": "SLOT %d" % (i + 1)}})
    sub_boxes.append({"box": {"id": mid, "maxclass": "live.menu", "numinlets": 1, "numoutlets": 3,
                              "outlettype": ["", "", "float"], "patching_rect": [float(x), float(yy + 20), 70.0, 15.0],
                              "parameter_enable": 1, "varname": long,
                              "saved_attribute_attributes": valueof(long, "S%d" % (i + 1), 2, parameter_enum=SLOTS,
                                                                    parameter_initial=[DEFAULT[i]], parameter_initial_enable=1,
                                                                    parameter_mmax=len(SLOTS) - 1)}})
    sub_boxes.append({"box": {"id": pid, "maxclass": "newobj", "numinlets": 2, "numoutlets": 1, "outlettype": [""],
                              "patching_rect": [float(x), float(yy + 45), 70.0, 22.0], "text": "prepend slot%d" % (i + 1)}})
    sub_lines.append({"patchline": {"source": [mid, 0], "destination": [pid, 0]}})
    sub_lines.append({"patchline": {"source": [pid, 0], "destination": ["r-out", 0]}})
    params[mid] = [long, "S%d" % (i + 1), 0]
for k, (nm, long, items, init, x) in enumerate((("slotlen", "Slot Length", ["BAR", "HALF BAR"], 0, 60),)):
    sub_boxes.append({"box": {"id": "r-sl", "maxclass": "live.menu", "numinlets": 1, "numoutlets": 3, "outlettype": ["", "", "float"],
                              "patching_rect": [60.0, 250.0, 80.0, 15.0], "parameter_enable": 1, "varname": long,
                              "saved_attribute_attributes": valueof(long, "Slot Len", 2, parameter_enum=items, parameter_initial=[init],
                                                                    parameter_initial_enable=1, parameter_mmax=1)}})
    sub_boxes.append({"box": {"id": "r-slp", "maxclass": "newobj", "numinlets": 2, "numoutlets": 1, "outlettype": [""],
                              "patching_rect": [60.0, 280.0, 90.0, 22.0], "text": "prepend slotlen"}})
    sub_lines.append({"patchline": {"source": ["r-sl", 0], "destination": ["r-slp", 0]}})
    sub_lines.append({"patchline": {"source": ["r-slp", 0], "destination": ["r-out", 0]}})
    params["r-sl"] = [long, "Slot Len", 0]
sub_boxes.append({"box": {"id": "r-rs", "maxclass": "live.text", "numinlets": 1, "numoutlets": 2, "outlettype": ["", ""],
                          "patching_rect": [160.0, 250.0, 60.0, 18.0], "parameter_enable": 1, "varname": "Routine Reset",
                          "text": "RESET", "texton": "RESET", "mode": 0,
                          "saved_attribute_attributes": valueof("Routine Reset", "Reset", 2, parameter_enum=["off", "on"], parameter_mmax=1)}})
sub_boxes.append({"box": {"id": "r-rsp", "maxclass": "newobj", "numinlets": 2, "numoutlets": 1, "outlettype": [""],
                          "patching_rect": [160.0, 280.0, 80.0, 22.0], "text": "prepend reset"}})
sub_lines.append({"patchline": {"source": ["r-rs", 0], "destination": ["r-rsp", 0]}})
sub_lines.append({"patchline": {"source": ["r-rsp", 0], "destination": ["r-out", 0]}})
params["r-rs"] = ["Routine Reset", "Reset", 0]
sub = {"fileversion": 1, "appversion": APP, "classnamespace": "box", "rect": [200.0, 200.0, 680.0, 380.0],
       "openinpresentation": 0, "default_fontsize": 10.0, "default_fontname": "Arial Bold",
       "gridsize": [8.0, 8.0], "boxes": sub_boxes, "lines": sub_lines,
       "description": "", "digest": "", "tags": "", "style": "", "subpatcher_template": ""}
rout = obj("p routine", [px[0], 580, 80, 22], 1, 1, patcher=sub)
line(pc, 0, rout, 0)
line(rout, 0, js, 0)

# the shuffle numbox reports in %, js expects 'shuffle' as the amount; rename its prepend
for bx in boxes:
    if bx["box"].get("text") == "prepend shuffle_amt":
        bx["box"]["text"] = "prepend shuffleamt"

# ----------------------------------------------------------------- patcher
patcher = {
    "fileversion": 1, "appversion": APP, "classnamespace": "box",
    "rect": [40.0, 80.0, 1400.0, 760.0], "openrect": [0.0, 0.0, float(DEVICE_W), 169.0],
    "bglocked": 0, "openinpresentation": 1, "default_fontsize": 10.0, "default_fontface": 0,
    "default_fontname": "Arial Bold", "gridonopen": 1, "gridsize": [8.0, 8.0], "gridsnaponopen": 1,
    "objectsnaponopen": 1, "statusbarvisible": 2, "toolbarvisible": 1, "boxanimatetime": 500,
    "enablehscroll": 1, "enablevscroll": 1, "devicewidth": float(DEVICE_W),
    "description": "Doubles: beat juggling on two modelled copies of a short recorded piece.",
    "digest": "", "tags": "", "style": "", "subpatcher_template": "",
    "boxes": boxes, "lines": lines,
    "parameters": dict(params, **{"inherited_shortname": 1}),
    "dependency_cache": [],
    "latency": 0, "is_mpe": 0, "minimum_live_version": "", "minimum_max_version": "", "platform_compatibility": 0,
    "autosave": 0,
}
device = {"patcher": dict(patcher, project={
    "version": 1, "creationdate": 3590052786, "modificationdate": 3590052786,
    "viewrect": [0.0, 0.0, 300.0, 500.0], "autoorganize": 1, "hideprojectwindow": 1, "showdependencies": 1,
    "autolocalize": 0, "contents": {"patchers": {}}, "layout": {}, "searchpath": {}, "detailsvisible": 0,
    "amxdtype": 1633771873, "readonly": 0, "devpathtype": 0, "devpath": ".", "sortmode": 0, "viewmode": 0,
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
    if r and (r[1] + r[3] > 169 or r[0] + r[2] > DEVICE_W):
        errs.append('outside the device view: %s %s' % (b["box"].get("text", b["box"]["maxclass"]), r))
if errs:
    print('\n'.join(errs))
    sys.exit(1)

# ----------------------------------------------------------------- write
body = json.dumps(device, indent='\t').encode('utf8') + b'\n\x00'
amxd = b'ampf' + struct.pack('<I', 4) + b'aaaa' + b'meta' + struct.pack('<I', 4) + struct.pack('<I', 0) \
    + b'ptch' + struct.pack('<I', len(body)) + body
open(os.path.join(ROOT, 'Doubles.amxd'), 'wb').write(amxd)
open(os.path.join(ROOT, 'Doubles.maxpat'), 'w').write(json.dumps(device, indent='\t') + '\n')
for f in ('doubles.js', 'doubles_strip.js'):
    shutil.copy(os.path.join(ROOT, 'src', f), os.path.join(ROOT, f))
print('Doubles.amxd: %d bytes, %d boxes, %d lines, %d parameters' % (len(amxd), len(boxes), len(lines), len(params)))
