"""Level against time in three bands, real pluck against model: checks the two-stage decay and the attack."""
import numpy as np, soundfile as sf, upright as U, os, sys
from scipy.signal import butter, sosfilt, resample_poly
D = '/tmp/bassref/mb/Samples/pizz/'; SR = U.SR
BANDS = [(25, 150), (150, 600), (600, 4000)]; T = [0.0, 0.03, 0.06, 0.1, 0.2, 0.4, 0.8]
def prof(x):
    out = []
    for lo, hi in BANDS:
        y = sosfilt(butter(4, [lo, hi], 'bandpass', fs=SR, output='sos'), x)
        out.append([10 * np.log10(np.mean(y[int(t * SR):int((t + 0.03) * SR)] ** 2) + 1e-12) for t in T])
    o = np.array(out); return o - o.max()
tot = []
for nm, midi in (('gb1', 30), ('a1', 33), ('c2', 36), ('eb2', 39), ('a2', 45), ('c3', 48)):
    for vl, vel in (('vl2', 0.6), ('vl4', 0.92)):
        x, sr = sf.read(D + '%s_%s_rr1.wav' % (nm, vl)); x = x if x.ndim == 1 else x.mean(1)
        if sr != SR: x = resample_poly(x, SR, sr)
        pk = np.argmax(np.abs(x) > 0.1 * np.abs(x).max()); x = x[max(0, pk - 200):]
        m = U.play([dict(t=0.004, pitch=midi, vel=vel, dur=1.2)], U.Tone(growl=0.1, age=0.0, pickup=0.1), total=1.4)
        a, b = prof(x), prof(m); tot.append(np.abs(a - b).clip(0, 30).mean())
        if len(sys.argv) > 1:
            print(nm, vl)
            for i, bd in enumerate(BANDS): print('  %4d-%4d real %s model %s' % (bd[0], bd[1], np.round(a[i]).astype(int), np.round(b[i]).astype(int)))
print('mean band-envelope error %.1f dB' % np.mean(tot))
