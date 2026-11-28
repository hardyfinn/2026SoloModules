import numpy as np, sys, os
sys.path.insert(0, '.')
from sawstack_proto2 import *
os.makedirs('analog', exist_ok=True)
lin = np.linspace
secs = lambda s: int(s * SR)
def fade(x, t=0.03):
    n = secs(t); w = np.ones(len(x)); w[:n] = lin(0, 1, n); w[-n:] = lin(1, 0, n); return x * w[:, None]
def seq(notes, step, gate_frac=0.5, tail=1.0):
    n = secs(step * len(notes) + tail); p = np.zeros(n); g = np.zeros(n); last = 0.0
    for i, nt in enumerate(notes):
        a = secs(i * step); b = secs((i + 1) * step)
        if nt is not None: last = nt / 12.0; g[a:a + int((b - a) * gate_frac)] = 1
        p[a:] = last
    return p, g

SETTINGS = [('1-as-before', 0.25, 0.0), ('2-aged', 0.6, 0.35), ('3-tired-and-driven', 1.0, 0.75)]

def unison(age, drive):      # held unison note, then a slow move into light swarm
    n = secs(12); sp = np.concatenate([np.zeros(secs(6)), lin(0, 0.3, secs(4)), np.full(secs(2), 0.3)])
    return render(np.full(n, -1.0), sp, 1.0, 0.0, np.ones(n), age, drive, seed=3)
def sine_stack(age, drive):  # sines, spread sweep to the octave stack
    n = secs(12); sp = np.concatenate([np.zeros(secs(1)), lin(0, 1, secs(8)), np.ones(secs(3))])
    return render(np.full(n, 0.0), sp, 0.0, 0.0, np.ones(n), age, drive, seed=4)
def richness(age, drive):
    n = secs(10); rc = np.concatenate([lin(0, 1, secs(5)), lin(1, 0, secs(5))])
    return render(np.full(n, -2.0), 0.2, 1.0, rc, np.ones(n), age, drive, seed=6)
def bass(age, drive):
    notes = [-24, None, -24, -12, -24, None, -21, -24, -24, None, -24, -12, -17, None, -19, -14] * 2
    p, g = seq(notes, 0.121, 0.55)
    e2, _ = envelope(g, 0.002, 0.16, age, 1); e1, _ = envelope(g, 0.002, 0.11, age, 2)
    return render(p, 0.26, 1.0, 0.1 + 0.6 * e1, e2, age, drive, seed=7)
def lead(age, drive):
    notes = [0, 3, 7, 10, 12, 10, 7, 3, -2, 2, 5, 9, 12, 9, 5, 2]
    p, g = seq(notes, 0.5, 0.7, tail=2.5)
    e2, _ = envelope(g, 0.02, 0.9, age, 3); e1, _ = envelope(g, 0.3, 0.6, age, 4)
    vel = np.repeat(np.random.default_rng(2).uniform(0.25, 1.0, len(notes) + 6), secs(0.5))[:len(p)]
    return render(p - 1, 0.3 + 0.05 * e1, vel * (0.4 + 0.6 * e2), 0.0, e2, age, drive, seed=8)
def plucks(age, drive):
    notes = [0, None, 7, 0, 3, None, 10, 7, 0, None, 5, 0, 3, 2, None, -2] * 2
    p, g = seq(notes, 0.14, 0.4, tail=1.5)
    e2, _ = envelope(g, 0.002, 0.22, age, 5); e1, _ = envelope(g, 0.002, 0.08, age, 6)
    return render(p - 1, 0.93, 0.15 + 0.85 * e1, 0.0, e2, age, drive, seed=10)

PIECES = [('a-unison-then-swarm', unison), ('b-sine-stack', sine_stack), ('c-richness', richness),
          ('d-bass', bass), ('e-lead', lead), ('f-plucks', plucks)]
gap = np.zeros((secs(0.6), 2)); side = []; gal = {s[0]: [] for s in SETTINGS}
for pname, fn in PIECES:
    for sname, age, drive in SETTINGS:
        x = fade(fn(age, drive))
        print(pname, sname, 'peak %.3f rms %.3f' % (np.abs(x).max(), np.sqrt((x**2).mean())), flush=True)
        write_wav('analog/%s-%s.wav' % (pname, sname), x)
        side += [x, gap]; gal[sname] += [x, gap]
    side.append(gap)
write_wav('analog/00-side-by-side.wav', np.concatenate(side))
for sname in gal: write_wav('analog/00-gallery-%s.wav' % sname, np.concatenate(gal[sname]))
