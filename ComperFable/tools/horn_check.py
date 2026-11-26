#!/usr/bin/env python3
"""Measures the sustained-note grid from test/render_horn: pitch error, level, brightness, harmonics."""
import sys, glob, os, wave, struct
import numpy as np
def load(p):
    b = open(p, 'rb').read(); i = b.find(b'data'); n = struct.unpack('<I', b[i+4:i+8])[0]
    return np.frombuffer(b[i+8:i+8+n], dtype='<f4').astype(np.float64)
def pitch(x, sr, f_exp):
    # autocorrelation near the expected period, then refine by parabolic fit; search an octave either side
    n = len(x); x = x - x.mean(); w = np.hanning(n); X = np.fft.rfft(x * w, 1 << 18); P = np.abs(X) ** 2
    ac = np.fft.irfft(P); lo = int(sr / (f_exp * 2.2)); hi = int(sr / (f_exp / 2.2))
    seg = ac[lo:hi]; 
    # pick the smallest lag whose peak is within 90% of the max (true period, not a multiple)
    m = seg.max(); 
    if m <= 0: return 0.0
    idx = None
    for i in range(1, len(seg) - 1):
        if seg[i] > seg[i-1] and seg[i] >= seg[i+1] and seg[i] > 0.85 * m: idx = i; break
    if idx is None: return 0.0
    a, b, c = seg[idx-1], seg[idx], seg[idx+1]; d = 0.5 * (a - c) / (a - 2*b + c)
    return sr / (lo + idx + d)
def harmonics(x, sr, f0, nh=24):
    n = len(x); w = np.hanning(n); X = np.abs(np.fft.rfft(x * w, 1 << 18)); bw = (1 << 18) / sr
    out = []
    for k in range(1, nh + 1):
        c = int(round(k * f0 * bw)); 
        if c + 30 >= len(X): break
        out.append(X[max(0, c-25):c+26].max())
    return np.array(out) / (n / 4)
def main(d):
    sr = 48000; names = ['tenor', 'trumpet', 'trombone']
    rows = []
    for f in sorted(glob.glob(os.path.join(d, 'g_*.wav')), key=lambda s: [int(t) for t in os.path.basename(s)[2:-4].split('_')]):
        k, p, b = [int(t) for t in os.path.basename(f)[2:-4].split('_')]
        x = load(f); seg = x[int(0.6*sr):int(min(1.15, len(x)/sr - 0.02)*sr)]; fexp = 440 * 2 ** ((p - 69) / 12)
        rms = np.sqrt(np.mean(seg ** 2)); f0 = pitch(seg, sr, fexp) if rms > 1e-5 else 0
        cents = 1200 * np.log2(f0 / fexp) if f0 > 0 else float('nan')
        h = harmonics(seg, sr, f0 if f0 > 0 else fexp); hdb = 20 * np.log10(h / h.max() + 1e-9)
        cen = (h * np.arange(1, len(h) + 1)).sum() / h.sum() * (f0 if f0 > 0 else fexp)
        # onset: time to reach half of steady rms
        e = np.sqrt(np.convolve(x ** 2, np.ones(240) / 240, 'same')); on = np.argmax(e > 0.5 * rms) / sr * 1000
        dc = seg.mean()
        rows.append((k, p, b, cents, 20 * np.log10(rms + 1e-9), cen, on, hdb, np.abs(x).max()))
        if len(sys.argv) > 2 and names[k] not in sys.argv[2]: continue
        if len(sys.argv) > 3 and abs(cents) < float(sys.argv[3]): continue
        print(f"{names[k]:8s} p{p:3d} b{b} {cents:+8.1f}c  {20*np.log10(rms+1e-9):6.1f} dB  pk {np.abs(x).max():5.2f} cen {cen:6.0f} Hz  on {on:4.0f} ms  H: " + ' '.join(f"{v:4.0f}" for v in hdb[:12]))
    return rows
if __name__ == '__main__': main(sys.argv[1] if len(sys.argv) > 1 else 'test/out/horn')
