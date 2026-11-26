#!/usr/bin/env python3
# Writes presets/LiveWire/NN_Name.vcvm, test/presets.txt (the same settings in knob units plus how the offline
# test drives each one) and src/defaults.hpp (the Init values the module's knobs default to).
# Values are given in physical units (BUILD-INSTRUCTIONS.md section 5) and converted with the inverse maps of
# tools/proto/mapping.h, which this file mirrors.
import os, json, math
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")

def iexp(v, lo, hi): return math.log(v / lo) / math.log(hi / lo)
INV = {
    "VOLTS": lambda v: v / 4.0,
    "MAINS": lambda v: v,
    "FREQ": lambda v: iexp(v, 20.0, 500.0),
    "CURRENT": lambda v: iexp(v, 0.01, 4.0),
    "CHOKE": lambda v: iexp(v, 5e-5, 2e-2),
    "GAP": lambda v: math.sqrt((v - 0.01) / 4.0),
    "HEAT": lambda v: iexp(v, 2e-4, 0.6),
    "COOL": lambda v: iexp(v, 8e-6, 3e-3),
    "QUENCH": lambda v: iexp(v, 2e-3, 0.5),
    "MEMORY": lambda v: iexp(v, 1e-3, 0.2),
    "RESTRIKE": lambda v: v / 0.95,
    "JITTER": lambda v: v / 0.6,
    "DRAW": lambda v: iexp(v, 0.05, 10.0),
    "WIND": lambda v: math.sqrt(v / 0.15),
    "CLIMB": lambda v: (v / 40.0) ** (1 / 3),
    "TUNE": lambda v: (math.log2(v / 261.6256) + 4.0) / 10.0,
    "RING": lambda v: iexp(v, 0.7, 60.0),
    "LOAD": lambda v: iexp(v, 8.0, 0.15),
    "IRON": lambda v: math.sqrt(v / 3.0),
    "CORONA": lambda v: v / 3.0,
    "HUM": lambda v: v / 0.3,
    "FB": lambda v: v / 1.6,
    "DIST": lambda v: iexp(v, 3e-4, 2e-2),
    "AGE": lambda v: v,
    "IN": lambda v: math.sqrt(v / 2.0),
    "ARC": lambda v: (v / 8.0) ** (1 / 3),
    "FIELD": lambda v: math.sqrt(v / 2.0),
    "OUT": lambda v: math.sqrt(v / 4.0),
    "TRACK": lambda v: v,
    "VTARGET": lambda v: v,     # 0 TANK, 1 BOTH, 2 MAINS
}
# Param order = the module's ParamIds enum (the buttons come after these and are not saved in presets).
IDS = ["VOLTS", "MAINS", "FREQ", "CURRENT", "CHOKE", "GAP", "HEAT", "COOL", "QUENCH", "MEMORY", "RESTRIKE", "JITTER", "DRAW",
       "WIND", "CLIMB", "TUNE", "RING", "LOAD", "IRON", "CORONA", "HUM", "FB", "DIST", "AGE", "IN", "ARC", "FIELD", "OUT",
       "TRACK", "VTARGET"]

INIT = dict(VOLTS=1.7, MAINS=1.0, FREQ=50.0, CURRENT=1.0, CHOKE=0.003, GAP=0.5, HEAT=0.35, COOL=90e-6, QUENCH=3e-3, MEMORY=0.006,
            RESTRIKE=0.8, JITTER=0.08, DRAW=2.0, WIND=0.03, CLIMB=0.0, TUNE=2500.0, RING=4.0, LOAD=6.0, IRON=0.0, CORONA=0.0,
            HUM=0.05, FB=0.0, DIST=0.002, AGE=0.0, IN=0.0, ARC=1.0, FIELD=0.15, OUT=1.0, TRACK=0, VTARGET=0)

# The singing presets are tuned so that 0 V on V/OCT gives C4 on the FIELD output (measured with test/render tune).
SING = dict(VOLTS=1.6, MAINS=0.0, CURRENT=1.1, CHOKE=0.012, GAP=0.25, LOAD=0.4, RING=28.0, TRACK=1, COOL=63e-6, JITTER=0.05,
            WIND=0.004, MEMORY=0.02, ARC=0.8, FIELD=0.45, HUM=0.0)
KICK = dict(VOLTS=2.5, MAINS=0.0, GAP=4.0, CURRENT=0.04, CHOKE=0.0002, HEAT=0.0012, COOL=1.2e-3, MEMORY=0.004, RESTRIKE=0.5,
            QUENCH=0.2, JITTER=0.1, WIND=0.01, TUNE=44.0, LOAD=0.6, RING=14.0, IRON=0.22, ARC=6.0, FIELD=1.3, HUM=0.0)
HAT = dict(VOLTS=2.5, MAINS=0.0, GAP=4.0, CURRENT=0.02, CHOKE=0.0002, HEAT=0.01, COOL=15e-6, QUENCH=0.01, MEMORY=0.004,
           RESTRIKE=0.5, JITTER=0.15, TUNE=8200.0, LOAD=5.0, RING=7.0, ARC=3.0, FIELD=0.07, HUM=0.0, WIND=0.01)
RELAX = dict(VOLTS=2.6, MAINS=0.0, GAP=0.5, CURRENT=0.05, CHOKE=0.0001, HEAT=0.03, COOL=30e-6, QUENCH=0.2, RESTRIKE=0.3,
             TUNE=700.0, LOAD=1.5, RING=12.0, ARC=3.0, FIELD=1.0, HUM=0.0, OUT=1.6)
SUB = dict(VOLTS=0.95, MAINS=1.0, GAP=1.25, CURRENT=1.5, CHOKE=0.004, COOL=140e-6, MEMORY=0.008, JITTER=0.2, WIND=0.02, LOAD=3.0,
           RING=10.0, TUNE=740.0, FIELD=0.3, HUM=0.14, CORONA=1.6)
def w(*ds, **kw):
    r = {}
    for d in ds: r.update(d)
    r.update(kw); return r

# (name, driver for the offline test, physical settings over INIT)
P = [
    ("Init", "-", {}),
    ("Drawn Arc", "touch", dict(GAP=1.3, DRAW=4.0, WIND=0.05)),
    ("Jacobs Ladder", "-", dict(VOLTS=2.0, GAP=0.12, CURRENT=1.3, CHOKE=0.005, COOL=150e-6, MEMORY=0.012, CLIMB=1.0, WIND=0.02)),
    ("Runaway Ladder", "-", dict(VOLTS=2.8, GAP=0.12, CURRENT=2.4, CHOKE=0.005, COOL=60e-6, MEMORY=0.012, CLIMB=6.0, WIND=0.055,
                                 LOAD=2.5, RING=9.0, TUNE=2000.0, FB=0.5, DIST=0.0023, OUT=0.5)),
    ("Singing Arc", "notes", w(SING, OUT=1.8, TUNE=281.7021)),
    ("Starved Arc", "notes", w(SING, OUT=1.8, CURRENT=0.45, WIND=0.015, TUNE=323.0305)),
    ("Iron Throat", "notes", w(SING, VOLTS=1.9, CURRENT=0.5, IRON=1.5, LOAD=0.28, COOL=100e-6, TUNE=280.4034)),
    ("Howl", "notes", w(SING, VOLTS=2.3, CURRENT=0.9, GAP=0.6, WIND=0.04, IRON=0.8, FB=1.1, DIST=0.0031, TUNE=226.1851)),
    ("Spark Kick", "kick", KICK),
    ("Erratic Kick", "kick", w(KICK, VOLTS=3.0, JITTER=0.3, IRON=0.5, MEMORY=0.06, RESTRIKE=0.62)),
    ("Spark Snare", "snare", dict(VOLTS=0.6, MAINS=0.0, CURRENT=0.04, GAP=1.2, CHOKE=0.0002, HEAT=0.3, COOL=25e-6, QUENCH=0.02,
                                  MEMORY=0.002, RESTRIKE=0.92, JITTER=0.5, WIND=0.07, CLIMB=30.0, TUNE=205.0, LOAD=1.0, RING=5.0,
                                  IRON=0.3, ARC=0.11, FIELD=0.5, HUM=0.0)),
    ("Spark Hat", "hat", HAT),
    ("Erratic Hat", "hat", w(HAT, JITTER=0.45, MEMORY=0.02, RESTRIKE=0.4, CORONA=0.05)),
    ("Relaxation Buzz", "-", RELAX),
    ("Geiger", "-", w(RELAX, CURRENT=0.012, JITTER=0.5, TUNE=3000.0, RING=3.0, ARC=8.0, FIELD=2.0, OUT=2.2)),
    ("Substation", "surge", SUB),
    ("Fault", "-", w(SUB, VOLTS=2.4, CURRENT=2.8, CLIMB=3.0, WIND=0.05, CORONA=2.2, FB=0.5, DIST=0.004, OUT=0.3)),
    ("Mains Hum", "-", dict(VOLTS=0.7, GAP=4.0, HUM=0.3, CORONA=0.0, FIELD=0.3, AGE=0.3, OUT=0.6)),
    ("Power Line", "-", dict(VOLTS=1.0, GAP=1.6, CORONA=2.0, HUM=0.04, WIND=0.03)),
    ("Plasma Speaker", "audio", dict(VOLTS=1.8, MAINS=0.0, CURRENT=1.4, GAP=0.3, CHOKE=0.001, LOAD=6.0, RING=1.0, IN=1.6, HUM=0.0, ARC=4.0, OUT=2.0)),
    ("Worn Out", "-", dict(AGE=0.8)),
    ("Tesla", "notesMains", dict(VOLTS=3.0, FREQ=300.0, GAP=1.5, CURRENT=1.5, TUNE=1200.0, RING=20.0, LOAD=1.5, VTARGET=2)),
]

def knobs(phys):
    s = dict(INIT); s.update(phys)
    out = []
    for n in IDS:
        k = INV[n](s[n])
        if n != "VTARGET" and (k < -1e-9 or k > 1 + 1e-9): raise SystemExit("%s out of range for %s: %g" % (n, phys, k))
        out.append(k if n == "VTARGET" else min(1.0, max(0.0, k)))
    return out

if __name__ == "__main__":
    outdir = os.path.join(ROOT, "presets", "LiveWire")
    os.makedirs(outdir, exist_ok=True)
    for f in os.listdir(outdir):
        if f.endswith(".vcvm"): os.remove(os.path.join(outdir, f))
    lines = ["# name driver " + " ".join(IDS)]
    for i, (name, drv, ph) in enumerate(P):
        kv = knobs(ph)
        doc = {"plugin": "LiveWireFable", "model": "LiveWire", "version": "2.0.0",
               "params": [{"value": round(v, 6), "id": j} for j, v in enumerate(kv)],
               "data": {"seed": "%016x" % (0x1E000 + i)}}
        json.dump(doc, open(os.path.join(outdir, "%02d_%s.vcvm" % (i + 1, name)), "w"), indent=1)
        lines.append(name.replace(" ", "_") + " " + drv + " " + " ".join("%.6f" % v for v in kv))
    os.makedirs(os.path.join(ROOT, "test"), exist_ok=True)
    open(os.path.join(ROOT, "test", "presets.txt"), "w").write("\n".join(lines) + "\n")
    kv = knobs({})
    hdr = "// Generated by tools/make_presets.py: the Init knob values, in ParamIds order.\n#pragma once\nstatic const float LW_DEFAULTS[%d] = {%s};\n" % (
        len(kv), ", ".join("%.6ff" % v for v in kv))
    open(os.path.join(ROOT, "src", "defaults.hpp"), "w").write(hdr)
    print("wrote", len(P), "presets")
