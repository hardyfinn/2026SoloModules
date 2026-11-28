#!/usr/bin/env python3
# Demo audio: pitch, gate and push from the C++ engine (test/tool), played through the stand-in saw synth of
# tools/proto/bulgaria_proto.py. The tone is NOT the module's: Bulgaria makes no sound.
#   make demos            (writes MP3s to the folder given, default test/demos)
import os, sys, subprocess, tempfile
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
sys.path.insert(0, os.path.join(HERE, "proto"))
from bulgaria_proto import render, reverb, write, SR   # noqa: E402

OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "test", "demos"))
os.makedirs(OUT, exist_ok=True)
TOOL = os.path.join(ROOT, "test", "tool")
IDS = ["ROOT", "OCT", "MODE", "TUNE", "VOICES", "SPREAD", "RUB", "UNIT", "WHEN", "CHANCE", "CADENCE", "SLIDE", "SHAKE", "WHOOP", "LOOSE"]
PRESETS = []
for line in open(os.path.join(ROOT, "test", "presets.txt")):
    if line.startswith("#"): continue
    f = line.split()
    PRESETS.append((f[0].replace("_", " "), dict(zip(IDS, map(float, f[1:])))))

def seq(items, t=0.0, gatefrac=0.92):
    notes = []
    for semi, dur, *c in items:
        if semi is not None: notes.append((t, dur * gatefrac, semi, bool(c and c[0])))
        t += dur
    return notes, t

def events(notes, offset=0):
    ev = []
    for (t, d, s, cad) in notes:
        ev.append((t, (s + offset) / 12.0, 10, 0))
        if cad:
            ev.append((t + max(0.0, d - 0.06), (s + offset) / 12.0, 10, 10))
            ev.append((t + max(0.0, d - 0.05), (s + offset) / 12.0, 10, 0))
        ev.append((t + d, (s + offset) / 12.0, 0, 0))
    ev.sort(key=lambda e: e[0])
    return ev

def run(params, ev, secs, seed=1):
    with tempfile.TemporaryDirectory() as d:
        ef = os.path.join(d, "ev.txt"); of = os.path.join(d, "out.f32")
        with open(ef, "w") as f:
            for (t, v, g, ph) in ev: f.write("%.6f %.6f %g %g 0 0 0\n" % (t, v, g, ph))
        pv = " ".join("%g" % params[n] for n in IDS)
        subprocess.run([TOOL, pv, ef, of, "%.3f" % secs, str(seed)], check=True)
        a = np.fromfile(of, dtype=np.float32).reshape(-1, 8, 3)
    nv = int(params["VOICES"])
    pitch = a[:, :nv, 0].T.astype(float); gate = a[:, :nv, 1].T.astype(float); push = a[:, :nv, 2].T.astype(float) / 10.0
    return render(pitch, gate, push, 261.6256)

def norm(x): return x / (np.max(np.abs(x)) + 1e-12)

def cat(parts, gap=0.5):
    z = np.zeros((2, int(gap * SR))); o = []
    for p in parts: o += [p, z]
    return np.concatenate(o, axis=1)

def save(name, x):
    w = os.path.join(OUT, name + ".wav")
    write(w, reverb(x))
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", w, "-b:a", "192k", os.path.join(OUT, name + ".mp3")], check=True)
    os.remove(w)

def kick(T, step):
    N = int(T * SR); y = np.zeros(N); L = int(.28 * SR); t = np.arange(L) / SR
    k = np.sin(2 * np.pi * (48 * t + 14 * (1 - np.exp(-t / 0.03)))) * np.exp(-t / 0.09)
    for b in range(int(T / (step * 4)) + 1):
        i = int(b * step * 4 * SR)
        if i + L < N: y[i:i + L] += k
    return np.vstack([y, y])

TUNE = [(3, .45), (5, .45), (7, .9), (5, .3), (3, .3), (5, 1.5), (None, .35),
        (7, .45), (8, .3), (7, .3), (5, .6), (3, .45), (2, .45), (3, 1.9, 1), (None, .6),
        (5, .3), (7, .3), (5, .3), (3, .6), (2, .9), (0, .6), (2, 2.2, 1), (None, .6)]
S16 = 60 / 126 / 4
PAT = [0, None, 12, 0, 3, None, 0, 10, None, 0, 12, 1, 0, None, 7, 5]

def pattern():
    items = []
    for bar in range(4):
        for i, s in enumerate(PAT):
            items.append((s, S16) if not (i == 15 and bar % 2 == 1) else (s, S16, 1))
    n, T = seq(items, gatefrac=0.7)
    n = [(t, (d if (i % 5) else d * 2.2), s, c) for i, (t, d, s, c) in enumerate(n)]
    return n, T

def P(i, **kw):
    d = dict(PRESETS[i][1]); d.update(kw); return d

def plain(p): return dict(p, VOICES=1, TUNE=0, SLIDE=0, SHAKE=0, WHOOP=0, LOOSE=0)

def groove(p, seed):
    n, T = pattern()
    T += 0.4
    dry = norm(run(plain(p), events(n), T, seed)); wet = norm(run(p, events(n), T, seed))
    k = kick(T, S16) * 0.4
    return np.concatenate([dry * 0.8 + k[:, :dry.shape[1]], wet + k[:, :wet.shape[1]]], axis=1)

if __name__ == "__main__":
    notes, T = seq(TUNE); T += 0.4
    init = P(0)
    # scene 1: the idea
    save("scene-1-plain-then-three-parts", cat([norm(run(plain(init), events(notes), T)), norm(run(init, events(notes), T))]))
    # scene 2: rub in cents, then in Hz, on a rising line from C3
    rise, T2 = seq([(s, 1.1) for s in (-9, -5, 0, 3, 7, 12, 15)])
    base2 = dict(init, MODE=1, VOICES=2, LOOSE=0.1, SLIDE=0, SHAKE=0, WHOOP=0, OCT=-1)
    c = run(dict(base2, UNIT=0, RUB=(170 - 25) / 225), events(rise, -12), T2 + 0.3)
    h = run(dict(base2, UNIT=1, RUB=((32 - 1) / 79) ** (1 / 2.2)), events(rise, -12), T2 + 0.3)
    save("scene-2-rub-cents-then-hz", cat([norm(c), norm(h)]))
    # scene 3: ornaments and cadences
    orn = dict(init, SLIDE=0.6, SHAKE=0.8, WHOOP=1.0, LOOSE=0.8)
    save("scene-3-ornaments-tight-then-resolve", cat([norm(run(orn, events(notes), T, 3)), norm(run(dict(orn, CADENCE=1, WHOOP=0), events(notes), T, 3))]))
    # scene 4: eight-voice cluster over a kick at 126 bpm
    save("scene-4-eight-voice-cluster-126bpm", groove(P(15, ROOT=9, OCT=-1), 4))
    # one clip per preset
    for i, (name, p) in enumerate(PRESETS):
        fn = "preset-%02d-%s" % (i + 1, name.replace(" ", "_").replace("(", "").replace(")", ""))
        if 14 <= i <= 18: save(fn, groove(dict(p, ROOT=9, OCT=-1), 10 + i))
        else: save(fn, norm(run(p, events(notes), T, 10 + i)))
    print("ok", OUT)
