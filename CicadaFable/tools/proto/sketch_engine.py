"""Cicada sketch prototype (design stage, offline).  One engine for tymbal, file and wing insects.

Every sound is a list of mechanical impulses (rib buckles, file teeth, wing strokes).  Each impulse
rings a resonator (tymbal plate, wing harp), which is exactly a decaying sine "grain" per impulse.
A second, movable resonator (abdomen air sac) follows.  Song structure is a list of muscle cycles.
"""
import numpy as np, soundfile as sf
from scipy import signal
SR = 48000

class Insect:
    def __init__(s, dur, seed=1):
        s.n = int(dur*SR); s.out = np.zeros((s.n, 2)); s.rng = np.random.default_rng(seed)
    def grain(s, t, amp, f, q, pan=0.5):
        bw = f/q; L = int(min(6.9/(np.pi*bw), 0.2)*SR)
        i0 = int(np.ceil(t*SR))
        if i0 < 0 or i0 >= s.n: return
        L = min(L, s.n-i0); tt = (i0+np.arange(L))/SR - t
        g = amp*np.exp(-np.pi*bw*tt)*np.sin(2*np.pi*f*tt)
        s.out[i0:i0+L, 0] += g*np.cos(pan*np.pi/2); s.out[i0:i0+L, 1] += g*np.sin(pan*np.pi/2)
    # ---- mechanisms -------------------------------------------------------------------
    def tymbal(s, t0, f, amp=1., ribs=4, rib_dt=None, drop=0.06, q=9, out=0.35, out_at=0.004,
               side=0, loose=0.05, spread=0.3):
        """One inward buckle of one tymbal: ribs pop in sequence, each a click, each a little lower.
        Then the tymbal springs back with a quieter, slightly higher out-click."""
        r = s.rng; fs = f*(1+0.012*(1 if side else -1)); pan = 0.5+(spread/2)*(1 if side else -1)
        rib_dt = rib_dt or 1.0/f
        t = t0
        for k in range(ribs):
            a = amp*(1.0-0.5*k/max(ribs, 1))*(1+loose*2*r.standard_normal())
            s.grain(t, a, fs*(1-drop*k/max(ribs-1, 1)), q, pan)
            t += rib_dt*(1+loose*r.standard_normal())
        if out > 0: s.grain(t0+out_at*(1+loose*r.standard_normal()), amp*out, fs*1.08, q*0.7, pan)
    def file(s, t0, dur, f, amp=1., lock=1.0, q=20, glide=-0.05, loose=0.0, shape=1.0, pan=0.5, tooth_rate=None):
        """One wing closure: the scraper crosses the file.  lock=1: one tooth per cycle of the carrier
        (cricket escapement, pure tone).  lock=0: teeth at their own jittered rate (katydid, noise)."""
        r = s.rng; t = 0.0
        while t < dur:
            x = t/dur; fc = f*(1+glide*(x-0.5))
            a = amp*np.sin(np.pi*min(max(x, 0), 1))**shape if x > 0 else 0.0
            a = max(a, 0.02*amp)
            jit = (1-lock)*0.45+loose
            s.grain(t0+t, a*(1+jit*r.standard_normal()), fc*(1+0.5*(1-lock)*0.1*r.standard_normal()), q, pan)
            rate = fc if tooth_rate is None else tooth_rate
            t += max(0.2/rate, (1/rate)*(1+jit*r.standard_normal()))
    # ---- body and air --------------------------------------------------------------------
    def abdomen(s, f_closed, f_open, wah, q=4, mix=0.8):
        """Movable air-sac resonator after the tymbal.  wah: array 0..1 (0 closed, 1 lifted/open)."""
        def bp(f): return signal.sosfilt(signal.butter(1, [f/(1+0.5/q), f*(1+0.5/q)], btype='band', fs=SR, output='sos'), s.out, axis=0)
        a, b = bp(f_closed), bp(f_open); w = wah[:, None]
        s.out = (1-mix)*s.out + mix*((1-w)*a + w*b)
    def finish(s, path, distance=0.15, level=0.5, fade=0.05):
        y = s.out
        if distance > 0:   # air and leaves: treble loss, a few scattered reflections
            fc = 18000*(1-distance)**2+2500
            y = signal.sosfilt(signal.butter(1, fc, fs=SR, output='sos'), y, axis=0)
            r = np.random.default_rng(7); wet = np.zeros_like(y)
            for k in range(24):
                d = int(r.uniform(0.004, 0.12)*SR); g = distance*0.5*np.exp(-d/SR/0.05)*r.choice([-1, 1])
                ch = r.integers(0, 2); wet[d:, ch] += g*y[:-d, r.integers(0, 2)]
            y = y+signal.sosfilt(signal.butter(1, 5000, fs=SR, output='sos'), wet, axis=0)
        y = signal.sosfilt(signal.butter(2, 120, btype='high', fs=SR, output='sos'), y, axis=0)
        f = int(fade*SR); y[:f] *= np.linspace(0, 1, f)[:, None]; y[-f:] *= np.linspace(1, 0, f)[:, None]
        act = np.abs(y).max(1) > 0.02*np.abs(y).max(); rms = np.sqrt(np.mean(y[act]**2))
        y = np.tanh(y/rms*0.16*(level/0.5))          # even loudness across sketches, soft ceiling
        sf.write(path, y, SR, subtype='PCM_16'); return y

def env(n, pts):
    """piecewise-linear control: pts = [(time_s, value), ...] -> array of n samples"""
    t = np.arange(n)/SR; return np.interp(t, [p[0] for p in pts], [p[1] for p in pts])
