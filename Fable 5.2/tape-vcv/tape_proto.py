"""Lo-fi tape machine prototype (sketch quality, offline). Fable, 2026-11-23.
Units: 1.0 = reference flux (0 VU, 250 nWb/m). Lengths in micrometres, speed in cm/s.
One 'machine' = NR encode -> record EQ -> tape (saturation, modulation noise) -> transport
(wow/flutter/skew) -> wavelength losses (thickness, spacing, gap, azimuth, dropouts) ->
head bump, crosstalk, hiss, hum -> NR decode.  All channels of one call share one transport."""
import numpy as np, numba as nb
from scipy.signal import resample_poly, butter, sosfilt, lfilter
SR = 48000; OS = 4
db = lambda d: 10 ** (d / 20.0)

MACHINES = {
 # v cm/s, delta = effective recorded depth, d0 = head-tape spacing, gap, w = track width (mm), cap = max record boost dB,
 # mol dB re 0VU (3% 3rd harmonic), hiss dB re 0VU unweighted no NR, wow % wrms, capstan/pinch diameters mm, bump Hz, xt dB
 'porta':   dict(v=4.76, delta=1.5, d0=0.20, gap=1.2, w=0.6, cap=12, mol=3.0, hiss=-50, wow=0.07, dc=2.5, dp=11, bump=80, hp=35, xt=-42, skew=0.9, nr='dbx'),
 'porta_hi':dict(v=9.53, delta=1.5, d0=0.20, gap=1.2, w=0.6, cap=12, mol=3.0, hiss=-52, wow=0.05, dc=2.5, dp=11, bump=110,hp=35, xt=-45, skew=0.9, nr='dbx'),
 'deck':    dict(v=4.76, delta=2.0, d0=0.25, gap=1.2, w=0.6, cap=17, mol=4.5, hiss=-51, wow=0.08, dc=2.5, dp=11, bump=70, hp=25, xt=-40, skew=0.9, nr='dolby'),
 'deck2':   dict(v=4.76, delta=1.5, d0=0.20, gap=1.2, w=0.6, cap=17, mol=3.0, hiss=-54, wow=0.08, dc=2.5, dp=11, bump=70, hp=25, xt=-40, skew=0.9, nr='dolby'),
 'boombox': dict(v=4.76, delta=2.0, d0=0.45, gap=2.0, w=0.6, cap=9,  mol=3.5, hiss=-45, wow=0.22, dc=2.0, dp=10, bump=110,hp=70, xt=-30, skew=0.9, nr=None),
 'reel8':   dict(v=38.1, delta=6.0, d0=0.50, gap=3.0, w=0.45,cap=12, mol=8.0, hiss=-57, wow=0.06, dc=6.0, dp=40, bump=65, hp=28, xt=-45, skew=0.8, nr='dolbyc'),
 'reel2':   dict(v=19.05,delta=6.0, d0=0.50, gap=3.0, w=2.0, cap=12, mol=8.0, hiss=-63, wow=0.05, dc=6.0, dp=45, bump=45, hp=20, xt=-50, skew=4.0, nr=None),
}

def fftfilt(x, H):
    n = len(x); N = 1 << int(np.ceil(np.log2(n + SR // 2)))
    f = np.fft.rfftfreq(N, 1 / SR); h = H(f)
    return np.fft.irfft(np.fft.rfft(x, N, axis=0) * h[:, None], N, axis=0)[:n]

def loss(f, v, delta, d, gap, wtan):
    """Playback wavelength loss (linear gain). v cm/s; delta, d, gap, wtan in um."""
    lam = v * 1e4 / np.maximum(f, 1.0)            # wavelength in um
    k = 2 * np.pi / lam
    thick = (1 - np.exp(-k * delta)) / (k * delta)
    return thick * np.exp(-k * d) * np.abs(np.sinc(gap / lam)) * np.abs(np.sinc(wtan / lam))

def smooth_noise(rng, n, fc, sr=SR):
    sos = butter(2, fc / (sr / 2), output='sos')
    x = sosfilt(sos, rng.standard_normal(n + sr))[sr:]
    return x / (np.std(x) + 1e-12)

def band_noise(rng, n, f1, f2, sr=SR):
    sos = butter(2, [f1 / (sr / 2), f2 / (sr / 2)], btype='band', output='sos')
    x = sosfilt(sos, rng.standard_normal(n + sr // 4))[sr // 4:]
    return x / (np.std(x) + 1e-12)

# ---------------- noise reduction ----------------
def shelf(x, fz, fp, inverse=False):
    """first-order shelf: zero at fz, pole at fp (boost if fz<fp)."""
    if inverse: fz, fp = fp, fz
    wz, wp = 2 * SR * np.tan(np.pi * fz / SR), 2 * SR * np.tan(np.pi * fp / SR)
    b = np.array([2 * SR + wz, wz - 2 * SR]) / wz; a = np.array([2 * SR + wp, wp - 2 * SR]) / wp
    return lfilter(b / a[0], a / a[0], x, axis=0)

@nb.njit(cache=True)
def _dbx(x, encode, sr, tau):
    n = len(x); y = np.empty(n); s = 1e-3; ca = 1 - np.exp(-1 / (tau * sr)); cr = 1 - np.exp(-1 / (6 * tau * sr))
    ah = 1 / (1 + 2 * np.pi * 40 / sr); al = 1 - np.exp(-2 * np.pi * 11000 / sr)
    asf = 1 / (1 + 2 * np.pi * 1600 / sr)
    hp = 0.0; xp = 0.0; lp = 0.0; sh = 0.0; shp = 0.0
    for i in range(n):
        r = np.sqrt(s)
        if r < 3e-4: r = 3e-4
        if encode:
            g = 1.0 / r                 # feedback: detector sees the encoded (tape) signal
            if g > 30.0: g = 30.0
            o = x[i] * g; d = o
        else:
            d = x[i]; o = x[i] * r      # decoder: detector also sees the tape signal
        hp = ah * (hp + d - xp); xp = d                 # control path only: 40 Hz high-pass,
        lp += al * (hp - lp)                            # 11 kHz low-pass (Type II band limit),
        sh = asf * (sh + lp - shp); shp = lp            # treble weighting
        w = 1.0 * (lp + 6.0 * sh)
        s += (ca if w * w > s else cr) * (w * w - s)      # fast up, slower down, as the hardware's detector behaves
        y[i] = o
    return y

def dbx_encode(x): return np.stack([_dbx(c, True, SR, 0.005) for c in shelf(x, 400, 1600).T], 1)
def dbx_decode(x): return shelf(np.stack([_dbx(np.ascontiguousarray(c), False, SR, 0.005) for c in x.T], 1), 400, 1600, inverse=True)

@nb.njit(cache=True)
def _dolby(x, encode, sr, G, f0, thr):
    """Sliding-band compander: side chain = G * highpass whose corner slides up with level."""
    n = len(x); y = np.empty(n); hp = 0.0; inp = 0.0; env = 0.0
    att = 1 - np.exp(-1 / (0.002 * sr)); rel = 1 - np.exp(-1 / (0.06 * sr))
    for i in range(n):
        fc = f0 * (1 + (env / thr) ** 2)
        if fc > 60000.0: fc = 60000.0
        a = 1 / (1 + 2 * np.pi * fc / sr)
        c = a * (hp - inp)
        if encode:
            u = x[i]; h = a * u + c; o = u + G * h
        else:
            o = (x[i] - G * c) / (1 + G * a); u = o; h = a * u + c     # zero-delay solve of y = x - side(y)
        hp = h; inp = u
        m = abs(G * h)
        env += (att if m > env else rel) * (m - env)
        y[i] = o
    return y

def dolby(x, encode, kind='dolby'):
    G, f0, thr = (2.16, 1800.0, 0.03) if kind == 'dolby' else (9.0, 500.0, 0.03)   # 'dolbyc': crude 20 dB version
    return np.stack([_dolby(np.ascontiguousarray(c), encode, SR, G, f0, thr) for c in x.T], 1)

def nr_encode(x, nr):
    if nr == 'dbx': return dbx_encode(x)
    if nr in ('dolby', 'dolbyc'): return dolby(x, True, nr)
    return x
def nr_decode(x, nr):
    if nr == 'dbx': return dbx_decode(x)
    if nr in ('dolby', 'dolbyc'): return dolby(x, False, nr)
    return x

# ---------------- the machine ----------------
def machine(x, name, rec=0.0, age=0.15, seed=1, nr='auto', nr_dec='auto', hiss=1.0, wear=0.0, bias=0.0, wowx=1.0):
    """x: [n, C] tracks sharing one tape. rec: record level trim in dB. age 0..1: state of the machine.
    wear 0..1: state of the tape. bias -1..+1: under / over bias."""
    m = MACHINES[name]; rng = np.random.default_rng(seed)
    x = np.atleast_2d(x.T).T.astype(float); n, C = x.shape
    enc = m['nr'] if nr == 'auto' else nr
    dec = enc if nr_dec == 'auto' else nr_dec
    v = m['v']
    # --- age: fixed per-machine errors, scaled by one amount
    az_static = (2 + 16 * age) / 60 * np.pi / 180                       # azimuth error, radians (2' new, 18' neglected)
    d_head = m['d0'] + 0.35 * age + 0.3 * wear                           # dirt on the head adds spacing
    gap = m['gap'] * (1 + 0.6 * age)                                    # worn head, wider gap
    sens = db(rng.uniform(-1, 1, C) * 2.0 * age - 1.0 * wear)           # tape sensitivity / level calibration error
    x = x * db(rec)
    y = nr_encode(x, enc)
    # --- record EQ: inverse of the nominal loss of a new machine, limited by the record amp
    nom = lambda f: loss(f, v, m['delta'], m['d0'], m['gap'], 0.0)
    pre = lambda f: np.minimum(1 / nom(f), db(m['cap'])) / np.sqrt(1 + (f / 21000) ** 8)
    y = fftfilt(y, pre)
    # --- tape: anhysteretic curve (AC bias), 3% third harmonic at MOL; under-bias adds crossover kink
    up = resample_poly(y, OS, 1, axis=0); N = len(up); sro = SR * OS
    Ms = db(m['mol'] - 2.5 * wear + 1.5 * bias) / 0.55
    ub = max(0.0, -bias) + 0.25 * age
    dc = 0.04 * age * Ms                                                # magnetised head: a little 2nd harmonic
    f = lambda u: Ms * np.tanh((u + dc) / Ms) - Ms * np.tanh(dc / Ms)
    up = f(up) - ub * 0.02 * Ms * np.tanh(up / (0.02 * Ms))
    # modulation noise: rides on the signal, not underneath it
    mod = np.stack([smooth_noise(rng, N, 2500, sro) for _ in range(C)], 1)
    up = up * (1 + db(-34 + 6 * age + 6 * wear) * mod)
    # --- transport: speed errors integrate to a delay
    t = np.arange(N) / sro
    fcap = v * 10 / (np.pi * m['dc']); fpin = v * 10 / (np.pi * m['dp'])
    amp = m['wow'] / 100 * (1 + 5 * age ** 1.5) * wowx
    dev = (0.7 * np.sin(2 * np.pi * fcap * t + rng.uniform(0, 6.28) + 0.6 * np.cumsum(smooth_noise(rng, N, 0.5, sro)) / sro)
           + (0.6 + 1.5 * age) * np.sin(2 * np.pi * fpin * t + rng.uniform(0, 6.28))
           + (0.5 + 2.0 * age) * smooth_noise(rng, N, 1.2, sro)
           + 0.7 * band_noise(rng, N, 8, 45, sro)) * amp * np.sqrt(2) / 1.3
    delay = np.cumsum(dev) / sro; delay -= delay.min()
    scrape = band_noise(rng, N, 2500, 6000, sro) * 1.5e-7 * (1 + 3 * age)      # scrape flutter, seconds of jitter
    weave = smooth_noise(rng, N, 0.7, sro) * (1 + 7 * age) / 60 * np.pi / 180  # tape weaving past the head
    idx0 = np.arange(N) - (delay + scrape) * sro - 8
    out = np.empty_like(up)
    for c in range(C):
        skew = (c - (C - 1) / 2) * m['skew'] * 1e3 * np.tan(az_static + weave) / (v * 1e4)   # seconds, per track
        out[:, c] = np.interp(idx0 - skew * sro, np.arange(N), up[:, c])
    y = resample_poly(out, 1, OS, axis=0)[:n]
    # --- wavelength losses, with drop-outs as moments of extra head-to-tape spacing
    wtan = m['w'] * 1e3 * np.tan(az_static)
    grid = np.array([0, 0.5, 1, 2, 4, 8.0])
    bank = [fftfilt(y, lambda f, d=d: loss(f, v, m['delta'], d_head + d, gap, wtan) / np.sqrt(1 + (f / 19000) ** 4)) for d in grid]
    z = np.zeros_like(y)
    rate = 0.15 + 3.0 * age ** 2 + 5.0 * wear
    for c in range(C):
        edge = 1.6 if c in (0, C - 1) else 1.0                         # edge tracks suffer more
        d = 0.08 * (1 + 4 * wear) * np.abs(smooth_noise(rng, n, 30))
        for _ in range(rng.poisson(rate * edge * n / SR)):
            L = int(SR * rng.uniform(0.004, 0.09)); p = rng.integers(0, max(1, n - L))
            d[p:p + L] += rng.exponential(0.5 + 1.5 * wear + 0.8 * age) * np.hanning(L)[:len(d[p:p + L])]
        pos = np.interp(d, grid, np.arange(len(grid))); i0 = np.minimum(pos.astype(int), len(grid) - 2); fr = pos - i0
        for j in range(len(grid) - 1):
            sel = i0 == j
            z[sel, c] = (1 - fr[sel]) * bank[j][sel, c] + fr[sel] * bank[j + 1][sel, c]
    y = z * sens * (1 + db(-32 + 10 * age) * np.stack([smooth_noise(rng, n, 4) for _ in range(C)], 1))
    # --- head bump and low-end roll-off
    fb = m['bump']
    y = fftfilt(y, lambda f: (1 + 0.22 * np.exp(-0.5 * (np.log2(np.maximum(f, 1) / fb) / 0.45) ** 2)) * (f / m['hp']) ** 2 / np.sqrt(1 + (f / m['hp']) ** 4))
    # --- crosstalk from the neighbouring track, worse at low frequencies
    if C > 1:
        xt = db(m['xt'] + 8 * age); lowp = sosfilt(butter(1, 200 / (SR / 2), output='sos'), y, axis=0)
        leak = y + 4 * lowp; z = y.copy()
        for c in range(C):
            if c > 0: z[:, c] += xt * leak[:, c - 1]
            if c < C - 1: z[:, c] += xt * leak[:, c + 1]
        y = z
    # --- hiss (tape + playback amp) and hum
    if hiss > 0:
        for c in range(C):
            w = rng.standard_normal(n)
            h = fftfilt(w[:, None], lambda f: np.sqrt((1 + (f / 1500) ** 2) / (1 + (f / 7000) ** 2)) * (1 + 60 / np.maximum(f, 20)) / np.sqrt(1 + (f / 17000) ** 4))[:, 0]
            y[:, c] += h / np.std(h) * db(m['hiss'] + 4 * age + 3 * wear) * hiss
        tt = np.arange(n) / SR
        y += (db(-78 + 22 * age) * hiss * (np.sin(2 * np.pi * 50 * tt) + 0.5 * np.sin(2 * np.pi * 100 * tt + 1) + 0.3 * np.sin(2 * np.pi * 150 * tt + 2)))[:, None]
    y = nr_decode(y, dec)
    return y / db(rec)

@nb.njit(cache=True)
def _comp(x, sr, thr, ratio, att, rel, knee_hard):
    n = len(x); g = np.empty(n); e = 0.0
    ca = 1 - np.exp(-1 / (att * sr)); cr = 1 - np.exp(-1 / (rel * sr))
    for i in range(n):
        m = abs(x[i]); e += (ca if m > e else cr) * (m - e)
        over = 20 * np.log10(max(e, 1e-6) / thr)
        g[i] = 10 ** (-(over - over / ratio) / 20) if over > 0 else 1.0
    return g

def bus_comp(x, thr_db=-10, ratio=4, att=0.003, rel=0.12, makeup=0.0):
    """Budget VCA compressor, stereo linked, hard knee."""
    g = _comp(np.max(np.abs(x), axis=1), SR, db(thr_db), ratio, att, rel, True)
    return x * g[:, None] * db(makeup)

def desk(x, drive_db=0.0, lo=0.0, hi=0.0):
    """Cassette multitracker channel: trim into an op-amp that clips, then 100 Hz / 10 kHz shelves."""
    y = x * db(drive_db)
    y = np.clip(1.5 * np.tanh(y / 1.5) + 0.03 * y * y, -1.6, 1.45)
    if lo: y = y + (db(lo) - 1) * sosfilt(butter(1, 100 / (SR / 2), output='sos'), y, axis=0)
    if hi: y = y + (db(hi) - 1) * sosfilt(butter(1, 10000 / (SR / 2), btype='high', output='sos'), y, axis=0)
    return y / db(drive_db)
