"""Where the model still differs from the recordings: level in octave bands against time, model minus real,
averaged over notes. Both are set to the same overall loudness first. Positive = model too loud there."""
import numpy as np, sys, upright as U, attack_check as A
from scipy.signal import butter, sosfilt
SR = U.SR
BANDS = [(30, 90), (90, 180), (180, 360), (360, 720), (720, 1500), (1500, 3000), (3000, 6000), (6000, 12000)]
FR = [(0, 10), (10, 30), (30, 60), (60, 120), (120, 250), (250, 500), (500, 1000)]
NOTES = A.NOTES + [('eb1', 27), ('gb2', 42), ('eb3', 51)]
def tf(x):
    x = x / np.sqrt(np.mean(x[:SR] ** 2)); out = np.zeros((len(BANDS), len(FR)))
    for i, (lo, hi) in enumerate(BANDS):
        y = sosfilt(butter(4, [lo, hi], 'bandpass', fs=SR, output='sos'), x)
        for j, (a, b) in enumerate(FR): out[i, j] = 10 * np.log10(np.mean(y[int(a * SR / 1000):int(b * SR / 1000)] ** 2) + 1e-10)
    return out
def run(mod=U, vls=(('vl2', 0.6), ('vl4', 0.92)), tone=None, verbose=True):
    Dm = []; Rm = []
    for vl, vel in vls:
        for nm, midi in NOTES:
            try: x = A.load(nm, vl)
            except Exception: continue
            st = int(np.argmax(np.abs(x) > 0.02)); x = x[st:]
            if len(x) < SR + 100: continue
            m = mod.play([dict(t=0.0, pitch=midi, vel=vel, dur=1.15)], tone or mod.Tone(growl=0.1, age=0.0, pickup=0.1), total=1.3)
            st = int(np.argmax(np.abs(m) > 0.02 * np.abs(m).max())); m = m[st:]
            r = tf(x); Rm.append(r); Dm.append(tf(m) - r)
    D = np.mean(Dm, 0); R = np.mean(Rm, 0); S = np.std(Dm, 0)
    if verbose:
        print('              ' + ' '.join('%4d-%-4d' % f for f in FR), ' ms')
        for i, b in enumerate(BANDS): print('%5d-%-5d Hz ' % b + ' '.join('  %+5.1f  ' % v for v in D[i]) + '   | real level ' + ' '.join('%4d' % v for v in R[i]))
    w = np.clip((R + 50) / 30.0, 0.1, 1.0)                     # louder cells matter more
    e = float((np.abs(D) * w).sum() / w.sum()); 
    print('weighted mean difference %.1f dB (bias only), spread between notes %.1f dB' % (e, float((S * w).sum() / w.sum())))
    return D, e
if __name__ == '__main__':
    if len(sys.argv) > 1:
        import upright_v1; print('FIRST MODEL'); run(upright_v1)
    print('NEW MODEL'); run()
