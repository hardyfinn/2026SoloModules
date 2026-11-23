"""Reference renders from the reviewed engine at the default setting (AGE 0.6, DRIVE 0.35) unless named."""
import numpy as np, sys, os
sys.path.insert(0, '.')
from sawstack_ref import *
os.makedirs('reference', exist_ok=True)
lin = np.linspace; secs = lambda s: int(s * SR)
def fade(x, t=0.03):
    n = secs(t); w = np.ones(len(x)); w[:n] = lin(0, 1, n); w[-n:] = lin(1, 0, n); return x * w[:, None]
def seq(notes, step, gate_frac=0.5, tail=1.0):
    n = secs(step * len(notes) + tail); p = np.zeros(n); g = np.zeros(n); last = 0.0
    for i, nt in enumerate(notes):
        a = secs(i * step); b = secs((i + 1) * step)
        if nt is not None: last = nt / 12.0; g[a:a + int((b - a) * gate_frac)] = 10.0
        p[a:] = last
    return p, g
P = {}
n = secs(16); sp = np.concatenate([np.zeros(secs(2)), lin(0, 1, secs(11)), np.ones(secs(3))])
P['01-spread-sweep-saw'] = lambda **k: render(np.full(n, -1.0), sp, 1.0, 0.0, np.ones(n), seed=3, **k)
P['02-spread-sweep-sine'] = lambda **k: render(np.full(n, 0.0), sp, 0.0, 0.0, np.ones(n), seed=4, **k)
P['03-spread-sweep-fifths'] = lambda **k: render(np.full(n, -1.0), sp, 0.6, 0.0, np.ones(n), seed=4, voicing=1, **k)
P['04-spread-sweep-harmonics'] = lambda **k: render(np.full(n, 0.585), sp, 0.0, 0.0, np.ones(n), seed=4, voicing=2, **k)
m = secs(12); tb = np.concatenate([np.zeros(secs(1)), lin(0, 1, secs(9)), np.ones(secs(2))])
P['05-timbre-sweep'] = lambda **k: render(np.full(m, -1.0), 0.28, tb, 0.0, np.ones(m), seed=5, **k)
rc = np.concatenate([np.zeros(secs(1)), lin(0, 1, secs(5)), lin(1, 0, secs(5)), np.zeros(secs(1))])
P['06-richness-sweep'] = lambda **k: render(np.full(m, -2.0), 0.2, 1.0, rc, np.ones(m), seed=6, **k)
def bass(**k):
    notes = [-24, None, -24, -12, -24, None, -21, -24, -24, None, -24, -12, -17, None, -19, -14] * 2
    p, g = seq(notes, 0.121, 0.55); age = k.get('age', 0.6)
    e2, _ = envelope(g, 0.002, 0.16, age, 1); e1, _ = envelope(g, 0.002, 0.11, age, 2)
    return render(p, 0.26, 1.0, 0.1 + 0.6 * e1, e2, seed=7, **k)
def lead(**k):
    notes = [0, 3, 7, 10, 12, 10, 7, 3, -2, 2, 5, 9, 12, 9, 5, 2]
    p, g = seq(notes, 0.5, 0.7, tail=2.5); age = k.get('age', 0.6)
    e2, _ = envelope(g, 0.02, 0.9, age, 3); e1, _ = envelope(g, 0.3, 0.6, age, 4)
    vel = np.repeat(np.random.default_rng(2).uniform(0.25, 1.0, len(notes) + 6), secs(0.5))[:len(p)]
    return render(p - 1, 0.3 + 0.05 * e1, vel * (0.4 + 0.6 * e2), 0.0, e2, seed=8, **k)
def plucks(**k):
    notes = [0, None, 7, 0, 3, None, 10, 7, 0, None, 5, 0, 3, 2, None, -2] * 2
    p, g = seq(notes, 0.14, 0.4, tail=1.5); age = k.get('age', 0.6)
    e2, _ = envelope(g, 0.002, 0.22, age, 5); e1, _ = envelope(g, 0.002, 0.08, age, 6)
    return render(p - 1, 0.93, 0.15 + 0.85 * e1, 0.0, e2, seed=10, **k)
def unison(**k):
    q = secs(12); s2 = np.concatenate([np.zeros(secs(6)), lin(0, 0.3, secs(4)), np.full(secs(2), 0.3)])
    return render(np.full(q, -1.0), s2, 1.0, 0.0, np.ones(q), seed=3, **k)
P['07-bass-env-to-richness'] = bass; P['08-chorale-lead'] = lead; P['09-octave-stack-plucks'] = plucks
P['10-unison-then-swarm'] = unison
gal = []; gap = np.zeros((secs(0.6), 2))
for name in sorted(P):
    x = fade(P[name]()); write_wav('reference/%s.wav' % name, x); gal += [x, gap]
    print(name, 'peak %.3f rms %.3f' % (np.abs(x).max(), np.sqrt((x ** 2).mean())), flush=True)
write_wav('reference/00-gallery.wav', np.concatenate(gal))
side = []
for name, fn in (('unison', unison), ('bass', bass), ('lead', lead)):
    for tag, age, drive in (('clean', 0.0, 0.0), ('default', 0.6, 0.35), ('full', 1.0, 0.75)):
        x = fade(fn(age=age, drive=drive)); write_wav('reference/age-%s-%s.wav' % (name, tag), x); side += [x, gap]
        print('age', name, tag, 'peak %.3f' % np.abs(x).max(), flush=True)
write_wav('reference/00-age-side-by-side.wav', np.concatenate(side))
