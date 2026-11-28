"""The body as a set of ringing resonances, read from the eight knock recordings: frequency, how long each rings, how strong."""
import numpy as np, soundfile as sf, json
from scipy.signal import resample_poly, find_peaks
SR = 48000; N = 1 << 16; fr = np.fft.rfftfreq(N, 1 / SR)
def spec(x, a, b):
    s = x[int(a * SR):int(b * SR)]; return np.abs(np.fft.rfft(s * np.hanning(len(s)), N)) / len(s)
K = []
for i in range(1, 9):
    x, sr = sf.read('/tmp/bassref/mb/Samples/pizz/perc%d_rr1.wav' % i); x = x if x.ndim == 1 else x.mean(1)
    if sr != SR: x = resample_poly(x, SR, sr)
    st = int(np.argmax(np.abs(x) > 0.05 * np.abs(x).max())); x = x[st:] / np.sqrt(np.mean(x[st:st + int(0.2 * SR)] ** 2)); K.append(x)
early = np.sqrt(np.mean([spec(x, 0.0, 0.12) ** 2 for x in K], 0)); late = np.sqrt(np.mean([spec(x, 0.08, 0.20) ** 2 for x in K], 0))
Ld = 20 * np.log10(early / early.max() + 1e-9)
pk, _ = find_peaks(Ld, distance=int(9 / (SR / N)), prominence=2.5); pk = pk[(fr[pk] > 40) & (fr[pk] < 3500) & (Ld[pk] > -52)]
modes = []
for p in pk:
    dec = 20 * np.log10(early[p] / (late[p] + 1e-12)) / 0.08          # dB/s
    modes.append(dict(f=float(fr[p]), db=float(Ld[p]), decay=float(np.clip(dec, 25, 400))))
modes.sort(key=lambda m: -m['db']); modes = sorted(modes[:40], key=lambda m: m['f'])
json.dump(modes, open('body_modes.json', 'w'), indent=0)
print(len(modes), 'modes'); print(' '.join('%d Hz %d dB %d dB/s |' % (m['f'], m['db'], m['decay']) for m in modes))
