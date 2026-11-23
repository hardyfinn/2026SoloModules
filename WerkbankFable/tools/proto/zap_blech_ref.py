"""ZAP and BLECH reference engines. Both are built on one element: a two-pole state-variable
filter (Cytomic/Zavalishin form) that is "kicked" by adding to its band-pass state.
    g = tan(pi f / fs); a1 = 1/(1+g(g+k)); a2 = g a1; a3 = g a2
    v3 = x - ic2; v1 = a1 ic1 + a2 v3; v2 = ic2 + a2 ic1 + a3 v3
    ic1 = 2 v1 - ic1; ic2 = 2 v2 - ic2;   band = v1, low = v2, high = x - k v1 - v2
Damping k: amplitude decays as exp(-pi f k t). Level-dependent damping k + nl*band^2 stands in for
the saturation of the real circuits and sets the self-oscillation level."""
import numpy as np
from numba import njit
from wb_common import *

# ------------------------------------------------------------------ ZAP
@njit(cache=True)
def zap_k(res):
    """RESONANCE 0..1 to damping. 0..0.9: k = 1.4 down to 0.0014 (exponential). 0.9..1: down to -0.02 (self-oscillation)."""
    if res <= 0.9:
        return 1.4 * 10.0 ** (-3.0 * res / 0.9)
    return 0.0014 + (res - 0.9) / 0.1 * (-0.02 - 0.0014)

@njit(cache=True)
def zap_render(trig, pitch, start_hz, end_hz, sweep, res, decay, noise, click, drive, age, U, seed32, fs):
    """trig[n] > 0 = a hit of that velocity (0..1.6) at sample n. pitch[n] = V/OCT in octaves.
    start_hz, end_hz 20..16000; sweep = pitch envelope time constant, s; decay = VCA time to -60 dB, s
    (decay >= 8 means the VCA stays open: drone). noise, click, drive, age 0..1. U = unit(seed, 4)."""
    N = len(trig); out = np.zeros(N)
    ic1 = 0.0; ic2 = 0.0; pe = 0.0; ae = 0.0; ne = 0.0; v1 = 0.0
    s = np.int64(seed32); d = np.zeros(2); jit = 1.0; lvl = 1.0
    cp = np.exp(-1.0 / (max(sweep, 1e-4) * fs))
    ca = 1.0 if decay >= 8.0 else np.exp(-6.908 / (decay * fs))
    cn = np.exp(-6.908 / (min(decay, 0.25) * fs))        # noise burst never longer than 250 ms
    k0 = zap_k(res)
    tune = 1.0 + age * 0.02 * U[0]
    lr = np.log(start_hz / end_hz)
    dg = 1.0 + 3.0 * drive; dn = np.tanh(dg)
    hiss = 0.0 if age <= 0.6 else (age - 0.6) / 0.4 * 2.0e-4
    for n in range(N):
        s, r = xs32(s)
        dr = drift_tick(d, r, fs)
        if trig[n] > 0.0:
            s, r1 = xs32(s); s, r2 = xs32(s)
            jit = 1.0 + age * 0.012 * r1                       # hit-to-hit pitch scatter
            lvl = 10.0 ** (age * 1.2 * r2 / 20.0)              # hit-to-hit level scatter, dB
            pe = 1.0; ae = trig[n] * lvl; ne = trig[n] * lvl
            ic1 += 2.0 * click * trig[n] * lvl                 # the kick
        f = end_hz * np.exp(lr * pe) * 2.0 ** pitch[n] * tune * jit * (1.0 + age * 0.004 * dr)
        f = min(max(f, 10.0), 0.45 * fs)
        pe *= cp
        g = np.tan(np.pi * f / fs)
        k = k0 + 0.05 * v1 * v1
        a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2
        x = 1.5 * noise * ne * r
        v3 = x - ic2; v1 = a1 * ic1 + a2 * v3; v2 = ic2 + a2 * ic1 + a3 * v3
        ic1 = 2.0 * v1 - ic1; ic2 = 2.0 * v2 - ic2
        y = v1 * ae
        ae *= ca; ne *= cn
        s, r3 = xs32(s)
        out[n] = np.tanh(dg * y) / dn + hiss * r3
    return out

# ------------------------------------------------------------------ BLECH
# Voice order: 0 BASS, 1 SNARE, 2 HAT, 3 CYMBAL, 4 TOM, 5 CLAVE.
# Per box, per voice: [tone f Hz, tone T60 s, bend (fraction), bend tau s, tone level,
#                      noise filter f Hz, noise filter k, noise T60 s, noise attack s, noise level, click level]
# Noise filter type is fixed by voice: SNARE band-pass then 900 Hz one-pole high-pass; HAT high-pass; CYMBAL band-pass
# then 3 kHz one-pole high-pass. BASS, TOM, CLAVE have no noise.
BOX = np.array([
 [  # BOX 1: bright Italian organ rhythm unit
  [58.0, 0.35, 0.90, 0.009, 1.00, 0, 0, 0, 0, 0, 0.12],
  [190.0, 0.09, 0.15, 0.006, 0.50, 4200.0, 1.20, 0.16, 0.0003, 0.80, 0.05],
  [0, 0, 0, 0, 0, 7500.0, 0.60, 0.045, 0.0002, 0.55, 0.0],
  [0, 0, 0, 0, 0, 6500.0, 0.90, 0.70, 0.003, 0.50, 0.0],
  [150.0, 0.18, 0.25, 0.015, 0.85, 0, 0, 0, 0, 0, 0.08],
  [2450.0, 0.040, 0.0, 0.01, 0.60, 0, 0, 0, 0, 0, 0.03]],
 [  # BOX 2: soft American "rhythm king" style
  [50.0, 0.50, 0.50, 0.012, 1.00, 0, 0, 0, 0, 0, 0.07],
  [165.0, 0.11, 0.10, 0.008, 0.60, 3000.0, 1.40, 0.20, 0.0005, 0.70, 0.04],
  [0, 0, 0, 0, 0, 6000.0, 0.90, 0.070, 0.0004, 0.50, 0.0],
  [0, 0, 0, 0, 0, 5000.0, 1.10, 0.90, 0.005, 0.45, 0.0],
  [110.0, 0.25, 0.18, 0.020, 0.90, 0, 0, 0, 0, 0, 0.06],
  [900.0, 0.050, 0.0, 0.01, 0.70, 0, 0, 0, 0, 0, 0.03]],
 [  # BOX 3: tight, tuned for sequencer-driven electro
  [66.0, 0.22, 1.80, 0.006, 1.00, 0, 0, 0, 0, 0, 0.16],
  [230.0, 0.06, 0.30, 0.004, 0.45, 5500.0, 1.00, 0.11, 0.0002, 0.85, 0.06],
  [0, 0, 0, 0, 0, 9000.0, 0.50, 0.030, 0.0002, 0.55, 0.0],
  [0, 0, 0, 0, 0, 8000.0, 0.80, 0.45, 0.002, 0.50, 0.0],
  [220.0, 0.12, 0.50, 0.010, 0.85, 0, 0, 0, 0, 0, 0.10],
  [3100.0, 0.025, 0.0, 0.01, 0.60, 0, 0, 0, 0, 0, 0.03]],
])

@njit(cache=True)
def blech_render(trig, box, tune, decay, dyn, ped_pitch, tone, age, U, seed32, fs):
    """trig[6, N] > 0 = hit (value = accent factor: 1.0 normal, 1.6 accented).
    box = BOX[i] (6 x 11). tune[6] octaves (-1..1), decay[6] multipliers (0.25..4).
    dyn 0..1 (DYNAMICS pedal; 0.7 when unpatched), ped_pitch octaves (-1..1), tone 0..1 (1 = open).
    U = unit(seed, 24): four per voice [tuning, decay, level, noise filter].
    Returns out[6, N]. The MIX output is the plain sum of the six."""
    N = trig.shape[1]; out = np.zeros((6, N))
    ic1 = np.zeros(6); ic2 = np.zeros(6); bv = np.zeros(6); pe = np.zeros(6)
    n1 = np.zeros(6); n2 = np.zeros(6)                 # noise filter state
    ne = np.zeros(6); na = np.zeros(6); nt = np.zeros(6)   # noise env, attack ramp, target
    hp = np.zeros(6); cl = np.zeros(6); clk = np.zeros(6); lp = np.zeros(6)
    lvl = np.ones(6); d = np.zeros(2); s = np.int64(seed32)
    vel0 = 0.25 + dyn
    ctone = 1.0 - np.exp(-2.0 * np.pi * min(600.0 * 30.0 ** tone, 0.45 * fs) / fs)
    cclk = 1.0 - np.exp(-2.0 * np.pi * 2500.0 / fs)
    hiss = 0.0 if age <= 0.6 else (age - 0.6) / 0.4 * 1.5e-4
    hum = 0.0 if age <= 0.6 else (age - 0.6) / 0.4 * 3.0e-4
    leak_all = 0.0
    for n in range(N):
        s, r = xs32(s)                                   # one noise source shared by SNARE, HAT, CYMBAL
        dr = drift_tick(d, r, fs)
        leak = 0.0
        for v in range(6):
            p = box[v]
            if trig[v, n] > 0.0:
                s, r1 = xs32(s)
                lvl[v] = 10.0 ** (age * (1.5 * r1 + 1.0 * U[4 * v + 2]) / 20.0)
                a = min(vel0 * trig[v, n], 1.8) * lvl[v]
                if p[0] > 0.0:
                    ic1[v] += 2.0 * a; pe[v] = 1.0
                nt[v] = a; na[v] = 0.0; ne[v] = 1.0
                clk[v] = a * round(0.001 * fs)           # 1 ms trigger pulse, counted in samples
            y = 0.0
            if p[0] > 0.0:                                # struck resonator
                T60 = p[1] * decay[v] * (1.0 - 0.25 * age * abs(U[4 * v + 1]))
                f = p[0] * 2.0 ** (tune[v] + ped_pitch) * (1.0 + age * 0.04 * U[4 * v]) * (1.0 + age * 0.006 * dr)
                f *= 1.0 + p[2] * pe[v] * min(nt[v], 1.5)
                pe[v] *= np.exp(-1.0 / (p[3] * fs))
                f = min(f, 0.45 * fs)
                g = np.tan(np.pi * f / fs)
                k = 6.908 / (np.pi * f * T60) + 0.03 * bv[v] * bv[v]
                a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2
                v3 = -ic2[v]; v1 = a1 * ic1[v] + a2 * v3; v2 = ic2[v] + a2 * ic1[v] + a3 * v3
                ic1[v] = 2.0 * v1 - ic1[v]; ic2[v] = 2.0 * v2 - ic2[v]; bv[v] = v1
                y += p[4] * v1
            c = 0.0                                       # trigger pulse feeding through
            if clk[v] > 0.0:
                c = nt[v]; clk[v] -= nt[v]
                if clk[v] < 1e-9: clk[v] = 0.0
            cl[v] += cclk * (c - cl[v])
            y += p[10] * cl[v]; leak += cl[v]
            if p[5] > 0.0:                                # gated, filtered noise
                T60 = p[7] * decay[v] * (1.0 - 0.25 * age * abs(U[4 * v + 1]))
                na[v] += (1.0 - na[v]) * (1.0 - np.exp(-1.0 / (p[8] * fs)))
                e = nt[v] * na[v] * ne[v]
                ne[v] *= np.exp(-6.908 / (T60 * fs))
                f = min(p[5] * 2.0 ** (0.5 * (tune[v] + ped_pitch)) * (1.0 + age * 0.08 * U[4 * v + 3]), 0.45 * fs)
                g = np.tan(np.pi * f / fs); k = p[6]
                a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2
                v3 = r - n2[v]; v1 = a1 * n1[v] + a2 * v3; v2 = n2[v] + a2 * n1[v] + a3 * v3
                n1[v] = 2.0 * v1 - n1[v]; n2[v] = 2.0 * v2 - n2[v]
                if v == 2:
                    z = r - k * v1 - v2                   # HAT: high-pass
                else:
                    z = v1                                # SNARE, CYMBAL: band-pass
                    fh = 900.0 if v == 1 else 3000.0
                    hp[v] += (1.0 - np.exp(-2.0 * np.pi * fh / fs)) * (z - hp[v]); z = z - hp[v]
                y += p[9] * e * z
            y = np.tanh(1.2 * y) / 0.8337                 # the output transistor: soft limit
            lp[v] += ctone * (y - lp[v])
            out[v, n] = lp[v]
        s, r4 = xs32(s)
        common = age * 0.02 * leak_all + hiss * r4 + hum * np.sin(2.0 * np.pi * 50.0 * n / fs)
        leak_all = leak
        for v in range(6):
            out[v, n] += common / 6.0
    return out
