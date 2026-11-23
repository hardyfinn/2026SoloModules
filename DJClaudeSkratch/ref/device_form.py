"""The cartridge stage in the form the device can run (Fable, 2026-11-23).

scratch_engine.py cuts the record with an FFT and filters with scipy. A Max device cannot do
that the same way, so this file is the stage as the build should do it, and a check that it
gives the same sound.

  on load (JavaScript):   up = 4x upsample of the record (windowed sinc, 16 taps per phase)
                          cut = PRE(up), two first-order sections at 4 x SR
  per sample (gen~):      four sub-steps: read cut at p (linear), times v, PLAY filter,
                          then a low-pass; keep the fourth result
  PLAY is the phono curve by bilinear transform at 4 x SR. PRE is its exact inverse
  (numerator and denominator swapped), so at playing speed they cancel to rounding error.
"""
import numpy as np
from scipy.signal import lfilter, ellip, sosfilt, freqz
SR = 44100; OS = 4; FS = SR * OS
T1, T2, T3 = 3180e-6, 318e-6, 75e-6
F4 = 20000.0                                   # extra corner so the inverse is a proper filter


def section(fz, fp):
    """(1 + s/wz) / (1 + s/wp) by bilinear transform at FS, corners prewarped."""
    a = np.tan(np.pi * fz / FS); b = np.tan(np.pi * fp / FS)
    num = np.array([1 + 1 / a, 1 - 1 / a]); den = np.array([1 + 1 / b, 1 - 1 / b])
    return num / den[0], den / den[0]

F1, F2, F3 = (1 / (2 * np.pi * t) for t in (T1, T2, T3))
PLAY = [section(F2, F1), section(F4, F3)]      # phono: zeros at 500 Hz and F4, poles at 50 Hz and 2122 Hz
_, _h = freqz(np.convolve(PLAY[0][0], PLAY[1][0]), np.convolve(PLAY[0][1], PLAY[1][1]), [1000.0], fs=FS)
G1K = 1 / abs(_h[0])                           # make the phono stage unity at 1 kHz
LOWPASS = ellip(6, 0.1, 70, 19000, fs=FS, output='sos')   # three biquads, run at 4 x SR


def upsample4(x, taps=16):
    n = taps * OS; k = np.arange(-n, n + 1)
    h = np.sinc(k / OS * 0.92) * 0.92 * np.kaiser(2 * n + 1, 9.0)
    y = np.zeros(len(x) * OS); y[::OS] = x
    return np.convolve(y, h, 'same')


def cut(x):
    y = upsample4(x)
    for b, a in PLAY: y = lfilter(a / b[0], b / b[0], y)   # the inverse section
    return y / G1K


def play(cutbuf, p, v):
    """p in seconds and v, both at SR. Returns audio at SR (before the fader)."""
    n = len(p); k = np.arange(n * OS) / OS
    po = np.interp(k, np.arange(n), p); vo = np.interp(k, np.arange(n), v)
    idx = np.clip(po * FS, 0, len(cutbuf) - 2); i = idx.astype(int); fr = idx - i
    y = (cutbuf[i] * (1 - fr) + cutbuf[i + 1] * fr) * vo
    for b, a in PLAY: y = lfilter(b, a, y)
    y = sosfilt(LOWPASS, y * G1K)
    return y[OS - 1::OS]


if __name__ == '__main__':
    import sys
    from scratch_engine import Scratch
    from sounds import record
    rec, cues = record(); c = cut(rec)
    # 1. playing speed gives the record back
    n = len(rec) - 4000; p = np.arange(n) / SR; y = play(c, p, np.ones(n))
    best = min(range(0, 40), key=lambda d: np.abs(y[d + 2000:n] - rec[2000:n - d]).max())
    e = np.abs(y[best + 2000:n] - rec[2000:n - best]).max() / np.abs(rec).max()
    print('playing speed: delay %d samples, worst error %.1f dB' % (best, 20 * np.log10(e)))
    # 2. the same performance through both engines
    s = Scratch(rec, 96, 16, hands=0.5, seed=3, cue=cues['ah'], swing=0.58)
    s.baby(0, .25, 2); s.stab(1); s.stab(1.5); s.drag(2, 1.0); s.lazer(4); s.chirp(5, .25, 2); s.flare(6, .5, 1, 1); s.scribble(7, 1.0)
    s.transform(8, 2, .25); s.release(10, 1.5, .5); s.tear(12, .25); s.crab(13, .5)
    ref, pp, vv, g = s.render(); dev = play(c, pp, vv) * g
    d = min(range(0, 40), key=lambda d: ((dev[d:] - ref[:len(ref) - d]) ** 2).sum())
    err = dev[d:] - ref[:len(ref) - d]
    print('performance: delay %d, difference %.1f dB below the signal' % (d, 10 * np.log10((ref ** 2).sum() / (err ** 2).sum())))
    print('PLAY sections (b0 b1 / a0 a1):'); [print('  ', np.round(b, 9), np.round(a, 9)) for b, a in PLAY]
    print('gain %.6f' % G1K); print('LOWPASS sos:'); print(np.round(LOWPASS, 9))
    f = np.array([1000, 5000, 10000, 15000, 18000, 20000, 24100, 26000, 30000, 44100.0])
    _, H = freqz(*__import__('scipy.signal', fromlist=['sos2tf']).sos2tf(LOWPASS), f, fs=FS); print('lowpass dB', dict(zip(f.astype(int), np.round(20 * np.log10(abs(H)), 1))))
