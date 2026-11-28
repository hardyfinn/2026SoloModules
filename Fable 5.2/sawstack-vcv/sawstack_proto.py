"""Sawtooth-stack voice prototype (sketch quality: 8x oversampled naive waveforms).
Five triangle-core oscillators -> sine/saw shaper (TIMBRE) with saw multiplier (RICHNESS)
-> stereo spread -> VCA. Two linear rise/fall envelopes with end pulses.
All control inputs are arrays at the audio rate SR (they are upsampled inside)."""
import numpy as np
from scipy.signal import resample_poly

SR = 48000
OS = 8
# SPREAD: octave offsets of the five oscillators per unit of spread amount
RATIOS = np.array([-2.0, -1.0, 0.0, 1.0, 2.0])
# pan positions, -1 left .. +1 right, in the same order (interleaved so the bass is not on one side)
PANS = np.array([-0.45, 0.9, 0.0, -0.9, 0.45])
SPREAD_MAX_OCT = 1.0      # inner pair +-1 octave, outer pair +-2 octaves at full
RICH_MAX = 8.0            # saw multiplied up to 8x

def spread_law(x):
    """knob 0..1 -> octaves for the inner pair. Bottom third is fine detune."""
    x = np.clip(x, 0, 1)
    return SPREAD_MAX_OCT * x ** 3

def drift(n, rng, cents=2.5, rate=0.4):
    """slow random pitch wander per oscillator, in octaves (smoothed noise) plus a fixed offset"""
    m = n // 2000 + 4
    pts = rng.standard_normal(m)
    a = np.exp(-rate * 2000 / (SR * OS))
    for i in range(1, m):
        pts[i] = a * pts[i - 1] + np.sqrt(1 - a * a) * pts[i]
    d = np.interp(np.arange(n), np.arange(m) * 2000, pts)
    return (d * cents + rng.uniform(-1.5, 1.5)) / 1200.0

def up(x, n):
    x = np.asarray(x, dtype=np.float64)
    if x.ndim == 0:
        return np.full(n, float(x))
    return np.repeat(x, OS)[:n]

def render(pitch_oct, spread, timbre, richness, amp, seed=1, width=1.0, f0=261.626):
    """pitch_oct: V/oct relative to f0. spread/timbre/richness/amp: 0..1 (may exceed for CV)."""
    N = len(np.atleast_1d(amp)) if np.ndim(amp) else len(pitch_oct)
    n = N * OS
    rng = np.random.default_rng(seed)
    p = up(pitch_oct, n); s = spread_law(up(spread, n))
    t = np.clip(up(timbre, n), 0, 1); k = 1.0 + (RICH_MAX - 1.0) * np.clip(up(richness, n), 0, 1) ** 2
    L = np.zeros(n); R = np.zeros(n)
    for i in range(5):
        # small tracking error on the spread per oscillator so wide settings still beat
        track = 1.0 + rng.uniform(-0.004, 0.004)
        octv = p + RATIOS[i] * s * track + drift(n, rng)
        f = f0 * 2.0 ** octv
        f = np.minimum(f, 20000.0)
        ph = (np.cumsum(f) / (SR * OS) + rng.uniform()) % 1.0
        sine = np.sin(2 * np.pi * ph)
        x = k * (2 * ph - 1)
        saw = ((x + 1) % 2) - 1          # amplified saw wrapped back into range
        # crossfade; saw scaled so loudness stays roughly even
        y = (1 - t) * sine + t * 0.8 * saw
        pan = PANS[i] * width
        gl = np.cos((pan + 1) * np.pi / 4); gr = np.sin((pan + 1) * np.pi / 4)
        L += y * gl; R += y * gr
    a = up(amp, n)
    L *= a * 0.36; R *= a * 0.36
    L = np.tanh(L * 1.1) / 1.1; R = np.tanh(R * 1.1) / 1.1
    out = np.stack([resample_poly(L, 1, OS), resample_poly(R, 1, OS)], axis=1)
    return out

def envelope(gate, rise, fall):
    """linear rise/fall, rise always completes, holds while gate is high. returns env 0..1 and end pulse"""
    n = len(gate); e = np.zeros(n); endp = np.zeros(n)
    v = 0.0; stage = 0; prev = 0.0; hold = 0
    rise = np.broadcast_to(rise, (n,)); fall = np.broadcast_to(fall, (n,))
    for i in range(n):
        g = gate[i]
        if g > 0.5 and prev <= 0.5:
            stage = 1
        prev = g
        if stage == 1:
            v += 1.0 / (rise[i] * SR)
            if v >= 1.0:
                v = 1.0; stage = 2
        elif stage == 2:
            if g <= 0.5:
                stage = 3
        elif stage == 3:
            v -= 1.0 / (fall[i] * SR)
            if v <= 0.0:
                v = 0.0; stage = 0; hold = int(0.001 * SR)
        if hold > 0:
            endp[i] = 1.0; hold -= 1
        e[i] = v
    return e, endp

def write_wav(path, x):
    import wave
    x = np.clip(x, -1, 1)
    d = (x * 32767).astype('<i2')
    with wave.open(path, 'wb') as w:
        w.setnchannels(d.shape[1]); w.setsampwidth(2); w.setframerate(SR); w.writeframes(d.tobytes())
