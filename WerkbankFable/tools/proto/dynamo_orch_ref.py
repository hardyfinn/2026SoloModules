"""DYNAMO (mono/poly bass and lead voice) and ORCHESTER (optical-disc keyboard) reference engines."""
import numpy as np
from numba import njit
from scipy.signal import resample_poly
from wb_common import *

# ------------------------------------------------------------------ DYNAMO
# One voice. Oscillators and ladder run at 2x the engine rate; output is decimated by 2.
# P (scalars): 0 wave1 (0 saw, 1 pulse)  1 wave2  2 detune (semitones, osc 2)  3 pw (0.05..0.95)
#  4 mix (0 = osc 1 only, 1 = osc 2 only)  5 sub (0..1)  6 noise (0..1)  7 cutoff knob (0..1 -> 20 Hz * 1000^knob)
#  8 res (0..1)  9 env amount (0..1 -> 0..7 octaves)  10 key track (0..1)
#  11 fA  12 fD  13 fS  14 aA  15 aD  16 aS   (times in seconds, sustains 0..1)
#  17 glide (s, time constant)  18 drive (0..1)  19 age (0..1)
# Envelope: RC attack towards 1.25 (reaches 1.0, then decays towards S with time constant D/2.3);
# when the gate falls it decays towards 0 with the same decay time (release = decay, as on the original).
@njit(cache=True)
def dynamo_voice(pitch, gate, accent, cutcv, P, U, seed32, fs):
    """pitch[n] octaves from C4, gate[n] 0/1, accent[n] 0/1 (read when the gate rises), cutcv[n] octaves.
    All at the engine rate fs. U = unit(seed, 8). Returns the 2x-rate output (decimate by 2 afterwards)."""
    N = len(pitch); o = np.zeros(2 * N); f2 = 2.0 * fs
    age = P[19]
    ph1 = 0.3; ph2 = 0.7; phs = 0.1
    s = np.zeros(4); y4 = 0.0
    fe = 0.0; ae = 0.0; fst = 0; ast = 0; g0 = 0.0; acc = 0.0
    pg = pitch[0]; d = np.zeros(2); rs = np.int64(seed32)
    et = 1.0                                             # per-note envelope time error
    t1 = age * 4.0 * U[0] / 1200.0; t2 = age * 6.0 * U[1] / 1200.0
    vt = 1.0 + age * 0.004 * U[2]                        # V/oct scale error
    pwo = age * 0.04 * U[3]; cuo = age * 0.08 * U[4]     # pulse width and cutoff offsets (octaves for cutoff)
    crv = age * 0.08                                     # saw curvature
    hiss = 0.0 if age <= 0.6 else (age - 0.6) / 0.4 * 1.5e-4
    bleed = 0.0 if age <= 0.6 else (age - 0.6) / 0.4 * 6.0e-4
    cg = 0.0 if P[17] <= 1e-4 else np.exp(-1.0 / (P[17] * fs))
    dg = 0.5 + 2.5 * P[18]
    r = 4.3 * P[8]
    ns = np.sqrt(f2 / 96000.0)
    for n in range(N):
        rs, rn = xs32(rs)
        dr = drift_tick(d, rn, fs)
        # ---- control rate = engine rate
        if gate[n] > 0.5 and g0 <= 0.5:
            fst = 1; ast = 1; acc = accent[n]
            rs, r1 = xs32(rs); et = 1.0 + age * 0.10 * r1
        if gate[n] <= 0.5:
            fst = 0; ast = 0
        g0 = gate[n]
        if fst == 1:
            fe += (1.25 - fe) * (1.0 - np.exp(-1.0 / (P[11] * et * fs / 1.609)))   # 1.609 = ln(1.25/0.25): knob = time to reach 1
            if fe >= 1.0: fe = 1.0; fst = 2
        elif fst == 2:
            fe += (P[13] - fe) * (1.0 - np.exp(-2.3 / (P[12] * et * fs)))
        else:
            fe += (0.0 - fe) * (1.0 - np.exp(-2.3 / (P[12] * et * fs)))
        if ast == 1:
            ae += (1.25 - ae) * (1.0 - np.exp(-1.0 / (P[14] * et * fs / 1.609)))
            if ae >= 1.0: ae = 1.0; ast = 2
        elif ast == 2:
            ae += (P[16] - ae) * (1.0 - np.exp(-2.3 / (P[15] * et * fs)))
        else:
            ae += (0.0 - ae) * (1.0 - np.exp(-2.3 / (P[15] * et * fs)))
        pg = pitch[n] + cg * (pg - pitch[n])
        pv = pg * vt + age * (5.0 * dr) / 1200.0
        fr1 = C4 * 2.0 ** (pv + t1); fr2 = C4 * 2.0 ** (pv + t2 + P[2] / 12.0 + age * 3.0 * d[0] / 1200.0)
        d1 = min(fr1 / f2, 0.45); dd2 = min(fr2 / f2, 0.45); dsub = 0.5 * d1
        pw = min(max(P[3] + pwo, 0.05), 0.95)
        amt = P[9] * 7.0 * (1.0 + 0.5 * acc)
        fc = 20.0 * 1000.0 ** P[7] * 2.0 ** (amt * fe + P[10] * pg + cutcv[n] + cuo + age * 0.03 * dr)
        fc = min(max(fc, 10.0), 0.45 * f2)
        g = np.tan(np.pi * fc / f2); G = g / (1.0 + g)
        amp = ae * (1.0 + 0.4 * acc) + bleed
        for k in range(2):
            # osc 1
            ph1 += d1
            if ph1 >= 1.0: ph1 -= 1.0
            saw = 2.0 * ph1 - 1.0 - blep(ph1, d1)
            if P[0] < 0.5:
                x1 = saw + crv * (saw * saw - 0.3333)
            else:
                p2 = ph1 - pw
                if p2 < 0.0: p2 += 1.0
                x1 = saw - (2.0 * p2 - 1.0 - blep(p2, d1)) + (2.0 * pw - 1.0)
            # osc 2
            ph2 += dd2
            if ph2 >= 1.0: ph2 -= 1.0
            saw = 2.0 * ph2 - 1.0 - blep(ph2, dd2)
            if P[1] < 0.5:
                x2 = saw + crv * (saw * saw - 0.3333)
            else:
                p2 = ph2 - pw
                if p2 < 0.0: p2 += 1.0
                x2 = saw - (2.0 * p2 - 1.0 - blep(p2, dd2)) + (2.0 * pw - 1.0)
            # sub: square one octave below osc 1
            phs += dsub
            if phs >= 1.0: phs -= 1.0
            p2 = phs - 0.5
            if p2 < 0.0: p2 += 1.0
            sq = (2.0 * phs - 1.0 - blep(phs, dsub)) - (2.0 * p2 - 1.0 - blep(p2, dsub))
            rs, rn2 = xs32(rs)
            x = (1.0 - P[4]) * x1 + P[4] * x2 + P[5] * sq + 1.2 * P[6] * rn2 * ns
            # ladder: four one-pole sections, feedback through tanh, one sample of delay in the loop
            u = np.tanh(dg * (0.6 * x) - r * y4)
            for j in range(4):
                v = (u - s[j]) * G; u = v + s[j]; s[j] = u + v
            y4 = u
            o[2 * n + k] = y4 * (1.0 + 0.6 * r) * amp / (0.35 + 0.65 * np.tanh(dg * 0.6)) * 1.0 + hiss * rn2 * ns
    return o

def halfband():
    """Sawstack's 2:1 decimator: 47-tap Blackman-windowed half-band FIR with exact zeros."""
    N = 47; M = N - 1; n = np.arange(N); t = n - M / 2.0
    s = np.where(t == 0, 0.5, np.sin(np.pi * 0.5 * t) / (np.pi * np.where(t == 0, 1, t)))
    w = 0.42 - 0.5 * np.cos(2 * np.pi * n / M) + 0.08 * np.cos(4 * np.pi * n / M)
    h = s * w; h /= h.sum(); h[((n - M // 2) % 2 == 0) & (n != M // 2)] = 0.0
    return h

def decimate2(o):
    """Output sample k is the filter over input samples up to and including 2k+1 (as Decimator2.process(a, b))."""
    y = np.convolve(o, halfband())[:len(o)]
    return y[1::2]

def dynamo_render(pitch, gate, accent, cutcv, P, seed=1, fs=SR):
    o = dynamo_voice(pitch, gate, accent, cutcv, np.asarray(P, float), unit(seed, 8), seed & 0x7FFFFFFF | 1, float(fs))
    return decimate2(o)

# ------------------------------------------------------------------ ORCHESTER
REV = 1.8      # seconds per disc revolution (an assumption: no published figure)
# Disc definition, 24 numbers:
#  0 wave (0 saw, 1 pulse, 2 additive)  1 pulse width  2 singers (1..3)
#  3..5 detune cents per singer   6..8 vibrato Hz per singer   9..11 vibrato cents per singer
#  12 source one-pole low-pass Hz (0 = none)   13 direct level   14 breath noise level
#  15 playback low-pass Hz   16 level trim   17 octave shift   18 tremolo depth (level vibrato, tied to singer 0)
#  19..23 unused
# FORM: four band-pass formants per disc: f Hz, Q, gain.  ADD: 8 partial gains for wave 2.
DISCS = np.array([
 #  wave pw  n   detune          vib Hz           vib cents     srcLP dir  breath LP    trim oct trem
 [0, .5, 3, -9, 4, 13,     5.2, 5.8, 4.6,   18, 25, 14,   900, .05, .020, 4800, 1.0, 0, .00, 0,0,0,0,0],  # 0 CHOIR
 [0, .5, 3, -7, 0, 9,      5.5, 6.1, 5.0,   10, 12, 9,      0, .35, .000, 5500, 0.55, 0, .00, 0,0,0,0,0],  # 1 STRINGS
 [2, .5, 1, 0, 0, 0,       5.0, 0, 0,        8, 0, 0,        0, 1.0, .060, 5000, 0.9, 0, .08, 0,0,0,0,0],  # 2 FLUTE
 [2, .5, 1, 0, 0, 0,       0, 0, 0,          0, 0, 0,        0, 1.0, .000, 5200, 0.7, 0, .00, 0,0,0,0,0],  # 3 ORGAN
 [0, .5, 1, 0, 0, 0,       5.6, 0, 0,       22, 0, 0,     2500, .20, .010, 4500, 0.5, -1, .04, 0,0,0,0,0], # 4 CELLO
 [0, .5, 2, -5, 5, 0,      0, 0, 0,          0, 0, 0,      700, .30, .010, 4200, 0.55, 0, .00, 0,0,0,0,0],  # 5 HORN
 [0, .5, 1, 0, 0, 0,       5.8, 0, 0,       35, 0, 0,     1100, .03, .030, 5000, 1.0, 0, .06, 0,0,0,0,0],  # 6 VOX (solo voice)
 [1, .30, 1, 0, 0, 0,      5.0, 0, 0,       12, 0, 0,     3000, .15, .030, 4800, 0.42, 0, .03, 0,0,0,0,0],  # 7 REED
])
FORM = np.array([
 [[650, 5, 1.0], [1080, 7, .50], [2650, 9, .25], [3300, 9, .12]],
 [[290, 4, .60], [520, 4, .50], [1300, 3, .60], [2800, 3, .50]],
 [[0, 1, 0], [0, 1, 0], [0, 1, 0], [0, 1, 0]],
 [[0, 1, 0], [0, 1, 0], [0, 1, 0], [0, 1, 0]],
 [[180, 3, .70], [420, 4, .60], [900, 3, .30], [2200, 4, .25]],
 [[450, 3, 1.0], [900, 4, .30], [0, 1, 0], [0, 1, 0]],
 [[850, 5, 1.0], [1220, 7, .55], [2810, 9, .30], [3800, 9, .12]],
 [[600, 4, 1.0], [1500, 5, .50], [2700, 6, .30], [0, 1, 0]],
])
ADD = np.array([
 [0,0,0,0,0,0,0,0],[0,0,0,0,0,0,0,0],
 [1.0, .25, .08, .02, 0, 0, 0, 0],        # FLUTE
 [1.0, .80, .50, .40, 0, .25, 0, .20],    # ORGAN: partials 1 2 3 4 6 8
 [0,0,0,0,0,0,0,0],[0,0,0,0,0,0,0,0],[0,0,0,0,0,0,0,0],[0,0,0,0,0,0,0,0]])

def note_personality(disc, semitone):
    """Each note is its own recording. 8 bipolar uniforms from (disc, note): tuning, level, tilt, splice phase,
    vib phase x3, flutter phase."""
    return unit(0xD15C0000 + disc * 1000 + (int(semitone) + 500), 8)

@njit(cache=True)
def orch_voice(pitch, gate, D, F, A, NP, wobble, tone, attack, release, age, theta0, seed32, fs):
    """One key. pitch[n] octaves from C4, gate[n] 0/1. D, F, A = the disc rows. NP = note_personality.
    wobble 0..1 (0.5 = as found), tone 0..1 (0.5 = as found), attack/release seconds (0.005 / 0.03 = original).
    theta0 = disc angle 0..1 at sample 0: the angle is GLOBAL to the module, shared by every voice.
    Output is the voice before the shared hiss, in units."""
    N = len(pitch); o = np.zeros(N)
    ns = int(D[2]); ph = np.zeros(3)
    for i in range(3): ph[i] = 0.37 * i + 0.21
    fb1 = np.zeros(4); fb2 = np.zeros(4)
    sl = 0.0; l1 = 0.0; l2 = 0.0; hp = 0.0; env = 0.0; th = theta0
    b1 = 0.0; b2 = 0.0; thump = 0.0; rs = np.int64(seed32)
    wdepth = 0.0025 * (2.0 * wobble) ** 2 * (1.0 + 2.0 * age)       # pitch wobble fraction: 0.25 % at knob 0.5, AGE triples it
    fdepth = 0.10 * (2.0 * wobble) * (1.0 + 1.5 * age)               # level flutter: 10 % at knob 0.5
    flp = min(D[15] * 2.0 ** (3.0 * (tone - 0.5) + 0.3 * NP[2]) * (1.0 - 0.35 * age), 0.45 * fs)
    gl = np.tan(np.pi * flp / fs); kl = 1.3
    la1 = 1.0 / (1.0 + gl * (gl + kl)); la2 = gl * la1; la3 = gl * la2
    chp = 1.0 - np.exp(-2.0 * np.pi * 90.0 / fs)
    ca = 1.0 - np.exp(-1.0 / (max(attack, 0.001) * fs / 3.0)); cr = 1.0 - np.exp(-1.0 / (max(release, 0.001) * fs / 3.0))
    csrc = 1.0 if D[12] <= 0.0 else 1.0 - np.exp(-2.0 * np.pi * D[12] / fs)
    splice = 0.5 + 0.5 * NP[3]
    lvl = D[16] * 10.0 ** (1.5 * NP[1] / 20.0)
    dust = age * age * 6.0 / fs                                       # dust clicks per sample at AGE 1: 6 per second per voice
    fa = np.zeros((4, 3))
    for j in range(4):
        if F[j, 2] > 0.0:
            g = np.tan(np.pi * min(F[j, 0], 0.45 * fs) / fs); k = 1.0 / F[j, 1]
            fa[j, 0] = 1.0 / (1.0 + g * (g + k)); fa[j, 1] = g * fa[j, 0]; fa[j, 2] = g * fa[j, 1]
    nsc = np.sqrt(fs / 48000.0)
    cb1 = 1.0 - np.exp(-2.0 * np.pi * 2724.6 / fs); cb2 = 1.0 - np.exp(-2.0 * np.pi * 391.8 / fs)   # 0.30 and 0.05 at 48 kHz
    for n in range(N):
        th_prev = th
        th += 1.0 / (REV * fs)
        if th >= 1.0: th -= 1.0
        w = np.sin(2.0 * np.pi * th) + 0.3 * np.sin(4.0 * np.pi * th + 1.0)       # off-centre disc: shared by all keys
        base = C4 * 2.0 ** (pitch[n] + D[17] + 6.0 * NP[0] / 1200.0) * (1.0 + wdepth * w)
        x = 0.0
        for i in range(ns):
            vr = round(D[6 + i] * REV) / REV                           # vibrato locked to the revolution so the loop repeats
            vib = np.sin(2.0 * np.pi * (vr * REV * th + 0.5 * NP[4 + i]))
            f = base * 2.0 ** ((D[3 + i] + D[9 + i] * vib) / 1200.0)
            dt = min(f / fs, 0.45)
            ph[i] += dt
            if ph[i] >= 1.0: ph[i] -= 1.0
            if D[0] == 0.0:
                x += 2.0 * ph[i] - 1.0 - blep(ph[i], dt)
            elif D[0] == 1.0:
                p2 = ph[i] - D[1]
                if p2 < 0.0: p2 += 1.0
                x += (2.0 * ph[i] - 1.0 - blep(ph[i], dt)) - (2.0 * p2 - 1.0 - blep(p2, dt))
            else:
                for h in range(8):
                    if A[h] > 0.0 and (h + 1) * f < 0.45 * fs:
                        x += 0.6 * A[h] * np.sin(2.0 * np.pi * (h + 1) * ph[i])
            if i == 0:
                trem = 1.0 + D[18] * vib
        x = x / np.sqrt(ns) * trem
        rs, rn = xs32(rs)
        sl += csrc * (x - sl); x = sl
        y = D[13] * x
        for j in range(4):
            if F[j, 2] > 0.0:
                v3 = x - fb2[j]; v1 = fa[j, 0] * fb1[j] + fa[j, 1] * v3; v2 = fb2[j] + fa[j, 1] * fb1[j] + fa[j, 2] * v3
                fb1[j] = 2.0 * v1 - fb1[j]; fb2[j] = 2.0 * v2 - fb2[j]
                y += F[j, 2] * v1 / F[j, 1] * 3.0
        # breath / bow noise recorded with the note: band-passed around 2.5 kHz by a one-pole pair
        b1 += cb1 * (rn * nsc - b1); b2 += cb2 * (b1 - b2)
        y += D[14] * 4.0 * (b1 - b2)
        # optical playback
        y *= (1.0 + fdepth * np.sin(2.0 * np.pi * th + np.pi * NP[7]))
        if (th_prev < splice <= th) or (th < th_prev and (splice > th_prev or splice <= th)):
            thump = 0.05 + 0.10 * age                                  # the loop splice passes the lamp once per revolution
        rs, rd = xs32(rs)
        if (rd + 1.0) * 0.5 < dust:
            rs, rd2 = xs32(rs); thump += 0.25 * rd2
        y += thump; thump *= np.exp(-1.0 / (0.0015 * fs))
        v3 = y - l2; v1 = la1 * l1 + la2 * v3; v2 = l2 + la2 * l1 + la3 * v3
        l1 = 2.0 * v1 - l1; l2 = 2.0 * v2 - l2; y = v2
        hp += chp * (y - hp); y -= hp
        env += ((ca if gate[n] > 0.5 else cr)) * ((1.0 if gate[n] > 0.5 else 0.0) - env)
        o[n] = 0.5 * lvl * y * env
    return o

def orch_render(notes, disc, dur, wobble=0.5, tone=0.5, attack=0.005, release=0.03, age=0.25, fs=SR):
    """notes: list of (semitone from C4, start s, length s). Returns the mix with the shared hiss."""
    N = int(dur * fs); out = np.zeros(N); envsum = np.zeros(N)
    for i, (st, t0, ln) in enumerate(notes):
        pitch = np.full(N, st / 12.0); gate = np.zeros(N); gate[int(t0 * fs):int((t0 + ln) * fs)] = 1
        out += orch_voice(pitch, gate, DISCS[disc], FORM[disc], ADD[disc], note_personality(disc, st),
                          wobble, tone, attack, release, age, 0.0, 777 + 31 * i, float(fs))
        envsum += gate
    # shared photocell hiss and lamp hum, present while any key is down (follows the loudest key envelope in the C++)
    rng = np.random.default_rng(5)
    hiss = 10.0 ** ((-66.0 + 20.0 * age) / 20.0)
    hum = 0.0 if age <= 0.6 else (age - 0.6) / 0.4 * 4.0e-4
    t = np.arange(N) / fs
    out += np.minimum(envsum, 1.0) * (hiss * rng.uniform(-1, 1, N) * 1.73 + hum * np.sin(2 * np.pi * 100 * t))
    return out
