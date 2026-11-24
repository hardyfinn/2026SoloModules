import numpy as np, sys
sys.path.insert(0, '.')
from mosaic_proto import *
rng = np.random.default_rng(2)
nz = rng.normal(0, 0.1, (4*SR, 2)); sil = np.zeros((4*SR, 2))
t = np.arange(4*SR)/SR; sine = np.repeat((0.25*np.sin(2*np.pi*440*t))[:, None], 2, 1)
r = lambda v: 20*np.log10(np.sqrt(np.mean(v**2))+1e-12)
print('AGE on the grains (wet only, pitch 0, delay 0.25 s, 16 grains/s of 0.25 s)')
for age in (0.0, 0.25, 0.5, 0.75, 1.0):
    y, _ = process(nz, rate=16, size=0.25, scan=0.25, age=age)
    X = np.abs(np.fft.rfft(y[SR:, 0]))**2; f = np.fft.rfftfreq(len(y)-SR, 1/SR)
    band = lambda a, b: 10*np.log10(X[(f > a) & (f < b)].mean())
    h, _ = process(sil, rate=16, size=0.25, scan=0.25, age=age)
    s, _ = process(sine, rate=16, size=0.25, scan=0.25, age=age)
    S = np.abs(np.fft.rfft(s[SR:, 0]*np.hanning(len(s)-SR))); pk = f[np.argmax(S)]
    # spread of the 440 Hz peak: width holding 90% of the energy between 400 and 480 Hz
    m = (f > 400) & (f < 480); c = np.cumsum(S[m]**2); c /= c[-1]; wdt = f[m][np.searchsorted(c, 0.95)] - f[m][np.searchsorted(c, 0.05)]
    print(f'  AGE {age:.2f}: noise in, 8-12 kHz against 0.2-2 kHz {band(8000, 12000) - band(200, 2000):+6.1f} dB; silence in, output {r(h[SR:]):7.1f} dB; 440 Hz tone, 90% of its energy within {wdt:5.1f} Hz')
