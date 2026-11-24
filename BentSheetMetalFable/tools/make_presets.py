#!/usr/bin/env python3
# Writes presets/BentSheetMetal/NN_Name.vcvm and test/presets.txt (the same settings in knob units, for the
# offline preset check). The second column of presets.txt says how the test patches it (LFOs, triggers,
# audio); "-" means nothing patched. The preset notes in the manual say the same.
import os, json
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
# Param order = the module's ParamIds enum.
IDS = ["PITCH", "TAPER", "GRIP", "DECAY", "DIP", "DEPTH", "STRETCH", "PICKUP", "OUT", "AGE", "LEVEL",
       "BEND", "SHAPE", "SPOT", "HOLD", "SNAP", "CRACK", "BOW", "SPEED", "BOWPOS", "HARD", "ROLL", "INLEVEL"]
DEF = dict(PITCH=0.0, TAPER=0.7, GRIP=0.7, DECAY=1.0, DIP=0.0, DEPTH=0.5, STRETCH=1.0, PICKUP=0.2, OUT=0.0, AGE=0.3, LEVEL=0.7,
           BEND=0.45, SHAPE=1.0, SPOT=0.0, HOLD=0.9, SNAP=0.0, CRACK=0.35, BOW=0.27, SPEED=0.44, BOWPOS=0.0, HARD=0.5, ROLL=0.0, INLEVEL=0.5)
# test columns, in this order (see test/render.cpp loadPresets)
TCOL = ["PITCH", "TAPER", "GRIP", "DECAY", "BEND", "SHAPE", "SPOT", "HOLD", "SNAP", "CRACK", "DIP", "DEPTH", "BOW", "SPEED", "BOWPOS",
        "HARD", "ROLL", "INLEVEL", "STRETCH", "PICKUP", "OUT", "AGE", "LEVEL"]
STRUCK = dict(BOW=0.0, SHAPE=0.0, TAPER=0.0)
P = [
    ("Init", "-", {}),
    ("Alive", "lfoBend+lfoBow", dict(BOW=0.55, SPEED=0.77, GRIP=0.6, AGE=0.3, STRETCH=1.3)),
    ("Dragged Under", "lfoDip", dict(BOW=0.65, SPEED=0.9, DIP=0.35, DEPTH=0.55)),
    ("Break-up", "-", dict(TAPER=0.5, GRIP=0.2, SHAPE=0.5, HOLD=0.3, SNAP=0.45, BOW=0.9, SPEED=0.85, AGE=0.6, STRETCH=1.8)),
    ("Water Gong", "strike+lfoDip", dict(STRUCK, BEND=0.2, GRIP=0.0, DECAY=2.0)),
    ("Mercury", "strike", dict(STRUCK, BEND=0.2, GRIP=0.0, DEPTH=1.0, DIP=0.5, DECAY=2.0)),
    ("Lighter Than Air", "lfoDip", dict(BOW=0.35, SPEED=0.58, DEPTH=-1.0, DIP=0.3)),
    ("Singing Saw", "seqSpot", dict(BOW=0.27, SPEED=0.44, GRIP=1.0, TAPER=1.0)),
    ("Saw Fifths", "-", dict(BOWPOS=0.25, BOW=0.4)),
    ("Overbowed", "-", dict(BOW=0.8, SPEED=0.9, STRETCH=2.0)),
    ("Scream", "-", dict(BOW=1.0, SPEED=0.8, PICKUP=1.0, OUT=0.8, STRETCH=3.0, GRIP=0.3, SHAPE=0.6, AGE=0.6)),
    ("Flexatone", "lfoBend", dict(STRUCK, ROLL=0.65, GRIP=0.1, BEND=0.35, HARD=0.6)),
    ("Wobble Board", "wobble", dict(STRUCK, HOLD=0.2, SNAP=0.37, GRIP=0.2, CRACK=0.5, BEND=0.0)),
    ("Thunder Sheet", "thunder", dict(STRUCK, HOLD=0.38, SNAP=0.6, GRIP=0.05, STRETCH=2.0, PITCH=-1.0, BEND=0.0)),
    ("Snap Flip", "flip", dict(SNAP=1.0, HOLD=0.1, BOW=0.6, SPEED=0.77, BEND=0.0, CRACK=0.6)),
    ("Sub Sheet", "-", dict(PITCH=-2.5, BOW=0.5, SPEED=0.4, BEND=0.3)),
    ("Glass Blade", "-", dict(PITCH=2.0, GRIP=1.0, BOW=0.25, DECAY=3.0)),
    ("Rusted", "-", dict(AGE=1.0, BOW=0.5, GRIP=0.4)),
    ("Linear Sheet", "-", dict(STRETCH=0.0, BOW=0.3)),
    ("Resonator", "audio", dict(BOW=0.0, INLEVEL=0.7, GRIP=0.3)),
]

outdir = os.path.join(ROOT, "presets", "BentSheetMetal")
os.makedirs(outdir, exist_ok=True)
for f in os.listdir(outdir):
    if f.endswith(".vcvm"): os.remove(os.path.join(outdir, f))
lines = ["# name drive " + " ".join(TCOL)]
for i, (name, drive, ch) in enumerate(P):
    s = dict(DEF); s.update(ch)
    doc = {"plugin": "BentSheetMetalFable", "model": "BentSheetMetal", "version": "2.0.0",
           "params": [{"value": round(float(s[n]), 6), "id": k} for k, n in enumerate(IDS)],
           "data": {"seed": "%016x" % (0xB5000 + i)}}
    json.dump(doc, open(os.path.join(outdir, "%02d_%s.vcvm" % (i + 1, name)), "w"), indent=1)
    lines.append(name.replace(" ", "_") + " " + drive + " " + " ".join("%g" % s[n] for n in TCOL))
open(os.path.join(ROOT, "test", "presets.txt"), "w").write("\n".join(lines) + "\n")
print("wrote", len(P), "presets")
