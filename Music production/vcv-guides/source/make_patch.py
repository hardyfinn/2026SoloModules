#!/usr/bin/env python3
"""Build the house-in-F-minor example patch as a VCV Rack 2 .vcv file.

Long Form + The Changes + Weighted + Mixxa, with drums from the
Francois Markovian plugin (Markovian -> Interlock -> Rewrite -> Governor)
and VCV Fundamental voices. Writes a Rack 2 .vcv (tar.zst holding patch.json).
"""
import io, json, math, sys, tarfile, time
import zstandard

FUND = ("Fundamental", "2.6.0")
CORE = ("Core", "2.6.4")
WT = ("Weighted", "2.6.0")
LFM = ("LongFormMixxa", "2.1.0")
FMK = ("FrancoisMarkovian", "2.5.0")

modules, cables = [], []
_next = [1]


def mod(plugin, model, x, row, params=None, data=None):
    mid = _next[0]
    _next[0] += 1
    m = {"id": mid, "plugin": plugin[0], "model": model, "version": plugin[1],
         "params": [{"id": k, "value": float(v)} for k, v in sorted((params or {}).items())],
         "pos": [x, row]}
    if data is not None:
        m["data"] = data
    modules.append(m)
    return mid


COL = {"clock": "#f3374b", "link": "#ffb437", "harm": "#c94cf5", "pitch": "#00b56e",
       "gate": "#3695ef", "trig": "#ff8a00", "act": "#4cc95a", "cv": "#8b4ade", "audio": "#f2f2f2"}


def cable(om, oid, im, iid, kind):
    cables.append({"id": len(cables) + 1, "outputModuleId": om, "outputId": oid,
                   "inputModuleId": im, "inputId": iid, "color": COL[kind]})


# ---- helpers for Fundamental knob values ----
C4 = 261.6256
def vco_semi(hz): return 12 * math.log2(hz / C4)
def vcf_freq(hz): return (math.log2(hz / (C4 / 32))) / 10
def adsr_t(sec): return max(0.0, math.log10(sec * 1000) / 4)      # 1 ms .. 10 s
def delay_t(sec): return math.log10(sec * 1000) / 4

BPM = 124
SIXTEENTH_HZ = BPM / 60 * 4

# ---------------- row 0: clock, form, chords, melody ----------------
clock = mod(FUND, "LFO", 0, 0, {2: math.log2(SIXTEENTH_HZ), 0: 1})          # unipolar square = 16ths
lf = mod(LFM, "LongForm", 9, 0, {18: 0.15})                                    # DRIFT 15%
ch = mod(WT, "TheChanges", 61, 0, {0: 7, 1: 11, 2: 0, 3: 1, 4: 2, 5: 1.0, 6: 0.5, 7: 0.3, 8: 0.2})
wt = mod(WT, "Weighted", 89, 0, {5: 0.5, 6: 2, 13: 1, 18: 0.5})               # STEP 50%, RANGE 2, OCT +1, PULL +50%
rnd = mod(FUND, "Random", 109, 0, {})

# ---------------- row 1: Francois Markovian drum brain ----------------
NS = 8
def straight():
    return [[1.0 if c == (r + 1) % NS else 0.0 for c in range(NS)] for r in range(NS)]

mx = straight()
my = straight()
my[3] = [0, 0, 0, 0.3, 0.7, 0, 0, 0]          # step 4 sometimes repeats (a roll)
my[7] = [0.6, 0, 0, 0, 0.4, 0, 0, 0]          # step 8 sometimes jumps back to the half-bar
my[5] = [0, 0, 0, 0, 0, 0, 0.75, 0.25]        # step 6 sometimes skips ahead

#        steps:  1    2     3    4     5    6     7    8
cx = [[1.0, 0,    0,   0,    1.0, 0,    0,   0],     # A kick: four on the floor
      [0,   0,    1.0, 0,    0,   0,    1.0, 0.25],  # B bass: off-beat 8ths
      [0,   0.3,  1.0, 0.3,  0,   0.3,  1.0, 0.3],   # C hats: open off-beats, light 16ths
      [0,   0,    0,   0,    1.0, 0,    0,   0],     # D clap: beats 2 and 4
      [0,   0.2,  0,   0.4,  0,   0,    0.3, 0.35],  # E perc
      [0] * 8]                                       # F unused
cy = [[1.0, 0,    0,   0,    1.0, 0,    0,   0.2],
      [0,   0.2,  1.0, 0.3,  0,   0.2,  1.0, 0.5],
      [0.5, 0.8,  1.0, 0.8,  0.5, 0.8,  1.0, 0.8],
      [0,   0,    0,   0.15, 1.0, 0,    0,   0.3],
      [0.2, 0.5,  0.2, 0.7,  0.2, 0.5,  0.6, 0.7],
      [0] * 8]
fm_data = {"snapshots": [
    {"matrix": [v for row in mx for v in row], "chances": cx},
    {"matrix": [v for row in my for v in row], "chances": cy}],
    "edit": 0, "freeze": False}
fm = mod(FMK, "FrancoisMarkovian", 0, 1, {1: 0, 2: 1, 3: 0}, fm_data)      # DIV /2 = 8ths

grid = [0.0] * 36
def g(a, b, v): grid[a * 6 + b] = v
g(0, 4, 0.4)    # kick pushes perc
g(3, 2, -0.6)   # clap holds back the hat after it
g(2, 2, -0.3)   # hats damp their own rolls
g(4, 1, 0.3)    # perc adds ghost bass notes
il = mod(FMK, "Interlock", 34, 1, {0: 0.7, 1: 1}, {"grid": grid})

rw_params = {24: 1, 27: 1}                     # lock A (kick) and D (clap)
rw = mod(FMK, "Rewrite", 50, 1, rw_params)
gov = mod(FMK, "Governor", 78, 1, {0: 0.0, 1: 0.3, 4: 2})                  # density target from Long Form energy, window 32

# ---------------- row 2: drum voices (Fundamental) ----------------
kvco = mod(FUND, "VCO", 0, 2, {2: vco_semi(48), 4: 0.35})
kenv = mod(FUND, "ADSR", 9, 2, {0: 0, 1: adsr_t(0.2), 2: 0, 3: adsr_t(0.22)})
kvca = mod(FUND, "VCA-1", 18, 2, {0: 1.0})
noise = mod(FUND, "Noise", 21, 2)
hvcf = mod(FUND, "VCF", 24, 2, {0: vcf_freq(8000), 2: 0.15})
henv = mod(FUND, "ADSR", 31, 2, {0: 0, 1: adsr_t(0.03), 2: 0, 3: adsr_t(0.035)})
hvca = mod(FUND, "VCA-1", 40, 2, {0: 0.6})
cvcf = mod(FUND, "VCF", 43, 2, {0: vcf_freq(1200), 2: 0.35})
cenv = mod(FUND, "ADSR", 50, 2, {0: 0, 1: adsr_t(0.1), 2: 0, 3: adsr_t(0.12)})
cvca = mod(FUND, "VCA-1", 59, 2, {0: 0.8})
pvco = mod(FUND, "VCO", 62, 2, {2: vco_semi(880), 4: 0.2})
penv = mod(FUND, "ADSR", 71, 2, {0: 0, 1: adsr_t(0.05), 2: 0, 3: adsr_t(0.06)})
pvca = mod(FUND, "VCA-1", 80, 2, {0: 0.6})

# ---------------- row 3: bass, pad and lead voices ----------------
bvco = mod(FUND, "VCO", 0, 3, {2: 0})
bvcf = mod(FUND, "VCF", 9, 3, {0: vcf_freq(300), 2: 0.3, 3: 0.25})
benv = mod(FUND, "ADSR", 16, 3, {0: 0, 1: adsr_t(0.15), 2: 0, 3: adsr_t(0.16)})
bvca = mod(FUND, "VCA-1", 25, 3, {0: 0.9})
avco = mod(FUND, "VCO", 28, 3, {2: 0})
avcf = mod(FUND, "VCF", 37, 3, {0: vcf_freq(1100), 2: 0.2, 3: 0.12})
aenv = mod(FUND, "ADSR", 44, 3, {0: adsr_t(0.25), 1: adsr_t(0.6), 2: 0.8, 3: adsr_t(0.45)})
avca = mod(FUND, "VCA-1", 53, 3, {0: 0.3})
lvco = mod(FUND, "VCO", 56, 3, {2: 0, 5: 0.35})
lvcf = mod(FUND, "VCF", 65, 3, {0: vcf_freq(900), 2: 0.35, 3: 0.3})
lenv = mod(FUND, "ADSR", 72, 3, {0: 0, 1: adsr_t(0.18), 2: 0, 3: adsr_t(0.2)})
lvca = mod(FUND, "VCA-1", 81, 3, {0: 0.7})
dly = mod(FUND, "Delay", 84, 3, {0: delay_t(60 / BPM * 0.75), 1: 0.35, 2: 0.4, 3: 0.25})

# ---------------- row 4: mixer and audio ----------------
mx_params = {}
LEVEL, PAN, DUCK = 0, 8, 16
for i, (lvl, pan, duck) in enumerate([(0.85, 0, 0), (0.78, 0, 0.5), (0.6, 0.25, 0.2), (0.7, -0.1, 0.2),
                                      (0.6, -0.3, 0.2), (0.75, 0, 0.6), (0.72, 0.15, 0.3), (0.0, 0, 0)]):
    mx_params[LEVEL + i] = lvl
    mx_params[PAN + i] = pan
    mx_params[DUCK + i] = duck
mx_params[50] = 0.3          # GLUE (DICE 40 COUNT 41 BACK 42 KICK 43 SCENES 44-47 RELEASE 48 SHAPE 49 GLUE 50 MASTER 51)
mix = mod(LFM, "Mixxa", 0, 4, mx_params)
audio = mod(CORE, "AudioInterface2", 58, 4)

# ---------------- cables ----------------
# clock (LFO square out = 3)
for m in (lf, fm, il, rw, gov):
    cable(clock, 3, m, 0, "clock")
# Long Form outs: LANE 0-7, ACT 8, ENERGY 9, RAMP 10, FILL 11, CHANGE 12, BAR 13, PHRASE 14, END 15, LINK 16
cable(lf, 16, ch, 9, "link")        # LINK -> The Changes LINK
cable(lf, 16, mix, 22, "link")      # LINK -> Mixxa LINK
cable(lf, 8, il, 10, "act")         # ACT -> Interlock ACT IN (lanes 1-6 = A-F)
cable(lf, 9, gov, 2, "cv")          # ENERGY -> Governor density TARGET
cable(lf, 13, wt, 2, "gate")        # BAR -> Weighted ACCENT
# The Changes outs: CHORD 0 BASS 1 ROOT 2 TOP 3 GATE 4 CHANGE 5 TENSION 6 HARM 7
cable(ch, 7, wt, 5, "harm")
cable(ch, 0, avco, 0, "pitch")
cable(ch, 4, aenv, 4, "gate")
cable(ch, 1, bvco, 0, "pitch")
cable(ch, 6, avcf, 0, "cv")         # TENSION opens the pad filter
# drum chain: FM TRIG poly 7 -> Interlock TRIG poly 9; Interlock TRIG 6 / ACT 7 -> Rewrite TRIG 7 / ACT 8
cable(fm, 7, il, 9, "trig")
cable(il, 6, rw, 7, "trig")
cable(il, 7, rw, 8, "act")
cable(rw, 6, gov, 10, "trig")
cable(rw, 7, gov, 11, "act")
# Governor: density CTRL 0 -> Markovian EVOLVE CV 2; variety SAME (UNDER 9) -> Rewrite NOW 3; VARIED (OVER 7) -> HOLD 4
cable(gov, 0, fm, 2, "cv")
cable(gov, 9, rw, 3, "gate")
cable(gov, 7, rw, 4, "gate")
# Markovian DIV (8ths) -> Weighted TRIG and Random TRIG
cable(fm, 6, wt, 1, "clock")
cable(fm, 6, rnd, 2, "clock")
cable(rnd, 0, wt, 0, "cv")
# Weighted -> lead
cable(wt, 0, lvco, 0, "pitch")
cable(wt, 1, lenv, 4, "gate")
# Rewrite track outs -> drum and bass envelopes
cable(rw, 0, kenv, 4, "trig")
cable(rw, 1, benv, 4, "trig")
cable(rw, 2, henv, 4, "trig")
cable(rw, 3, cenv, 4, "trig")
cable(rw, 4, penv, 4, "trig")
# kick
cable(kenv, 0, kvco, 1, "cv")
cable(kenv, 0, kvca, 0, "cv")
cable(kvco, 0, kvca, 1, "audio")
# hats and clap from one noise
cable(noise, 0, hvcf, 3, "audio")
cable(noise, 0, cvcf, 3, "audio")
cable(hvcf, 1, hvca, 1, "audio")
cable(henv, 0, hvca, 0, "cv")
cable(cvcf, 1, cvca, 1, "audio")
cable(cenv, 0, cvca, 0, "cv")
# perc
cable(pvco, 1, pvca, 1, "audio")
cable(penv, 0, pvca, 0, "cv")
# bass
cable(bvco, 2, bvcf, 3, "audio")
cable(bvcf, 0, bvca, 1, "audio")
cable(benv, 0, bvca, 0, "cv")
cable(benv, 0, bvcf, 0, "cv")
# pad
cable(avco, 2, avcf, 3, "audio")
cable(avcf, 0, avca, 1, "audio")
cable(aenv, 0, avca, 0, "cv")
# lead
cable(lvco, 3, lvcf, 3, "audio")
cable(lvcf, 0, lvca, 1, "audio")
cable(lenv, 0, lvca, 0, "cv")
cable(lenv, 0, lvcf, 0, "cv")
cable(lvca, 0, dly, 4, "audio")
# into Mixxa: 1 kick, 2 bass, 3 hats, 4 clap, 5 perc, 6 chords, 7 lead
for src, chn in ((kvca, 0), (bvca, 1), (hvca, 2), (cvca, 3), (pvca, 4), (avca, 5)):
    cable(src, 0, mix, chn, "audio")
cable(dly, 0, mix, 6, "audio")
cable(mix, 0, audio, 0, "audio")
cable(mix, 1, audio, 1, "audio")

patch = {"version": "2.6.4", "zoom": 0.8, "gridOffset": [-1.0, -0.5],
         "modules": modules, "cables": cables}

out = sys.argv[1]
js = json.dumps(patch, indent=2).encode()
buf = io.BytesIO()
with tarfile.open(fileobj=buf, mode="w", format=tarfile.PAX_FORMAT) as tf:
    def add(name, data=None, d=False):
        ti = tarfile.TarInfo(name)
        ti.mtime = int(time.time())
        if d:
            ti.type = tarfile.DIRTYPE
            ti.mode = 0o755
            tf.addfile(ti)
        else:
            ti.size = len(data)
            ti.mode = 0o644
            tf.addfile(ti, io.BytesIO(data))
    add(".", d=True)
    add("./modules", d=True)
    add("./patch.json", js)
open(out, "wb").write(zstandard.ZstdCompressor(level=19).compress(buf.getvalue()))
open(out + ".json", "wb").write(js)
print(len(modules), "modules,", len(cables), "cables ->", out)
