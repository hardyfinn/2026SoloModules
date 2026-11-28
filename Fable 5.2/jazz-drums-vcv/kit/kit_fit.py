"""Fit a struck-cymbal model to recordings and play it back.
A cymbal recording turns out to be two things (see texture.py): a handful of steady partials, and a restless wash that
behaves like noise in every band above about 1 kHz. So the model, which is what the module will run, has three parts:
  - partials: decaying sine modes on the frequencies that stand out in every recorded take (two-pole resonators),
  - wash: one noise source, given a fixed fine colour, split into bands; each band's level is a sum of decaying
    (and, for crashes, swelling) energy stores that every stroke tops up, so strokes build on one another,
  - click: a few milliseconds of shaped noise for the stick.
Level and brightness follow how hard the cymbal is hit, from the recorded loudness layers.
"""
import os, sys, json, glob
import numpy as np, soundfile as sf
from scipy.signal import find_peaks, butter, sosfiltfilt
from scipy.optimize import nnls
SR = 48000
F_RES = 9000.0          # partials are looked for below this
EDGES = np.array([35, 60, 90, 130, 180, 260, 360, 480, 620, 800, 1000, 1250, 1550, 1900, 2300, 2600, 3100, 3700, 4400, 5200, 6200, 7400, 8800, 10500, 12500, 15000, 19000.0])
NB = len(EDGES) - 1
HI = [i for i in range(NB) if EDGES[i] >= 1000]
TW = [(0.0, 0.006), (0.006, 0.02), (0.02, 0.06), (0.06, 0.15), (0.15, 0.4), (0.4, 1.0), (1.0, 2.0), (2.0, 4.0)]
RATES = np.array([3.0, 6.0, 12.0, 24.0, 48.0, 100.0, 250.0, 700.0])   # dB per second: the wash's energy stores
CLICK = np.array([2000.0, 6000.0])                               # these are the stick click
TAUS = np.array([0.004, 0.015, 0.06])                                   # swell times
HOP = 48


def load(path, sr=SR):
    x, s = sf.read(path)
    m = x.mean(1) if x.ndim > 1 else x
    if s == 44100:
        from scipy.signal import resample_poly
        m = resample_poly(m, 160, 147)
    else: assert s == sr, s
    on = int(np.argmax(np.abs(m) > 0.05 * np.abs(m).max()))
    return m[max(0, on - 24):]


def bands(x):
    out = np.full((NB, len(TW)), -200.0)
    for j, (a, b) in enumerate(TW):
        seg = x[int(a * SR):int(b * SR)]
        if len(seg) < 64: continue
        n = len(seg); X = np.abs(np.fft.rfft(seg * np.hanning(n))) ** 2 / n; fr = np.fft.rfftfreq(n, 1 / SR)
        for i in range(NB):
            k = (fr >= EDGES[i]) & (fr < EDGES[i + 1])
            if k.any() and (b - a) >= 1.5 / (EDGES[i + 1] - EDGES[i]) and len(seg) >= 0.6 * (b - a) * SR: out[i, j] = 10 * np.log10(X[k].sum() + 1e-30)
    return out


def early(x, ms=4.0):
    """band energies of the first few milliseconds, dB (no taper at the start, so the click counts in full)"""
    n = int(ms * SR / 1000); seg = x[:n] * np.concatenate([np.ones(n // 2), np.hanning(n)[n // 2:]])
    X = np.abs(np.fft.rfft(seg, 2048)) ** 2; fr = np.fft.rfftfreq(2048, 1 / SR)
    return np.array([10 * np.log10(X[(fr >= EDGES[i]) & (fr < EDGES[i + 1])].sum() + 1e-30) for i in range(NB)])


def band_power(x, i, hop=HOP):
    sos = butter(4, [EDGES[i], min(EDGES[i + 1], SR / 2 - 100)], btype='band', fs=SR, output='sos')
    y = sosfiltfilt(sos, x); n = len(y) // hop
    return np.mean(y[:n * hop].reshape(n, hop) ** 2, axis=1)


def shimmer(x, i):
    e = 10 * np.log10(band_power(x, i, 480) + 1e-30)[10:150]
    if len(e) < 100: return 0.0
    t = np.arange(len(e)); p = np.polyfit(t, e, 2)
    return float(np.std(e - np.polyval(p, t)))


def basis(tt):
    """columns: plain decays, swelling decays, click decays, constant floor"""
    cols, meta = [], []
    for r in RATES: cols.append(10 ** (-r * tt / 10)); meta.append((float(r), 0.0))
    for r in RATES:
        for tau in TAUS: cols.append(10 ** (-r * tt / 10) * (1 - np.exp(-tt / tau)) ** 2); meta.append((float(r), float(tau)))
    for r in CLICK: cols.append(10 ** (-r * tt / 10)); meta.append((float(r), -1.0))
    cols.append(np.ones_like(tt)); meta.append((0.0, -2.0))
    return np.stack(cols, 1), meta


def fit(takes_by_layer, name, seed=1, npart=70, swell=True, tmax=6.0, N=2 ** 14, cs=(0.22, 0.5, 0.9, 1.4, 2.0), rmax=150.0):
    takes = takes_by_layer; mid = len(takes) // 2 if len(takes) > 1 else 0
    T = min(tmax, min(len(t) for t in takes[mid]) / SR - 0.05, max(len(t) for t in takes[mid]) / SR)
    n = int(T * SR) // HOP; tt = (np.arange(n) + 0.5) * HOP / SR
    edges = np.unique(np.concatenate([np.arange(0, 12), np.geomspace(12, n, 70).astype(int)])); edges = edges[edges <= n]
    def binned(v): return np.array([v[a:b].mean(axis=0) for a, b in zip(edges[:-1], edges[1:])])
    A, meta = basis(tt); Ab = binned(A)
    if not swell:
        keep = [k for k, m in enumerate(meta) if m[1] <= 0]; Ab = Ab[:, keep]; meta = [meta[k] for k in keep]
    comp = []
    for i in range(NB):
        y = binned(np.mean([band_power(t[:n * HOP + HOP], i)[:n] for t in takes[mid]], axis=0))
        w = 1.0 / (y + 2e-6 * y.max())
        sc = y.max() + 1e-30
        try: c, _ = nnls(Ab * (w * sc)[:, None], y * w, maxiter=3000); c = c * sc
        except RuntimeError: c = np.zeros(Ab.shape[1]); c[0] = sc
        comp.append([(meta[k][0], meta[k][1], float(c[k])) for k in range(len(meta)) if c[k] > 0 and meta[k][1] > -2])
    # partials: peaks that stand out of the spectrum averaged over all takes (the wash averages flat, true partials do not)
    cs = list(cs); fr = np.fft.rfftfreq(2 ** 16, 1 / SR); df = fr[1] - fr[0]
    def spec(tc):
        P = 0
        for t in takes[mid]:
            a_ = int(tc * SR) - N // 2; seg = t[a_:a_ + N]
            if len(seg) < N: return None
            P = P + (np.abs(np.fft.rfft(seg * np.hanning(N), 2 ** 16)) / (N / 4.0)) ** 2
        return P / len(takes[mid])
    S = [spec(c) for c in cs]; S = [x for x in S if x is not None]; cs = cs[:len(S)]
    L0 = 10 * np.log10(S[0] + (S[1] if len(S) > 1 else 0) + 1e-30)
    from scipy.ndimage import median_filter
    base = median_filter(L0, size=int(300 / df) | 1)
    k = np.where((fr > 38) & (fr < F_RES))[0]
    pk, pr = find_peaks(L0[k], prominence=6, distance=int(8 / df)); pk = k[pk]
    pk = pk[(L0[pk] - base[pk]) > 8.0]
    pk = pk[np.argsort(-L0[pk])][:npart]
    res = []
    for j in pk:
        lv = np.array([10 * np.log10(x[j - 3:j + 4].max() + 1e-30) for x in S]); fl = np.array([10 * np.log10(np.median(x[j - 60:j + 61]) + 1e-30) for x in S])
        ok = lv > fl + 5; ok[:2] = True
        p = np.polyfit(np.array(cs)[ok], lv[ok], 1) if len(S) > 1 else [-rmax, lv[0] + rmax * cs[0]]; r = float(np.clip(-p[0], 2.5, rmax))
        if r < 0.9 * rmax: res.append([float(fr[j]), float(10 ** ((p[1]) / 20)), r])      # anything faster is not a ringing partial: the wash carries it
    # level against hit strength: per band, tail and click separately, as a table over the recorded layers
    def valid_mean(B, cols):
        v = B[:, cols]; v = np.where(v > -150, v, np.nan); return np.nanmean(v, axis=1)
    tot = [10 * np.log10(np.mean([np.sum(t[:int(0.4 * SR)] ** 2) for t in tk])) for tk in takes]
    ear = np.array([np.mean([valid_mean(bands(t), [2, 3, 4]) for t in tk], axis=0) for tk in takes])
    clk = np.array([10 * np.log10(np.mean([10 ** (early(t) / 10) for t in tk], axis=0)) for tk in takes])
    x = np.array(tot) - tot[mid]; ear = np.nan_to_num(ear - ear[mid], nan=0.0); clk = clk - clk[mid]
    if len(takes) == 1 or np.ptp(x) < 6:      # layers too close together to learn from: everything follows the level one for one
        x = np.array([-10.0, 0.0, 10.0]); ear = np.outer(x, np.ones(NB)); clk = ear.copy()
    else:
        o = np.argsort(x); x, ear, clk = x[o], ear[o], clk[o]
        # where a layer's band has sunk into the room noise, the table would flatten: keep at least a 0.6 slope
        for tab in (ear, clk):
            for q in range(len(x) - 1):
                tab[q + 1] = np.maximum(tab[q + 1], tab[q] + 0.3 * (x[q + 1] - x[q])) if x[q + 1] > 0 else tab[q + 1]
            for q in range(len(x) - 1, 0, -1):
                if x[q - 1] < 0: tab[q - 1] = np.minimum(tab[q - 1], tab[q] - 0.5 * (x[q] - x[q - 1]))
    return dict(name=name, seed=seed, comp=comp, res=res, lx=x.tolist(), ltail=ear.tolist(), lclick=clk.tolist(),
                tot_db=[float(v) for v in tot], ref_bands=(lambda RB: np.where(RB.min(axis=0) > -150, RB.mean(axis=0), -200.0))(np.array([bands(t) for t in takes[mid]])).tolist(),
                ref_early=(10 * np.log10(np.mean([10 ** (early(t) / 10) for t in takes[mid]], axis=0))).tolist())


class Cymbal:
    def __init__(self, d):
        self.d = d; rng = np.random.RandomState(d['seed'])
        P = np.array(d['res']).reshape(-1, 3)
        self.f, self.a, self.r = P[:, 0], P[:, 1], P[:, 2]; self.band = np.clip(np.searchsorted(EDGES, self.f) - 1, 0, NB - 1)
        self.click = np.zeros((NB, len(CLICK))); self.wash = [[c for c in d['comp'][i] if c[1] >= 0] for i in range(NB)]
        for i in range(NB):
            for (rt, ta, c) in d['comp'][i]:
                if ta < 0: self.click[i, list(CLICK).index(rt)] = c
        self.gain = np.array(d.get('gain', np.ones(NB))); self.cgain = np.array(d.get('cgain', np.ones(NB)))
        self.lx = np.array(d['lx']); self.ltail = np.array(d['ltail']); self.lclick = np.array(d['lclick'])
        # the wash's fixed fine colour: the noise plus a dozen faint copies of itself a few milliseconds late
        self.taps = [(0, 1.0)] + [(int(rng.uniform(0.0006, 0.022) * SR), rng.choice([-1, 1]) * rng.uniform(0.2, 0.42)) for _ in range(14)]
        nrm = np.sqrt(sum(g * g for _, g in self.taps)); self.taps = [(dl, g / nrm) for dl, g in self.taps]

    def curve(self, tab, level_db):
        """per-band dB for a stroke at level_db (the table between layers, its end slopes beyond them)"""
        x = self.lx
        if level_db <= x[0]: return tab[0] + (tab[1] - tab[0]) / (x[1] - x[0]) * (level_db - x[0])
        if level_db >= x[-1]: return tab[-1] + (tab[-1] - tab[-2]) / (x[-1] - x[-2]) * (level_db - x[-1])
        return np.array([np.interp(level_db, x, tab[:, i]) for i in range(NB)])

    def stroke(self, level_db=0.0, dur=6.0, rng=None, vary=0.0, click=1.0, parts=(1, 1, 1)):
        """one stroke. level_db is against the reference layer. vary: how differently the stick lands each time (dB)."""
        rng = rng or np.random.RandomState(7)
        n = int(dur * SR); t = np.arange(n) / SR; y = np.zeros(n)
        # partials
        tl = self.curve(self.ltail, level_db); cl = self.curve(self.lclick, level_db)
        g = self.a * 10 ** (tl[self.band] / 20.0) * parts[0]
        if vary > 0: g = g * 10 ** (rng.normal(0, vary, len(g)) / 20.0)
        lam = self.r * np.log(10) / 20.0; ph = rng.uniform(0, 2 * np.pi, len(g))
        for i in range(len(g)):
            m = int(min(n, (100.0 / self.r[i] + 0.3) * SR))
            e = np.exp(-lam[i] * t[:m]); k = int(min(0.006, 1.0 / self.f[i]) * SR)      # the stick is in contact for a moment: no partial starts with a step
            e[:k] *= 0.5 - 0.5 * np.cos(np.pi * np.arange(k) / k)
            y[:m] += g[i] * e * np.sin(2 * np.pi * self.f[i] * t[:m] + ph[i])
        # wash
        m2 = n + 2048; nz = rng.normal(0, 1, m2); col = np.zeros(m2)
        for dl, gg in self.taps: col[dl:] += gg * nz[:m2 - dl]
        X = np.fft.rfft(col[2048:2048 + n]); fr = np.fft.rfftfreq(n, 1 / SR)
        for i in range(NB):
            if not self.wash[i]: continue
            e2 = np.zeros(n)
            for rt, ta, c in self.wash[i]:
                e2 += c * 10 ** (-rt * t / 10) * ((1 - np.exp(-t / ta)) ** 2 if ta > 0 else 1.0)
            bw = (EDGES[i + 1] - EDGES[i]) / (SR / 2)
            yb = np.fft.irfft(X * ((fr >= EDGES[i]) & (fr < EDGES[i + 1])), n)
            y += parts[1] * yb * np.sqrt(e2 / bw) * self.gain[i] * 10 ** (tl[i] / 20.0)
        # stick click
        m = 2048; fc = np.fft.rfftfreq(m, 1 / SR); tc = np.arange(m) / SR
        for k, rt in enumerate(CLICK):
            sh = np.zeros(len(fc))
            for i in range(NB):
                kk = (fc >= EDGES[i]) & (fc < EDGES[i + 1])
                if EDGES[i + 1] <= 180: continue
                sh[kk] = np.sqrt(self.click[i, k] / ((EDGES[i + 1] - EDGES[i]) / (SR / 2))) * self.cgain[i] * 10 ** (cl[i] / 20.0)
            y[:m] += parts[2] * click * np.fft.irfft(np.fft.rfft(rng.normal(0, 1, m)) * sh, m) * 10 ** (-rt * tc / 20)
        y[:8] *= np.linspace(0, 1, 8)
        return y

    def calibrate(self, ref_bands, ref_early, iters=4):
        R = np.array(ref_bands); RE = np.array(ref_early); self.cgain[:] = 1.0
        for _ in range(iters):
            y = self.stroke(); M = bands(y); ME = early(y)
            dur = np.array([w[1] - w[0] for w in TW])
            for i in range(NB):
                ok = (R[i] > -150) & (M[i] > -150); ok[:2] = False
                if not ok.any() or not self.wash[i]: continue
                if R[i].max() < R.max() - 45 or EDGES[i + 1] <= min(250.0, self.f.min() if len(self.f) else 250.0): self.gain[i] = 0.0; continue      # room rumble, or below the lowest partial
                er = np.sum((10 ** (R[i] / 10) * dur)[ok]); em = np.sum((10 ** (M[i] / 10) * dur)[ok])
                self.gain[i] = np.clip(self.gain[i] * np.sqrt(er / em), 0.05, 20.0)
            lo = EDGES[:-1] < 1000
            dl = 10 * np.log10(np.sum(10 ** (RE[lo] / 10)) / np.sum(10 ** (ME[lo] / 10)))
            for i in range(NB):
                self.cgain[i] = np.clip(self.cgain[i] * 10 ** ((dl if lo[i] else RE[i] - ME[i]) / 20.0), 0.03, 30.0)
        self.d['gain'] = self.gain.tolist(); self.d['cgain'] = self.cgain.tolist()


def report(cym, takes, d, label):
    lv = np.array(d['tot_db']) - d['tot_db'][len(takes) // 2 if len(takes) > 1 else 0]; out = []
    for k, tk in enumerate(takes):
        RB = np.array([bands(t) for t in tk]); R = np.where(RB.min(axis=0) > -150, RB.mean(axis=0), -200.0); M = bands(cym.stroke(level_db=lv[k]))
        ok = (R > R.max() - 45) & (cym.gain > 0)[:, None]; dd = (M - R)[ok]
        out.append((float(np.mean(np.abs(dd))), float(np.max(np.abs(dd))), float(np.mean(dd))))
        print('  %s layer %d (%+5.1f dB): mean |diff| %.1f dB, worst %.1f, bias %+.1f over %d cells' % (label, k + 1, lv[k], out[-1][0], out[-1][1], out[-1][2], ok.sum()))
    ref = takes[len(takes) // 2 if len(takes) > 1 else 0][0]; m = cym.stroke()
    print('  first 4 ms, model - recording by band:', ' '.join('%+.0f' % v for v in early(m) - np.array(d['ref_early'])))
    print('  shimmer above 1 kHz: recording %.1f dB, model %.1f dB;  %d partials' % (np.mean([shimmer(ref, i) for i in HI]), np.mean([shimmer(m, i) for i in HI]), len(cym.f)))
    return out


def build(name, layers, **kw):
    takes = [[load(p) for p in ps] for ps in layers]
    d = fit(takes, name, **kw); c = Cymbal(d); c.calibrate(d['ref_bands'], d['ref_early'])
    return c, takes, d


def fit_sweep(pattern):
    """the brush sweep is not a stroke but a steady hiss the hand shapes: fit its spectrum at each recorded pressure"""
    rows = []
    for ps in layers_of(pattern):
        P = 0
        for p in ps:
            x = load_whole(p); x = x[SR // 2:len(x) - SR // 2]; X = np.abs(np.fft.rfft(x)) ** 2 / len(x) ** 2 * 2; fr = np.fft.rfftfreq(len(x), 1 / SR)
            P = P + np.array([X[(fr >= EDGES[i]) & (fr < EDGES[i + 1])].sum() for i in range(NB)])
        rows.append(10 * np.log10(P / len(ps) + 1e-30))
    rows = np.array(rows); tot = 10 * np.log10(np.sum(10 ** (rows / 10), axis=1)); o = np.argsort(tot)
    return dict(name='brush-sweep', lx=tot[o].tolist(), bands=rows[o].tolist(), seed=5)


def load_whole(path):
    x, s = sf.read(path); m = x.mean(1) if x.ndim > 1 else x
    if s == 44100:
        from scipy.signal import resample_poly
        m = resample_poly(m, 160, 147)
    return m


class Sweep:
    def __init__(self, d):
        self.d = d; self.lx = np.array(d['lx']); self.tab = np.array(d['bands']); rng = np.random.RandomState(d['seed'])
        self.taps = [(0, 1.0)] + [(int(rng.uniform(0.0006, 0.012) * SR), rng.choice([-1, 1]) * rng.uniform(0.1, 0.25)) for _ in range(8)]
        nrm = np.sqrt(sum(g * g for _, g in self.taps)); self.taps = [(dl, g / nrm) for dl, g in self.taps]

    def render(self, level_db, rng=None):
        """level_db: the hand's pressure and speed over time, as a level in dB (same scale as the recordings), one value per sample"""
        rng = rng or np.random.RandomState(3); n = len(level_db); nz = rng.normal(0, 1, n + 1024); col = np.zeros(n + 1024)
        for dl, g in self.taps: col[dl:] += g * nz[:n + 1024 - dl]
        X = np.fft.rfft(col[1024:]); fr = np.fft.rfftfreq(n, 1 / SR); y = np.zeros(n)
        lv = np.clip(level_db, self.lx[0] - 30, self.lx[-1] + 6); inside = np.clip(lv, self.lx[0], self.lx[-1])
        for i in range(NB):
            if EDGES[i + 1] <= 130: continue
            db = np.interp(inside, self.lx, self.tab[:, i]) + (lv - inside)
            y += np.fft.irfft(X * ((fr >= EDGES[i]) & (fr < EDGES[i + 1])), n) * 10 ** (db / 20.0) / np.sqrt((EDGES[i + 1] - EDGES[i]) / (SR / 2))
        return y


def layers_of(pattern):
    """group files by loudness layer (vlN or dlN in the name), quietest first"""
    import re
    g = {}
    for p in sorted(glob.glob(pattern)):
        m = re.search(r'_[vd]l(\d+)', p); g.setdefault(int(m.group(1)) if m else 1, []).append(p)
    return [g[k] for k in sorted(g)]


V = os.environ.get('DRUMREF', '/tmp/drumref/virt/Samples/oh'); W = os.environ.get('BRUSHREF', '/tmp/drumref/swirly/Samples')
DRUM = dict(N=4096, cs=(0.05, 0.09, 0.14, 0.2, 0.3), rmax=500.0, tmax=2.5)
SHORT = dict(N=4096, cs=(0.05, 0.1, 0.16, 0.24), rmax=500.0, tmax=1.5)
JOBS = [('ride', V + '/ride/oh_ride_ride_*', {}), ('bell', V + '/ride/oh_ride_bell_*', {}), ('crash', V + '/flatride/oh_flatride_crash_*', {}),
        ('hat-closed', V + '/hh/oh_hh_closed_*', SHORT), ('hat-foot', V + '/hh/oh_hh_pedal_*', SHORT), ('hat-open', V + '/hh/oh_hh_open_*', dict(tmax=4.0)),
        ('snare', V + '/snare/oh_snare_center_*', DRUM), ('snare-edge', V + '/snare/oh_snare_offcenter_*', DRUM), ('cross-stick', V + '/snare/oh_snare_crossstick_*', SHORT),
        ('kick', V + '/kick/oh_kick_snon_*', DRUM), ('tom-high', V + '/htom/oh_htom_center_*', DRUM), ('tom-low', V + '/ltom/oh_ltom_center_*', DRUM),
        ('brush-tap', W + '/snare_main/snare_hit_*_top.wav', DRUM), ('brush-hat-foot', W + '/hat_foot/*', SHORT), ('brush-ride', W + '/ride/*', {})]

if __name__ == '__main__':
    P = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'kit_fit.json')
    out = json.load(open(P)) if os.path.exists(P) else {}
    for name, pat, kw in JOBS:
        if len(sys.argv) > 1 and name not in sys.argv[1:]: continue
        L = layers_of(pat)
        if len(L) > 5: L = [L[i] for i in np.round(np.linspace(0, len(L) - 1, 5)).astype(int)]
        c, takes, d = build(name, L, **kw); print(name, [len(x) for x in L]); report(c, takes, d, name); out[name] = d
    if len(sys.argv) == 1 or 'brush-sweep' in sys.argv: out['brush-sweep'] = fit_sweep(W + '/snare_stir/stir_dl?_rr*.wav'); print('brush-sweep levels', np.round(out['brush-sweep']['lx'], 1))
    json.dump(out, open(P, 'w'))
