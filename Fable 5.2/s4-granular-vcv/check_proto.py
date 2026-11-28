import numpy as np, sys
sys.path.insert(0, '.')
from mosaic_proto import *
def peakf(x, lo=50, hi=8000):
    X = np.abs(np.fft.rfft(x*np.hanning(len(x)))); f = np.fft.rfftfreq(len(x), 1/SR)
    m = (f > lo) & (f < hi); return f[m][np.argmax(X[m])]
def rmsdb(x): return 20*np.log10(np.sqrt(np.mean(x**2))+1e-12)
t = np.arange(int(6*SR))/SR
sine = np.repeat((0.25*np.sin(2*np.pi*440*t))[:, None], 2, 1)
print('PITCH (440 Hz sine in, delay scan 0.25 s, 16 grains/s, size 0.25 s)')
for st in (-12, -7, 0, 7, 12, 24):
    y, s = process(sine, pitch=st, rate=16, size=0.25, scan=0.25)
    print(f'  {st:+3d} st: expect {440*2**(st/12):7.1f} Hz, got {peakf(y[2*SR:5*SR,0]):7.1f} Hz, max grains {s["max_grains"]}, shortened {s["shortened"]}')
print('DELAY (noise bursts in, pitch 0, no spray): lag of best match between input and wet')
rng = np.random.default_rng(3); nz = rng.normal(0, 0.1, len(t))*((t % 0.5) < 0.05)
nz2 = np.repeat(nz[:, None], 2, 1)
for d in (0.125, 0.25, 0.5, 1.0):
    y, s = process(nz2, rate=32, size=0.1, scan=d)
    a = nz[SR:5*SR]; bsig = y[SR:5*SR, 0]
    c = np.fft.irfft(np.fft.rfft(bsig)*np.conj(np.fft.rfft(a)))
    lag = np.argmax(c[:2*SR])/SR
    print(f'  set {d:.3f} s, measured {lag:.4f} s')
print('STRETCH (sweep 200 to 2000 Hz over 4 s, speed 0.5): pitch heard at t should be the sweep at t/2')
sw = 0.25*np.sin(2*np.pi*(200*t + 0.5*450*t**2)); sw[t > 4] = 0
y, s = process(np.repeat(sw[:, None], 2, 1), scan_mode='stretch', scan=0.5, rate=40, size=0.08)
for tt in (2.0, 3.0, 4.0, 5.0):
    seg = y[int((tt-0.1)*SR):int((tt+0.1)*SR), 0]
    print(f'  t={tt:.0f} s: expect {200+450*tt/2:6.0f} Hz, got {peakf(seg):6.0f} Hz')
print('LEVEL (low-passed noise in at about -20 dB RMS; wet only)')
w = rng.normal(0, 1, (len(t), 2)); w = w + np.roll(w, 1, 0) + np.roll(w, 2, 0); w *= 0.1/np.sqrt(np.mean(w**2))
print(f'  input {rmsdb(w[SR:]):.1f} dB')
for rate, size in ((4, 0.25), (16, 0.25), (64, 0.25), (64, 1.0)):
    for spray in (0.0, 0.5):
        y, s = process(w, rate=rate, size=size, scan=0.5, spray=spray, spray_mode='random_mono')
        print(f'  rate {rate:3d}/s size {size:.2f} s (overlap {rate*size:4.0f}) spray {spray:.1f}: wet {rmsdb(y[2*SR:]):6.1f} dB, peak {20*np.log10(np.abs(y).max()):6.1f} dB, max grains {s["max_grains"]}')
print('FEEDBACK stability (sine in for 1 s then silence, grain feedback, pitch 0): RMS of last second')
x = sine.copy(); x[SR:] = 0; x = np.vstack([x, np.zeros((6*SR, 2))])
for fb in (0.5, 0.9, 1.0, 1.5):
    y, s = process(x, rate=16, size=0.25, scan=0.25, feedback=fb)
    print(f'  feedback {fb:.1f}: {rmsdb(y[-SR:]):6.1f} dB, peak overall {20*np.log10(np.abs(y).max()+1e-12):5.1f} dB')
