"""Sawstack reference engine (reviewed design, 2026-11-23). One voice, per-sample, numba.
This is the model the C++ module must match. Runs at 2x the engine rate and decimates by 2.

Inputs to render() are arrays at the engine rate SR (already-combined knob + CV values):
  pitch   octaves relative to C4 (FREQUENCY knob + V/OCT + FREQ CV)
  spread, timbre, rich, amp   0..1
Scalars: age 0..1, drive 0..1, voicing 0/1/2, seed (unit personality), mono (bool).
"""
import numpy as np
from numba import njit
from scipy.signal import resample_poly, lfilter

SR = 48000
OS = 2
F0 = 261.6256
RICH_MAX = 8.0
SPREAD_MAX = 1.0
LOG2_3_2 = np.log2(1.5)
VOICINGS = np.array([
    [-2.0, -1.0, 0.0, 1.0, 2.0],                                   # 0 OCTAVES
    [-1.0, LOG2_3_2 - 1.0, 0.0, LOG2_3_2, 1.0],                    # 1 FIFTHS (just)
    [np.log2(1 / 3), np.log2(2 / 3), 0.0, np.log2(4 / 3), np.log2(5 / 3)],  # 2 HARMONICS 1..5, centre = 3rd
])
PANS = np.array([-0.45, 0.9, 0.0, -0.9, 0.45])
MASK = (1 << 64) - 1

def splitmix(state):
    state = (state + 0x9E3779B97F4A7C15) & MASK
    z = state
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & MASK
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & MASK
    z = z ^ (z >> 31)
    return state, (z >> 11) / float(1 << 53)

def make_unit(seed):
    """Unit personality at AGE = 1. Nine uniforms per oscillator, drawn in this order."""
    st = seed & MASK
    U = np.zeros((5, 9))
    for i in range(5):
        for j in range(9):
            st, u = splitmix(st)
            U[i, j] = u
    b = lambda j, a: (2 * U[:, j] - 1) * a
    return dict(
        tune=b(0, 5.0) / 1200.0,                 # octaves
        vtrack=b(1, 0.004),                      # V/oct scale error
        strack=b(2, 0.05),                       # SPREAD scale error
        shp=b(3, 0.10),                          # sine shaper drive error
        dc=b(4, 0.02),                           # shaper offset
        lvl_db=b(5, 1.2),                        # level error, dB
        lp_hz=6000.0 + 5000.0 * U[:, 6],         # how far the reset corner drops below 20 kHz
        droop=5.0 + 15.0 * U[:, 7],              # cents flat at 4 kHz
        ph0=U[:, 8],
    )

@njit(cache=True)
def _blep_after(a):   # sample just after a step that happened a fraction `a` of a sample ago (0..1)
    return -0.5 * (1.0 - a) * (1.0 - a)

@njit(cache=True)
def _blep_before(a):  # sample just before a step that will happen `a` of a sample later (0..1)
    return 0.5 * (1.0 - a) * (1.0 - a)

@njit(cache=True)
def _wrap(x):
    return ((x + 1.0) % 2.0) - 1.0

@njit(cache=True)
def _shape(sine, saw, t, dc, od, ob, lvl, onorm):
    y = (1.0 - t) * sine + t * 0.8 * saw + dc
    return lvl * (np.tanh(od * (y + ob)) - np.tanh(od * ob)) / onorm

@njit(cache=True)
def _sine(p, shp, snorm):
    tri = 4.0 * abs(p - 0.5) - 1.0
    return -np.sin(0.5 * np.pi * tri * shp) / snorm

@njit(cache=True)
def voice(pitch, spread, timbre, rich, ratios, age, drive, seed32,
          tune, vtrack, strack, shp, dc, lvl, lpc, droop, ph0, fs, kdrift=1.0, kjit=1.0):
    """returns the five oscillator signals (5, n) at rate fs (= OS * SR), before pan / VCA / output stage"""
    n = len(pitch)
    out = np.zeros((5, n))
    np.random.seed(seed32)
    ph = ph0.copy()
    pend = np.zeros(5)                  # polyBLEP correction owed to the previous output sample
    prev = np.zeros(5)                  # previous output sample (one-sample delay)
    z = np.zeros(5)                     # reset-rounding filter state
    spike = np.zeros(5)                 # rail spike left by each oscillator's reset
    jz = np.zeros(5)
    d1 = np.zeros(5); d2 = np.zeros(5); d3 = np.zeros(5); dcm = 0.0
    for i in range(5):
        d1[i] = np.random.randn(); d2[i] = np.random.randn(); d3[i] = np.random.randn()
    dcm = np.random.randn()
    CB = 32
    dt = CB / fs
    a1 = np.exp(-dt / 0.3); a2 = np.exp(-dt / 2.5); a3 = np.exp(-dt / 25.0); ac = np.exp(-dt / 20.0)
    ja = np.exp(-2 * np.pi * 400.0 / fs)
    jgain = kjit * age * 0.001156 / np.sqrt((1 - ja) / (1 + ja))     # 2 cents rms after smoothing at AGE 1
    sdecay = np.exp(-1.0 / (160e-6 * fs))                     # rail spike time constant 160 us
    couple = age * 0.001
    rail = 0.0
    od = 0.35 + 1.4 * drive; ob = 0.12 * age + 0.1 * drive; onorm = np.tanh(od)
    snorm = np.zeros(5)
    for i in range(5):
        snorm[i] = np.sin(0.5 * np.pi * min(shp[i], 1.0))
    gw = 40e-6                                                # turnaround glitch width, seconds
    drift = np.zeros(5)
    alpha = np.zeros(5)
    for s in range(n):
        if s % CB == 0:
            dcm = ac * dcm + np.sqrt(1 - ac * ac) * np.random.randn()
            for i in range(5):
                d1[i] = a1 * d1[i] + np.sqrt(1 - a1 * a1) * np.random.randn()
                d2[i] = a2 * d2[i] + np.sqrt(1 - a2 * a2) * np.random.randn()
                d3[i] = a3 * d3[i] + np.sqrt(1 - a3 * a3) * np.random.randn()
                drift[i] = kdrift * age * (1.7 * d1[i] + 5.0 * d2[i] + 5.0 * d3[i] + 3.0 * dcm) / 1200.0
        sp = spread[s]; t = timbre[s]
        sa = SPREAD_MAX * sp * sp * sp
        # ---- pass 1: advance phases, note who resets and when (rail is from the end of the previous sample)
        for i in range(5):
            octv = pitch[s] * vtrack[i] + ratios[i] * sa * strack[i] + tune[i] + drift[i]
            f = F0 * 2.0 ** octv
            f *= 2.0 ** (-droop[i] * f / 4000.0 / 1200.0)
            jz[i] = ja * jz[i] + (1 - ja) * jgain * np.random.randn()
            f *= 1.0 + jz[i]
            if f > 0.225 * fs: f = 0.225 * fs
            if f < 0.01: f = 0.01
            dp = f / fs
            k = 1.0 + (RICH_MAX - 1.0) * rich[s] * rich[s]
            kmax = 0.45 / dp
            if k > kmax: k = max(1.0, kmax)
            p0 = ph[i]; p1 = p0 + dp
            y_corr_now = 0.0
            # in-cycle wraps of the multiplied saw: x = k(2p-1) crossing odd integers
            thr0 = 1.0 - couple * (rail - spike[i])
            x0 = k * (2 * p0 - 1); x1 = k * (2 * min(p1, thr0) - 1)
            m = np.ceil((x0 - 1.0) / 2.0)
            xc = 2.0 * m + 1.0
            while xc < x1 and xc < k - 1e-9:
                if xc > x0:
                    pc = (xc / k + 1.0) * 0.5
                    a = (p1 - pc) / dp                         # fraction of a sample since the step
                    sn = _sine(pc, shp[i], snorm[i])
                    h = _shape(sn, -1.0, t, dc[i], od, ob, lvl[i], onorm) - _shape(sn, 1.0, t, dc[i], od, ob, lvl[i], onorm)
                    y_corr_now += h * _blep_after(a); pend[i] += h * _blep_before(1.0 - a)
                xc += 2.0
            alpha[i] = -1.0
            # rail coupling: spikes left by the other oscillators' resets lower this one's reset threshold
            thr = 1.0 - couple * (rail - spike[i])
            if p1 >= thr:
                a = (p1 - thr) / dp
                if a > 1.0: a = 1.0
                alpha[i] = a
                pb = min(thr, 1.0 - 1e-12)
                h = _shape(_sine(0.0, shp[i], snorm[i]), _wrap(-k), t, dc[i], od, ob, lvl[i], onorm) \
                    - _shape(_sine(pb, shp[i], snorm[i]), _wrap(k * (2 * pb - 1)), t, dc[i], od, ob, lvl[i], onorm)
                y_corr_now += h * _blep_after(a); pend[i] += h * _blep_before(1.0 - a)
                p1 = a * dp
                # wraps of the multiplied saw after the reset, inside the same sample
                x0 = -k; x1 = k * (2 * p1 - 1)
                m = np.ceil((x0 - 1.0) / 2.0); xc = 2.0 * m + 1.0
                while xc < x1:
                    if xc > x0:
                        pc = (xc / k + 1.0) * 0.5
                        a2_ = (p1 - pc) / dp
                        sn = _sine(pc, shp[i], snorm[i])
                        h = _shape(sn, -1.0, t, dc[i], od, ob, lvl[i], onorm) - _shape(sn, 1.0, t, dc[i], od, ob, lvl[i], onorm)
                        y_corr_now += h * _blep_after(a2_); pend[i] += h * _blep_before(1.0 - a2_)
                    xc += 2.0
            ph[i] = p1
            # naive shaped value at the new phase
            sn = _sine(p1, shp[i], snorm[i])
            saw = _wrap(k * (2 * p1 - 1))
            w = dp * gw * fs
            if w > 0.1: w = 0.1
            g = (p1 - 0.5) / w
            if abs(g) < 4.0:
                gl = age * 0.06 * np.exp(-g * g)
                saw -= gl; sn -= 0.5 * gl
            y = _shape(sn, saw, t, dc[i], od, ob, lvl[i], onorm) + y_corr_now
            # one-sample delay so the 'before' half of the correction can be added
            done = prev[i] + pend[i]
            prev[i] = y; pend[i] = 0.0
            z[i] = lpc[i] * z[i] + (1 - lpc[i]) * done
            out[i, s] = z[i]
        # ---- pass 2: update the rail spikes
        rail = 0.0
        for i in range(5):
            spike[i] *= sdecay
            if alpha[i] >= 0.0:
                spike[i] += sdecay ** alpha[i]
            rail += spike[i]
    return out

def unit_arrays(seed, age):
    u = make_unit(seed)
    lpc = np.exp(-2 * np.pi * (20000.0 - age * u['lp_hz']) / (SR * OS))
    return (age * u['tune'], 1.0 + age * u['vtrack'], 1.0 + age * u['strack'], 1.0 + age * u['shp'],
            age * u['dc'], 10 ** (age * u['lvl_db'] / 20), lpc, age * u['droop'], u['ph0'])

def output_stage(mix, amp, age, drive, rng):
    """mix, amp at the 2x rate. returns the engine-rate signal, full scale = 1.0 (5 V)"""
    quiet = max(0.0, age - 0.6) / 0.4
    a = amp + quiet * 0.0008
    g = 0.30 * (1.0 + 2.5 * drive); b = 0.10 * age + 0.12 * drive
    x = (np.tanh(mix * a * g + b) - np.tanh(b)) / (0.75 + 1.35 * drive)
    x = x + quiet * 2.2e-4 * np.sqrt(OS) * rng.standard_normal(len(x))
    y = resample_poly(x, 1, OS)
    c = np.exp(-2 * np.pi * 8.0 / SR)
    return lfilter([1.0, -1.0], [1.0, -c], y)

def render(pitch, spread, timbre, rich, amp, age=0.6, drive=0.35, voicing=0, seed=1, mono=False, oscs=False, noise=True):
    """noise=False removes drift and jitter (for the deterministic C++ agreement test)"""
    N = len(pitch); n = N * OS
    up = lambda x: np.full(n, float(x)) if np.ndim(x) == 0 else np.repeat(np.asarray(x, float), OS)[:n]
    ua = unit_arrays(seed, age)
    o = voice(up(pitch), np.clip(up(spread), 0, 1), np.clip(up(timbre), 0, 1), np.clip(up(rich), 0, 1),
              VOICINGS[voicing], age, drive, seed & 0x7FFFFFFF, *ua, float(SR * OS),
              1.0 if noise else 0.0, 1.0 if noise else 0.0)
    if oscs: return o
    rng = np.random.default_rng(seed + 77)
    a = np.clip(up(amp), 0, 1)
    if mono:
        m = o.sum(0) * np.sqrt(0.5)
        y = output_stage(m, a, age, drive, rng)
        return np.stack([y, y], axis=1)
    gl = np.cos((PANS + 1) * np.pi / 4); gr = np.sin((PANS + 1) * np.pi / 4)
    return np.stack([output_stage(gl @ o, a, age, drive, rng), output_stage(gr @ o, a, age, drive, rng)], axis=1)

@njit(cache=True)
def _env(gate, rise, fall, age, seed32, sr):
    np.random.seed(seed32)
    n = len(gate); e = np.zeros(n); endp = np.zeros(n)
    c = 0.6 * age
    nr = 1.0; nf = 1.0
    if c > 1e-4:
        nr = np.log((1 + 0.8 * c) / (1 - 0.5 * c)) / (1.3 * c)
        nf = np.log((1 + 1.2 * c) / (1 - c)) / (2.2 * c)
    v = 0.0; stage = 0; high = False; hold = 0; sc = 1.0
    for i in range(n):
        g = gate[i]
        if not high and g >= 1.0:
            high = True; stage = 1; sc = 1.0 + age * 0.06 * (2 * np.random.rand() - 1)
        elif high and g < 0.5:
            high = False
        if stage == 1:
            v += (1.0 + c * (1.3 * (1 - v) - 0.5)) * nr / (rise[i] * sc * sr)
            if v >= 1.0: v = 1.0; stage = 2
        elif stage == 2:
            if not high: stage = 3
        elif stage == 3:
            v -= (1.0 + c * (2.2 * v - 1.0)) * nf / (fall[i] * sc * sr)
            if v <= 0.0: v = 0.0; stage = 0; hold = int(0.001 * sr)
        if hold > 0: endp[i] = 1.0; hold -= 1
        e[i] = v
    return e, endp

def envelope(gate, rise, fall, age=0.6, seed=5):
    """gate in volts. rise, fall in seconds (scalars or arrays). returns envelope 0..1 and end pulse 0/1"""
    n = len(gate)
    r = np.broadcast_to(np.asarray(rise, float), (n,)).copy(); f = np.broadcast_to(np.asarray(fall, float), (n,)).copy()
    return _env(np.asarray(gate, float), r, f, age, seed, float(SR))

def write_wav(path, x):
    import wave
    x = np.clip(x, -1, 1); d = (x * 32767).astype('<i2')
    with wave.open(path, 'wb') as w:
        w.setnchannels(d.shape[1]); w.setsampwidth(2); w.setframerate(SR); w.writeframes(d.tobytes())
