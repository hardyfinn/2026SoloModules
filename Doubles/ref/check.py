"""Check a simulator render against the plan: every gated slot must sound the right part of the record."""
import json, sys, wave
import numpy as np

def load(p):
    w = wave.open(p); x = np.frombuffer(w.readframes(w.getnframes()), '<i2').reshape(-1, w.getnchannels()) / 32768
    return x.mean(1), w.getframerate()

def check(render, loop, tl, rec_start_beat, beats_per_loop, verbose=False, from_beat=0):
    y, sr = load(render); src, _ = load(loop)
    N = len(src); spb = N / beats_per_loop
    # record sample i = loop sample (i + rec_start_beat*spb) mod N
    off = int(round(rec_start_beat * spb)) % N
    rec = np.roll(src, -off)
    L = len(rec)
    res = []
    for d, plays in enumerate(tl):
        for p in plays:
            if p['kind'] != 0 or p['cm'] == 1 or p['bs'] < from_beat:
                continue
            slots = []
            if p['gm'] == 0:
                slots = [(p['bs'], p['be'])]
            else:
                j = int(np.ceil((p['bs'] - p['g0']) / p['gd'] - 1e-6))
                while p['g0'] + j * p['gd'] < p['be'] - 1e-9:
                    if j % 2 == p['gm'] - 1:
                        slots.append((max(p['bs'], p['g0'] + j * p['gd']), min(p['be'], p['g0'] + (j + 1) * p['gd'])))
                    j += 1
            for a, b in slots:
                i0 = int(a * spb + 0.004 * sr); i1 = int(b * spb - 0.002 * sr)
                if i1 - i0 < 0.02 * sr or i1 > len(y):
                    continue
                seg = y[i0:i1]
                pr = p['p0'] + (a - p['bs']) * spb * p['rate'] + 0.004 * sr * p['rate']
                best = (-1, 0)
                for lag in range(-1800, 1801, 3):
                    idx = (np.arange(len(seg)) * p['rate'] + pr + lag).astype(int) % L
                    ref = rec[idx]
                    c = float(np.dot(seg, ref) / np.sqrt(np.dot(seg, seg) * np.dot(ref, ref) + 1e-12))
                    if c > best[0]:
                        best = (c, lag)
                res.append((d, a, b, best[0], best[1] / sr * 1000))
    res.sort(key=lambda r: r[1])
    if verbose:
        for r in res:
            print('deck %d  %7.2f-%7.2f  corr %.3f  lag %+6.1f ms' % r)
    c = np.array([r[3] for r in res]); lg = np.array([r[4] for r in res])
    return c, lg

if __name__ == '__main__':
    render, loop, tlp, rs, bpl = sys.argv[1:6]
    tl = json.load(open(tlp))
    c, lg = check(render, loop, tl, float(rs), float(bpl), verbose='-v' in sys.argv)
    print('slots %d  corr min %.3f median %.3f  |lag| mean %.2f ms max %.2f ms' % (len(c), c.min(), np.median(c), np.abs(lg).mean(), np.abs(lg).max()))
