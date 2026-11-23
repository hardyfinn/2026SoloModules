"""Dry multitrack material for the tape sketches: a deep house loop, a hip hop loop, a techno loop.
Each returns a dict of stems: drums (mono), bass (mono), keys (stereo), extra (stereo)."""
import numpy as np, numba as nb
from scipy.signal import butter, sosfilt
SR = 48000
T = lambda d: np.arange(int(d * SR)) / SR
def bq(x, f, kind='low', order=2): return sosfilt(butter(order, np.asarray(f) / (SR / 2), btype=kind, output='sos'), x, axis=0)
def place(buf, x, t, g=1.0):
    i = int(t * SR); x = x[:max(0, len(buf) - i)]; buf[i:i + len(x)] += g * x

@nb.njit(cache=True)
def svf(x, fc, q, sr):
    y = np.empty(len(x)); lp = 0.0; bp = 0.0
    for i in range(len(x)):
        f = 2 * np.sin(np.pi * min(fc[i], sr / 6.5) / sr)
        for _ in range(2):
            hp = x[i] - lp - q * bp; bp += 0.5 * f * hp; lp += 0.5 * f * bp
        y[i] = np.tanh(lp)
    return y

def kick(rng, f0=50, sweep=220, ts=0.028, dec=0.16, click=0.35, d=0.5, drive=1.6):
    t = T(d); ph = 2 * np.pi * np.cumsum(f0 + sweep * np.exp(-t / ts)) / SR
    return np.tanh(drive * np.sin(ph)) * np.exp(-t / dec) + click * bq(rng.standard_normal(len(t)), 2500, 'high') * np.exp(-t / 0.003)
def snare(rng, d=0.35, tone=190, dec=0.11):
    t = T(d); return 0.7 * (np.sin(2 * np.pi * tone * t) + 0.5 * np.sin(2 * np.pi * tone * 1.78 * t)) * np.exp(-t / 0.045) + 0.6 * bq(rng.standard_normal(len(t)), [900, 9000], 'band') * np.exp(-t / dec)
def clap(rng, d=0.4):
    t = T(d); nz = bq(rng.standard_normal(len(t)), [700, 5000], 'band')
    env = sum(np.where(t >= o, np.exp(-(t - o) / 0.007), 0) for o in (0, 0.012, 0.024)) * 0.7 + np.where(t >= 0.034, np.exp(-(t - 0.034) / 0.09), 0)
    return nz * env
def hat(rng, dec=0.03, d=0.5, fc=7500):
    t = T(d); m = sum(np.sign(np.sin(2 * np.pi * f * t)) for f in (263, 400, 421, 474, 587, 845)) / 6
    return bq(m + 0.3 * rng.standard_normal(len(t)), fc, 'high', 4) * np.exp(-t / dec) * 3
def saw(f, d, det=0.0, ph=0.0): 
    t = T(d); return 2 * ((f * (1 + det) * t + ph) % 1.0) - 1
def chord_juno(rng, notes, d, cutoff=1800, env_t=0.5, atk=0.01):
    t = T(d); out = np.zeros((len(t), 2))
    for nn in notes:
        f = 440 * 2 ** ((nn - 69) / 12)
        s = sum(saw(f, d, dt, rng.uniform()) for dt in (-0.004, 0.0, 0.004)) / 3 + 0.4 * np.sign(saw(f / 2, d, 0, rng.uniform()))
        fc = 300 + cutoff * np.exp(-t / env_t)
        s = svf(s * 0.5, fc, 0.9, SR) * np.minimum(t / atk, 1) * np.exp(-t / (env_t * 2.2))
        # chorus: two modulated delays, opposite phase
        for c, sgn in ((0, 1), (1, -1)):
            dl = (0.008 + sgn * 0.0025 * np.sin(2 * np.pi * 0.6 * t + nn)) * SR
            out[:, c] += 0.6 * s + 0.6 * np.interp(np.arange(len(t)) - dl, np.arange(len(t)), s)
    return out / len(notes)
def epiano(notes, d, vel=0.8):
    t = T(d); out = np.zeros((len(t), 2))
    for k, nn in enumerate(notes):
        f = 440 * 2 ** ((nn - 69) / 12)
        tine = np.sin(2 * np.pi * f * t + vel * 1.6 * np.exp(-t / 0.35) * np.sin(2 * np.pi * f * t) + 0.25 * np.exp(-t / 0.02) * np.sin(2 * np.pi * 14 * f * t))
        s = tine * np.exp(-t / 1.4) * np.minimum(t / 0.003, 1)
        pan = 0.5 + 0.4 * np.sin(2 * np.pi * 4.5 * t + k)          # tremolo pan
        out[:, 0] += s * pan; out[:, 1] += s * (1 - pan)
    return out / len(notes)
def bassnote(f, d, kind='house'):
    t = T(d)
    if kind == 'sub': return np.sin(2 * np.pi * f * t + 0.6 * np.exp(-t / 0.08) * np.sin(4 * np.pi * f * t)) * np.exp(-t / (d * 0.7)) * np.minimum(t / 0.004, 1)
    s = saw(f, d) + 0.6 * np.sign(saw(f, d, 0.003))
    return svf(0.5 * s, 180 + 1400 * np.exp(-t / 0.09), 0.7, SR) * np.exp(-t / (d * 0.8)) * np.minimum(t / 0.003, 1)
def crush(x, bits=12, rate=26040):
    """SP-1200 style: hold samples at 26 kHz, 12 bit."""
    idx = (np.floor(np.arange(len(x)) * rate / SR) * SR / rate).astype(int)
    return np.round(x[idx] * 2 ** (bits - 1)) / 2 ** (bits - 1)
def norm(x, p=1.0): return x / np.max(np.abs(x)) * p

def house(bars=8, bpm=120, seed=5):
    rng = np.random.default_rng(seed); s = 60 / bpm / 4; n = int(bars * 16 * s * SR)
    dr = np.zeros(n); ba = np.zeros(n); ke = np.zeros((n, 2)); ex = np.zeros((n, 2))
    k = kick(rng); c = clap(rng); h = hat(rng, 0.022); oh = hat(rng, 0.11); rim = bq(snare(rng, 0.08, 420, 0.02), 500, 'high')
    prog = [[57, 60, 64, 67, 71], [53, 57, 60, 64, 67], [50, 57, 60, 65, 69], [52, 56, 59, 62, 67]]     # Am9 Fmaj9 Dm9 E7#9-ish
    roots = [45, 41, 38, 40]
    for b in range(bars):
        for i in range(16):
            t = (b * 16 + i) * s
            if i % 4 == 0: place(dr, k, t, 1.0)
            if i % 8 == 4: place(dr, c, t, 0.55)
            place(dr, h, t, 0.16 * (1.0 if i % 2 == 0 else 0.55) * rng.uniform(0.8, 1.0))
            if i % 4 == 2: place(dr, oh, t, 0.26)
            if i in (3, 10, 13) and b % 2 == 1: place(dr, rim, t, 0.25)
        r = 440 * 2 ** ((roots[b % 4] - 69) / 12)
        for i, sm in ((0, 0), (3, 0), (6, 12), (8, 0), (11, 7), (14, 12)):
            place(ba, bassnote(r * 2 ** (sm / 12), s * 2.6), (b * 16 + i) * s, 0.9)
        for i, dur in ((2, 3), (7, 2), (10, 5)):
            ch = chord_juno(rng, prog[b % 4], s * dur + 0.6, 2200 if i == 10 else 1500, 0.35)
            place(ke, ch, (b * 16 + i) * s, 0.9)
        if b >= 4:
            for i in (0, 6, 12): 
                nn = prog[b % 4][-1] + 12 + (0 if i else 3)
                t0 = T(0.5); bell = (np.sin(2 * np.pi * 440 * 2 ** ((nn - 69) / 12) * t0 + 2 * np.exp(-t0 / 0.1) * np.sin(2 * np.pi * 3.01 * 440 * 2 ** ((nn - 69) / 12) * t0)) * np.exp(-t0 / 0.18))[:, None] * np.array([0.4, 1.0] if i % 12 else [1.0, 0.4])
                place(ex, bell, (b * 16 + i) * s, 0.5); place(ex, bell[:, ::-1], (b * 16 + i + 3) * s, 0.22)
    return dict(drums=norm(dr, 0.9), bass=norm(ba, 0.7), keys=norm(ke, 0.6), extra=norm(ex, 0.3) if bars > 4 else ex, bpm=bpm)

def hiphop(bars=8, bpm=91, seed=9):
    rng = np.random.default_rng(seed); s = 60 / bpm / 4; n = int(bars * 16 * s * SR); sw = 0.32 * s
    dr = np.zeros(n); ba = np.zeros(n); ke = np.zeros((n, 2)); ex = np.zeros((n, 2))
    k = kick(rng, 55, 120, 0.02, 0.2, 0.25, 0.5, 2.2); sn = snare(rng) + 0.5 * clap(rng)[:int(0.35 * SR)]; h = hat(rng, 0.03, 0.3, 6000)
    prog = [[50, 57, 60, 64, 65], [43, 53, 59, 62, 64]]; roots = [38, 43]
    kp = {0: 1, 7: .7, 10: .9}; kp2 = {0: 1, 3: .6, 10: .9, 11: .5}
    for b in range(bars):
        for i in range(16):
            t = (b * 16 + i) * s + (sw if i % 2 else 0) + rng.normal(0, 0.004)
            pat = kp if b % 2 == 0 else kp2
            if i in pat: place(dr, k, max(t, 0), pat[i])
            if i % 8 == 4: place(dr, sn, t + 0.008, 0.85)
            if i % 2 == 0 or rng.uniform() < 0.25: place(dr, h, max(t, 0), 0.2 * rng.uniform(0.6, 1.0))
        r = 440 * 2 ** ((roots[(b // 2) % 2] - 69) / 12)
        for i, sm, du in ((0, 0, 5), (7, 0, 2), (10, 12, 2), (12, 7 if b % 2 else 10, 3)):
            place(ba, bassnote(r * 2 ** (sm / 12), s * du, 'sub'), (b * 16 + i) * s + (sw if i % 2 else 0), 0.9)
        if b % 2 == 0:
            place(ke, epiano(prog[(b // 2) % 2], s * 30, 0.9), b * 16 * s, 0.9)
            place(ke, epiano([n_ + 12 for n_ in prog[(b // 2) % 2][2:]], s * 12, 0.5), (b * 16 + 22) * s + sw, 0.5)
    dr = crush(bq(norm(dr), 12000, 'low', 4)); ke = np.stack([crush(c) for c in bq(norm(ke), 9000, 'low', 4).T], 1)
    return dict(drums=norm(dr, 0.9), bass=norm(ba, 0.8), keys=norm(ke, 0.55), extra=ex, bpm=bpm)

def techno(bars=8, bpm=132, seed=3):
    rng = np.random.default_rng(seed); s = 60 / bpm / 4; n = int(bars * 16 * s * SR)
    dr = np.zeros(n); ba = np.zeros(n); ke = np.zeros((n, 2)); ex = np.zeros((n, 2))
    k = kick(rng, 47, 300, 0.035, 0.2, 0.5, 0.5, 3.0); h = hat(rng, 0.018, 0.2, 9000); oh = hat(rng, 0.09); c = clap(rng); ride = hat(rng, 0.25, 0.6, 5000)
    line = [33, 33, 45, 33, 36, 33, 45, 31, 33, 33, 45, 33, 40, 33, 43, 36]; acc = [1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0]
    acid = np.zeros(n); cut = np.zeros(n); f_now = 55.0; phs = 0.0
    for b in range(bars):
        for i in range(16):
            t = (b * 16 + i) * s; a = int(t * SR); L = int(s * SR)
            if i % 4 == 0: place(dr, k, t, 1.0)
            place(dr, h, t, 0.12 * (1.0 if i % 4 == 2 else 0.5))
            if i % 4 == 2: place(dr, oh, t, 0.22)
            if i % 8 == 4 and b >= 2: place(dr, c, t, 0.4)
            if b >= 4 and i % 2 == 0: place(dr, ride, t, 0.07)
            f = 440 * 2 ** ((line[i] - 69) / 12); tt = np.arange(min(L, n - a)) / SR
            fr = f + (f_now - f) * np.exp(-tt / 0.02) if acc[i - 1] else np.full(len(tt), f); f_now = f
            p = phs + np.cumsum(fr) / SR; phs = p[-1] if len(p) else phs
            acid[a:a + len(tt)] = (2 * (p % 1) - 1) * np.exp(-tt / (0.25 if acc[i] else 0.12))
            sweep = 0.5 + 0.5 * np.sin(2 * np.pi * (b * 16 + i) / (16 * bars) - 1.5)
            cut[a:a + len(tt)] = 150 + (400 + 3500 * sweep) * (1.5 if acc[i] else 1.0) * np.exp(-tt / 0.07)
        if b % 2 == 1:
            st = chord_juno(rng, [57, 60, 64], s * 3, 3000, 0.12, 0.002)
            for r_, g in enumerate((1.0, 0.5, 0.28, 0.15)): place(ke, st[:, ::(1 if r_ % 2 == 0 else -1)], (b * 16 + 6 + r_ * 3) * s, g)
    ba = svf(acid * 0.7, cut, 0.18, SR)
    return dict(drums=norm(dr, 0.9), bass=norm(ba, 0.6), keys=norm(ke, 0.45), extra=ex, bpm=bpm)

def mix(st, d=1.0, b=1.0, k=1.0, e=1.0):
    return np.stack([st['drums'] * d + st['bass'] * b] * 2, 1) + st['keys'] * k + st['extra'] * e
