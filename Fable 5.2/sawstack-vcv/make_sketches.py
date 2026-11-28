import numpy as np, sys
sys.path.insert(0, '.')
from sawstack_proto import *

def lin(a, b, n): return np.linspace(a, b, n)
def secs(s): return int(s * SR)
def fade(x, t=0.03):
    n = secs(t); w = np.ones(len(x)); w[:n] = lin(0, 1, n); w[-n:] = lin(1, 0, n)
    return x * w[:, None]

def seq(notes, step, gate_frac=0.5, tail=1.0):
    """notes: semitones rel. to C4 (None = rest). returns pitch_oct, gate arrays"""
    n = secs(step * len(notes) + tail); p = np.zeros(n); g = np.zeros(n); last = 0.0
    for i, nt in enumerate(notes):
        a = secs(i * step); b = secs((i + 1) * step)
        if nt is not None:
            last = nt / 12.0; g[a:a + int((b - a) * gate_frac)] = 1
        p[a:] = last
    return p, g

out = {}
# 01 SPREAD sweep, full saw, held note C3
n = secs(20); sp = np.concatenate([np.zeros(secs(2)), lin(0, 1, secs(15)), np.ones(secs(3))])
out['01-spread-sweep-saw'] = render(np.full(n, -1.0), sp, 1.0, 0.0, np.ones(n), seed=3)
# 02 SPREAD sweep with TIMBRE at sine: five sines fan out to octaves (additive / organ)
out['02-spread-sweep-sine'] = render(np.full(n, 0.0), sp, 0.0, 0.0, np.ones(n), seed=4)
# 03 TIMBRE sweep sine -> saw at a light swarm detune
n = secs(12); tb = np.concatenate([np.zeros(secs(1)), lin(0, 1, secs(9)), np.ones(secs(2))])
out['03-timbre-sweep'] = render(np.full(n, -1.0), 0.28, tb, 0.0, np.ones(n), seed=5)
# 04 RICHNESS sweep, unison then light detune
n = secs(14); rc = np.concatenate([np.zeros(secs(1)), lin(0, 1, secs(6)), lin(1, 0, secs(6)), np.zeros(secs(1))])
spd = np.concatenate([np.zeros(secs(7)), np.full(secs(7), 0.3)])
out['04-richness-sweep'] = render(np.full(n, -2.0), spd, 1.0, rc, np.ones(n), seed=6)
# 05 swarm bass line: env2 -> amplitude, env1 -> richness (the normalled patch)
notes = [-24, None, -24, -12, -24, None, -21, -24, -24, None, -24, -12, -17, None, -19, -14] * 2
p, g = seq(notes, 0.125 * 60 / 62 * 2 / 2, 0.55)
e2, _ = envelope(g, 0.002, 0.16); e1, _ = envelope(g, 0.002, 0.11)
out['05-swarm-bass-env-to-richness'] = render(p, 0.26, 1.0, 0.1 + 0.6 * e1, e2, seed=7)
# 06 slow lead, light swarm, velocity -> timbre feel, long falls
notes = [0, 3, 7, 10, 12, 10, 7, 3, -2, 2, 5, 9, 12, 9, 5, 2]
p, g = seq(notes, 0.5, 0.7, tail=2.5)
e2, _ = envelope(g, 0.02, 0.9); e1, _ = envelope(g, 0.3, 0.6)
vel = np.repeat(np.random.default_rng(2).uniform(0.25, 1.0, len(notes) + 6), secs(0.5))[:len(p)]
out['06-chorale-lead'] = render(p - 1, 0.3 + 0.05 * e1, vel * (0.4 + 0.6 * e2), 0.0, e2, seed=8)
# 07 looping envelope (end pulse patched back to gate) sweeping SPREAD: octave fans in and out
n = secs(16); gate = np.zeros(n); e = np.zeros(n)
per = secs(4.0)
tri = np.abs(((np.arange(n) / per) % 1.0) * 2 - 1); e = 1 - tri
out['07-looped-envelope-to-spread'] = render(np.full(n, -1.0), 0.15 + 0.85 * e, 0.55, 0.0, np.ones(n), seed=9)
# 08 pluck sequence at wide spread (stack of octaves), short falls, timbre opening on the envelope
notes = [0, None, 7, 0, 3, None, 10, 7, 0, None, 5, 0, 3, 2, None, -2] * 2
p, g = seq(notes, 0.14, 0.4, tail=1.5)
e2, _ = envelope(g, 0.002, 0.22); e1, _ = envelope(g, 0.002, 0.08)
out['08-octave-stack-plucks'] = render(p - 1, 0.93, 0.15 + 0.85 * e1, 0.0, e2, seed=10)
# 09 everything moving: drone, spread, timbre and richness under slow LFOs
n = secs(24); tt = np.arange(n) / SR
sp = 0.45 + 0.4 * np.sin(2 * np.pi * tt / 19 - 1.2); tb = 0.5 + 0.5 * np.sin(2 * np.pi * tt / 11)
rc = np.clip(0.35 * np.sin(2 * np.pi * tt / 7.3), 0, 1)
out['09-drone-all-moving'] = render(np.full(n, -2.0), sp, tb, rc, np.ones(n), seed=11)

gal = []
for k in sorted(out):
    x = fade(out[k]); out[k] = x
    pk = np.abs(x).max(); print(k, round(len(x) / SR, 1), 's peak', round(pk, 3), 'rms', round(float(np.sqrt((x**2).mean())), 3))
    write_wav('sketches/%s.wav' % k, x)
    gal += [x, np.zeros((secs(0.7), 2))]
write_wav('sketches/00-gallery.wav', np.concatenate(gal))
