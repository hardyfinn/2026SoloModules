#!/usr/bin/env python3
"""Checks the demo phrases from test/render_horn demo: every note reaches its pitch (no wrong register), levels, clicks."""
import sys, os, struct
import numpy as np
def load(p):
    b = open(p, 'rb').read(); i = b.find(b'data'); n = struct.unpack('<I', b[i+4:i+8])[0]
    x = np.frombuffer(b[i+8:i+8+n], dtype='<f4').astype(np.float64); return x.reshape(-1, 2)
def pitch(seg, sr, fexp):
    seg = seg - seg.mean(); n = len(seg)
    if n < 256 or np.sqrt(np.mean(seg**2)) < 1e-4: return 0.0
    X = np.fft.rfft(seg * np.hanning(n), 1 << 16); ac = np.fft.irfft(np.abs(X) ** 2)
    lo = int(sr / (fexp * 2.3)); hi = min(int(sr / (fexp / 2.3)), n - 2); s = ac[lo:hi]; m = s.max()
    for i in range(1, len(s) - 1):
        if s[i] > s[i-1] and s[i] >= s[i+1] and s[i] > 0.8 * m:
            a, b, c = s[i-1], s[i], s[i+1]; d = 0.5 * (a - c) / (a - 2*b + c); return sr / (lo + i + d)
    return 0.0
d = sys.argv[1]; sr = 48000; cache = {}; bad = 0; stats = {}
for line in open(os.path.join(d, 'notes.txt')):
    name, kind, t0, t1, p = line.split(); t0 = float(t0); t1 = float(t1); p = float(p)
    if name not in cache: cache[name] = load(os.path.join(d, name + '.wav'))
    x = cache[name].mean(axis=1); fexp = 440 * 2 ** ((p - 69) / 12)
    dur = t1 - t0; a = t0 + min(0.6 * dur, max(0.07, 0.45 * dur)); b = min(t1 - 0.01, a + 0.12)
    if 'swell' in name: a, b = t0 + 2.5, t0 + 3.5
    seg = x[int(a*sr):int(b*sr)]; f = pitch(seg, sr, fexp); c = 1200 * np.log2(f / fexp) if f > 0 else float('nan')
    rms = 20 * np.log10(np.sqrt(np.mean(seg**2)) + 1e-9)
    st = stats.setdefault(name, []); st.append((c, rms))
    if not (abs(c) < 45): bad += 1; print(f"  BAD {name} t={t0:.2f} p={p:.0f} got {c:+.0f}c  {rms:.1f} dB")
for name, x in cache.items():
    m = x.mean(axis=1); pk = np.abs(x).max(); dif = np.abs(np.diff(m)).max(); c = np.array([s[0] for s in stats[name]]); r = np.array([s[1] for s in stats[name]])
    print(f"{name:20s} peak {pk:5.2f}  rms {20*np.log10(np.sqrt(np.mean(m**2))+1e-9):6.1f} dB  max step {dif:5.3f}  notes {len(c):3d}  cents mean {np.nanmean(c):+5.1f} sd {np.nanstd(c):4.1f}  note level {r.min():5.1f}..{r.max():5.1f} dB")
print("bad notes:", bad)
