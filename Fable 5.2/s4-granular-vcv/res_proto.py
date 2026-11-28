"""Prototype of the 48-band tuned resonator (design stage, Fable 2026-11-24).
Parallel bank of 2-pole band-passes (TPT state-variable), alternating polarity,
band gains drawn from a filter curve. Offline, per-sample loop vectorised over bands."""
import numpy as np
SR = 44100
SCALES = {
 'chromatic':[0,1,2,3,4,5,6,7,8,9,10,11],'major':[0,2,4,5,7,9,11],'minor':[0,2,3,5,7,8,10],
 'dorian':[0,2,3,5,7,9,10],'lydian':[0,2,4,6,7,9,11],'super_locrian':[0,1,3,4,6,8,10],
 'mixolydian':[0,2,4,5,7,9,10],'hex_aeolian':[0,3,5,7,8,10],'hex_dorian':[0,2,3,5,7,10],
 'blues':[0,3,5,6,7,10],'pentatonic':[0,3,5,7,10],'hirajoshi':[0,2,3,7,8],'kumoi':[0,2,3,7,9],
 'iwato':[0,1,5,6,10],'whole_tone':[0,2,4,6,8,10],'pelog':[0,1,3,7,8],'tetratonic':[0,4,7,11],
 'fifths':[0,7],'octaves':[0]}
C1 = 32.703
NB = 48

def band_notes(scale='chromatic'):
    """Semitones above C1 for each band. Base grid: 6 bands an octave over 8 octaves,
    even pitch classes in even octaves and odd ones in odd octaves, so chromatic
    holds all 12 notes. Other scales: each band snaps to the nearest scale note, duplicates merge."""
    base = [12*o + 2*i + (o % 2) for o in range(8) for i in range(6)]
    if scale == 'chromatic' or scale == 'free': return np.array(base, float)
    deg = SCALES[scale]; out = []
    for b in base:
        cands = [12*o + d for o in range(-1, 10) for d in deg]
        n = min(cands, key=lambda c: (abs(c-b), -c))
        if n not in out and n >= 0: out.append(n)
    return np.array(sorted(out), float)

def curve(f, cutoff, res, slope, tone):
    """Zero-phase band gains: magnitude of a 2-pole LP/BP/HP at `cutoff`, morphed by slope
    (-1 LP, 0 BP, +1 HP), resonance 0..1, then a tilt about 1 kHz."""
    q = 0.707*(1-res) + 12*res**2
    w = f/cutoff
    den = np.sqrt((1-w*w)**2 + (w/q)**2)
    lp = 1/den; hp = w*w/den; bp = (w/q)/den        # bp peaks at 1
    if slope <= 0: g = -slope*lp + (1+slope)*bp
    else:          g = slope*hp + (1-slope)*bp
    g = g*(f/1000.0)**(tone*0.5)
    return g

def q_for(f, decay):
    """decay 0..1. At 0 the bands are just wide enough to sum flat (Q0). Above that, ring time
    T60 grows from 0.05 s to 20 s, shorter for higher bands."""
    Q0 = 11.0
    if decay <= 0: return np.full_like(f, Q0)
    T = 0.05*(400.0)**decay * (f/261.6)**-0.3
    return np.maximum(Q0, np.pi*f*T/6.91)

def process(x, scale='chromatic', pitch=0.0, cutoff=20000.0, res=0.0, slope=-1.0, tone=0.0,
            decay=0.0, mode='sustain', wet=1.0, detune=None, makeup=True, age=0.0, seed=1):
    """x mono (n,). Returns mono out."""
    notes = band_notes(scale) + pitch
    f = C1*2**(notes/12.0)
    if detune is not None: f = f*2**(detune[:len(f)]/1200.0)
    arng = np.random.default_rng(seed)
    e_tune = arng.normal(0, 1, NB); e_q = arng.normal(0, 1, NB); e_g = arng.normal(0, 1, NB)   # fixed per unit
    if age > 0: f = f*2**(18.0*age*e_tune[:len(f)]/1200.0)                                      # 18 cents rms at full AGE
    keep = f < 0.45*SR; f = f[keep]; K = len(f)
    Q = q_for(f, decay)
    g = curve(f, cutoff, res, slope, tone)
    xb = x
    if age > 0:
        Q = np.maximum(3.0, Q*2**(0.4*age*e_q[:K])); g = g*10**(2.0*age*e_g[:K]/20)             # ring time and level scatter
        xb = np.tanh(x*(1+3*age))/(1+3*age) + arng.normal(0, 10**((-90+36*age)/20), len(x))     # drive and hiss into the bank
    pol = np.where(np.arange(K) % 2 == 0, 1.0, -1.0) if ALT else np.ones(K)
    Q0 = 11.0
    mk = (Q/Q0)**0.25 if makeup else 1.0
    G = np.tan(np.pi*f/SR); R = 1/(2*Q); d = 1/(1 + 2*R*G + G*G)
    s1 = np.zeros(K); s2 = np.zeros(K)
    gin = g if mode == 'sustain' else np.ones(K)
    gout = (np.ones(K) if mode == 'sustain' else g)*pol*mk
    # end caps: a 2-pole low-pass below the first band and a 2-pole high-pass above the last
    flo, fhi = f[0]*2**(-CAPST/12), f[-1]*2**(CAPST/12)
    glo = curve(np.array([flo/2]), cutoff, res, slope, tone)[0]*CAPLO
    ghi = curve(np.array([min(fhi*1.5, 0.45*SR)]), cutoff, res, slope, tone)[0]*CAPHI*(pol[-1]*CAPSIGN)
    Gc = np.tan(np.pi*np.array([flo, min(fhi, 0.45*SR)])/SR); Rc = 1/(2*CAPQ); dc = 1/(1 + 2*Rc*Gc + Gc*Gc)
    c1 = np.zeros(2); c2 = np.zeros(2)
    BG = 10**(1.7/20)
    y = np.zeros(len(x))
    for n in range(len(x)):
        xn = xb[n]
        v = gin*xn
        hp = (v - (2*R + G)*s1 - s2)*d
        bp = G*hp + s1; s1 = G*hp + bp
        lp = G*bp + s2; s2 = G*bp + lp
        ch = (xn - (2*Rc + Gc)*c1 - c2)*dc
        cb = Gc*ch + c1; c1 = Gc*ch + cb
        cl = Gc*cb + c2; c2 = Gc*cb + cl
        y[n] = BG*np.dot(gout, 2*R*bp) + glo*CAPLOSIGN*cl[0] + ghi*ch[1]
    return x*(1-wet) + y*wet, f
CAPLO = 0.7; CAPHI = 0.7; ALT = True; CAPST = 4.0; CAPSIGN = -1.0; CAPLOSIGN = -1.0; CAPQ = 1.0
