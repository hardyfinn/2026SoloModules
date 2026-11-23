"""Checks the reference engine against the review. Run: python3 check_ref.py"""
import numpy as np, sys
sys.path.insert(0, '.')
import sawstack_ref as R
SR = R.SR
def pitch_of(x, lo, hi):
    """dominant frequency in [lo, hi] by parabolic-interpolated FFT peak"""
    w = np.hanning(len(x)); S = np.abs(np.fft.rfft(x * w)); f = np.fft.rfftfreq(len(x), 1 / (SR * R.OS))
    m = (f >= lo) & (f <= hi); i = np.argmax(S * m)
    a, b, c = np.log(S[i - 1]), np.log(S[i]), np.log(S[i + 1]); d = 0.5 * (a - c) / (a - 2 * b + c)
    return (i + d) * (f[1] - f[0])
cents = lambda a, b: 1200 * np.log2(a / b)
N = SR * 8; z = np.zeros(N); one = np.ones(N)

print('1. clean engine (AGE 0, DRIVE 0): saw harmonics and alias floor')
o = R.render(z + 2.0, 0.0, 1.0, 0.0, one, age=0.0, drive=0.0, oscs=True)[2]   # C6 = 1046.5 Hz
S = np.abs(np.fft.rfft(o[-65536*2:] * np.hanning(65536*2))); f = np.fft.rfftfreq(65536*2, 1 / (SR * R.OS))
f1 = 1046.502; harm = np.zeros(len(f), bool)
for h in range(1, 46):
    harm |= np.abs(f - h * f1) < 30
band = f < 20000
print('   worst non-harmonic component below 20 kHz: %.1f dB below the fundamental' % (20 * np.log10(S[1:][harm[1:] & band[1:]].max() / S[band & ~harm][5:].max())))

print('2. coupling: two-oscillator and octave locking at AGE 1 (drift and jitter removed for the test)')
def lock_test(ratio_oct, detune_cents, couple_on):
    n = SR * R.OS * 6
    tune = np.zeros(5); tune[3] = ratio_oct + detune_cents / 1200.0; tune[0] = -3.3; tune[1] = -4.1; tune[4] = -5.2
    args = (np.zeros(n), np.zeros(n), np.ones(n), np.zeros(n), np.zeros(5), 1.0 if couple_on else 0.0, 0.0, 3,
            tune, np.ones(5), np.ones(5), np.ones(5), np.zeros(5), np.ones(5), np.full(5, np.exp(-2*np.pi*20000/(SR*R.OS))), np.zeros(5),
            np.array([0.1, 0.3, 0.0, 0.52, 0.9]), float(SR * R.OS), 0.0, 0.0)
    o = R.voice(*args)
    fa = pitch_of(o[2][n // 2:], 200, 330); fb = pitch_of(o[3][n // 2:], 200 * 2 ** ratio_oct, 330 * 2 ** ratio_oct)
    return cents(fb / 2 ** ratio_oct, fa)
for ratio in (0.0, 1.0, R.LOG2_3_2):
    for det in (0.5, 1.0, 2.0, 4.0):
        print('   interval %.3f oct, detuned %.1f cents: residual with coupling %+.2f cents, without %+.2f' % (ratio, det, lock_test(ratio, det, True), lock_test(ratio, det, False)))

print('3. mono output: level of each oscillator alone, mono vs stereo')
gl = np.cos((R.PANS + 1) * np.pi / 4); gr = np.sin((R.PANS + 1) * np.pi / 4)
print('   stereo L+R gains (old mono):', np.round(20 * np.log10((gl + gr) / (gl + gr)[0]), 2), 'dB;  new mono: all equal at', round(20 * np.log10(np.sqrt(0.5)), 2), 'dB')

print('4. envelope times stay true as AGE bends the segments (rise 0.5 s, fall 2 s)')
g = np.zeros(SR * 4); g[100:100 + SR] = 10.0
for age in (0.0, 0.6, 1.0):
    ts = []
    for sd in range(40):
        e, endp = R.envelope(g, 0.5, 2.0, age, sd)
        top = np.argmax(e >= 1.0); rise = (top - 100) / SR
        fall = (np.argmax(endp > 0) - (100 + SR)) / SR
        ts.append((rise, fall))
    ts = np.array(ts); print('   AGE %.1f: rise %.3f s (+-%.1f%%), fall %.3f s (+-%.1f%%)' % (age, ts[:, 0].mean(), 100 * ts[:, 0].std() / 0.5, ts[:, 1].mean(), 100 * ts[:, 1].std() / 2.0))

print('5. closed VCA at the default setting is silent; hiss and bleed only above AGE 0.6')
for age in (0.6, 0.8, 1.0):
    y = R.render(z[:SR * 2] - 1, 0.2, 1.0, 0.0, 0.0, age=age, drive=0.35)
    r = np.sqrt((y[SR // 2:] ** 2).mean()); print('   AGE %.2f: %s' % (age, 'silent' if r < 1e-7 else '%.1f dBFS' % (20 * np.log10(r))))

print('6. tracking: how flat the top oscillator of the octave stack sits (centre note C5, SPREAD full, AGE 1, no drift)')
tune, vt, st, shp, dc, lvl, lpc, droop, ph0 = R.unit_arrays(1, 1.0)
print('   droop per oscillator at 4 kHz, cents:', np.round(droop, 1))
