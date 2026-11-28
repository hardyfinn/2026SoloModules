"""The first 120 ms of a pluck, real against model: level above 1.5 kHz in 2 ms steps (the finger's burst),
level of the whole sound over one period at a time (the swell of the note), and how noisy the start is."""
import numpy as np, soundfile as sf, sys, upright as U
from scipy.signal import butter, sosfilt, resample_poly
D = '/tmp/bassref/mb/Samples/pizz/'; SR = U.SR
NOTES = [('gb1', 30), ('a1', 33), ('c2', 36), ('eb2', 39), ('a2', 45), ('c3', 48)]
TH = [0, 2, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96]
def load(nm, vl):
    x, sr = sf.read(D + '%s_%s_rr1.wav' % (nm, vl)); x = x if x.ndim == 1 else x.mean(1)
    if sr != SR: x = resample_poly(x, SR, sr)
    return x / np.abs(x).max()
def measures(x, f0, start):
    hp = sosfilt(butter(4, 1500, 'highpass', fs=SR, output='sos'), x)
    mid = sosfilt(butter(4, [300, 1500], 'bandpass', fs=SR, output='sos'), x)
    def lv(y, t, w): 
        a = start + int(t * SR / 1000); return 10 * np.log10(np.mean(y[a:a + int(w * SR / 1000)] ** 2) + 1e-12)
    P = 1000.0 / f0
    ref = lv(x, 100, 2 * P)                                   # the settled note, 100 ms in
    H = [lv(hp, t, 2) - ref for t in TH]; M = [lv(mid, t, 4) - ref for t in TH]
    W = [lv(x, k * P, P) - ref for k in range(6)]             # whole sound, period by period
    return np.array(H), np.array(M), np.array(W)
if __name__ == '__main__':
    eh = []; em = []; ew = []
    for vl, vel in (('vl2', 0.6), ('vl4', 0.92)):
        for nm, midi in NOTES:
            x = load(nm, vl); st = int(np.argmax(np.abs(x) > 0.02))
            m = U.play([dict(t=0.02, pitch=midi, vel=vel, dur=1.0)], U.Tone(growl=0.1, age=0.0, pickup=0.1), total=1.3); m = m / np.abs(m).max()
            a = measures(x, U.hz(midi), st); b = measures(m, U.hz(midi), int(0.02 * SR) - 48)
            eh.append(np.abs(a[0] - b[0]).mean()); em.append(np.abs(a[1] - b[1]).mean()); ew.append(np.abs(a[2] - b[2]).mean())
            if len(sys.argv) > 1:
                r = lambda v: ' '.join('%4d' % q for q in np.round(v))
                print(nm, vl, '  ms:        ', ' '.join('%4d' % t for t in TH))
                print('   above 1.5k real ', r(a[0])); print('   above 1.5k model', r(b[0]))
                print('   300-1500   real ', r(a[1])); print('   300-1500   model', r(b[1]))
                print('   per period real ', r(a[2])); print('   per period model', r(b[2]))
    print('mean error: burst above 1.5 kHz %.1f dB, 300-1500 Hz %.1f dB, period-by-period swell %.1f dB' % (np.mean(eh), np.mean(em), np.mean(ew)))
