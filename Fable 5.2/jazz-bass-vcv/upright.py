"""Upright bass, pizzicato: design prototype (Fable, 2026-11-23).
A string as a bank of damped harmonics (levels and decay rates fitted to measurements of the CC0
Karoryfer Meatbass pizzicato samples), a one-sided fingerboard buzz, and a body of eight resonances
read off recordings of knocks on the same instrument. Pitch is continuous, so slides are real."""
import numpy as np
from scipy.signal import lfilter, fftconvolve
import json, os
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
FAST_M, FAST_DB, RISE_K = 0.6, 80.0, 0.08
# the pluck (attack_check.py, fit_beat.py): a string let go is a sharp corner, so for an instant every harmonic is there
BURST_FC, BURST_DB, BURST_LVL, BURST_BASE = 3000.0, 0.3, 0.36, 120.0      # how bright the burst is, how fast it dies per Hz (dB/s), its level
BEAT_HZ, BEAT_BODY = 1.3, 2.8                          # the fast motion is tuned apart from the slow one, most near the body's resonances
NOISE_LVL = 0.5
# the body rings (fit_modes.py): 40 resonances read from knocks on the instrument. They sound at their own pitches,
# between the note's harmonics, every time a pluck hits them. This is the wooden knock inside each note.
BURST_SNAP = 1.6                                      # the very first instant: gone in a hundredth of a second
RING_DECAY, RING_ADD = 1.0, 120.0                       # the ring in a played note dies faster than in a bare knock
SYMP = 0.08                                            # level of the other strings humming along
KNOCK, KNOCK_TILT = 0.8, 1.0                          # level of the ringing body, and dB per octave of tilt above 200 Hz
STRING_B = [6e-5, 5e-5, 4e-5, 3e-5]                   # stiffness of the E, A, D, G strings: high partials run sharp
BURST_FAST, BURST_FAST_F, BURST_SLOW, BURST_SLOW_F, BURST_KEEP = 260.0, 0.03, 35.0, 0.006, 0.10   # the burst dies in two stages (dB/s, per Hz), KEEP is the part that lingers
_MODES = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'body_modes.json')))
def body_ir():
    n = int(0.45 * SR); t = np.arange(n) / SR; h = np.zeros(n)
    for i, m in enumerate(_MODES):
        a = 10 ** ((m['db'] + KNOCK_TILT * max(0.0, np.log2(m['f'] / 200.0))) / 20.0) * (m['f'] / 65.0)
        h += a * 10 ** (-(RING_DECAY * m['decay'] + RING_ADD) * t / 20.0) * np.sin(2 * np.pi * m['f'] * t + 1.7 * i)
    h = h / np.sqrt((h ** 2).sum())
    # the other strings: nobody is holding them, so they hum along quietly for a second or more
    n2 = int(1.6 * SR); t2 = np.arange(n2) / SR; sy = np.zeros(n2); j = 0
    for o in OPEN:
        for kk in range(1, 9):
            f = hz(o) * kk; j += 1
            sy += (1.0 / kk) * 10 ** (-(12.0 + 0.02 * f) * t2 / 20.0) * (1 - np.exp(-t2 / 0.02)) * np.sin(2 * np.pi * f * t2 + 2.3 * j)
    sy = sy / np.sqrt((sy ** 2).sum()); out = SYMP * sy; out[:n] += h
    return out
TOUCH_S = 0.055
BODY_LP = 7000.0
BEND_CENTS, BEND_CYCLES = 200.0, 0.9                   # the first cycle is long: the string is still leaving the finger

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
        self.attack = 1.0        # the pluck as a whole: burst, finger noise and body knock. 1 = as measured, 2 = exaggerated
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

def render_note(tone, pitch, vel, dur, rng, fret=3, dead=False, scoop=0.0, cents=0.0, vib=0.0, tail=0.6, stopped=False, string=1):
    """vel 0..1, dur seconds until the hand damps it. Returns (string force, thump) before the body."""
    n = int((dur + tail) * SR); t = np.arange(n) / SR
    f0 = hz(pitch) * 2 ** (cents / 1200.0)
    # pitch: scoop from below, late vibrato
    pc = -scoop * np.exp(-t / 0.035)
    pc = pc - BEND_CENTS * (0.45 + 0.55 * vel) * np.exp(-t * f0 / BEND_CYCLES)
    if vib > 0: pc = pc + vib * np.clip((t - 0.25) / 0.3, 0, 1) * np.sin(2 * np.pi * 5.2 * t)
    f = f0 * 2 ** (pc / 1200.0)
    ph = 2 * np.pi * np.cumsum(f) / SR
    K = int(min(40, 5000 / f0))
    Bs = STRING_B[string] * 2 ** (fret / 6.0)                      # a shorter string is stiffer for its length
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
    rel = (700.0 + 1.0 * fk) if stopped else (170.0 + 0.25 * fk)    # the plucking finger lands on the string and stops it, or the left hand lets go
    sig = sig[:, None]; rel = rel[:, None]
    tt = t[None, :]
    # two-stage decay (measured): most of the pluck is gone in the first 100 ms, the rest rings on.
    # (the string's up-and-down motion drives the bridge hard and dies fast; its side-to-side motion lingers)
    mf = (FAST_M + 0.2 * np.minimum(1.0, fk / 1000.0))[:, None]
    relenv = 10 ** (-(rel * np.clip(tt - dur, 0, None)) / 20.0)
    env = (1 - mf) * 10 ** (-(sig * tt) / 20.0) * relenv
    envf = mf * 10 ** (-((sig + FAST_DB) * tt) / 20.0) * relenv
    # the two motions drift apart and back: the dip about a tenth of a second in, and the recovery after it
    wob = np.array([rng.gauss(0, 1) for _ in k]).clip(-1.5, 1.5)
    dk = (BEAT_HZ + BEAT_BODY * tone.body / 0.6 * np.exp(-0.5 * ((fk - 125.0) / 70.0) ** 2)) * (1 + 0.35 * wob) * (1 + 0.004 * fk / 10.0)
    dk = np.minimum(dk, 7.0)[:, None]
    # the string leaves the finger over a few ms; low partials take longest
    rise = 1 - np.exp(-tt / (0.0008 + RISE_K / fk[:, None]))
    pk = (k * np.sqrt(1 + Bs * k * k))[:, None] * ph[None, :] + 0.5 * (k[:, None] % 2)
    s = (A[:, None] * rise * (env * np.sin(pk) + envf * np.sin(pk + 2 * np.pi * dk * tt))).sum(0)
    # the burst: all harmonics of the corner, up to 9 kHz, each gone within a few hundredths of a second
    nb = min(n, int(0.6 * SR)); tb = t[:nb]; KB = int(min(220, 11000 / f0)); kb = np.arange(1, KB + 1); fb = kb * f0
    Ab = np.sqrt(np.sin(kb * np.pi * beta) ** 2 + 0.1) / kb * np.exp(-fb / (BURST_FC * (0.5 + 0.8 * vel) * (0.6 + 0.8 * tone.bright) * (1 - 0.4 * age)))
    Ab = Ab * (1 - np.exp(-fb / 400.0)) * (1 + 0.3 * np.array([rng.gauss(0, 1) for _ in kb]).clip(-2, 2))
    sb = np.zeros(nb)
    if not dead:
        for j in range(0, KB, 24):
            kk = kb[j:j + 24, None]; ff = fb[j:j + 24, None]
            sb += (Ab[j:j + 24, None] * (BURST_SNAP * 10 ** (-(900.0 * tb[None, :]) / 20.0) + (1 - BURST_KEEP) * 10 ** (-((BURST_FAST + BURST_FAST_F * ff) * tb[None, :]) / 20.0) + BURST_KEEP * 10 ** (-((BURST_SLOW + BURST_SLOW_F * ff) * (1.6 - 1.2 * tone.sustain) * tb[None, :]) / 20.0)) * relenv[:1, :nb] * (1 - np.exp(-tb[None, :] / 0.0005)) * np.sin(kk * np.sqrt(1 + Bs * kk * kk) * ph[None, :nb] + 0.5 * (kk % 2))).sum(0)
    s[:nb] += BURST_LVL * tone.attack * tone.thump / 0.5 * sb
    s *= vel ** 1.35 * 2.2
    # thump: finger leaving the string + the body being pushed
    th = np.zeros(n); m = int(0.03 * SR)
    noise = np.array([rng.gauss(0, 1) for _ in range(m)])
    th[:m] = noise * (np.exp(-np.arange(m) / (0.003 * SR)) + 0.25 * np.exp(-np.arange(m) / (0.012 * SR)))
    if stopped:                                                    # the finger landing on a moving string: a soft tick
        i0 = int(dur * SR); m2 = min(n - i0, int(0.012 * SR))
        if m2 > 0: th[i0:i0 + m2] += 0.35 * np.array([rng.gauss(0, 1) for _ in range(m2)]) * np.exp(-np.arange(m2) / (0.0025 * SR))
    b, a = biquad_lp(900 + 1500 * tone.bright, 0.7); th = lfilter(b, a, th) * NOISE_LVL * tone.attack * tone.thump * vel
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
    b, a = biquad_lp(BODY_LP, 0.6); y = lfilter(b, a, y)
    ring = fftconvolve(x + 2.0 * thump, body_ir())[:len(x)]
    y = y + KNOCK * (0.4 + 0.6 * tone.attack) * tone.body / 0.6 * ring
    # pickup: the string itself, less body, more highs and lows
    b, a = biquad_hp(28, 0.7); p = lfilter(b, a, x + 0.3 * thump)
    b, a = biquad_lp(6500, 0.7); p = lfilter(b, a, p)
    return (1 - tone.pickup) * y + tone.pickup * p * 0.9

def play(notes, tone, seed=1, total=None):
    """notes: dicts with t, pitch, vel(0..1), dur, and optional dead, scoop, cents, vib."""
    import random
    rng = random.Random(seed)
    total = total or max(n['t'] + n['dur'] for n in notes) + 1.5
    out = np.zeros(int(total * SR)); th = np.zeros_like(out); hand = 2
    plan = []
    for n in notes:
        s_, fret = note_string(n['pitch'], hand)
        if fret > 0: hand = fret if abs(fret - hand) > 3 else hand
        plan.append([n, s_, fret, n['dur'], False])
    for a, b in zip(plan, plan[1:]):
        # the finger is on the string before it plucks: 30 to 70 ms, less at speed. On the same string that stops the old note dead.
        gap = b[0]['t'] - a[0]['t']; touch = min(TOUCH_S, 0.22 * gap)
        if a[1] == b[1] and a[0]['t'] + a[3] > b[0]['t'] - touch - 0.02: a[3] = max(0.04, gap - touch); a[4] = True
    for n, s_, fret, dur, stopped in plan:
        x, tp = render_note(tone, n['pitch'], n['vel'], dur, rng, fret=fret, dead=n.get('dead', False),
                            scoop=n.get('scoop', 0.0), cents=n.get('cents', 0.0), vib=n.get('vib', 0.0), stopped=stopped, string=s_)
        i = int(n['t'] * SR); j = min(len(out), i + len(x))
        if i < 0 or i >= len(out): continue
        out[i:j] += x[:j - i]; th[i:j] += tp[:j - i]
    return body(fingerboard(out, tone), tone, th)
