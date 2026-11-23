#!/usr/bin/env python3
# Writes presets/<Module>/NN_<name>.vcvm for the seven modules. Param ids follow the enums in src/*.cpp.
#   python3 tools/export_presets.py
import os, json, math, shutil

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "presets")
VERSION = "2.0.0"
lg = math.log


def write(model, presets, ids, defaults):
    d = os.path.join(OUT, model)
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    for i, pr in enumerate(presets):
        name, over = pr[0], pr[1]
        data = pr[2] if len(pr) > 2 else None
        vals = dict(defaults)
        for k, v in over.items():
            assert k in ids, (model, k)
            vals[k] = v
        params = [{"value": float(vals[k]), "id": ids[k]} for k in sorted(vals, key=lambda k: ids[k])]
        doc = {"plugin": "WerkbankFable", "model": model, "version": VERSION, "params": params}
        if data is not None:
            doc["data"] = data
        with open(os.path.join(d, "%02d_%s.vcvm" % (i + 1, name)), "w", encoding="utf-8") as f:
            json.dump(doc, f, indent=1, ensure_ascii=False)
            f.write("\n")
    print(model, len(presets))


def idmap(names):
    return {n: i for i, n in enumerate(names)}

# ------------------------------------------------------------------ ZAP
ZI = idmap("START END SWEEP RES DECAY NOISE CLICK DRIVE AGE LEVEL HIT".split())
hz = lambda f: lg(f / 20.0) / lg(800.0)
sw = lambda t: lg(t / 0.001) / lg(1000.0)
dc = lambda t: 1.0 if t >= 8 else lg(t / 0.005) / lg(1600.0)
ZD = dict(START=hz(4000), END=hz(100), SWEEP=sw(0.03), RES=0.8, DECAY=dc(0.4), NOISE=0, CLICK=1, DRIVE=0, AGE=0.3, LEVEL=0.8, HIT=0)
def Z(s, e, swp, res, dec, noise=0, click=1, drive=0, age=0.3):
    return dict(START=hz(s), END=hz(e), SWEEP=sw(swp), RES=res, DECAY=dc(dec), NOISE=noise, CLICK=click, DRIVE=drive, AGE=age)
write("Zap", [
    ("Init Zap", {}),
    ("Deep Kick", Z(400, 45, 0.06, 0.85, 0.9)),
    ("Electro Kick", Z(1500, 55, 0.012, 0.8, 0.35, drive=0.6)),
    ("Pew Snare", Z(6000, 180, 0.02, 0.6, 0.18, noise=0.5, drive=0.2)),
    ("Tick", Z(9000, 3000, 0.004, 0.5, 0.03, click=0.8)),
    ("Rim", Z(2500, 1700, 0.005, 0.75, 0.06)),
    ("Low Tom", Z(500, 90, 0.08, 0.85, 0.5)),
    ("High Tom", Z(900, 200, 0.06, 0.85, 0.35)),
    ("Laser", Z(12000, 300, 0.25, 0.95, 0.8)),
    ("Riser", Z(150, 3000, 0.3, 0.9, 0.9)),
    ("Sine Drone", Z(110, 110, 0.03, 1.0, 8.0, click=1)),
    ("Tired Zap", Z(4000, 100, 0.03, 0.8, 0.4, age=1.0)),
], ZI, ZD)

# ------------------------------------------------------------------ BLECH
BI = idmap(["BOX", "DYN", "PITCH", "TONE", "AGE", "LEVEL"] + ["PAD%d" % v for v in range(6)] + ["TUNE%d" % v for v in range(6)] + ["DECAY%d" % v for v in range(6)])
BD = dict(BOX=2, DYN=0.7, PITCH=0, TONE=1, AGE=0.3, LEVEL=0.8, **{"PAD%d" % v: 0 for v in range(6)}, **{"TUNE%d" % v: 0 for v in range(6)}, **{"DECAY%d" % v: 0 for v in range(6)})
allv = lambda key, x: {"%s%d" % (key, v): x for v in range(6)}
write("Blech", [
    ("Init Box 1", {}),
    ("Soft Box 2", dict(BOX=1, DYN=0.55)),
    ("Electro Box 3", dict(BOX=0, DYN=0.8)),
    ("Low and Long", {**allv("TUNE", -0.4), **allv("DECAY", 0.5)}),
    ("Tight and High", {**allv("TUNE", 0.4), **allv("DECAY", -0.5)}),
    ("Dull Pedal", dict(TONE=0.3, DYN=0.4)),
    ("Hot Pedal", dict(DYN=1.0)),
    ("Tired Box", dict(AGE=1.0)),
], BI, BD)

# ------------------------------------------------------------------ VOKODER
VI = idmap(["BAND%d" % i for i in range(20)] + "MODGAIN ATTACK RELEASE FORMANT SIB VOICING AGE LEVEL SHAPE WIDTH TUNE CHORD FREEZE".split())
va = lambda t: lg(t / 0.001) / lg(30.0)
vr = lambda t: lg(t / 0.010) / lg(60.0)
VD = dict(**{"BAND%d" % i: 0.5 for i in range(20)}, MODGAIN=1, ATTACK=va(0.004), RELEASE=vr(0.04), FORMANT=0, SIB=0.7, VOICING=0.5,
          AGE=0.3, LEVEL=0.8, SHAPE=0, WIDTH=0.3, TUNE=-1, CHORD=0, FREEZE=0)
tel = {"BAND%d" % i: 0.08 for i in list(range(5)) + list(range(16, 20))}
write("Vokoder", [
    ("Init", {}),
    ("Clear Robot", dict(ATTACK=va(0.0015), RELEASE=vr(0.015), CHORD=0, SHAPE=0)),
    ("Choir of Machines", dict(CHORD=3, RELEASE=vr(0.25), SHAPE=0.3)),
    ("Small Robot", dict(FORMANT=3)),
    ("Giant Robot", dict(FORMANT=-3, TUNE=-2)),
    ("Telephone", tel),
    ("Whisper Only", dict(SIB=1.0, VOICING=0.0)),
    ("Tired Unit", dict(AGE=1.0)),
], VI, VD)

# ------------------------------------------------------------------ SPRECHER
SI = idmap("LANG ENGINE HOLD PITCH INFLECT RATE SIZE WHISPER AGE LEVEL SPEAK RESET".split())
ph = lambda f: lg(f / 55.0) / lg(8.0)
SD = dict(LANG=0, ENGINE=1, HOLD=1, PITCH=ph(110), INFLECT=0.3, RATE=0, SIZE=1, WHISPER=0, AGE=0.3, LEVEL=0.8, SPEAK=0, RESET=0)
T = lambda s: {"text": s}
write("Sprecher", [
    ("Count to Eight", {}, T("1 2 3 4 5 6 7 8")),
    ("Zählen", dict(LANG=1), T("1 2 3 4 5 6 7 8")),
    ("Seven Tongues", dict(LANG=5), T("1 2 3 4 5 6 7 8 9 10")),
    ("Chip Counter", dict(LANG=1, ENGINE=0), T("1 2 3 4 5 6 7 8")),
    ("Music Machine", {}, T("we are the music machine")),
    ("Data Words", {}, T("computer data memory program energy")),
    ("Sung Vowels", dict(HOLD=1, INFLECT=0), T("[M 'AA] [N 'IY] [L 'UW] [M 'OW] [N 'EH]")),
    ("Small Voice", dict(SIZE=0.75, PITCH=ph(220)), T("1 2 3 4 5 6 7 8")),
    ("Giant", dict(SIZE=1.35, PITCH=ph(65)), T("1 2 3 4 5 6 7 8")),
    ("Whisperer", dict(WHISPER=1.0), T("computer data memory program energy")),
    ("Spelling", {}, T("h a e r")),
    ("Tired Clock", dict(AGE=1.0), T("1 2 3 4 5 6 7 8 9 10 11 12")),
], SI, SD)

# ------------------------------------------------------------------ ORCHESTER
OI = idmap("DISC WOBBLE TONE ATTACK RELEASE OCTAVE AGE LEVEL".split())
oa = lambda t: lg(t / 0.005) / lg(400.0)
orl = lambda t: lg(t / 0.03) / lg(4.0 / 0.03)
OD = dict(DISC=0, WOBBLE=0.5, TONE=0.5, ATTACK=0, RELEASE=0, OCTAVE=1, AGE=0.3, LEVEL=0.8)
discs = "Choir Strings Flute Organ Cello Horn Vox Reed".split()
write("Orchester", [(n, dict(DISC=i)) for i, n in enumerate(discs)] + [
    ("Slow Choir", dict(DISC=0, ATTACK=oa(0.4), RELEASE=orl(1.5))),
    ("Warped Disc", dict(WOBBLE=1.0, AGE=1.0)),
], OI, OD)

# ------------------------------------------------------------------ DYNAMO
DI = idmap("OCTAVE WAVE1 WAVE2 DETUNE WIDTH MIX SUB NOISE CUTOFF RES ENV KEY FA FD FS AA AD AS GLIDE DRIVE AGE LEVEL CUT_ATT".split())
ta = lambda t: lg(t / 0.001) / lg(5000.0)
td = lambda t: lg(t / 0.005) / lg(2000.0)
gl = lambda t: math.sqrt(t / 2.0)
def fromP(P, octave=1):
    W1, W2, DET, PW, MIX, SUB, NZ, CUT, RES, ENV, KEY, FA, FD, FS, AA, AD, AS, GLIDE, DRIVE, AGE = P
    return dict(OCTAVE=octave, WAVE1=1 - W1, WAVE2=1 - W2, DETUNE=DET, WIDTH=PW, MIX=MIX, SUB=SUB, NOISE=NZ, CUTOFF=CUT, RES=RES,
                ENV=ENV * 7.0, KEY=KEY, FA=ta(FA), FD=td(FD), FS=FS, AA=ta(AA), AD=td(AD), AS=AS, GLIDE=gl(GLIDE), DRIVE=DRIVE, AGE=0.3)
DD = dict(OCTAVE=1, WAVE1=1, WAVE2=1, DETUNE=0.1, WIDTH=0.5, MIX=0.5, SUB=0, NOISE=0, CUTOFF=0.45, RES=0.25, ENV=0.45 * 7, KEY=0.5,
          FA=ta(0.002), FD=td(0.25), FS=0.1, AA=ta(0.002), AD=td(0.4), AS=0.7, GLIDE=0, DRIVE=0.3, AGE=0.3, LEVEL=0.8, CUT_ATT=1)
bass = [0, 0, .10, .5, .5, .4, 0, .30, .25, .45, .3, .002, .18, .0, .002, .30, .6, 0, .3, 0]
pluck = [0, 1, 12., .3, .3, 0, 0, .35, .70, .60, .5, .002, .08, .0, .002, .12, .0, 0, .2, 0]
lead = [1, 1, .15, .35, .5, 0, 0, .55, .20, .25, .5, .020, .40, .6, .010, .30, .9, .04, .3, 0]
write("Dynamo", [
    ("Init", {}),
    ("Motor Bass", fromP(bass)),
    ("Clipped Pluck", fromP(pluck)),
    ("Sliding Lead", fromP(lead, octave=2)),
    ("Hollow Pulse Melody", dict(OCTAVE=2, WAVE1=0, WAVE2=0, WIDTH=0.25, DETUNE=0.08, CUTOFF=0.5, RES=0.3, ENV=0.3 * 7, FD=td(0.3), FS=0.3, AS=0.8)),
    ("Sub Bass", dict(OCTAVE=0, SUB=1.0, MIX=0.0, CUTOFF=0.3, RES=0.1, ENV=0.2 * 7, AD=td(0.6), AS=0.8, DRIVE=0.2)),
    ("Octave Bass", dict(DETUNE=12.0, CUTOFF=0.38, RES=0.3, ENV=0.5 * 7, FD=td(0.15))),
    ("Resonant Blip", dict(OCTAVE=2, RES=0.85, CUTOFF=0.3, ENV=0.6 * 7, FD=td(0.08), FS=0.0, AD=td(0.15), AS=0.0)),
    ("Soft Pad Lead", dict(OCTAVE=2, AA=ta(0.3), AD=td(1.0), AS=0.8, CUTOFF=0.4, ENV=0.25 * 7, FA=ta(0.4), FD=td(1.5), FS=0.4, GLIDE=gl(0.1), DETUNE=0.12, DRIVE=0.15)),
    ("Noise Sweep", dict(NOISE=1.0, MIX=0.5, CUTOFF=0.2, RES=0.7, ENV=0.8 * 7, FA=ta(1.0), FD=td(2.0), FS=0.0, AA=ta(0.5), AD=td(2.0), AS=0.6)),
    ("Factory Fresh", dict(AGE=0.0)),
    ("Tired Unit", dict(AGE=1.0)),
], DI, DD)

# ------------------------------------------------------------------ TAKT
TI = idmap(["PATTERN", "GATE", "LAST_A", "LAST_B", "RUN", "RESET", "RANGE", "CHAIN", "WHEN", "KEYS"] + ["KEY%d" % n for n in range(12)]
           + ["SLIDER%d" % k for k in range(32)] + ["MODE%d" % k for k in range(32)] + ["ACC%d" % k for k in range(32)])
RANGE_OCT = {0: 1, 1: 2, 2: 4}
TD = dict(PATTERN=0, GATE=0.5, LAST_A=16, LAST_B=16, RUN=1, RESET=0, RANGE=1, CHAIN=0, WHEN=0, KEYS=1, **{"KEY%d" % n: 0 for n in range(12)},
          **{"SLIDER%d" % k: 0 for k in range(32)}, **{"MODE%d" % k: 2 for k in range(32)}, **{"ACC%d" % k: 0 for k in range(32)})
MODEV = {"P": 2, "T": 1, "R": 0}   # panel switch value
MODEI = {"P": 0, "T": 1, "R": 2}   # pattern mode


def takt(semA, modeA="", accA=(), semB=None, modeB="", accB=(), lastA=16, lastB=16, rng=1, series=False, gate=0.5, scale=0xFFF, extra=None):
    octs = RANGE_OCT[rng]
    semB = semB or [0] * 16
    modeA = (modeA or "P" * 16).ljust(16, "P"); modeB = (modeB or "P" * 16).ljust(16, "P")
    sl = [s / (12.0 * octs) for s in list(semA) + list(semB)]
    md = list(modeA) + list(modeB)
    ac = [k in accA for k in range(16)] + [k in accB for k in range(16)]
    over = dict(LAST_A=lastA, LAST_B=lastB, RANGE=rng, CHAIN=1 if series else 0, GATE=gate)
    for k in range(32):
        over["SLIDER%d" % k] = sl[k]; over["MODE%d" % k] = MODEV[md[k]]; over["ACC%d" % k] = 1 if ac[k] else 0
    if extra: over.update(extra)
    pats = [{"slider": sl, "mode": [MODEI[m] for m in md], "accent": ac, "lastA": lastA, "lastB": lastB}]
    pats += [{"slider": [0.0] * 32, "mode": [0] * 32, "accent": [False] * 32, "lastA": 16, "lastB": 16} for _ in range(7)]
    return over, {"patterns": pats, "current": 0, "scale": scale, "keyTranspose": 0}


presets = []
def T_(name, *a, **k):
    over, data = takt(*a, **k)
    presets.append((name, over, data))

T_("Init", [0] * 16)
T_("Octave Motor", [0, 12, 0, 7, 0, 12, 3, 10, 0, 12, 0, 7, 5, 12, 3, 15], "PRPPPRPPPRPPPPRP", (0, 8))
T_("Fifths Climb", [0, 7, 14, 21, 28, 35, 42, 35, 28, 21, 14, 7, 0, 7, 14, 21], accA=(0, 6), rng=2, gate=0.4)
T_("Minor Arpeggio 5 against 16", [0, 3, 7, 12, 15, 19, 24, 19, 15, 12, 7, 3, 0, 3, 7, 3], "", (0, 8),
   [0, 0, 7, 0, 3] + [0] * 11, "PRPPR", (), 16, 5, gate=0.35, scale=0b010110101101)
T_("Bass and Answer", [0, 0, 12, 0, 0, 10, 0, 7, 0, 0, 12, 0, 3, 5, 7, 10], "PRPPRPPPPRPPPPPR", (0, 8),
   [12, 15, 19, 0, 0, 0, 0, 0, 14, 15, 17, 19, 0, 0, 0, 0], "PPPRRRRRPPPTRRRR", (0, 8), gate=0.45)
T_("Thirty-Two", [0, 3, 7, 10, 12, 10, 7, 3, 0, 5, 8, 12, 15, 12, 8, 5], "", (0,),
   [2, 5, 9, 12, 14, 12, 9, 5, 7, 10, 14, 17, 19, 17, 14, 10], "", (0,), series=True, gate=0.5)
T_("Rest Machine", [0, 0, 12, 0, 7, 0, 0, 3, 0, 12, 0, 0, 5, 0, 10, 0], "PRPRPRRPPPRRPRPR", (0, 2, 9, 12), gate=0.2)
T_("Tied Line", [0, 2, 3, 5, 7, 7, 8, 7, 5, 3, 5, 7, 12, 12, 10, 7], "PTTTPTPTTTPTPTTR", (0, 4, 12), gate=0.6)
write("Takt", presets, TI, TD)
