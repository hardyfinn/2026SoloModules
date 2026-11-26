#!/usr/bin/env python3
# Writes presets/Molten/NN_Name.vcvm and test/presets.txt (the same settings, for the offline preset check).
# Values are in the panel's own units; SIZE in Hz and GAIN as loop gain.
import os, json, math
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
# parameter ids in src/Molten.cpp order
IDS = ["HEAT", "QUENCH", "WATER", "MASS", "FLAME", "WIDTH", "LATENT", "SUPERCOOL", "SELF", "PINGS",
       "DRIVE", "GAIN", "FOCUS", "PITCH", "TRACK", "POS", "GRAB", "RELEASE", "TENSION", "LOOSE",
       "SAG", "CAST", "BODY", "SIZE", "THICK", "CURVE", "DECAY", "PICKUP", "AMP", "OUT",
       "MALLET", "INLEVEL", "BRIGHT", "AGE", "LEVEL"]
# test/presets.txt order (test/render.cpp PRESET_KEYS); QUENCH (a button) is left out
TXT = ["HEAT", "WATER", "MASS", "FLAME", "WIDTH", "LATENT", "SUPERCOOL", "SELF", "PINGS",
       "DRIVE", "GAIN", "FOCUS", "PITCH", "TRACK", "POS", "GRAB", "RELEASE", "TENSION", "LOOSE",
       "SAG", "CAST", "BODY", "SIZE", "THICK", "CURVE", "DECAY", "PICKUP", "AMP", "OUT",
       "MALLET", "INLEVEL", "BRIGHT", "AGE", "LEVEL"]
DEF = dict(HEAT=0.0, QUENCH=0, WATER=0.4, MASS=0.3, FLAME=0.3, WIDTH=1.0, LATENT=0.45, SUPERCOOL=0.0, SELF=0.0, PINGS=0.3,
           DRIVE=0.35, GAIN=60.0, FOCUS=0.5, PITCH=0.0, TRACK=1, POS=0.31, GRAB=0.0, RELEASE=0.0, TENSION=0.0, LOOSE=0.625,
           SAG=0.67, CAST=0.5, BODY=0, SIZE=65.4, THICK=0.5, CURVE=0.6, DECAY=1.0, PICKUP=0.2, AMP=0.2, OUT=0.0,
           MALLET=0.5, INLEVEL=0.5, BRIGHT=0.5, AGE=0.3, LEVEL=0.7)
P = [
    ("Init", {}),
    ("Red Heat", dict(HEAT=0.38)),
    ("Edge of Melting", dict(HEAT=0.5, LATENT=1.0, DRIVE=0.4)),
    ("Seething", dict(HEAT=0.9, DRIVE=0.6)),
    ("White Hot Drone", dict(HEAT=1.0, SIZE=30.0, DRIVE=0.25, PITCH=-1.0)),
    ("Foundry", dict(HEAT=0.9, MASS=0.1, DRIVE=0.45, CAST=0.8)),
    ("Flash Freeze", dict(HEAT=0.4, SUPERCOOL=1.0, DRIVE=0.3)),
    ("Walking Flame", dict(HEAT=0.8, WIDTH=0.2, FLAME=0.5, DRIVE=0.4)),
    ("Spot Weld", dict(HEAT=1.0, WIDTH=0.0, FLAME=0.7, DRIVE=0.4)),
    ("Slow Furnace", dict(HEAT=0.8, MASS=1.0, DRIVE=0.4)),
    ("Runaway", dict(SELF=1.0, DRIVE=0.8, HEAT=0.1)),
    ("Cold Bell", dict(DRIVE=0.0, DECAY=2.5, MALLET=0.7)),
    ("Annealed", dict(HEAT=0.3, DRIVE=0.0, DECAY=0.4, MALLET=0.2)),
    ("Mercury", dict(TENSION=-2.0, HEAT=0.8, DRIVE=0.45)),
    ("Light Metal", dict(TENSION=1.0, HEAT=0.7, BRIGHT=1.0, DRIVE=0.4)),
    ("Molten Lead", dict(TRACK=0, HEAT=0.75, FOCUS=0.8, DRIVE=0.45)),
    ("Scream", dict(HEAT=0.45, PICKUP=1.0, AMP=0.75, OUT=0.45, DRIVE=0.5)),
    ("Meltdown", dict(HEAT=0.5, DRIVE=0.9, GAIN=80.0, FOCUS=0.4, SIZE=49.0, PICKUP=1.0, AMP=0.6, OUT=0.6, BRIGHT=1.0,
                      LOOSE=0.35, PINGS=1.0, LATENT=0.17, MASS=0.15)),
    ("Stiff", dict(LOOSE=0.0, SAG=0.0, HEAT=0.7, DRIVE=0.5)),
    ("Resonator", dict(DRIVE=0.15, INLEVEL=0.8, HEAT=0.6)),
]


def raw(name, v):
    if name == "SIZE": return math.log10(v / 20.0)
    if name == "GAIN": return math.log(v / 20.0) / math.log(30.0)
    return float(v)


outdir = os.path.join(ROOT, "presets", "Molten")
os.makedirs(outdir, exist_ok=True)
for f in os.listdir(outdir):
    if f.endswith(".vcvm"): os.remove(os.path.join(outdir, f))
lines = []
for i, (name, ch) in enumerate(P):
    s = dict(DEF); s.update(ch)
    doc = {"plugin": "MoltenFable", "model": "Molten", "version": "2.0.0",
           "params": [{"value": round(raw(n, s[n]), 6), "id": k} for k, n in enumerate(IDS)],
           "data": {"seed": "%016x" % (0x51A7E0000 + 7919 * (i + 1))}}
    json.dump(doc, open(os.path.join(outdir, "%02d_%s.vcvm" % (i + 1, name)), "w"), indent=1)
    lines.append(name.replace(" ", "_") + " " + " ".join("%g" % raw(n, s[n]) for n in TXT))
open(os.path.join(ROOT, "test", "presets.txt"), "w").write("\n".join(lines) + "\n")
print("wrote", len(P), "presets")
