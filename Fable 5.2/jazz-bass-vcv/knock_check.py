"""How much of the start of a note is NOT the note: energy between the harmonics (body knock, finger, rattle)
against energy on them, real against model, in two bands and three time windows."""
import numpy as np, sys, upright as U, attack_check as A
SR = U.SR
WIN = [(0.0, 0.05), (0.05, 0.15), (0.2, 0.4)]
if __import__('os').environ.get('FINE'): WIN = [(0.0, 0.03), (0.03, 0.08), (0.08, 0.16), (0.16, 0.3), (0.3, 0.5)]
def between(x, f0):
    out = []
    for a, b in WIN:
        s = x[int(a * SR):int(b * SR)]; s = s * np.hanning(len(s)); N = 1 << 16; S = np.abs(np.fft.rfft(s, N)) ** 2; fr = np.fft.rfftfreq(N, 1 / SR)
        bw = max(0.12 * f0, 1.6 * SR / len(s))                       # main-lobe width of this window
        on = np.abs(((fr / f0 + 0.5) % 1.0) - 0.5) * f0 < bw
        for lo, hi in ((90, 900), (900, 5000)):
            m = (fr > lo) & (fr < hi); out.append(10 * np.log10(S[m & ~on].sum() / (S[m & on].sum() + 1e-20) + 1e-12))
    return out
def run(mod=U, label='model'):
    R = []; M = []
    for vl, vel in (('vl2', 0.6), ('vl4', 0.92)):
        for nm, midi in A.NOTES[2:] + [('eb3', 51), ('gb2', 42)]:    # from C2 up, where harmonics are far enough apart to see between them
            try: x = A.load(nm, vl)
            except Exception: continue
            st = int(np.argmax(np.abs(x) > 0.02)); R.append(between(x[st:], U.hz(midi)))
            m = mod.play([dict(t=0.0, pitch=midi, vel=vel, dur=1.15)], mod.Tone(growl=0.1, age=0.0, pickup=0.1), total=1.3)
            st = int(np.argmax(np.abs(m) > 0.02 * np.abs(m).max())); M.append(between(m[st:], U.hz(midi)))
    R = np.mean(R, 0); M = np.mean(M, 0)
    print('between-harmonics energy relative to on-harmonics (dB):   0-50 ms low, high | 50-150 ms low, high | 200-400 ms low, high')
    print('  real  ', ' '.join('%6.1f' % v for v in R)); print('  %-6s' % label, ' '.join('%6.1f' % v for v in M))
    return float(np.abs(R - M).mean())
if __name__ == '__main__':
    print('mean gap %.1f dB' % run())
