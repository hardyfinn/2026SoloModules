#!/usr/bin/env python3
# Horo demo audio. The C++ Horo engine (test/horo_tool) plays stand-in drums: kick on BAR, low tom on LONG, a tick on
# every PULSE at the ACCENT level. Where Bulgaria joins, a tune advances on each BEAT and Horo's PHRASE is patched to
# Bulgaria's PHRASE; the C++ Bulgaria engine (test/tool) plays through the stand-in saw synth. Neither module makes
# sound itself: the tone here is not the modules'.
#   make horo-demos
import os, sys, subprocess, tempfile
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
sys.path.insert(0, HERE)
import make_demos as bd   # noqa: E402
from bulgaria_proto import reverb, write, SR   # noqa: E402

OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "test", "demos", "horo"))
os.makedirs(OUT, exist_ok=True)
TOOL = os.path.join(ROOT, "test", "horo_tool")
SLOT = {"PAIDUSHKO": 1, "RUCHENITSA": 2, "DAICHOVO": 5, "KOPANITSA": 7, "BUCHIMISH": 10}
COUNT = {1: 5, 2: 7, 5: 9, 7: 11, 10: 15}

def horo(segs, accent=0.9, bars=4, mode=0):
    with tempfile.TemporaryDirectory() as d:
        f = os.path.join(d, "segs.txt")
        open(f, "w").write("".join("%.6f %.4f %d %.3f\n" % s for s in segs))
        out = subprocess.run([TOOL, f, str(accent), str(bars), str(mode)], check=True, capture_output=True, text=True).stdout
    ev = {}
    for line in out.split("\n"):
        if not line: continue
        t, k, v = line.split()
        ev.setdefault(k, []).append((float(t), float(v)))
    return ev

def hit(y, t, x, gain=1.0):
    i = int(t * SR)
    if i >= y.shape[0]: return
    n = min(len(x), y.shape[0] - i)
    y[i:i + n] += gain * x[:n]

def drums(ev, T, tick=0.5, tom=0.6, kick=0.9):
    y = np.zeros(int(T * SR))
    L = int(0.3 * SR); t = np.arange(L) / SR
    k = np.sin(2 * np.pi * (50 * t + 16 * (1 - np.exp(-t / 0.03)))) * np.exp(-t / 0.11)
    tm = np.sin(2 * np.pi * (110 * t + 6 * (1 - np.exp(-t / 0.04)))) * np.exp(-t / 0.16)
    rng = np.random.default_rng(5)
    nz = rng.standard_normal(int(0.04 * SR)); tk = np.diff(np.concatenate([[0], nz])) * np.exp(-np.arange(len(nz)) / SR / 0.008)
    for (tt, _) in ev.get("BAR", []): hit(y, tt, k, kick)
    for (tt, _) in ev.get("LONG", []): hit(y, tt, tm, tom)
    for (tt, a) in ev.get("PULSE", []): hit(y, tt, tk, tick * (a / 10.0) ** 1.6)
    return np.vstack([y, y])

TUNE = [3, 5, 7, 8, 7, 5, 3, 5, 3, 2, 3, 2]   # semitones from C, one per beat

def bulgaria_events(ev, T, tune=TUNE, offset=0, gatefrac=0.9):
    beats = [t for (t, _) in ev.get("BEAT", [])]
    phr = [t for (t, _) in ev.get("PHRASE", [])]
    out = []
    for i, t in enumerate(beats):
        nxt = beats[i + 1] if i + 1 < len(beats) else T
        s = tune[i % len(tune)] + offset
        out.append((t, s / 12.0, 10, 0))
        out.append((t + gatefrac * (nxt - t), s / 12.0, 0, 0))
    for t in phr:
        out.append((t + 0.0005, None, None, 10)); out.append((t + 0.004, None, None, 0))
    out.sort(key=lambda e: e[0])
    v, g = 0.0, 0
    res = []
    for (t, vv, gg, ph) in out:   # carry voct and gate through the phrase lines
        if vv is not None: v, g = vv, gg
        res.append((t, v, g, ph))
    return res

def norm(x): return x / (np.max(np.abs(x)) + 1e-12)

def fit(x, n): return x[:, :n] if x.shape[1] >= n else np.pad(x, ((0, 0), (0, n - x.shape[1])))

def save(name, x):
    w = os.path.join(OUT, name + ".wav")
    write(w, reverb(x, mix=0.1))
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", w, "-b:a", "192k", os.path.join(OUT, name + ".mp3")], check=True)
    os.remove(w)

if __name__ == "__main__":
    R = 8.0   # pulses a second (ruchenitsa bar 0.875 s)
    # 1: ruchenitsa, 8 bars straight then 8 bars LILT 0.5, tapan + Bulgaria preset 1, PHRASE linked
    bar = 7 / R
    T = 16 * bar + 1.5
    ev = horo([(8 * bar - 0.5 / R, R, 2, 0.0), (8 * bar + 0.5 / R, R, 2, 0.5), (1.5, R, 2, 0.5)])
    ev = {k: [e for e in v if e[0] < 16 * bar - 0.01] for k, v in ev.items()}
    voice = norm(bd.run(bd.P(0), bulgaria_events(ev, 16 * bar), T, 21))
    n = int(T * SR)
    save("horo-1-ruchenitsa-with-bulgaria", fit(voice, n) * 0.8 + fit(drums(ev, T), n) * 0.45)
    # 2: kopanitsa drums only, LILT 0, 1, -1
    bar = 11 / 10.0
    ev = horo([(4 * bar - 0.05, 10, 7, 0.0), (4 * bar, 10, 7, 1.0), (4 * bar + 0.05, 10, 7, -1.0)])
    T = 12 * bar + 0.5
    save("horo-2-lilt-off-on", norm(drums(ev, T, tick=0.7)))
    # 3: two bars each of five metres
    segs, t = [], 0.0
    names = ["PAIDUSHKO", "RUCHENITSA", "DAICHOVO", "KOPANITSA", "BUCHIMISH"]
    for i, nm in enumerate(names):
        d = 2 * COUNT[SLOT[nm]] / 9.0
        segs.append((d - (0.5 / 9 if i == 0 else 0), 9.0, SLOT[nm], 0.25))
    segs[-1] = (segs[-1][0] + 0.5 / 9 + 0.6, 9.0, SLOT["BUCHIMISH"], 0.25)
    ev = horo(segs, bars=2)
    T = sum(s[0] for s in segs)
    ev = {k: [e for e in v if e[0] < T - 0.6] for k, v in ev.items()}
    save("horo-3-metres", norm(drums(ev, T, tick=0.7)))
    # 4: 7 + 7 + 11 chained by METRE CV, Bulgaria preset 16 (Techno Cluster), kick on BAR
    R = 126 / 60 * 4   # sixteenths at 126 bpm
    seq = [2, 2, 7] * 3
    segs = []
    for i, s in enumerate(seq):
        d = COUNT[s] / R
        segs.append((d - (0.5 / R if i == 0 else 0), R, s, 0.1))
    T = sum(s[0] for s in segs) + 0.5 / R
    segs.append((1.2, R, 2, 0.1))
    ev = horo(segs, accent=1, bars=3)
    ev = {k: [e for e in v if e[0] < T - 0.01] for k, v in ev.items()}
    p = bd.P(15, ROOT=9, OCT=-1)
    tune = [0, 3, 7, 5, 0, 3, 8, 7, 3, 1, 0]
    voice = norm(bd.run(p, bulgaria_events(ev, T, tune, gatefrac=0.55), T + 1.2, 24))
    n = int((T + 1.2) * SR)
    save("horo-4-chained", fit(voice, n) * 0.7 + fit(drums(ev, T + 1.2, tick=0.35, tom=0.3, kick=1.0), n) * 0.55)
    print("ok", OUT)
