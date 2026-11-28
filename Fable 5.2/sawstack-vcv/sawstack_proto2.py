"""Sawtooth-stack voice prototype v2: adds analog behaviours behind AGE (0..1) and DRIVE (0..1).
Sketch quality (8x oversampled naive waveforms). age=0, drive=0 is a perfect digital stack.
age=0.25, drive=0 is close to the first prototype (sawstack_proto.py)."""
import numpy as np
from numba import njit
from scipy.signal import resample_poly

SR = 48000
OS = 8
FS = SR * OS
RATIOS = np.array([-2.0, -1.0, 0.0, 1.0, 2.0])
PANS = np.array([-0.45, 0.9, 0.0, -0.9, 0.45])
SPREAD_MAX_OCT = 1.0
RICH_MAX = 8.0

def unit(seed, age):
    """the fixed personality of one 'unit': per-oscillator component errors, scaled by age"""
    r = np.random.default_rng(seed)
    u = lambda a: r.uniform(-a, a, 5)
    return dict(
        tune=age * u(5.0) / 1200.0,            # fixed tuning error, octaves (up to 5 cents)
        vtrack=1.0 + age * u(0.004),           # V/oct scale error (about 5 cents per octave)
        strack=1.0 + age * u(0.015),           # SPREAD scale error
        shp=1.0 + age * u(0.10),               # sine shaper drive mismatch
        dc=age * u(0.04),                      # shaper offset
        lvl=10 ** (age * u(1.2) / 20),         # level mismatch, dB
        lp=np.exp(-2 * np.pi * (20000 - age * r.uniform(6000, 11000, 5)) / FS),  # reset rounding
        ph0=r.uniform(0, 1, 5),
    )

def ou(n, rng, tau, step=1024):
    """unit-variance slow random walk (Ornstein-Uhlenbeck), linearly interpolated to n samples"""
    m = n // step + 3
    a = np.exp(-step / (tau * FS)); x = np.zeros(m); x[0] = rng.standard_normal()
    w = rng.standard_normal(m) * np.sqrt(1 - a * a)
    for i in range(1, m): x[i] = a * x[i - 1] + w[i]
    return np.interp(np.arange(n), np.arange(m) * step, x)

@njit(cache=True)
def core(octv, t, k, jit, lp, shp, dc, lvl, ph0, gl, gr, f0, couple, odrv, obias, age):
    n = octv.shape[1]
    L = np.zeros(n); R = np.zeros(n)
    ph = ph0.copy(); z = np.zeros(5); jz = np.zeros(5)
    ja = np.exp(-2 * np.pi * 400.0 / FS)
    onorm = np.tanh(odrv)
    for s in range(n):
        # mutual pull between oscillators (shared supply): Kuramoto-style
        for i in range(5):
            pull = 0.0
            if couple > 0:
                for j in range(5):
                    if j != i:
                        pull += np.sin(2 * np.pi * (ph[j] - ph[i]))
            f = f0 * 2.0 ** octv[i, s]
            jz[i] = ja * jz[i] + (1 - ja) * jit[i, s]
            f *= 1.0 + jz[i] + couple * pull
            if f > 20000.0: f = 20000.0
            p = ph[i] + f / FS
            if p >= 1.0: p -= 1.0
            ph[i] = p
            tri = 4.0 * abs(p - 0.5) - 1.0
            # sine from the triangle through an imperfect shaper
            sine = -np.sin(0.5 * np.pi * tri * shp[i]) / np.sin(0.5 * np.pi * min(shp[i], 1.0))
            if sine > 1.0: sine = 1.0 - (sine - 1.0)          # over-driven shaper folds the tip slightly
            if sine < -1.0: sine = -1.0 - (sine + 1.0)
            x = k[s] * (2.0 * p - 1.0)
            saw = ((x + 1.0) % 2.0) - 1.0
            # triangle-core saws carry a small glitch at the triangle's turnaround (mid-cycle)
            d = (p - 0.5) * 60.0
            saw -= age * 0.06 * np.exp(-d * d)
            z[i] = lp[i] * z[i] + (1 - lp[i]) * saw           # rounded reset edge
            y = (1 - t[s]) * sine + t[s] * 0.8 * z[i] + dc[i]
            y = lvl[i] * (np.tanh(odrv * (y + obias)) - np.tanh(odrv * obias)) / onorm
            L[s] += y * gl[i]; R[s] += y * gr[i]
    return L, R

def render(pitch_oct, spread, timbre, richness, amp, age=0.25, drive=0.0, seed=1, f0=261.626):
    N = len(pitch_oct); n = N * OS
    rng = np.random.default_rng(seed + 1000)
    U = unit(seed, age)
    upz = lambda x: np.full(n, float(x)) if np.ndim(x) == 0 else np.repeat(np.asarray(x, float), OS)[:n]
    p = upz(pitch_oct); s = SPREAD_MAX_OCT * np.clip(upz(spread), 0, 1) ** 3
    t = np.clip(upz(timbre), 0, 1); k = 1.0 + (RICH_MAX - 1.0) * np.clip(upz(richness), 0, 1) ** 2
    common = age * 3.0 / 1200.0 * ou(n, rng, 20.0)             # shared slow wander (supply, temperature)
    octv = np.zeros((5, n)); jit = np.zeros((5, n))
    for i in range(5):
        slow = age * (5.0 * ou(n, rng, 2.5) + 5.0 * ou(n, rng, 25.0)) / 1200.0
        octv[i] = p * U['vtrack'][i] + RATIOS[i] * s * U['strack'][i] + U['tune'][i] + slow + common
        jit[i] = age * 0.02 * rng.standard_normal(n)           # fast pitch jitter (before 400 Hz smoothing)
    gl = np.cos((PANS + 1) * np.pi / 4); gr = np.sin((PANS + 1) * np.pi / 4)
    odrv = 0.35 + 1.4 * drive; obias = 0.12 * age + 0.1 * drive
    L, R = core(octv, t, k, jit, U['lp'], U['shp'], U['dc'], U['lvl'], U['ph0'], gl, gr, f0,
                age * 0.0005, odrv, obias, age)
    a = upz(amp)
    a = a + age * 0.0008                                        # VCA bleed
    out = []
    g = 0.30 * (1.0 + 2.5 * drive); b = 0.10 * age + 0.12 * drive
    for x in (L, R):
        x = x * a * g
        x = (np.tanh(x + b * (1 + 0 * x)) - np.tanh(b)) / (0.75 + 1.35 * drive)   # output stage, asymmetric
        x = x + age * 2.2e-4 * rng.standard_normal(n) * np.sqrt(OS)             # noise floor
        y = resample_poly(x, 1, OS)
        # DC blocker 8 Hz
        c = np.exp(-2 * np.pi * 8 / SR); o = np.zeros(len(y)); zz = 0.0; prev = 0.0
        d = np.diff(y, prepend=y[0])
        from scipy.signal import lfilter
        o = lfilter([1.0], [1.0, -c], d)
        out.append(o)
    return np.stack(out, axis=1)

def envelope(gate, rise, fall, age=0.25, seed=5):
    """rise/fall envelope. age bends the straight lines toward capacitor curves and scatters the times."""
    rng = np.random.default_rng(seed)
    n = len(gate); e = np.zeros(n); endp = np.zeros(n)
    v = 0.0; stage = 0; prev = 0.0; hold = 0; sc = 1.0
    curve = 0.6 * age
    for i in range(n):
        g = gate[i]
        if g > 0.5 and prev <= 0.5:
            stage = 1; sc = 1.0 + age * rng.uniform(-0.06, 0.06)
        prev = g
        if stage == 1:
            v += (1.0 + curve * (1.3 * (1 - v) - 0.5)) / (rise * sc * SR)
            if v >= 1.0: v = 1.0; stage = 2
        elif stage == 2:
            if g <= 0.5: stage = 3
        elif stage == 3:
            v -= (1.0 + curve * (2.2 * v - 1.0)) / (fall * sc * SR)
            if v <= 0.0: v = 0.0; stage = 0; hold = int(0.001 * SR)
        if hold > 0: endp[i] = 1.0; hold -= 1
        e[i] = v
    return e, endp

def write_wav(path, x):
    import wave
    x = np.clip(x, -1, 1); d = (x * 32767).astype('<i2')
    with wave.open(path, 'wb') as w:
        w.setnchannels(d.shape[1]); w.setsampwidth(2); w.setframerate(SR); w.writeframes(d.tobytes())
