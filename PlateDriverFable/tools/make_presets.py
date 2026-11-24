#!/usr/bin/env python3
# Writes presets/PlateDriver/NN_Name.vcvm and test/presets.txt (the same settings in knob units, for the
# offline preset check). Values are in the panel's own units; SIZE in Hz and GAIN as loop gain.
import os, json, math
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
IDS = ["TYPE", "CAST", "SIZE", "THICK", "CURVE", "STRETCH", "DECAY", "BRIGHT", "AGE", "LEVEL",
       "DRIVE", "GAIN", "FOCUS", "PITCH", "PHASE", "TRACK", "DRIVER", "SPREAD", "GRAB",
       "PICKUP", "AMP", "DRVSAT", "RATTLE", "BLEED", "OUT", "PUMP", "RATE", "MALLET", "INLEVEL", "RELEASE"]
DEF = dict(TYPE=0, CAST=0, SIZE=65.4, THICK=0.5, CURVE=0.6, STRETCH=1.0, DECAY=1.0, BRIGHT=0.5, AGE=0.3, LEVEL=0.7,
           DRIVE=0.35, GAIN=60.0, FOCUS=0.5, PITCH=0.0, PHASE=0.0, TRACK=1, DRIVER=0.31, SPREAD=0.0, GRAB=0.0,
           PICKUP=0.2, AMP=0.2, DRVSAT=0.1, RATTLE=0.0, BLEED=0.1, OUT=0.0, PUMP=0.0, RATE=0.3, MALLET=0.5, INLEVEL=0.5, RELEASE=0.0)
HEAT0 = dict(PICKUP=0, AMP=0, DRVSAT=0, RATTLE=0, BLEED=0, OUT=0)
P = [
    ("Init", {}),
    ("Pure Singer", dict(FOCUS=0.9, DRIVE=0.25, **HEAT0)),
    ("First Split", dict(DRIVE=0.5)),
    ("Full Roar", dict(DRIVE=0.95, FOCUS=0.3)),
    ("Infinite Gong", dict(DRIVE=0.4, DECAY=3.0, SIZE=55.0, GAIN=40.0)),
    ("Overblown Horn", dict(TRACK=0, SIZE=49.0, FOCUS=0.8, DRIVE=0.42)),
    ("Breathing Drone", dict(SIZE=27.5, PITCH=-1.0, DRIVE=0.45, DECAY=3.0, GAIN=35.0)),
    ("Sub Plate", dict(SIZE=20.0, PITCH=-2.0, DRIVE=0.45)),
    ("Glass Lead", dict(THICK=0.8, FOCUS=0.85, BRIGHT=0.8, DRIVE=0.4)),
    ("Scream Lead", dict(PICKUP=1.0, AMP=0.75, BLEED=0.35, OUT=0.45, DRIVE=0.5)),
    ("Square Pusher", dict(DRVSAT=1.0, BLEED=0.6, DRIVE=0.45)),
    ("Loose Head", dict(RATTLE=0.9, DRIVE=0.5)),
    ("Motorboat", dict(PUMP=1.0, RATE=0.8, DRIVE=0.6)),
    ("Slow Surge", dict(PUMP=0.7, RATE=0.15, DRIVE=0.8)),
    ("Metal Stabs", dict(GAIN=500.0, RELEASE=0.8, DRIVE=0.6, FOCUS=0.4)),
    ("Meltdown", dict(PICKUP=1, AMP=1, DRVSAT=1, RATTLE=1, BLEED=1, OUT=1, THICK=0.1, DRIVE=1.0)),
    ("Flat Sheet", dict(CURVE=0.0, STRETCH=2.0, DRIVE=0.55)),
    ("Bowl Halo", dict(TYPE=3, DRIVE=0.25, FOCUS=0.7, DECAY=2.5)),
    ("Bossed Organ", dict(TYPE=2, FOCUS=0.7, DRIVE=0.4)),
    ("Resonator", dict(DRIVE=0.3, INLEVEL=0.8)),
]

def raw(name, v):
    if name == "SIZE": return math.log10(v / 20.0)
    if name == "GAIN": return math.log(v / 20.0) / math.log(30.0)
    return float(v)

outdir = os.path.join(ROOT, "presets", "PlateDriver")
for f in os.listdir(outdir):
    if f.endswith(".vcvm"): os.remove(os.path.join(outdir, f))
lines = []
for i, (name, ch) in enumerate(P):
    s = dict(DEF); s.update(ch)
    doc = {"plugin": "PlateDriverFable", "model": "PlateDriver", "version": "2.0.0",
           "params": [{"value": round(raw(n, s[n]), 6), "id": k} for k, n in enumerate(IDS)],
           "data": {"seed": "%016x" % (0x51A7E0000 + 7919 * (i + 1))}}
    json.dump(doc, open(os.path.join(outdir, "%02d_%s.vcvm" % (i + 1, name)), "w"), indent=1)
    lines.append(name.replace(" ", "_") + " " + " ".join("%g" % s[n] for n in IDS))
open(os.path.join(ROOT, "test", "presets.txt"), "w").write("\n".join(lines) + "\n")
print("wrote", len(P), "presets")
