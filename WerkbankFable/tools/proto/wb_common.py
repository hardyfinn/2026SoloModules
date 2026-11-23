"""Werkbank reference engines: shared pieces. Design by Fable, 2026-11-23.
Everything here is per-sample and written to be ported line for line to C++.
Audio is in "units": 1.0 = 5 V at the module output unless a module says otherwise."""
import numpy as np
from numba import njit

SR = 48000
C4 = 261.6256
MASK = (1 << 64) - 1

def splitmix(state):
    """Same generator as Sawstack. Returns (new state, uniform 0..1)."""
    state = (state + 0x9E3779B97F4A7C15) & MASK
    z = state
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & MASK
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & MASK
    z = z ^ (z >> 31)
    return state, (z >> 11) / float(1 << 53)

def unit(seed, n):
    """n bipolar uniforms (-1..1) that define one unit's personality at AGE = 1."""
    st = seed & MASK
    out = np.zeros(n)
    for i in range(n):
        st, u = splitmix(st)
        out[i] = 2.0 * u - 1.0
    return out

@njit(cache=True)
def xs32(s):
    """xorshift32. s is a uint32 held in an int64. Returns (state, uniform -1..1)."""
    s ^= (s << 13) & 0xFFFFFFFF
    s ^= s >> 17
    s ^= (s << 5) & 0xFFFFFFFF
    return s, (s / 2147483648.0) - 1.0

@njit(cache=True)
def blep(t, dt):
    """polyBLEP residual for a unit downward step at phase 0. t = phase 0..1, dt = phase increment."""
    if t < dt:
        x = t / dt
        return x + x - x * x - 1.0
    if t > 1.0 - dt:
        x = (t - 1.0) / dt
        return x * x + x + x + 1.0
    return 0.0

# Slow drift: two one-pole filtered noises (0.3 s and 8 s), each normalised to about +-1 peak.
# state d = [fast, slow]; call once per sample; returns value about -1..1.
@njit(cache=True)
def drift_tick(d, r, fs):
    a1 = 1.0 / (0.3 * fs); a2 = 1.0 / (8.0 * fs)
    d[0] += a1 * (r * np.sqrt(3.0 / a1) * 0.33 - d[0])
    d[1] += a2 * (r * np.sqrt(3.0 / a2) * 0.33 - d[1])
    return 0.4 * d[0] + 0.6 * d[1]
