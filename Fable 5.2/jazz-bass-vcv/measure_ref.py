"""Measures the CC0 Karoryfer Meatbass pizzicato samples (github sfzinstruments/karoryfer.meatbass):
pitch, partial levels just after the pluck, each partial's decay rate, inharmonicity, attack."""
import numpy as np, soundfile as sf, glob, os, json, sys
D = '/tmp/bassref/mb/Samples/pizz/'
def analyse(path, nh=24):
    x, sr = sf.read(path); x = x if x.ndim == 1 else x.mean(1)
    pk = np.argmax(np.abs(x) > 0.2 * np.abs(x).max()); x = x[max(0, pk - 200):]
    # pitch by autocorrelation on 0.1..0.5 s
    seg = x[int(.1*sr):int(.5*sr)]; seg = seg - seg.mean()
    ac = np.correlate(seg, seg, 'full')[len(seg)-1:]
    lo, hi = int(sr/400), int(sr/25); lag = lo + np.argmax(ac[lo:hi]); f0 = sr / lag
    # refine with FFT peak
    N = 1 << 16
    def spec(a, b):
        s = x[int(a*sr):int(b*sr)]; w = np.hanning(len(s)); S = np.abs(np.fft.rfft(s*w, N)) / (w.sum()/2); return S
    fr = np.fft.rfftfreq(N, 1/sr)
    S = spec(0.03, 0.33)
    def peak(S, f, tol=0.04):
        m = (fr > f*(1-tol)) & (fr < f*(1+tol)); i = np.argmax(S[m]); return fr[m][i], S[m][i]
    f0, _ = peak(S, f0, 0.06)
    P = []; fk = f0
    for k in range(1, nh+1):
        f, a = peak(S, k*f0*(1+ (0 if k < 3 else 0.0)), 0.03 if k > 2 else 0.05)
        P.append((f, a))
    # decay: level of each partial in successive 150 ms windows
    T = np.arange(0.03, min(2.5, len(x)/sr - 0.2), 0.15); dec = []
    for k in range(1, nh+1):
        L = []
        for t in T:
            St = spec(t, t+0.15); _, a = peak(St, P[k-1][0], 0.02); L.append(20*np.log10(a+1e-12))
        L = np.array(L); ok = L > L.max() - 35
        n = max(3, min(len(L), np.argmin(ok) if not ok.all() else len(L)))
        sl = np.polyfit(T[:n], L[:n], 1)[0] if n >= 3 else np.nan
        early = np.polyfit(T[:3], L[:3], 1)[0]
        dec.append((sl, early))
    # inharmonicity B from partial frequencies: f_k = k f0 sqrt(1+B k^2)
    ks = np.arange(1, nh+1); fs_ = np.array([p[0] for p in P]); am = np.array([p[1] for p in P])
    good = am > am.max()*0.003
    y = (fs_[good]/(ks[good]*f0))**2 - 1; B = np.polyfit(ks[good]**2, y, 1)[0] if good.sum() > 4 else 0
    env = np.abs(x[:int(.08*sr)]); att = np.argmax(env)/sr
    return dict(f0=f0, amps_db=(20*np.log10(am/am.max())).round(1).tolist(), decay_db_s=[round(d[0],1) for d in dec],
                early_db_s=[round(d[1],1) for d in dec], B=B, attack_ms=att*1000, peak=float(np.abs(x).max()),
                rms_early=float(np.sqrt((x[:int(.2*sr)]**2).mean())))
out = {}
for f in sorted(glob.glob(D + '*_vl*_rr1.wav')):
    n = os.path.basename(f)[:-4]
    if n.startswith('perc'): continue
    out[n] = analyse(f)
json.dump(out, open('ref_measurements.json', 'w'), indent=1)
for n, m in out.items():
    if 'vl4' in n or 'vl1' in n:
        print(n, 'f0 %.1f B %.1e att %.0fms peak %.2f' % (m['f0'], m['B'], m['attack_ms'], m['peak']))
        print('   amps', m['amps_db'][:14]); print('   decay', m['decay_db_s'][:14])
