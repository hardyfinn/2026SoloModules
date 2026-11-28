"""Tapeizer reference engine. Fable, 2026-11-23.

This file is the definition of the sound. Everything here is causal and can be ported to C++
one stage at a time. Units: 1.0 = 0 VU = reference flux. Lengths in micrometres, tape speed in cm/s.

One deck pass, per track:
  NR encode -> record EQ (FIR) -> [x2 oversampling: tape curve, modulation noise, transport delay] ->
  playback loss (FIR) -> drop-outs -> head bump -> crosstalk -> hiss and hum -> NR decode
All tracks of one call share one tape and one transport.
"""
import numpy as np, numba as nb
from scipy.signal import resample_poly, butter, sosfilt, lfilter

db = lambda d: 10 ** (d / 20.0)

# ----------------------------------------------------------------------------- machines
# v: tape speed cm/s.  d0: head-to-tape spacing, new.  gap: playback gap.  w: track width mm.  pitch: track spacing mm.
# cap: most treble boost the record amp can give, dB.  hiss: dB re 0 VU, unweighted, no NR, tape II.
# wow: % weighted RMS, new.  dc, dp: capstan and pinch roller diameters mm.  bump: head bump Hz.  hp: low corner Hz.
# xt: neighbour crosstalk dB at 1 kHz.  reel: True for open reel (different tape table).
MACHINES = {
 # multitrack deck
 '4TR':      dict(v=4.76,  d0=0.20, gap=1.2, w=0.60, pitch=0.90, cap=12, hiss=-50, wow=0.07, dc=2.5, dp=11, bump=80,  hp=35, xt=-45, reel=False),
 '4TR HI':   dict(v=9.53,  d0=0.20, gap=1.2, w=0.60, pitch=0.90, cap=12, hiss=-52, wow=0.05, dc=2.5, dp=11, bump=110, hp=35, xt=-50, reel=False),
 '4TR C':    dict(v=9.53,  d0=0.20, gap=1.0, w=0.60, pitch=0.90, cap=16, hiss=-52, wow=0.05, dc=2.5, dp=11, bump=110, hp=35, xt=-50, reel=False),
 '8TR CASS': dict(v=9.53,  d0=0.20, gap=1.0, w=0.28, pitch=0.45, cap=13, hiss=-49, wow=0.04, dc=2.5, dp=11, bump=110, hp=35, xt=-44, reel=False),
 '8TR 7.5':  dict(v=19.05, d0=0.50, gap=3.0, w=0.45, pitch=0.80, cap=12, hiss=-55, wow=0.05, dc=6.0, dp=40, bump=50,  hp=28, xt=-48, reel=True),
 '8TR 15':   dict(v=38.1,  d0=0.50, gap=3.0, w=0.45, pitch=0.80, cap=12, hiss=-57, wow=0.06, dc=6.0, dp=40, bump=65,  hp=28, xt=-45, reel=True),
 # master deck
 'DECK':     dict(v=4.76,  d0=0.20, gap=1.2, w=0.60, pitch=0.90, cap=17, hiss=-51, wow=0.08, dc=2.5, dp=11, bump=70,  hp=25, xt=-40, reel=False),
 'BOOMBOX':  dict(v=4.76,  d0=0.45, gap=2.0, w=0.60, pitch=0.90, cap=9,  hiss=-43, wow=0.22, dc=2.0, dp=10, bump=110, hp=70, xt=-30, reel=False),
 '1/4 INCH': dict(v=19.05, d0=0.50, gap=3.0, w=2.00, pitch=4.00, cap=12, hiss=-61, wow=0.05, dc=6.0, dp=45, bump=45,  hp=20, xt=-55, reel=True),
}
# TAPE switch I / II / IV.  mol: level of 3% third harmonic, dB re 0 VU.  delta: effective recorded depth um.  hiss: dB offset.
TAPES_CASS = {'I': dict(mol=2.5, delta=2.0, hiss=4.0), 'II': dict(mol=3.0, delta=1.5, hiss=0.0), 'IV': dict(mol=7.5, delta=1.1, hiss=0.0)}
TAPES_REEL = {'I': dict(mol=6.0, delta=6.0, hiss=1.0), 'II': dict(mol=8.0, delta=6.0, hiss=0.0), 'IV': dict(mol=10.0, delta=6.0, hiss=-1.0)}

# ----------------------------------------------------------------------------- the unit (fixed errors of one machine)
MASK = (1 << 64) - 1
def splitmix(seed):
    s = seed & MASK
    while True:
        s = (s + 0x9E3779B97F4A7C15) & MASK
        z = s; z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & MASK; z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & MASK
        z ^= z >> 31
        yield (z >> 11) / float(1 << 53)

def make_unit(seed, C=10):
    """Fixed errors, each in -1..+1 (values at AGE 1). Draw order: per track c = 0..C-1: sens, cal; then az, bias."""
    g = splitmix(seed); u = lambda: 2 * next(g) - 1
    sens = np.zeros(C); cal = np.zeros(C)
    for c in range(C): sens[c] = u(); cal[c] = u()
    az = 1.0 if u() >= 0 else -1.0
    return dict(sens=sens, cal=cal, az=az, bias=u())

# ----------------------------------------------------------------------------- filters
def loss(f, v, delta, d, gap, wtan):
    """Playback wavelength loss, linear gain. v cm/s; delta, d, gap, wtan in um. d may be negative (under-bias)."""
    lam = v * 1e4 / np.maximum(f, 1.0); k = 2 * np.pi / lam
    thick = (1 - np.exp(-k * delta)) / (k * delta)
    return thick * np.minimum(np.exp(-k * d), db(4)) * np.abs(np.sinc(gap / lam)) * np.abs(np.sinc(wtan / lam))

def fir_design(H, fs, N=None):
    """Linear-phase FIR by frequency sampling. H(f) -> gain. 513 points 0..fs/2, Hann window. N = 63 up to 50 kHz, else 127."""
    if N is None: N = 63 if fs <= 50000 else 127
    Hk = H(np.arange(513) * fs / 1024.0)
    full = np.fft.irfft(Hk, 1024); h = np.empty(N); half = N // 2
    for i in range(N):
        n = i - half; h[i] = full[n % 1024] * (0.5 + 0.5 * np.cos(np.pi * n / (half + 1)))
    return h

def shelf1(x, fz, fp, fs):
    """First-order shelf (bilinear, pre-warped): zero at fz, pole at fp."""
    wz, wp = 2 * fs * np.tan(np.pi * fz / fs), 2 * fs * np.tan(np.pi * fp / fs)
    b = np.array([2 * fs + wz, wz - 2 * fs]) / wz; a = np.array([2 * fs + wp, wp - 2 * fs]) / wp
    return lfilter(b / a[0], a / a[0], x, axis=0)

def biquad_peak(f0, gain_db, Q, fs):
    A = 10 ** (gain_db / 40); w = 2 * np.pi * f0 / fs; al = np.sin(w) / (2 * Q)
    b = np.array([1 + al * A, -2 * np.cos(w), 1 - al * A]); a = np.array([1 + al / A, -2 * np.cos(w), 1 - al / A])
    return b / a[0], a / a[0]
def biquad_shelf(f0, gain_db, fs, high):
    """RBJ shelf, slope 1."""
    A = 10 ** (gain_db / 40); w = 2 * np.pi * f0 / fs; cs = np.cos(w); al = np.sin(w) / 2 * np.sqrt(2.0); s = 2 * np.sqrt(A) * al
    if high:
        b = np.array([A * ((A + 1) + (A - 1) * cs + s), -2 * A * ((A - 1) + (A + 1) * cs), A * ((A + 1) + (A - 1) * cs - s)])
        a = np.array([(A + 1) - (A - 1) * cs + s, 2 * ((A - 1) - (A + 1) * cs), (A + 1) - (A - 1) * cs - s])
    else:
        b = np.array([A * ((A + 1) - (A - 1) * cs + s), 2 * A * ((A - 1) - (A + 1) * cs), A * ((A + 1) - (A - 1) * cs - s)])
        a = np.array([(A + 1) + (A - 1) * cs + s, -2 * ((A - 1) + (A + 1) * cs), (A + 1) + (A - 1) * cs - s])
    return b / a[0], a / a[0]

def lp_noise(rng, n, fc, fs):
    """Unit-RMS gaussian noise through a 2-pole Butterworth low-pass. (RMS normalised analytically in C++.)"""
    x = sosfilt(butter(2, fc / (fs / 2), output='sos'), rng.standard_normal(n + int(fs)))[int(fs):]
    return x / (np.std(x) + 1e-12)
def bp_noise(rng, n, f1, f2, fs):
    x = sosfilt(butter(2, [f1 / (fs / 2), f2 / (fs / 2)], btype='band', output='sos'), rng.standard_normal(n + int(fs / 4)))[int(fs / 4):]
    return x / (np.std(x) + 1e-12)

# ----------------------------------------------------------------------------- noise reduction
@nb.njit(cache=True)
def _x2(x, encode, fs):
    """2:1 compander core (the pre/de-emphasis shelves are outside). Detector always looks at the tape-side signal."""
    n = len(x); y = np.empty(n); s = 1e-3
    ca = 1 - np.exp(-1 / (0.005 * fs)); cr = 1 - np.exp(-1 / (0.030 * fs))
    ah = 1 / (1 + 2 * np.pi * 40 / fs); al = 1 - np.exp(-2 * np.pi * 11000 / fs); asf = 1 / (1 + 2 * np.pi * 1600 / fs)
    hp = 0.0; xp = 0.0; lp = 0.0; sh = 0.0; shp = 0.0
    for i in range(n):
        r = np.sqrt(s)
        if r < 3e-4: r = 3e-4
        if encode:
            g = 1.0 / r
            if g > 30.0: g = 30.0
            o = x[i] * g; d = o
        else:
            d = x[i]; o = x[i] * r
        hp = ah * (hp + d - xp); xp = d              # 40 Hz high-pass
        lp += al * (hp - lp)                         # 11 kHz low-pass
        sh = asf * (sh + lp - shp); shp = lp         # 1.6 kHz high-pass, mixed in at x6: treble weighting
        w = lp + 6.0 * sh
        s += (ca if w * w > s else cr) * (w * w - s)
        y[i] = o
    return y

@nb.njit(cache=True)
def _slide(x, encode, fs, G, f0, thr):
    """One sliding-band stage. Side chain = G x one-pole high-pass whose corner rises with the side chain's own level."""
    n = len(x); y = np.empty(n); hp = 0.0; inp = 0.0; env = 0.0
    att = 1 - np.exp(-1 / (0.002 * fs)); rel = 1 - np.exp(-1 / (0.06 * fs))
    for i in range(n):
        fc = f0 * (1 + (env / thr) ** 2)
        if fc > 60000.0: fc = 60000.0
        a = 1 / (1 + 2 * np.pi * fc / fs)
        c = a * (hp - inp)
        if encode:
            u = x[i]; h = a * u + c; o = u + G * h
        else:
            o = (x[i] - G * c) / (1 + G * a); u = o; h = a * u + c      # zero-delay solve of y = x - side(y)
        hp = h; inp = u
        m = abs(G * h)
        env += (att if m > env else rel) * (m - env)
        y[i] = o
    return y

def _per(x, f, *a): return np.stack([f(np.ascontiguousarray(c), *a) for c in x.T], 1)
def nr_encode(x, kind, fs):
    if kind == 'X2': return _per(shelf1(x, 400, 1600, fs), _x2, True, fs)
    if kind == 'B': return _per(x, _slide, True, fs, 2.16, 1800.0, 0.03)
    if kind == 'C':          # anti-saturation shelf, then high-level stage, then low-level stage
        y = shelf1(x, 6300, 4000, fs)
        return _per(_per(y, _slide, True, fs, 2.16, 450.0, 0.03), _slide, True, fs, 2.16, 450.0, 0.003)
    return x
def nr_decode(x, kind, fs):
    if kind == 'X2': return shelf1(_per(x, _x2, False, fs), 1600, 400, fs)
    if kind == 'B': return _per(x, _slide, False, fs, 2.16, 1800.0, 0.03)
    if kind == 'C':
        y = _per(_per(x, _slide, False, fs, 2.16, 450.0, 0.003), _slide, False, fs, 2.16, 450.0, 0.03)
        return shelf1(y, 4000, 6300, fs)
    return x

# ----------------------------------------------------------------------------- tape core at 2 x fs
@nb.njit(cache=True)
def _core(up, Ms, dc, h, r, mod, mn, delay, skew, fs2):
    """Tape curve with a little memory, modulation noise, then the transport delay (cubic Lagrange read)."""
    N, C = up.shape; out = np.zeros((N, C)); t0 = np.tanh(dc / Ms)
    for c in range(C):
        p = 0.0; buf = np.empty(N)
        for i in range(N):
            u = up[i, c]
            if p < u - r: p = u - r                   # play operator (backlash) of half-width r
            elif p > u + r: p = u + r
            u = (1 - h) * u + h * p
            buf[i] = Ms * (np.tanh((u + dc) / Ms) - t0) * (1 + mn * mod[i, c])
        for i in range(N):
            d = (delay[i] + skew[i, c]) * fs2
            if d < 1.0: d = 1.0
            k = int(np.floor(d)); f = d - k; j = i - k
            if j - 2 < 0 or j + 1 >= N: continue
            x0 = buf[j + 1]; x1 = buf[j]; x2 = buf[j - 1]; x3 = buf[j - 2]      # x1 is the sample at delay k, f in [0,1) towards x2
            out[i, c] = (-f * (f - 1) * (f - 2) / 6 * x0 + (f + 1) * (f - 1) * (f - 2) / 2 * x1
                         - (f + 1) * f * (f - 2) / 2 * x2 + (f + 1) * f * (f - 1) / 6 * x3)
    return out

@nb.njit(cache=True)
def _dropouts(y, v_um, rate, mean_depth, contact, cn, fs, seed):
    """Extra head-to-tape spacing d(t) per track: two one-poles whose corner falls as d grows. cn: contact noise, [n, C]."""
    n, C = y.shape; np.random.seed(seed)
    for c in range(C):
        edge = 1.6 if (c == 0 or c == C - 1) else 1.0
        pr = rate * edge / fs; left = 0; L = 1; depth = 0.0; s1 = 0.0; s2 = 0.0
        for i in range(n):
            if left == 0 and np.random.random() < pr:
                L = int(fs * (0.004 + 0.086 * np.random.random())); left = L; depth = np.random.exponential(mean_depth)
            d = contact * abs(cn[i, c])
            if left > 0:
                ph = (L - left) / L; d += depth * (0.5 - 0.5 * np.cos(2 * np.pi * ph)); left -= 1
            fc = 0.086 * v_um / max(d, 1e-3)
            a = 1 - np.exp(-2 * np.pi * min(fc, 0.45 * fs * 4) / fs)
            if a > 1.0: a = 1.0
            s1 += a * (y[i, c] - s1); s2 += a * (s1 - s2); y[i, c] = s2
    return y

def deck(x, fs, machine, tape='II', rec_db=0.0, nr_enc=None, nr_dec=None, cal_db=0.0, bias=0.0, age=0.25, wear=0.0,
         wow=0.5, flutter=0.5, azimuth=0.0, dropout=0.5, hiss=0.5, unit_seed=1, noise_seed=1, hum_hz=50.0, quiet=False, strength=1.0):
    """One pass through one deck. x: [n, C] in 0 VU units (C tracks on one tape). Returns [n, C].
    wow, flutter, dropout, hiss: knobs 0..1, 0.5 = the machine's own amount. azimuth: knob -1..+1 = -20..+20 arc minutes.
    strength 0..1 scales every departure from a perfect machine: 0 is transparent, 1 is the real machine.
    quiet=True switches every random source off (for measurement). Latency: see latency()."""
    m = MACHINES[machine]; tp = (TAPES_REEL if m['reel'] else TAPES_CASS)[tape]
    x = np.atleast_2d(x.T).T.astype(float); n, C = x.shape
    rng = np.random.default_rng(noise_seed); U = make_unit(unit_seed)
    v = m['v']; v_um = v * 1e4; fs2 = 2 * fs
    kn = lambda k: (2 * k) ** 2                                   # knob law: 0.5 -> x1, 1 -> x4, 0 -> off
    S = float(strength); Sc = max(S, 0.02)
    # ---- age and wear
    bias_e = np.clip(bias + 0.3 * age * U['bias'], -1, 1)
    az = (U['az'] * (2 + 16 * age) + 20 * azimuth) / 60 * np.pi / 180
    d_head = m['d0'] + 0.35 * age + 0.3 * wear
    gap = m['gap'] * (1 + 0.6 * age)
    Ms = db(tp['mol'] - 2.5 * wear + 1.5 * bias_e) / 0.55
    ub = max(0.0, -bias_e) + 0.25 * age
    d_bias = 0.35 * bias_e if bias_e > 0 else 0.2 * bias_e        # over-bias dulls, under-bias brightens
    # ---- record
    g = db(rec_db); y = nr_encode(x * g, nr_enc, fs)
    nom = lambda f: loss(f, v, tp['delta'], m['d0'], m['gap'], 0.0)
    h_pre = fir_design(lambda f: (np.minimum(1 / nom(f), db(m['cap'])) / np.sqrt(1 + (f / 21000) ** 8)) ** S, fs)
    y = lfilter(h_pre, 1.0, y, axis=0)
    up = resample_poly(y, 2, 1, axis=0); N = len(up)              # C++: any good 2x half-band pair
    # ---- transport
    a = m['wow'] / 100 * (1 + 5 * age ** 1.5) * np.sqrt(2) / 1.3
    fcap = v * 10 / (np.pi * m['dc']); fpin = v * 10 / (np.pi * m['dp'])
    skew0 = 0.5 * (C - 1) * m['pitch'] * 1e3 * np.tan(40 / 60 * np.pi / 180) / v_um
    if quiet:
        delay = np.full(N, 0.0005 + skew0); weave = np.zeros(N); mod = np.zeros((N, C)); mn = 0.0
    else:
        leak = lambda z, tau: lfilter([1 / fs2], [1, -np.exp(-1 / (tau * fs2))], z)          # leaky integrator
        ph = 2 * np.pi * fcap * np.arange(N) / fs2 + rng.uniform(0, 6.28) + 0.6 * np.cumsum(lp_noise(rng, N, 0.5, fs2)) / fs2
        dly = kn(wow) * a * (0.7 / (2 * np.pi * fcap) * np.sin(ph)
                             + (0.6 + 1.5 * age) / (2 * np.pi * fpin) * np.sin(2 * np.pi * fpin * np.arange(N) / fs2 + rng.uniform(0, 6.28))
                             + (0.5 + 2.0 * age) * leak(lp_noise(rng, N, 1.2, fs2), 2.0))
        dly += kn(flutter) * (a * 0.7 * leak(bp_noise(rng, N, 8, 45, fs2), 2.0) + 1.5e-7 * (1 + 3 * age) * bp_noise(rng, N, 2500, 6000, fs2))
        sig = kn(wow) * a * (0.5 / (2 * np.pi * fcap) + (0.6 + 1.5 * age) * 0.71 / (2 * np.pi * fpin) + (0.5 + 2 * age) * 0.2) + kn(flutter) * a * 0.004
        delay = np.clip(0.0005 + skew0 + S * (4 * sig + dly), 2 / fs2, 0.045)                      # C++: smooth the base delay over 0.5 s
        weave = S * lp_noise(rng, N, 0.7, fs2) * (1 + 7 * age) / 60 * np.pi / 180
        mod = np.stack([lp_noise(rng, N, 2500, fs2) for _ in range(C)], 1); mn = S * db(-34 + 6 * age + 6 * wear)
    pos = np.arange(C) - 0.5 * (C - 1)
    skew = np.stack([pos[c] * m['pitch'] * 1e3 * np.tan(S * az + weave) / v_um for c in range(C)], 1)
    out = _core(up, Ms / Sc, 0.04 * age * Ms, S * min(0.35, 0.35 * ub), 0.03 * Ms, mod, mn, delay, skew, float(fs2))
    y = resample_poly(out, 1, 2, axis=0)[:n]
    # ---- playback
    wtan = m['w'] * 1e3 * np.tan(abs(az))
    h_los = fir_design(lambda f: (loss(f, v, tp['delta'], d_head + d_bias, gap, wtan) / np.sqrt(1 + (f / 19000) ** 4)) ** S, fs)
    y = lfilter(h_los, 1.0, y, axis=0)
    if not quiet:
        cn = np.stack([lp_noise(rng, n, 30, fs) for _ in range(C)], 1)
        rate = (0.15 + 3.0 * age ** 2 + 5.0 * wear) * kn(dropout)
        if rate > 0: y = _dropouts(y, v_um, rate, S * (0.5 + 1.5 * wear + 0.8 * age) * min(1.0, 2 * dropout), S * 0.08 * (1 + 4 * wear), cn, float(fs), int(noise_seed) + 7)
        y = y * (1 + S * db(-32 + 10 * age) * np.stack([lp_noise(rng, n, 4, fs) for _ in range(C)], 1))
    y = y * db(S * (U['sens'][:C] * 2.0 * age - 1.0 * wear))
    b, a_ = biquad_peak(m['bump'], 1.7 * S, 1.1, fs); y = lfilter(b, a_, y, axis=0)
    y = sosfilt(butter(2, max(8.0, m['hp'] * S) / (fs / 2), btype='high', output='sos'), y, axis=0)
    if C > 1:
        xt = S * db(m['xt'] + 8 * age); leakx = y + 4 * sosfilt(butter(1, 200 / (fs / 2), output='sos'), y, axis=0); z = y.copy()
        for c in range(C):
            if c > 0: z[:, c] += xt * leakx[:, c - 1]
            if c < C - 1: z[:, c] += xt * leakx[:, c + 1]
        y = z
    if not quiet and hiss > 0:
        for c in range(C):
            w = shelf1(rng.standard_normal(n + 2000), 1500, 7000, fs)[2000:]
            w = sosfilt(butter(2, min(17000, 0.45 * fs) / (fs / 2), output='sos'), w)
            y[:, c] += w / np.std(w) * S * db(m['hiss'] + tp['hiss'] + 4 * age + 3 * wear) * kn(hiss)
        tt = np.arange(n) / fs
        y += (S * db(-78 + 22 * age) * kn(hiss) * (np.sin(2 * np.pi * hum_hz * tt) + 0.5 * np.sin(4 * np.pi * hum_hz * tt + 1) + 0.3 * np.sin(6 * np.pi * hum_hz * tt + 2)))[:, None]
    y = y * db(cal_db + S * 1.5 * age * U['cal'][:C])                 # level error between tape and decoder
    y = nr_decode(y, nr_dec, fs)
    return y / g

def latency(fs, quiet=True):
    """Samples of delay through one deck pass when quiet: two FIRs, the 2x filters (resample_poly here: none), 0.5 ms base."""
    N = 63 if fs <= 50000 else 127
    return (N - 1) + 0.0005 * fs

# ----------------------------------------------------------------------------- the other sections
def desk(x, fs, trim_db=0.0, lo_db=0.0, lo_f=100.0, hi_db=0.0, hi_f=10000.0):
    """Multitracker channel: TRIM into an op-amp that clips (2x oversampled), then two swept shelves. Unity gain when clean."""
    up = resample_poly(np.atleast_2d(x.T).T * db(trim_db), 2, 1, axis=0)
    up = np.clip(1.5 * np.tanh(up / 1.5) + 0.03 * up * up, -1.6, 1.45)
    y = resample_poly(up, 1, 2, axis=0)[:len(x)] / db(trim_db)
    if lo_db: b, a = biquad_shelf(lo_f, lo_db, fs, False); y = lfilter(b, a, y, axis=0)
    if hi_db: b, a = biquad_shelf(hi_f, hi_db, fs, True); y = lfilter(b, a, y, axis=0)
    return y

@nb.njit(cache=True)
def _comp(x, fs, thr, ratio):
    n = len(x); g = np.empty(n); e = 0.0; ca = 1 - np.exp(-1 / (0.003 * fs)); cr = 1 - np.exp(-1 / (0.12 * fs))
    for i in range(n):
        m = abs(x[i]); e += (ca if m > e else cr) * (m - e)
        over = 20 * np.log10(max(e, 1e-6) / thr)
        g[i] = 10 ** (-(over - over / ratio) / 20) if over > 0 else 1.0
    return g
def bus_comp(x, fs, thr_db=-8.0, ratio=4.0):
    """Budget VCA compressor: stereo-linked peak detector, hard knee, 3 ms / 120 ms, half make-up."""
    g = _comp(np.max(np.abs(x), axis=1), float(fs), db(thr_db), ratio)
    return x * g[:, None] * db(-0.5 * thr_db * (1 - 1 / ratio))

def dat(x, fs, noise_seed=1):
    """DAT master: 16-bit with TPDF dither, full scale at +15 dB over 0 VU, 8th-order 20 kHz low-pass, hard clip."""
    rng = np.random.default_rng(noise_seed); fsc = db(15.0); q = fsc / 32768
    y = sosfilt(butter(8, min(20000, 0.45 * fs) / (fs / 2), output='sos'), x, axis=0)
    y = np.clip(y, -fsc, fsc - q)
    return np.round(y / q + rng.uniform(-0.5, 0.5, y.shape) + rng.uniform(-0.5, 0.5, y.shape)) * q
