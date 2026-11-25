#!/usr/bin/env python3
# Writes presets/LongMetal/NN_Name.vcvm and test/presets.txt (the same settings in knob units, for the offline
# preset check and the demos). The second column of presets.txt says how the test drives it (triggers, LFOs,
# audio); "-" means nothing patched. The preset notes in the manual say the same.
import os, json
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
# Param order = the module's ParamIds enum.
IDS = ["PITCH", "LENGTH", "SPACING", "GAUGE", "STIFF", "SUPPORTS", "ENDA", "ENDB", "GROUND", "BRIGHT", "AGE",
       "LIVE", "DISTANCE", "LISTEN", "PASSTIME", "PASS", "WARP", "SWELL", "LOOSE", "GRIP",
       "TRAIN", "SPEED", "FLATS", "STRIKE", "HARD", "INLEVEL", "EVEN", "WIDTH", "LEVEL"]
DEF = dict(PITCH=0.0, LENGTH=320, SPACING=9, GAUGE=0, STIFF=0.51, SUPPORTS=0.71, ENDA=0.31, ENDB=0.31, GROUND=0.5, BRIGHT=0.42, AGE=0.35,
           LIVE=0.225, DISTANCE=0.7, LISTEN=0.12, PASSTIME=30, PASS=0, WARP=0, SWELL=0, LOOSE=0, GRIP=0,
           TRAIN=0.5, SPEED=0.49, FLATS=0.5, STRIKE=0, HARD=0.8, INLEVEL=0.5, EVEN=1.0, WIDTH=1.0, LEVEL=0.7)
# the self-singing line of scenes `live` and `live2`
LIVE = dict(LENGTH=200, SPACING=7, STIFF=0.54, SUPPORTS=0.35, ENDA=-0.92, ENDB=0.92, LIVE=0.76, WARP=0.3, TRAIN=0.0,
            BRIGHT=0.09, GROUND=0.14, LISTEN=0.3, DISTANCE=0.2, AGE=0.4)
# the struck cable of scene `hard`
HARD = dict(LENGTH=256, GAUGE=1, SPACING=6, STIFF=0.77, SUPPORTS=0.33, ENDA=-0.97, ENDB=-0.97, LIVE=0.36, BRIGHT=0.65,
            GROUND=0.0, WARP=0.42, SWELL=0.3, AGE=0.3, LISTEN=0.04, DISTANCE=0.89, TRAIN=0.0, HARD=0.74)
P = [
    ("Init", "-", {}),
    ("Train Coming", "pass", dict(PASSTIME=30)),
    ("Close Rail", "-", dict(DISTANCE=0.12)),
    ("Live Line", "lfoStiff", dict(LIVE)),
    ("Live Glide", "lfoStiff+lfoSupports", dict(LIVE, SPACING=3, STIFF=0.6, SUPPORTS=0.44, LIVE=0.74, WARP=0.0)),
    ("Live Scream", "-", dict(LIVE, LIVE=1.0, WARP=0.5, LOOSE=0.2, SPACING=14, STIFF=0.6, SUPPORTS=0.39)),
    ("Live Gated", "gate", dict(LIVE, LIVE=0.45)),
    ("Knocked Alive", "strike", dict(LIVE, LIVE=0.72, HARD=0.6)),
    ("Hard Hits", "strike", dict(HARD)),
    ("Hard Cable", "strike", dict(HARD, SPACING=3, SUPPORTS=0.0, AGE=0.2)),
    ("Rattling Sleepers", "strike", dict(HARD, SPACING=5, SUPPORTS=0.48, LOOSE=0.63, WARP=0.67, AGE=0.45, LIVE=0.38)),
    ("Break Loose", "strikeSoftHard", dict(HARD, SPACING=18, SUPPORTS=0.3, LOOSE=0.25, WARP=0.5, AGE=0.35, LIVE=0.34, GRIP=0.67)),
    ("Laser", "strike", dict(HARD, STIFF=0.77, SPACING=2, SUPPORTS=0.0, WARP=0.0, SWELL=0.0, AGE=0.0, LIVE=0.37)),
    ("Never Repeats", "strike", dict(HARD, STIFF=0.77, SPACING=40, SUPPORTS=0.0, WARP=0.0, SWELL=0.0, AGE=0.0, LIVE=0.32)),
    ("Glass Rail", "-", dict(SUPPORTS=0.9, AGE=0.0, TRAIN=0.4)),
    ("Rust Rail", "-", dict(AGE=1.0, SUPPORTS=0.8, GROUND=0.8)),
    ("Giant", "-", dict(PITCH=-2.0)),
    ("Wire", "seqVoct", dict(PITCH=1.0, LENGTH=64, SPACING=2, ENDA=-0.97, ENDB=-0.97, STIFF=0.4, SUPPORTS=0.0, TRAIN=0.0,
                             LIVE=0.4, DISTANCE=0.6, LISTEN=0.1, HARD=0.9, AGE=0.1)),
    ("Feedback Rail", "-", dict(TRAIN=0.3, LIVE=0.49, SUPPORTS=0.75)),
    ("Resonator", "audio", dict(TRAIN=0.0, INLEVEL=0.7, LIVE=0.4, SUPPORTS=0.6)),
]

outdir = os.path.join(ROOT, "presets", "LongMetal")
os.makedirs(outdir, exist_ok=True)
for f in os.listdir(outdir):
    if f.endswith(".vcvm"): os.remove(os.path.join(outdir, f))
lines = ["# name drive " + " ".join(IDS)]
for i, (name, drive, ch) in enumerate(P):
    s = dict(DEF); s.update(ch)
    doc = {"plugin": "LongMetalFable", "model": "LongMetal", "version": "2.0.0",
           "params": [{"value": round(float(s[n]), 6), "id": k} for k, n in enumerate(IDS)]}
    json.dump(doc, open(os.path.join(outdir, "%02d_%s.vcvm" % (i + 1, name)), "w"), indent=1)
    lines.append(name.replace(" ", "_") + " " + drive + " " + " ".join("%g" % s[n] for n in IDS))
os.makedirs(os.path.join(ROOT, "test"), exist_ok=True)
open(os.path.join(ROOT, "test", "presets.txt"), "w").write("\n".join(lines) + "\n")
print("wrote", len(P), "presets")
