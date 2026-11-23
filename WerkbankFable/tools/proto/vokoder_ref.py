"""VOKODER reference engine: 20-band channel vocoder with voiced/unvoiced switching."""
import numpy as np
from numba import njit
from wb_common import *

NB = 20
F_LO, F_HI = 100.0, 8000.0
EDGES = np.geomspace(F_LO, F_HI, NB + 1)
CENTRES = np.sqrt(EDGES[:-1] * EDGES[1:])          # 111.6 Hz ... 7170 Hz, ratio 1.2447
QBAND = 3.6

@njit(cache=True)
def bp_coef(f, q, fs):
    """RBJ band-pass, 0 dB peak. Returns b0, b2 (b1 = 0), a1, a2 normalised."""
    w = 2.0 * np.pi * min(f, 0.45 * fs) / fs
    al = np.sin(w) / (2.0 * q); a0 = 1.0 + al
    return al / a0, -al / a0, -2.0 * np.cos(w) / a0, (1.0 - al) / a0

@njit(cache=True)
def vok_render(mod, car, centres, sliders, attack, release, shift, sib, thresh, freeze, age, U, seed32, fs):
    """mod, car: modulator and carrier at the engine rate, in units (1.0 = 5 V).
    sliders[20] linear gains (1 = centre). attack, release seconds. shift = FORMANT in bands (-5..5).
    sib 0..1 = SIBILANCE noise level. thresh = VOICING knob 0..1 (0.5 = neutral). freeze[n] 0/1.
    U = unit(seed, 60): per band [centre error, Q error, gain error].
    Returns (out, voiced flag 0..1, speech envelope 0..1)."""
    N = len(mod); out = np.zeros(N); vflag = np.zeros(N); senv = np.zeros(N)
    ca = np.zeros((NB, 4)); cs = np.zeros((NB, 4)); gb = np.zeros(NB)
    for i in range(NB):
        fc = centres[i] * (1.0 + age * 0.03 * U[3 * i]); q = QBAND * (1.0 + age * 0.15 * U[3 * i + 1])
        ca[i, 0], ca[i, 1], ca[i, 2], ca[i, 3] = bp_coef(fc, q, fs)
        fc2 = centres[i] * (1.0 - age * 0.03 * U[3 * i]); 
        cs[i, 0], cs[i, 1], cs[i, 2], cs[i, 3] = bp_coef(fc2, q, fs)
        gb[i] = sliders[i] * 10.0 ** (age * 1.5 * U[3 * i + 2] / 20.0) * (1.0 if i % 2 == 0 else -1.0)   # alternate polarity
    za = np.zeros((NB, 8)); zs = np.zeros((NB, 8))     # two cascaded biquads each: x1 x2 y1 y2 twice
    e1 = np.zeros(NB); e2 = np.zeros(NB); es = np.zeros(NB)
    kat = 1.0 - np.exp(-1.0 / (attack * fs)); krl = 1.0 - np.exp(-1.0 / (release * fs))
    ksm = 1.0 - np.exp(-1.0 / ((0.003 - 0.002 * age) * fs))       # ripple filter: 3 ms, 1 ms at AGE 1
    # pre-emphasis of the modulator: first-order high shelf, +6 dB above 2 kHz
    pz = 0.0; kpe = 1.0 - np.exp(-2.0 * np.pi * 2000.0 / fs)
    # voiced/unvoiced detector: energy above 3 kHz against energy below 800 Hz (state-variable filters, k = 1.414)
    gh = np.tan(np.pi * 3000.0 / fs); gl = np.tan(np.pi * 800.0 / fs)
    h1 = 0.0; h2 = 0.0; l1 = 0.0; l2 = 0.0; eh = 0.0; el = 0.0; uv = 0.0; sp = 0.0
    kd = 1.0 - np.exp(-1.0 / (0.008 * fs)); kuv = 1.0 - np.exp(-1.0 / (0.004 * fs))
    ratio = 0.5 * 10.0 ** (1.2 * (thresh - 0.5))                  # 0.5 neutral -> HF must exceed 0.5 x LF
    floor = 0.004
    rs = np.int64(seed32)
    nh1 = 0.0; nh2 = 0.0; gn = np.tan(np.pi * 2500.0 / fs)        # sibilance noise is high-passed at 2.5 kHz before the bank
    bleed = 0.0 if age <= 0.6 else (age - 0.6) / 0.4 * 0.003
    hiss = 0.0 if age <= 0.6 else (age - 0.6) / 0.4 * 1.5e-4
    for n in range(N):
        m = mod[n]
        pz += kpe * (m - pz); mp = m + 1.0 * (m - pz)
        # detector
        k = 1.414
        a1 = 1.0 / (1.0 + gh * (gh + k)); a2 = gh * a1; a3 = gh * a2
        v3 = m - h2; v1 = a1 * h1 + a2 * v3; v2 = h2 + a2 * h1 + a3 * v3; h1 = 2 * v1 - h1; h2 = 2 * v2 - h2
        hf = m - k * v1 - v2
        a1 = 1.0 / (1.0 + gl * (gl + k)); a2 = gl * a1; a3 = gl * a2
        v3 = m - l2; v1 = a1 * l1 + a2 * v3; v2 = l2 + a2 * l1 + a3 * v3; l1 = 2 * v1 - l1; l2 = 2 * v2 - l2
        lf = v2
        eh += kd * (abs(hf) - eh); el += kd * (abs(lf) - el)
        tgt = 1.0 if (eh > ratio * el and eh > floor) else 0.0
        uv += kuv * (tgt - uv)
        sp += kd * ((1.0 if (eh + el) > floor else 0.0) - sp)
        vflag[n] = 1.0 - uv; senv[n] = min(1.0, 4.0 * (eh + el))
        # carrier with noise substitution
        rs, r = xs32(rs)
        a1 = 1.0 / (1.0 + gn * (gn + k)); a2 = gn * a1; a3 = gn * a2
        v3 = r - nh2; v1 = a1 * nh1 + a2 * v3; v2 = nh2 + a2 * nh1 + a3 * v3; nh1 = 2 * v1 - nh1; nh2 = 2 * v2 - nh2
        nz = r - k * v1 - v2
        c = (1.0 - uv) * car[n] + uv * sib * 1.4 * nz
        y = 0.0
        frz = freeze[n] > 0.5
        for i in range(NB):
            # analysis: two cascaded biquads (direct form 1)
            x = mp
            for sgi in range(2):
                o4 = 4 * sgi
                yy = ca[i, 0] * x + ca[i, 1] * za[i, o4 + 1] - ca[i, 2] * za[i, o4 + 2] - ca[i, 3] * za[i, o4 + 3]
                za[i, o4 + 1] = za[i, o4]; za[i, o4] = x; za[i, o4 + 3] = za[i, o4 + 2]; za[i, o4 + 2] = yy; x = yy
            if not frz:
                rect = abs(x)
                e1[i] += (kat if rect > e1[i] else krl) * (rect - e1[i])
                e2[i] += ksm * (e1[i] - e2[i])
        for i in range(NB):
            # FORMANT shift: synthesis band i takes the envelope of analysis band i - shift (linear interpolation)
            p = i - shift
            if p <= 0.0: e = e2[0]
            elif p >= NB - 1: e = e2[NB - 1]
            else:
                j = int(p); fr = p - j; e = e2[j] * (1.0 - fr) + e2[j + 1] * fr
            x = c
            for sgi in range(2):
                o4 = 4 * sgi
                yy = cs[i, 0] * x + cs[i, 1] * zs[i, o4 + 1] - cs[i, 2] * zs[i, o4 + 2] - cs[i, 3] * zs[i, o4 + 3]
                zs[i, o4 + 1] = zs[i, o4]; zs[i, o4] = x; zs[i, o4 + 3] = zs[i, o4 + 2]; zs[i, o4 + 2] = yy; x = yy
            y += gb[i] * x * e
            es[i] = e
        rs, r2 = xs32(rs)
        out[n] = np.tanh(0.5 * (5.0 * y * sp + bleed * car[n])) * 2.0 + hiss * r2
    return out, vflag, senv

@njit(cache=True)
def carrier(pitch, shape, pw, chord, fs):
    """Internal carrier, one note: saw <-> pulse blend. chord: 0 none, 1 fifth (+7), 2 minor (+3 +7), 3 major (+4 +7).
    pitch[n] octaves from C4. Level: one note peaks at 1.0."""
    N = len(pitch); o = np.zeros(N); ph = np.zeros(3); semi = np.zeros(3); nn = 1
    if chord == 1: nn = 2; semi[1] = 7
    if chord == 2: nn = 3; semi[1] = 3; semi[2] = 7
    if chord == 3: nn = 3; semi[1] = 4; semi[2] = 7
    for i in range(3): ph[i] = 0.31 * i
    for n in range(N):
        y = 0.0
        for i in range(nn):
            dt = min(C4 * 2.0 ** (pitch[n] + semi[i] / 12.0) / fs, 0.45)
            ph[i] += dt
            if ph[i] >= 1.0: ph[i] -= 1.0
            saw = 2.0 * ph[i] - 1.0 - blep(ph[i], dt)
            p2 = ph[i] - pw
            if p2 < 0.0: p2 += 1.0
            pul = saw - (2.0 * p2 - 1.0 - blep(p2, dt)) + (2.0 * pw - 1.0)
            y += (1.0 - shape) * saw + shape * pul
        o[n] = y / np.sqrt(nn)
    return o
