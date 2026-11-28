"""Texture measures a band-energy fit cannot see: is a band a few steady tones, or a restless wash?
 bin wobble : how much single 23 Hz wide bins fluctuate around their own smooth decay (dB). Steady tones give ~0, noise gives ~5.6.
 static     : how uneven the time-averaged spectrum is from bin to bin inside a band (dB). Plain noise gives ~1.5; fixed ringing partials give much more.
"""
import numpy as np
from kit_fit import *
def texture(x, t0=0.15, t1=1.8, lo=HI[0], hi=NB):
    N = 2048; hop = 2048; a = int(t0 * SR); fr = np.fft.rfftfreq(N, 1 / SR); w = np.hanning(N)
    F = np.array([np.abs(np.fft.rfft(x[s:s + N] * w)) ** 2 for s in range(a, int(t1 * SR) - N, hop)]) + 1e-30
    L = 10 * np.log10(F); t = np.arange(len(L)); out = []
    for i in range(lo, hi):
        k = np.where((fr >= EDGES[i]) & (fr < EDGES[i + 1]))[0]
        res = []
        for b in k:
            p = np.polyfit(t, L[:, b], 1); res.append(np.std(L[:, b] - np.polyval(p, t)))
        st = 10 * np.log10(F[:, k].mean(0)); out.append((float(np.mean(res)), float(np.std(st))))
    return np.array(out)
if __name__ == '__main__':
    import json, glob
    d = json.load(open('kit_fit.json')); REF = '/tmp/drumref/virt/Samples/oh'
    for k, p in [('ride', '/ride/oh_ride_ride_vl2_rr1*'), ('bell', '/ride/oh_ride_bell_vl2_rr1*'), ('crash', '/flatride/oh_flatride_crash_vl3*')]:
        r = texture(load(glob.glob(REF + p)[0])); m = texture(Cymbal(d[k]).stroke(dur=3.0))
        print(k); print('  Hz        ', ' '.join('%5d' % EDGES[i] for i in HI))
        print('  wobble rec', ' '.join('%5.1f' % v for v in r[:, 0])); print('  wobble mod', ' '.join('%5.1f' % v for v in m[:, 0]))
        print('  static rec', ' '.join('%5.1f' % v for v in r[:, 1])); print('  static mod', ' '.join('%5.1f' % v for v in m[:, 1]))
