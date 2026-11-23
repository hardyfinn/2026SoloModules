"""Test sounds for the scratch demos: two synthesized phrases and a plain beat."""
import numpy as np
from scipy.signal import lfilter, butter
SR = 44100
rng = np.random.default_rng(11)

def _res(x, f, bw):
    r = np.exp(-np.pi * bw / SR); c = 2 * r * np.cos(2 * np.pi * f / SR)
    return lfilter([1 - r], [1, -c, r * r], x)

def voiced(dur, f0a, f0b, forms, breath=0.04, vib=0.012):
    n = int(dur * SR); t = np.arange(n) / SR
    f0 = f0a * (f0b / f0a) ** (t / dur) * (1 + vib * np.sin(2 * np.pi * 5.3 * t))
    ph = 2 * np.pi * np.cumsum(f0) / SR; src = np.zeros(n)
    for k in range(1, 60):
        src += np.where(k * f0 < 17000, np.sin(k * ph) / k ** 1.25, 0)
    src += breath * rng.normal(0, 1, n)
    y = sum(a * _res(src, f, bw) for f, bw, a in forms)
    return y / np.abs(y).max()

def noise(dur, lo, hi):
    n = int(dur * SR); b, a = butter(2, [lo / (SR / 2), hi / (SR / 2)], 'band')
    y = lfilter(b, a, rng.normal(0, 1, n)); return y / np.abs(y).max()

def env(n, a, r):
    k = np.arange(n) / SR; return np.minimum(1, k / a) * np.minimum(1, (n / SR - k) / r)

AH = [(730, 90, 1.0), (1090, 110, 0.7), (2440, 160, 0.35), (3400, 200, 0.15)]
EH = [(530, 80, 1.0), (1840, 120, 0.6), (2480, 160, 0.3)]

def phrase_ah():
    """A long open vowel with a falling pitch, the classic kind of scratch sound."""
    y = voiced(0.95, 235, 180, AH); n = len(y)
    return y * env(n, 0.012, 0.25) * (0.75 + 0.25 * np.exp(-np.arange(n) / (0.2 * SR)))

def phrase_word():
    """A three-part word: breath noise, a short vowel, a hiss. Gives three cue points."""
    f = noise(0.11, 1500, 7000) * env(int(0.11 * SR), 0.02, 0.03) * 0.45
    v = voiced(0.20, 210, 190, EH) * env(int(0.20 * SR), 0.01, 0.04)
    sh = noise(0.26, 1800, 5200) * env(int(0.26 * SR), 0.03, 0.14) * 0.6
    return np.concatenate([f, v, sh]), (0.0, 0.11, 0.31)

def record():
    """The demo record: 'ah', a gap, then the word. Returns audio and cue points in seconds."""
    a = phrase_ah(); w, wc = phrase_word(); gap = np.zeros(int(0.15 * SR)); lead = np.zeros(int(0.03 * SR))
    x = np.concatenate([lead, a, gap, w, np.zeros(int(0.1 * SR))])
    o = (len(lead) + len(a) + len(gap)) / SR
    return x, dict(ah=0.03, word=o + wc[0], vowel=o + wc[1], hiss=o + wc[2])

def beat(bpm, beats, swing=0.58):
    spb = 60 / bpm * SR; n = int(beats * spb) + SR // 2; x = np.zeros(n + SR)
    def kick():
        m = int(.3 * SR); t = np.arange(m) / SR; f = 50 + 100 * np.exp(-t / .03)
        return np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t / .12)
    def snare():
        m = int(.25 * SR); t = np.arange(m) / SR; b, a = butter(2, [900 / (SR / 2), 7000 / (SR / 2)], 'band')
        return 1.3 * lfilter(b, a, rng.normal(0, 1, m)) * np.exp(-t / .07) + .6 * np.sin(2 * np.pi * 190 * t) * np.exp(-t / .05)
    def hat(v):
        m = int(.08 * SR); t = np.arange(m) / SR; b, a = butter(2, 7000 / (SR / 2), 'high')
        return v * .4 * lfilter(b, a, rng.normal(0, 1, m)) * np.exp(-t / .025)
    def put(b, s):
        i = int(b * spb); x[i:i + len(s)] += s
    for bar in range(int(np.ceil(beats / 4))):
        o = bar * 4
        for b in (0, 1.5 + swing / 2 - .25, 2.5): put(o + b, kick())
        for b in (1, 3): put(o + b, snare())
        for k in range(8): put(o + k / 2, hat(.8)); put(o + k / 2 + swing / 2, hat(.35))
    x = x[:n]; return x / np.abs(x).max()
