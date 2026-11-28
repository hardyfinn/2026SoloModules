"""Model against the measured samples: partial levels just after the pluck and decay rates."""
import json, numpy as np, random
import upright as U
M = json.load(open('ref_measurements.json'))
NAMES = {'c1': 24, 'eb1': 27, 'gb1': 30, 'a1': 33, 'c2': 36, 'eb2': 39, 'gb2': 42, 'a2': 45, 'c3': 48, 'eb3': 51}
def analyse(x, f0, nh=10):
    sr = U.SR; N = 1 << 16; fr = np.fft.rfftfreq(N, 1 / sr)
    def spec(a, b):
        s = x[int(a * sr):int(b * sr)]; w = np.hanning(len(s)); return np.abs(np.fft.rfft(s * w, N)) / (w.sum() / 2)
    def peak(S, f):
        m = (fr > f * 0.97) & (fr < f * 1.03); return S[m].max()
    S = spec(0.03, 0.33); am = np.array([peak(S, k * f0) for k in range(1, nh + 1)])
    T = np.arange(0.03, 1.5, 0.15); dec = []
    for k in range(1, nh + 1):
        L = np.array([20 * np.log10(peak(spec(t, t + 0.15), k * f0) + 1e-12) for t in T]); ok = L > L.max() - 35
        n = max(3, np.argmin(ok) if not ok.all() else len(L)); dec.append(np.polyfit(T[:n], L[:n], 1)[0])
    return 20 * np.log10(am / am.max()), np.array(dec)
tone = U.Tone(growl=0.0, age=0.0, pickup=0.0, thump=0.0)
errs = []; VERB = True
for name, midi in NAMES.items():
    for vl, vel in (('vl1', 0.3), ('vl4', 0.95)):
        m = M.get('%s_%s_rr1' % (name, vl))
        if not m or abs(m['f0'] - U.hz(midi)) > 3: continue
        x = U.play([dict(t=0.0, pitch=midi, vel=vel, dur=2.2)], tone, total=3.0)
        a, d = analyse(x, U.hz(midi))
        ra = np.array(m['amps_db'][:10]); rd = np.array(m['decay_db_s'][:10])
        e = np.abs(a[:6] - ra[:6]).mean(); errs.append(e)
        if VERB: print('%-4s %s  model amps %s' % (name, vl, np.round(a[:8]).astype(int)))
        if VERB: print('         real  amps %s   mean diff first six %.1f dB' % (np.round(ra[:8]).astype(int), e))
        if VERB: print('         decay model %s real %s' % (np.round(-d[:6]).astype(int), np.round(-rd[:6]).astype(int)))
print('mean level error over the first six partials: %.1f dB' % np.mean(errs))
