#!/usr/bin/env python3
# Writes src/KrkaShapes.hpp: Krka's own library of 260 envelope / function
# shapes in 26 banks (A-Z) of 10. Each shape is a list of breakpoints
# (time 0..1, value 0..1, curve of the segment that ends there).
#   curve -1..+1 : power bend of the segment (0 = straight line)
#   curve  2     : smooth (cosine) segment, for sines and bumps
# Two points at the same time make a vertical jump (steps, gates).
# Seeded, so the library is the same every time it is regenerated.
#   python3 res/make_shapes.py
import math, os, random, json

HERE = os.path.dirname(os.path.abspath(__file__))
MAXP = 40
S = 2.0  # smooth segment
banks = []  # (letter, family, [(name, points)])


def bank(letter, family, shapes):
    assert len(shapes) == 10, (letter, len(shapes))
    for n, p in shapes:
        assert 2 <= len(p) <= MAXP, (letter, n, len(p))
        assert abs(p[0][0]) < 1e-9 and abs(p[-1][0] - 1) < 1e-9, (letter, n)
        for a, b in zip(p, p[1:]):
            assert b[0] >= a[0] - 1e-9, (letter, n)
    banks.append((letter, family, shapes))


def ad(att, dc, ka=0.0, kd=-0.6):
    return [(0, 0, 0), (att, 1, ka), (1, 0, kd)]


def waves(cycles, start=0.0, decay=None, chirp=None, amp=None):
    # cosine-segmented wave between 0 and 1 with optional decay / chirp
    pts = [(0, 0.5 if start else 0.0, 0)]
    n = int(cycles * 2)
    times = []
    for i in range(1, n + 1):
        x = i / n
        if chirp:
            x = x ** chirp
        times.append(x)
    for i, x in enumerate(times):
        top = (i % 2 == 0)
        a = 1.0
        if decay:
            a = math.exp(-decay * x)
        if amp:
            a = amp(x)
        v = 0.5 + 0.5 * a if top else 0.5 - 0.5 * a
        if start == 0.0:
            v = a if top else 0.0
        pts.append((x, v, S))
    return pts


rng = random.Random(1973)

# A  attack-decay: the basic envelope, attack getting longer
A = []
for i, att in enumerate([0.005, 0.02, 0.05, 0.1, 0.2, 0.3, 0.45, 0.6, 0.8, 0.95]):
    A.append((f"AD {i}", ad(att, 1, -0.3, -0.6)))
bank("A", "Attack-decay", A)

# B  percussive: instant attack, decays from snappy to linear to slow
B = []
for i, k in enumerate([-0.95, -0.85, -0.7, -0.55, -0.4, -0.25, -0.1, 0.0, 0.3, 0.6]):
    B.append((f"Perc {i}", [(0, 0, 0), (0.002, 1, 0), (1, 0, k)]))
bank("B", "Percussive", B)

# C  plateau: rise, hold, release (an ADSR in one shape)
C = []
for i, (a, d, s, r) in enumerate([(0.02, 0.1, 0.7, 0.8), (0.02, 0.2, 0.5, 0.75), (0.05, 0.15, 0.8, 0.9),
                                   (0.1, 0.2, 0.6, 0.7), (0.2, 0.3, 0.6, 0.75), (0.01, 0.05, 0.9, 0.6),
                                   (0.3, 0.4, 0.4, 0.6), (0.05, 0.3, 0.3, 0.9), (0.4, 0.5, 0.85, 0.85), (0.02, 0.02, 1.0, 0.98)]):
    C.append((f"ADSR {i}", [(0, 0, 0), (a, 1, -0.3), (d, s, -0.5), (r, s, 0), (1, 0, -0.5)]))
bank("C", "Plateau (ADSR)", C)

# D  ramps up
D = []
for i, k in enumerate([0.0, 0.3, 0.6, 0.9, -0.3, -0.6, -0.9]):
    D.append((f"Rise {i}", [(0, 0, 0), (1, 1, k)]))
D.append(("Rise S", [(0, 0, 0), (1, 1, S)]))
D.append(("Rise late", [(0, 0, 0), (0.5, 0, 0), (1, 1, 0.4)]))
D.append(("Rise drop", [(0, 0, 0), (0.9, 1, 0), (1, 0, 0)]))
bank("D", "Ramps up", D)

# E  ramps down
E = []
for i, k in enumerate([0.0, 0.3, 0.6, 0.9, -0.3, -0.6, -0.9]):
    E.append((f"Fall {i}", [(0, 1, 0), (1, 0, k)]))
E.append(("Fall S", [(0, 1, 0), (1, 0, S)]))
E.append(("Fall late", [(0, 1, 0), (0.5, 1, 0), (1, 0, -0.4)]))
E.append(("Fall step", [(0, 1, 0), (0.5, 1, 0), (0.5, 0.5, 0), (1, 0, 0)]))
bank("E", "Ramps down", E)

# F  one-cycle LFO shapes
F = [("Sine", [(0, 0.5, 0), (0.25, 1, S), (0.75, 0, S), (1, 0.5, S)]),
     ("Cosine", [(0, 0, 0), (0.5, 1, S), (1, 0, S)]),
     ("Triangle", [(0, 0, 0), (0.5, 1, 0), (1, 0, 0)]),
     ("Tri lean", [(0, 0, 0), (0.25, 1, 0), (1, 0, 0)]),
     ("Tri lean back", [(0, 0, 0), (0.75, 1, 0), (1, 0, 0)]),
     ("Shark", [(0, 0, 0), (0.5, 1, -0.6), (1, 0, 0.6)]),
     ("Shark back", [(0, 0, 0), (0.5, 1, 0.6), (1, 0, -0.6)]),
     ("Parabola", [(0, 0, 0), (0.5, 1, -0.5), (1, 0, 0.5)]),
     ("Bulb", [(0, 0, 0), (0.5, 1, 0.5), (1, 0, -0.5)]),
     ("Bipolar tri", [(0, 0.5, 0), (0.25, 1, 0), (0.75, 0, 0), (1, 0.5, 0)])]
bank("F", "One-cycle LFOs", F)

# G  gates and pulses
G = []
for i, w in enumerate([0.01, 0.05, 0.1, 0.25, 0.5, 0.75, 0.9]):
    G.append((f"Gate {int(w * 100)}%", [(0, 1, 0), (w, 1, 0), (w, 0, 0), (1, 0, 0)]))
G.append(("Late gate", [(0, 0, 0), (0.5, 0, 0), (0.5, 1, 0), (1, 1, 0)]))
G.append(("Middle gate", [(0, 0, 0), (0.25, 0, 0), (0.25, 1, 0), (0.75, 1, 0), (0.75, 0, 0), (1, 0, 0)]))
G.append(("Two levels", [(0, 1, 0), (0.5, 1, 0), (0.5, 0.5, 0), (1, 0.5, 0)]))
bank("G", "Gates and pulses", G)


def stairs(n, up=True, ends=True):
    pts = [(0, 0 if up else 1, 0)]
    for i in range(1, n):
        x = i / n
        a = (i - 1) / (n - 1) if n > 1 else 0
        b = i / (n - 1)
        if not up:
            a, b = 1 - a, 1 - b
        pts.append((x, a, 0))
        pts.append((x, b, 0))
    pts.append((1, pts[-1][1], 0))
    return pts


# H  stairs up
H = [(f"Up {n}", stairs(n)) for n in (2, 3, 4, 5, 6, 7, 8, 12)]
H.append(("Up 3 and drop", [(0, 0, 0), (0.25, 0, 0), (0.25, 1 / 3, 0), (0.5, 1 / 3, 0), (0.5, 2 / 3, 0), (0.75, 2 / 3, 0), (0.75, 1, 0), (1, 1, 0)][:6] + [(0.75, 0, 0), (1, 0, 0)]))
H.append(("Pyramid", [(0, 0, 0), (0.2, 0, 0), (0.2, 0.5, 0), (0.4, 0.5, 0), (0.4, 1, 0), (0.6, 1, 0), (0.6, 0.5, 0), (0.8, 0.5, 0), (0.8, 0, 0), (1, 0, 0)]))
bank("H", "Stairs up", H)

# I  stairs down
I = [(f"Down {n}", stairs(n, up=False)) for n in (2, 3, 4, 5, 6, 7, 8, 12)]
I.append(("Down skip", [(0, 1, 0), (0.3, 1, 0), (0.3, 0.4, 0), (0.6, 0.4, 0), (0.6, 0.7, 0), (0.8, 0.7, 0), (0.8, 0, 0), (1, 0, 0)]))
I.append(("Down smooth", [(0, 1, 0), (0.25, 0.75, S), (0.5, 0.5, S), (0.75, 0.25, S), (1, 0, S)]))
bank("I", "Stairs down", I)

# J  double hits
J = []
for i, (gap, second) in enumerate([(0.15, 1), (0.25, 1), (0.35, 1), (0.5, 1), (0.25, 0.6), (0.5, 0.6), (0.15, 0.4), (0.3, 1.0), (0.6, 0.8), (0.4, 0.3)]):
    k = -0.6 if i < 7 else -0.2
    J.append((f"Double {i}", [(0, 0, 0), (0.005, 1, 0), (gap, 0.05, k), (gap + 0.005, second, 0), (1, 0, k)]))
bank("J", "Double hits", J)

# K  multi hits (flams to ratchets)
K = []
for i, n in enumerate([3, 4, 5, 6, 8, 3, 4, 5, 6, 8]):
    pts = [(0, 0, 0)]
    accel = i >= 5
    for j in range(n):
        t0 = (j / n) ** (0.7 if accel else 1.0)
        t1 = ((j + 1) / n) ** (0.7 if accel else 1.0)
        pts.append((t0 + 0.002, 1 - 0.5 * j / n if not accel else 0.5 + 0.5 * j / n, 0))
        pts.append((min(t1, 1), 0, -0.6))
    pts[-1] = (1, 0, -0.6)
    K.append((f"{'Roll' if accel else 'Hits'} {n}", pts))
bank("K", "Multi hits", K)

# L  bouncing ball
L = []
for i, (e, n) in enumerate([(0.5, 8), (0.6, 10), (0.7, 12), (0.75, 14), (0.8, 16), (0.4, 6), (0.85, 18), (0.65, 9), (0.55, 7), (0.9, 19)]):
    # bounce heights h_k = e^(2k); durations proportional to sqrt(h)
    hs = [e ** (2 * k) for k in range(n)]
    ds = [math.sqrt(h) for h in hs]
    tot = sum(ds)
    pts = [(0, 1, 0)]
    t = 0.0
    for k in range(n):
        h, d = hs[k], ds[k] / tot
        if k == 0:
            t += d / 2
            pts.append((t, 0, 0.6))
        else:
            pts.append((t + d / 2, h, -0.6))
            t += d
            pts.append((min(t, 1.0), 0, 0.6))
    pts[-1] = (1, 0, 0.6)
    L.append((f"Bounce {i}", pts))
bank("L", "Bouncing ball", L)

# M  swells (reverse envelopes)
M = []
for i, k in enumerate([0.95, 0.85, 0.7, 0.55, 0.4, 0.2]):
    M.append((f"Swell {i}", [(0, 0, 0), (0.995, 1, k), (1, 0, 0)]))
M.append(("Swell hold", [(0, 0, 0), (0.7, 1, 0.7), (0.9, 1, 0), (1, 0, 0)]))
M.append(("Swell soft end", [(0, 0, 0), (0.8, 1, 0.7), (1, 0, -0.3)]))
M.append(("Tape start", [(0, 0, 0), (0.3, 0.8, -0.8), (1, 1, -0.5)]))
M.append(("Reverse hit", [(0, 0, 0), (0.6, 0.3, 0.8), (0.97, 1, 0.8), (1, 0, 0)]))
bank("M", "Swells", M)

# N  pluck (attack + damped wobble)
N = []
for i, (cyc, dec) in enumerate([(2, 3), (3, 4), (4, 5), (5, 4), (6, 6), (8, 5), (3, 2), (10, 7), (6, 3), (14, 8)]):
    pts = [(0, 0, 0), (0.003, 1, 0)]
    n = cyc * 2
    for j in range(1, n + 1):
        x = 0.003 + (1 - 0.003) * j / n
        a = math.exp(-dec * x)
        v = 0.5 * a + (0.5 * a * (1 if j % 2 == 0 else -1)) * 0.6 + 0.5 * a * 0.4
        pts.append((x, max(0.0, min(1.0, v)), S))
    pts[-1] = (1, 0, S)
    N.append((f"Pluck {i}", pts))
bank("N", "Plucks", N)

# O  resonators: damped oscillation around the middle (for filter pings)
O = []
for i, (cyc, dec) in enumerate([(2, 2), (3, 3), (4, 3), (5, 4), (6, 5), (8, 4), (10, 6), (12, 5), (16, 8), (18, 3)]):
    O.append((f"Ring {i}", waves(cyc, start=0.5, decay=dec)))
bank("O", "Resonators", O)

# P  tremolo envelopes
P = []
for i, (cyc, depth) in enumerate([(4, 0.3), (6, 0.5), (8, 0.3), (8, 0.7), (12, 0.5), (16, 0.4), (5, 0.9), (10, 0.9), (3, 0.6), (18, 0.3)]):
    pts = [(0, 0, 0)]
    n = cyc * 2
    for j in range(1, n + 1):
        x = j / n
        env = math.exp(-2.5 * x) if i % 2 == 0 else math.sin(math.pi * x)
        top = j % 2 == 1
        v = env * (1.0 if top else 1 - depth)
        pts.append((x, v, S))
    pts[-1] = (1, 0, S)
    P.append((f"Tremolo {i}", pts))
bank("P", "Tremolo envelopes", P)

# Q  stutter (on/off bursts, irregular)
Q = []
for i in range(10):
    r = random.Random(100 + i)
    n = r.choice([4, 5, 6, 7, 8, 9])
    edges = [0.0] + sorted(r.uniform(0.05, 0.95) for _ in range(n - 1)) + [1.0]
    pts = [(0, 0, 0)]
    for a, b in zip(edges, edges[1:]):
        if b - a < 0.02 or r.random() > 0.75:
            continue
        lvl = r.choice([1.0, 0.8, 0.6, 0.4])
        off = a + (b - a) * r.uniform(0.3, 0.9)
        pts += [(a, 0, 0), (a, lvl, 0), (off, lvl, 0), (off, 0, 0)]
    pts.append((1, 0, 0))
    Q.append((f"Stutter {i}", pts))
bank("Q", "Stutters", Q)

# R  random walks
R = []
for i in range(10):
    r = random.Random(200 + i)
    n = 12 + i * 2
    v = 0.5
    pts = [(0, 0.5, 0)]
    for j in range(1, n + 1):
        v = max(0.0, min(1.0, v + r.gauss(0, 0.18 + 0.02 * i)))
        pts.append((j / n, v, 0))
    R.append((f"Walk {i}", pts))
bank("R", "Random walks", R)

# S  smooth random
Sx = []
for i in range(10):
    r = random.Random(300 + i)
    n = 4 + i * 2
    pts = [(0, r.random(), 0)]
    for j in range(1, n + 1):
        pts.append((j / n, r.random(), S))
    Sx.append((f"Smooth {i}", pts))
bank("S", "Smooth random", Sx)

# T  sample and hold steps
T = []
for i in range(10):
    r = random.Random(400 + i)
    n = 4 + i * 2 if i < 9 else 19
    pts = [(0, r.random(), 0)]
    for j in range(1, n):
        x = j / n
        pts.append((x, pts[-1][1], 0))
        pts.append((x, r.random(), 0))
    pts.append((1, pts[-1][1], 0))
    T.append((f"S&H {i}", pts))
bank("T", "Sample and hold", T)

# U  spikes and glitches
U = []
for i in range(10):
    r = random.Random(500 + i)
    n = 2 + i
    pos = sorted(r.uniform(0.02, 0.95) for _ in range(n))
    pts = [(0, 0, 0)]
    for p in pos:
        if p <= pts[-1][0] + 0.02:
            continue
        pts.append((p, 0, 0))
        pts.append((p + 0.002, r.uniform(0.4, 1.0), 0))
        pts.append((p + 0.002 + r.uniform(0.005, 0.03), 0, -0.8))
    pts.append((1, 0, 0))
    U.append((f"Spikes {i}", pts))
bank("U", "Spikes", U)

# V  chirps: waves that speed up or slow down
V = []
for i, (cyc, ch) in enumerate([(4, 0.6), (6, 0.5), (8, 0.4), (12, 0.35), (16, 0.3), (4, 1.6), (6, 2.0), (8, 2.5), (12, 3.0), (10, 0.5)]):
    V.append((f"{'Faster' if ch < 1 else 'Slower'} {i}", waves(cyc, chirp=ch, decay=3 if i == 9 else None)))
bank("V", "Chirps", V)

# W  multi-cycle waves (LFO bursts)
W = []
for i, cyc in enumerate([2, 3, 4, 5, 6, 7, 8, 12, 16, 3.5]):
    W.append((f"Wave {cyc}", waves(cyc)))
bank("W", "Waves", W)

# X  S-curves and sigmoids
X = []
for i, k in enumerate([0.2, 0.4, 0.6, 0.8, 0.95]):
    X.append((f"Sigmoid {i}", [(0, 0, 0), (0.5, 0.5, k), (1, 1, -k)]))
for i, k in enumerate([0.2, 0.4, 0.6, 0.8, 0.95]):
    X.append((f"Inverse S {i}", [(0, 0, 0), (0.5, 0.5, -k), (1, 1, k)]))
bank("X", "S-curves", X)

# Y  humps
Y = []
for i, w in enumerate([0.05, 0.1, 0.2, 0.3, 0.45]):
    pts = [(0, 0, 0)]
    for j in range(1, 17):
        x = j / 16
        pts.append((x, math.exp(-((x - 0.5) / w) ** 2 / 2), S))
    pts[-1] = (1, pts[-1][1], S)
    Y.append((f"Bell {i}", pts))
Y.append(("Mesa", [(0, 0, 0), (0.2, 1, S), (0.8, 1, 0), (1, 0, S)]))
Y.append(("Early bell", [(0, 0, 0), (0.2, 1, S), (1, 0, S)]))
Y.append(("Late bell", [(0, 0, 0), (0.8, 1, S), (1, 0, S)]))
Y.append(("Camel", [(0, 0, 0), (0.25, 1, S), (0.5, 0.4, S), (0.75, 0.9, S), (1, 0, S)]))
Y.append(("Trapezoid", [(0, 0, 0), (0.33, 1, 0), (0.66, 1, 0), (1, 0, 0)]))
bank("Y", "Humps", Y)

# Z  organic: long breaths with flutter, for slow evolving moves
Z = []
for i in range(10):
    r = random.Random(600 + i)
    n = 24 + (i % 4) * 4
    pts = [(0, 0, 0)]
    peak = r.uniform(0.3, 0.7)
    for j in range(1, n + 1):
        x = j / n
        base = math.sin(math.pi * min(1, x / (2 * peak))) if x < peak else math.cos(math.pi / 2 * (x - peak) / (1 - peak))
        base = max(0.0, base)
        v = max(0.0, min(1.0, base * (1 + r.gauss(0, 0.06 + 0.01 * i))))
        pts.append((x, v, S))
    pts[-1] = (1, 0, S)
    Z.append((f"Breath {i}", pts))
bank("Z", "Breaths", Z)

assert len(banks) == 26

# ---- emit header
def lit(x):
    t = "%.5g" % x
    if "e" in t:
        t = "%.7f" % x
    return t if "." in t else t + ".0"


out = ["// Generated by res/make_shapes.py - do not edit by hand.",
       "// Krka's own shape library: 26 banks x 10 shapes.",
       "#pragma once", "", "namespace krka {", "",
       f"static const int NUM_BANKS = 26, SHAPES_PER_BANK = 10, NUM_SHAPES = 260, MAX_POINTS = {MAXP};",
       "struct ShapePoint { float t, v, k; };",
       "struct ShapeDef { const char* name; int n; ShapePoint p[MAX_POINTS]; };", "",
       "static const char* const BANK_NAMES[NUM_BANKS] = {"]
out.append("\t" + ", ".join(json.dumps(f) for _, f, _ in banks) + "};")
out.append("")
out.append("static const ShapeDef SHAPES[NUM_SHAPES] = {")
dump = []
for letter, fam, shapes in banks:
    out.append(f"\t// {letter}: {fam}")
    for name, pts in shapes:
        ps = ", ".join("{%sf, %sf, %sf}" % (lit(p[0]), lit(min(1, max(0, p[1]))), lit(p[2])) for p in pts)
        out.append(f"\t{{{json.dumps(name)}, {len(pts)}, {{{ps}}}}},")
        dump.append({"bank": letter, "name": name, "points": [[round(p[0], 5), round(min(1, max(0, p[1])), 5), p[2]] for p in pts]})
out.append("};")
out.append("")
out.append("} // namespace krka")
open(os.path.join(HERE, "..", "src", "KrkaShapes.hpp"), "w").write("\n".join(out) + "\n")
json.dump(dump, open(os.path.join(HERE, "shapes.json"), "w"))
print("wrote", sum(len(s) for _, _, s in banks), "shapes")
