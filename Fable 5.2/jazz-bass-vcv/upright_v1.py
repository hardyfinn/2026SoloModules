"""Upright bass, pizzicato: design prototype (Fable, 2026-11-23).
A string as a bank of damped harmonics (levels and decay rates fitted to measurements of the CC0
Karoryfer Meatbass pizzicato samples), a one-sided fingerboard buzz, and a body of eight resonances
read off recordings of knocks on the same instrument. Pitch is continuous, so slides are real."""
import numpy as np
from scipy.signal import lfilter
SR = 48000
OPEN = [28, 33, 38, 43]                     # E1 A1 D2 G2

def hz(m): return 440.0 * 2 ** ((m - 69) / 12.0)

def biquad_peak(f, q, gdb):
    A = 10 ** (gdb / 40); w = 2 * np.pi * f / SR; al = np.sin(w) / (2 * q)
    b = [1 + al * A, -2 * np.cos(w), 1 - al * A]; a = [1 + al / A, -2 * np.cos(w), 1 - al / A]
    return np.array(b) / a[0], np.array(a) / a[0]
def biquad_hp(f, q):
    w = 2 * np.pi * f / SR; al = np.sin(w) / (2 * q); c = np.cos(w)
    b = [(1 + c) / 2, -(1 + c), (1 + c) / 2]; a = [1 + al, -2 * c, 1 - al]
    return np.array(b) / a[0], np.array(a) / a[0]
def biquad_lp(f, q):
    w = 2 * np.pi * f / SR; al = np.sin(w) / (2 * q); c = np.cos(w)
    b = [(1 - c) / 2, (1 - c), (1 - c) / 2]; a = [1 + al, -2 * c, 1 - al]
    return np.array(b) / a[0], np.array(a) / a[0]

# body: (Hz, Q, dB) from knock recordings; the 65 Hz air resonance is the big one
BODY = [(65, 9, 4), (104, 9, 3), (150, 8, 2.5), (200, 8, 2.5), (226, 9, 2), (268, 8, 2), (300, 8, 1.5), (440, 5, 2)]
# broad shape fitted from 321 measured partials (fit_body.py): strong 50-80 Hz, a hollow at 115-165 Hz
SHAPE = [(66, 1.6, 9.0), (138, 1.5, -5.0), (850, 1.2, 3.0)]

FC_LO, FC_HI, FC_EXP, FC_KEY, HP_F = 240.0, 600.0, 1.4, 0.9, 45.0   # fitted: fit_body.py
FAST_M, FAST_DB, RISE_K = 0.6, 110.0, 0.15

class Tone:
    def __init__(self, **kw):
        self.pluck = 0.17        # where the finger plucks, fraction of the string from the bridge
        self.bright = 0.5        # 0 gut, soft fingers .. 1 steel, hard fingers
        self.sustain = 0.5       # 0 short thud .. 1 long ring
        self.growl = 0.35        # string buzzing on the fingerboard
        self.body = 0.6          # body resonance amount (mic) ..
        self.pickup = 0.25       # .. against bridge pickup (direct string)
        self.age = 0.3           # dead strings, uneven notes, rattle, looser pitch
        self.thump = 0.5         # finger and body noise
        for k, v in kw.items(): setattr(self, k, v)

def note_string(pitch, hand):
    """which string and how far up it: stay near the hand (position = semitones above the nut)"""
    best = None
    for s, o in enumerate(OPEN):
        fret = pitch - o
        if fret < 0 or fret > 15: continue
        cost = abs(fret - hand) if fret > 0 else 1.5      # open strings are cheap
        cost += 0.4 * fret / 3.0 + (0.3 if s == 3 else 0)
        if best is None or cost < best[0]: best = (cost, s, fret)
    if best is None: return 0, max(0, pitch - OPEN[0])
    return best[1], best[2]

def render_note(tone, pitch, vel, dur, rng, fret=3, dead=False, scoop=0.0, cents=0.0, vib=0.0, tail=0.6):
    """vel 0..1, dur seconds until the hand damps it. Returns (string force, thump) before the body."""
    n = int((dur + tail) * SR); t = np.arange(n) / SR
    f0 = hz(pitch) * 2 ** (cents / 1200.0)
    # pitch: scoop from below, late vibrato
    pc = -scoop * np.exp(-t / 0.035)
    if vib > 0: pc = pc + vib * np.clip((t - 0.25) / 0.3, 0, 1) * np.sin(2 * np.pi * 5.2 * t)
    f = f0 * 2 ** (pc / 1200.0)
    ph = 2 * np.pi * np.cumsum(f) / SR
    K = int(min(48, 7000 / f0))
    k = np.arange(1, K + 1); fk = k * f0
    open_ = (fret == 0)
    age = tone.age
    # pluck spectrum: 1/k from the pluck shape, a null pattern from where it is plucked, and a finger low-pass
    fc = FC_LO * (FC_HI / FC_LO) ** (vel ** FC_EXP) * (f0 / 65.0) ** FC_KEY
    fc *= (0.6 + 0.9 * tone.bright) * (1 - 0.45 * age) * (1.25 if open_ else 1.0)
    beta = tone.pluck * (1 + 0.012 * fret)
    A = np.sqrt(np.sin(k * np.pi * beta) ** 2 + 0.06) / k * np.exp(-fk / fc)   # a finger is wide: no deep nulls
    A *= 1 + 0.25 * age * np.array([rng.gauss(0, 1) for _ in k]).clip(-2, 2)
    # decay in dB/s (measured: about 6 + 0.055 f, more high up the neck, less on open strings)
    sig = 5.0 + fk * (0.03 + 0.05 * min(1.0, max(0.0, (f0 - 40.0) / 110.0))) + 0.4 * fret
    sig *= (0.7 if open_ else 1.0) * 2 ** (1.6 * (0.5 - tone.sustain)) * (1 + 1.2 * age * fk / 1000.0)
    for bf, bq, bg in BODY[:3]:                                    # the body drinks energy at its resonances
        sig += 9.0 * tone.body * np.exp(-0.5 * ((fk - bf) / (bf / bq * 1.5)) ** 2)
    if dead: sig = sig * 0 + 260.0 + 0.5 * fk; A = A * np.exp(-fk / 500.0)
    rel = 170.0 + 0.25 * fk                                        # the left hand lets go
    sig = sig[:, None]; rel = rel[:, None]
    tt = t[None, :]
    # two-stage decay (measured): most of the pluck is gone in the first 100 ms, the rest rings on.
    # (the string's up-and-down motion drives the bridge hard and dies fast; its side-to-side motion lingers)
    mf = (FAST_M + 0.2 * np.minimum(1.0, fk / 1000.0))[:, None]
    env = ((1 - mf) * 10 ** (-(sig * tt) / 20.0) + mf * 10 ** (-((sig + FAST_DB) * tt) / 20.0)) * 10 ** (-(rel * np.clip(tt - dur, 0, None)) / 20.0)
    # the string leaves the finger over a few ms; low partials take longest
    rise = 1 - np.exp(-tt / (0.0015 + RISE_K / fk[:, None]))
    s = (A[:, None] * env * rise * np.sin(k[:, None] * ph[None, :] + 0.5 * (k[:, None] % 2))).sum(0)
    s *= vel ** 1.35 * 2.2
    # thump: finger leaving the string + the body being pushed
    th = np.zeros(n); m = int(0.03 * SR)
    noise = np.array([rng.gauss(0, 1) for _ in range(m)])
    th[:m] = noise * np.exp(-np.arange(m) / (0.004 * SR))
    b, a = biquad_lp(700 + 900 * tone.bright, 0.7); th = lfilter(b, a, th) * 0.5 * tone.thump * vel
    return s, th

def fingerboard(s, tone):
    """one-sided buzz: the string touches the board on its way down"""
    g = tone.growl + 0.5 * tone.age
    if g <= 0: return s
    thr = 0.9 - 0.75 * min(1, g)
    over = np.clip(s - thr, 0, None)
    return s - over * 0.85 + 0.0

def body(x, tone, thump):
    y = x + thump
    for q in (0.52, 0.71, 1.93):                                   # the body cannot radiate below its air resonance
        b, a = biquad_hp(HP_F, q); y = lfilter(b, a, y)
    for f, q, g in BODY:
        b, a = biquad_peak(f, q, g * tone.body / 0.6); y = lfilter(b, a, y)
    for f, q, g in SHAPE:
        b, a = biquad_peak(f, q, g * min(1.3, tone.body / 0.6)); y = lfilter(b, a, y)
    b, a = biquad_lp(2600, 0.7); y = lfilter(b, a, y)
    # pickup: the string itself, less body, more highs and lows
    b, a = biquad_hp(28, 0.7); p = lfilter(b, a, x + 0.3 * thump)
    b, a = biquad_lp(4200, 0.7); p = lfilter(b, a, p)
    return (1 - tone.pickup) * y + tone.pickup * p * 0.9

def play(notes, tone, seed=1, total=None):
    """notes: dicts with t, pitch, vel(0..1), dur, and optional dead, scoop, cents, vib."""
    import random
    rng = random.Random(seed)
    total = total or max(n['t'] + n['dur'] for n in notes) + 1.5
    out = np.zeros(int(total * SR)); th = np.zeros_like(out); hand = 2
    for n in notes:
        s_, fret = note_string(n['pitch'], hand)
        if fret > 0: hand = fret if abs(fret - hand) > 3 else hand
        x, tp = render_note(tone, n['pitch'], n['vel'], n['dur'], rng, fret=fret, dead=n.get('dead', False),
                            scoop=n.get('scoop', 0.0), cents=n.get('cents', 0.0), vib=n.get('vib', 0.0))
        i = int(n['t'] * SR); j = min(len(out), i + len(x))
        if i < 0 or i >= len(out): continue
        out[i:j] += x[:j - i]; th[i:j] += tp[:j - i]
    return body(fingerboard(out, tone), tone, th)
