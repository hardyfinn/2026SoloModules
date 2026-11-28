"""The dip after the pluck. Each strong partial's level over the first 0.7 s is fitted as two motions of the string
that start together, one dying fast and tuned slightly apart from the other, so they cancel briefly and recover:
   a(t) = | (1-m) 10^(-s1 t/20) + m 10^(-s2 t/20) exp(i 2 pi d t) |"""
import numpy as np, soundfile as sf, os, json, upright as U
from scipy.signal import butter, sosfiltfilt, hilbert, resample_poly
from scipy.optimize import least_squares
D = '/tmp/bassref/mb/Samples/pizz/'; SR = U.SR
NM = {'c': 0, 'db': 1, 'd': 2, 'eb': 3, 'e': 4, 'f': 5, 'gb': 6, 'g': 7, 'ab': 8, 'a': 9, 'bb': 10, 'b': 11}
rows = []
for fn in sorted(os.listdir(D)):
    if len(fn[:-4].split('_')) != 3: continue
    nm, vl, _ = fn[:-4].split('_'); midi = NM[nm[:-1]] + 12 * (int(nm[-1]) + 1)
    if not (28 <= midi <= 55): continue
    x, sr = sf.read(D + fn); x = x if x.ndim == 1 else x.mean(1)
    if sr != SR: x = resample_poly(x, SR, sr)
    st = int(np.argmax(np.abs(x) > 0.02 * np.abs(x).max())); x = x[st:st + int(0.9 * SR)]; f0 = U.hz(midi)
    env = []
    for k in (1, 2, 3, 4):
        bw = min(0.45 * f0, 30.0)
        y = sosfiltfilt(butter(2, [k * f0 - bw, k * f0 + bw], 'bandpass', fs=SR, output='sos'), x); e = np.abs(hilbert(y)); env.append((np.sqrt(np.mean(y ** 2)), k, e))
    env.sort(key=lambda z: -z[0])
    for lvl, k, e in env[:2]:
        t = np.arange(0.03, 0.7, 0.005); a = e[(t * SR).astype(int)]; a = a / a[:8].max(); L = 20 * np.log10(a + 1e-6)
        def model(p, t=t):
            m, s1, s2, d = p; return 20 * np.log10(np.abs((1 - m) * 10 ** (-s1 * t / 20) + m * 10 ** (-s2 * t / 20) * np.exp(2j * np.pi * d * t)) + 1e-6)
        best = None
        for d0 in (1.0, 2.5, 4.0, 6.0):
            r = least_squares(lambda p: model(p) - L - (model(p)[:8].max()), [0.5, 10, 40, d0], bounds=([0, 0, 0, 0], [0.95, 80, 300, 9]))
            if best is None or r.cost < best.cost: best = r
        m, s1, s2, d = best.x; rms = np.sqrt(2 * best.cost / len(t))
        one = least_squares(lambda p: -p[0] * t - p[1] - L, [10, 0]); rms1 = np.sqrt(2 * one.cost / len(t))
        dip = L[(t > 0.06) & (t < 0.25)].min() - max(L[(t > 0.25) & (t < 0.4)].max(), -60)
        rows.append(dict(note=nm, vl=vl, midi=midi, k=k, f=k * f0, m=m, s1=s1, s2=s2, d=d, rms=rms, rms1=rms1, dip=dip))
        print('%-4s %s h%d %6.1f Hz  m %.2f slow %4.1f fast %5.1f dB/s  apart %.2f Hz  fit %.1f dB (plain decay %.1f)  dip-then-recovery %5.1f dB' % (nm, vl, k, k * f0, m, s1, s2, d, rms, rms1, dip))
json.dump(rows, open('beat_fit.json', 'w'))
g = [r for r in rows if r['rms'] < 2.5]
for key in ('m', 's1', 's2', 'd'): print(key, 'median %.2f  quartiles %.2f %.2f' % (np.median([r[key] for r in g]), np.percentile([r[key] for r in g], 25), np.percentile([r[key] for r in g], 75)))
print('fits', len(g), 'of', len(rows), ' mean fit %.1f dB against plain decay %.1f dB' % (np.mean([r['rms'] for r in rows]), np.mean([r['rms1'] for r in rows])))
print('partials whose level recovers by 2 dB or more after a dip:', sum(1 for r in rows if r['dip'] < -2), 'of', len(rows))
