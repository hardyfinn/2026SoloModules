#!/usr/bin/env python3
# Writes the factory presets: presets/Cymbolism/*.vcvm and presets/CymbolismExtended/*.vcvm,
# and test/presets.txt (the same presets as engine values, for test/preset_check).
#   python3 tools/make_presets.py
# A preset sets every knob and the hammering pattern (seed). It leaves the quality, threading and
# menu options as they are. Parameter ids follow the ParamId enums in src/Cymbolism*.cpp.
import json, math, os, shutil

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
VERSION = json.load(open(os.path.join(ROOT, "plugin.json")))["version"]

# name, size", edge mm, taper, rise, bell dia, bell mm, flange, play, position, rivets, hole  (src/CyGeometry.hpp)
TYPES = [
    ("SPLASH", 10, 0.55, 1.6, 0.060, 0.30, 12, 0, 1, 0.92, 0, 0),
    ("CRASH", 18, 0.85, 1.7, 0.070, 0.25, 18, 0, 1, 0.95, 0, 0),
    ("THIN CRASH", 17, 0.65, 1.5, 0.060, 0.24, 16, 0, 1, 0.95, 0, 0),
    ("CRASH RIDE", 20, 1.10, 1.8, 0.080, 0.23, 20, 0, 0, 0.60, 0, 0),
    ("RIDE", 21, 1.40, 1.9, 0.080, 0.22, 22, 0, 0, 0.55, 0, 0),
    ("HEAVY RIDE", 22, 1.90, 1.8, 0.090, 0.26, 26, 0, 0, 0.55, 0, 0),
    ("FLAT RIDE", 20, 1.10, 1.4, 0.015, 0.0, 0, 0, 0, 0.55, 0, 0),
    ("CHINA", 18, 0.75, 1.5, 0.050, 0.22, 22, 20, 1, 0.95, 0, 0),
    ("SWISH", 20, 0.90, 1.5, 0.050, 0.20, 18, 15, 0, 0.60, 6, 0),
    ("BELL", 8, 3.00, 1.2, 0.250, 0.60, 25, 0, 0, 0.10, 0, 0),
    ("GONG", 24, 1.60, 1.0, 0.010, 0.0, 0, -60, 2, 0.45, 0, 0),
    ("SHEET", 20, 0.80, 1.0, 0.0, 0.0, 0, 0, 2, 0.75, 0, 0),
    ("BOWL", 8, 3.00, 1.2, 0.90, 0.0, 0, 0, 5, 0.95, 0, 0),
    ("PLATE", 12, 4.00, 1.0, 0.0, 0.0, 0, 0, 2, 0.60, 0, 0),
    ("RING", 14, 2.00, 1.0, 0.0, 0.0, 0, 0, 0, 0.50, 0, 0.60),
    ("DOME", 10, 1.50, 1.3, 0.050, 0.75, 40, 0, 0, 0.30, 0, 0),
]
T = {t[0]: i for i, t in enumerate(TYPES)}
TIP, SHOULDER, MALLET, RODS, BRUSH, BOW, RAIN = range(7)
B20, B12, B8, BRASS, NICKEL, STEEL, ALUMINIUM, GLASS, WOOD = range(9)
CHROMATIC, MAJOR, MINOR, DORIAN, PENTMAJ, PENTMIN, BLUES, WHOLE, FIFTHS, OCTAVES, HARMONIC, CHORD = range(12)


def force(ms):      # FORCE knob for a strike speed in m/s
    return math.log(ms / 0.2) / math.log(60)
def decay(x):       # DECAY knob for a multiple of the natural ring time
    return math.log(x / 0.25) / math.log(16)
def rate(hz):       # RATE knob
    return math.log(hz / 2) / math.log(15)
def pivot(hz):
    return math.log(hz / 100) / math.log(50)
def swell(per_s):
    return math.log(per_s / 0.5) / math.log(60)
def colour(hz):
    return math.log(hz / 60) / math.log(166.67)
def flow(x):        # FLOW knob for a cascade-rate multiple
    return math.log(x) / math.log(20) if x < 1 else math.log10(x)


def base(type_name):
    t = TYPES[T[type_name]]
    return dict(type=T[type_name], position=t[9], force=force(3.0), size=1.0, weight=1.0, taper=t[3], profile=t[4], bell=1.0,
                flange=t[7], hole=t[11], alloy=B20, hammer=0.3, tension=0.0, decay=0.5, bloom=1.0, damp=0.0, mount=0.5,
                sizzle=t[10], mic=1.0, play=t[8], hardness=0.5, rate=0.45, exc=0.0,
                # Extended
                typeb=T["CHINA"], freeze=0, morph=0.0, warp=1.0, pivot=pivot(700), tune=0.0, scale=PENTMIN, root=2, tilt=0.0,
                sustain=0.0, swell=swell(6), colour=colour(1000), flow=0.0, load=0.0, loadpos=0.6, pickup=0.0, width=20.0, bounce=0.0,
                gesture="strike")


def P(name, type_name, note, **kw):
    d = base(type_name)
    for k in kw:
        assert k in d, k
    d.update(kw)
    d["name"], d["note"] = name, note
    return d


# ---------------- Cymbolism ----------------
CYM = [
    P("Studio Crash", "CRASH", "the all-rounder, from overhead", force=force(4.5), mic=3.0),
    P("Dark Thin Crash", "THIN CRASH", "hand hammered, loose, quick to open", size=1.06, weight=0.85, hammer=0.8, tension=-0.2, bloom=1.2, mic=1.5, force=force(3.5)),
    P("Bright Rock Crash", "CRASH", "16 inch, heavier, B8, machine clean", size=0.89, weight=1.3, alloy=B8, hammer=0.1, hardness=0.8, force=force(5), mic=2.0),
    P("Trash Splash", "SPLASH", "thin and nasty, gone in a second", weight=0.8, alloy=B8, bloom=1.4, decay=decay(0.7), force=force(4)),
    P("Jazz Ride", "RIDE", "20 inch, thin, hammered, wash under the stick", size=0.95, weight=0.8, hammer=0.85, mount=0.65, force=force(2.5)),
    P("Ping Ride", "HEAVY RIDE", "nylon tip, clear ping, little wash", hammer=0.15, hardness=0.9, position=0.5, bloom=0.8, force=force(1.5)),
    P("Ride Bell", "RIDE", "shoulder of the stick on the bell, close mic", position=0.08, play=SHOULDER, mic=0.0, force=force(3)),
    P("Dry Flat Ride", "FLAT RIDE", "no bell, a little tape, tight felt", damp=0.15, mount=0.2, force=force(3)),
    P("Sizzle Ride", "RIDE", "eight rivets on a loose, lighter ride", weight=0.85, sizzle=8, mount=0.7, force=force(2)),
    P("Crash Ride Wash", "CRASH RIDE", "crashed on the edge, big slow bloom", play=SHOULDER, position=0.95, bloom=1.3, force=force(5), mic=3.0),
    P("Trashy China", "CHINA", "the classic bark", bloom=1.4, force=force(3.5), mic=2.0),
    P("Swish Knocker", "SWISH", "six rivets, ride it or crash it", force=force(3.5)),
    P("Mallet Swell", "CRASH", "hold ROLL and ride FORCE up", play=MALLET, position=0.85, rate=rate(11), force=force(1.2), mic=3.0, gesture="roll"),
    P("Brush Sweep", "RIDE", "hold ROLL for the sweep, STRIKE for a tap", weight=0.8, play=BRUSH, position=0.6, rate=rate(5), force=force(6), hardness=0.8, gesture="roll"),
    P("Bowed Edge", "THIN CRASH", "hold ROLL to bow; FORCE is the pressure", play=BOW, position=0.98, rate=rate(8), force=force(2.5), decay=decay(1.5), gesture="roll"),
    P("Hot Rods Crash", "CRASH", "a bundle of dowels, soft attack", play=RODS, force=force(5)),
    P("Taped Practice Crash", "CRASH", "taped, tight, short", damp=0.6, mount=0.1, decay=decay(0.5), force=force(4)),
    P("Cup Chime", "BELL", "small, thick, pitched", force=force(2.5), hardness=0.7),
    P("Steel Garbage Lid", "CHINA", "steel, nearly flat, upturned rim, no bell", alloy=STEEL, weight=1.4, profile=0.02, bell=0.0, flange=25, hammer=0.9, bloom=1.8, force=force(4)),
    P("Giant Roaring Crash", "CRASH", "24 inch, paper thin, in a room", size=1.33, weight=0.6, bloom=2.0, decay=decay(1.6), mic=4.0, force=force(5)),
]

# ---------------- Cymbolism Extended ----------------
EXT = [
    P("Tuned Ride", "RIDE", "every mode on D minor pentatonic; try CHORD", tune=1.0, scale=PENTMIN, root=2, force=force(3)),
    P("Chord Crash", "CRASH", "a crash that lands on a minor ninth; patch CHORD", tune=0.85, scale=CHORD, root=2, force=force(5)),
    P("Acid Bell", "BELL", "harmonic series, short: play V/OCT and STRIKE", tune=1.0, scale=HARMONIC, root=2, decay=decay(0.5), force=force(2.2)),
    P("Reverse Swell", "CRASH", "gate HOLD to swell, CHOKE on the downbeat", sustain=0.7, swell=swell(4.5), colour=colour(2500), bloom=1.3, gesture="hold"),
    P("Endless China", "CHINA", "sings on its own; modulate COLOUR", sustain=0.85, swell=swell(6), colour=colour(1500), bloom=1.5, force=force(1.5), gesture="hold"),
    P("Melting Crash", "CRASH", "half way to a china; an envelope into MORPH", typeb=T["CHINA"], morph=0.5, force=force(5)),
    P("Mirror Metal", "RIDE", "the spectrum upside down; sweep PIVOT", warp=-1.0, pivot=pivot(900), force=force(1.6)),
    P("Gong Ride", "RIDE", "spectrum squeezed towards 500 Hz: dense and dark", warp=0.5, pivot=pivot(500), force=force(2.2)),
    P("Glass Stretch", "CRASH", "glass, spectrum stretched, highs that linger", warp=1.7, alloy=GLASS, tilt=-0.3, force=force(3)),
    P("Slow Motion Gong", "GONG", "the bloom takes seconds to arrive", flow=-1.0, bloom=1.6, force=force(7), mic=3.0),
    P("Thunder Sheet", "SHEET", "shake it with a hard mallet hit", bloom=1.5, force=force(4), mic=3.5),
    P("Rain On Tin", "SHEET", "hold ROLL; RATE is how hard it rains", alloy=STEEL, play=RAIN, rate=rate(14), force=force(10), hardness=0.55, gesture="roll"),
    P("Singing Bowl", "BOWL", "hold ROLL to bow the rim", play=BOW, rate=rate(6), force=force(11), mic=2.0, gesture="roll"),
    P("Glass Harmonica", "BOWL", "a smaller glass bowl under the bow", alloy=GLASS, size=0.7, play=BOW, rate=rate(5), force=force(11), mic=2.0, gesture="roll"),
    P("Bell Plate", "PLATE", "thick, flat, long and pitched", hardness=0.8, decay=decay(1.6), force=force(3)),
    P("Dub Siren Plate", "PLATE", "sings; a slow LFO into V/OCT", sustain=0.3, swell=swell(3), colour=colour(900), force=force(1.2), gesture="hold"),
    P("Ring Ricochet", "RING", "the stick stays down and bounces", bounce=0.6, force=force(4)),
    P("Log Disc", "CRASH", "a small, thick wooden disc: all knock", alloy=WOOD, weight=3.0, size=0.6, play=MALLET, hardness=0.8, force=force(3)),
    P("Piezo Dome", "DOME", "contact pickup, a weight on the bow, a bouncing tip", pickup=1.0, load=0.25, loadpos=0.7, bounce=0.4, force=force(10)),
    P("Halo", "CRASH", "the highs outlast the lows; press FREEZE to hold it", tilt=-0.8, bloom=1.5, decay=decay(1.6), force=force(5), mic=3.0),
]

CYM_IDS = ["type", None, "position", "force", None, None, "size", "weight", "taper", "profile", "bell", "flange", "alloy", "hammer",
           "tension", "decay", "bloom", "damp", "mount", "sizzle", "mic", "play", "hardness", "rate", "exc"]
EXT_IDS = ["type", "typeb", None, "position", "force", None, None, "freeze",
           "size", "weight", "taper", "profile", "bell", "flange", "hole", "alloy", "hammer", "tension",
           "morph", "warp", "pivot", "tune", "scale", "root", "tilt", "sustain", "swell", "colour", "flow", "load", "loadpos",
           "decay", "bloom", "damp", "mount", "sizzle", "mic", "pickup", "width", "play", "hardness", "rate", "bounce", "exc"]
# the ranges of Cymbolism's knobs (Extended's are wider)
CYM_RANGE = dict(size=(0.6, 1.5), weight=(0.5, 2.5), taper=(1, 2.5), profile=(0, 0.14), bell=(0, 2), flange=(-20, 25), alloy=(0, 5),
                 bloom=(0, 2), play=(0, 5), type=(0, 9))


def write(folder, model, ids, presets, lines):
    out = os.path.join(ROOT, "presets", model)
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    for n, p in enumerate(presets, 1):
        if model == "Cymbolism":
            # BELL's own rise (25%) is beyond Cymbolism's PROFILE knob, which the module clamps the same way
            p["profile"] = min(p["profile"], 0.14)
            for k, (lo, hi) in CYM_RANGE.items():
                assert lo <= p[k] <= hi, (p["name"], k, p[k])
        params = [{"id": i, "value": round(float(p[k]), 5)} for i, k in enumerate(ids) if k]
        seed = 1000 * (1 if model == "Cymbolism" else 2) + 2 * n + 1
        doc = {"plugin": "CymbolismFable", "model": model, "version": VERSION, "params": params, "data": {"seed": seed}}
        with open(os.path.join(out, f"{n:02d}_{p['name']}.vcvm"), "w") as f:
            json.dump(doc, f, indent=1)
            f.write("\n")
        # engine values for the check
        ev = dict(model=model, seed=seed, type=p["type"], typeb=p["typeb"], weight=p["weight"], taper=p["taper"], rise=p["profile"],
                  bell=p["bell"], flange=p["flange"], hole=p["hole"], alloy=p["alloy"], tension=p["tension"], size=p["size"],
                  decay=0.25 * 16 ** p["decay"], bloom=p["bloom"], damp=p["damp"], mount=p["mount"], hammer=p["hammer"], mic=p["mic"],
                  sizzle=p["sizzle"] / 12.0, position=p["position"], hardness=p["hardness"], force=0.2 * 60 ** p["force"],
                  rate=2 * 15 ** p["rate"], play=p["play"], morph=p["morph"], warp=p["warp"], pivot=100 * 50 ** p["pivot"], tune=p["tune"],
                  scale=p["scale"], root=p["root"], tilt=p["tilt"], sustain=p["sustain"], swell=0.5 * 60 ** p["swell"],
                  colour=60 * 166.67 ** p["colour"], flow=(20 ** p["flow"] if p["flow"] < 0 else 10 ** p["flow"]), load=p["load"],
                  loadpos=p["loadpos"], pickup=p["pickup"], width=p["width"] * math.pi / 180, bounce=p["bounce"])
        lines.append(p["name"] + "|" + p["gesture"] + "|" + " ".join(f"{k}={v}" for k, v in ev.items()))


lines = []
write("presets", "Cymbolism", CYM_IDS, CYM, lines)
write("presets", "CymbolismExtended", EXT_IDS, EXT, lines)
open(os.path.join(ROOT, "test", "presets.txt"), "w").write("\n".join(lines) + "\n")

# the preset tables in the manuals (between the PRESETS markers)
def table(presets):
    rows = "".join(f"<tr><td class=k>{n:02d} {p['name']}</td><td>{TYPES[p['type']][0]}</td><td>{p['note']}</td></tr>\n" for n, p in enumerate(presets, 1))
    return "<table><tr><th>Preset</th><th>Type</th><th>What it is</th></tr>\n" + rows + "</table>"
for doc, presets in (("guide.html", CYM), ("extended-guide.html", EXT)):
    path = os.path.join(ROOT, "docs", doc)
    h = open(path).read()
    a, b = h.index("<!--PRESETS-->") + len("<!--PRESETS-->"), h.index("<!--/PRESETS-->")
    open(path, "w").write(h[:a] + "\n" + table(presets) + "\n" + h[b:])
print(len(CYM), len(EXT), "presets written")
