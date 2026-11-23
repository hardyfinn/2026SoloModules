"""Scratch machine: reference engine (Fable, 2026-11-23). Design prototype, not the device.

One deck. A record hand moves the record, a fader hand opens and closes the fader.
Every scratch ("word") is a short script for the two hands, placed on the beat grid.

  record    p(t), position on the record in seconds; v(t) = dp/dt, 1.0 is playing speed
  cartridge out = v * pre(p), then the phono (RIAA) curve. pre is the phrase with the
            inverse curve applied, as it is cut on vinyl. At v = 1 the two cancel. Slow
            strokes come out quiet and dark, fast ones loud and bright, and a held record
            is silent, which is how a real cartridge behaves.
  fader     g(t), a sharp cut after the phono stage
"""
import numpy as np, wave
from scipy.signal import lfilter, bilinear, resample_poly

SR = 44100
OS = 4                      # the cartridge stage runs at 4 x SR
T1, T2, T3 = 3180e-6, 318e-6, 75e-6     # RIAA time constants


def riaa_gain(f):
    w = 2j * np.pi * f
    return (1 + w * T2) / ((1 + w * T1) * (1 + w * T3))


def cut_record(x):
    """Apply the recording (inverse RIAA) curve to the phrase and upsample it OS times."""
    n = len(x); N = 1 << int(np.ceil(np.log2(n + SR)))
    X = np.fft.rfft(x, N); f = np.fft.rfftfreq(N, 1 / SR)
    H = riaa_gain(f) / abs(riaa_gain(1000.0))
    X = X / H * np.clip((21000 - f) / 2000, 0, 1)
    X[f < 15] = 0
    y = np.fft.irfft(X, N)[:n]
    return resample_poly(y, OS, 1)


def phono(y):
    """Playback RIAA curve at OS x SR, unity at 1 kHz."""
    fs = SR * OS
    b = [T2, 1.0]; a = np.polymul([T1, 1.0], [T3, 1.0])
    bz, az = bilinear(b, a, fs)
    return lfilter(bz, az, y) / abs(riaa_gain(1000.0))


class Hands:
    """0 = machine, 1 = loose. Same four dials as Doubles."""
    def __init__(s, h=0.5, timing=None, weight=None, slop=None, risk=None):
        T = h if timing is None else timing; W = h if weight is None else weight
        S = h if slop is None else slop; R = h if risk is None else risk
        s.T, s.W, s.S, s.R = T, W, S, R
        s.grid_jit = 0.006 * T          # s, where a stroke that should land on the grid lands
        s.turn_jit = 0.010 * T          # s, the outer turn-round of a stroke
        s.drift = 0.6                   # a late hand stays late for a while
        s.fad_jit = 0.005 * T           # s, fader hand against record hand
        s.trav = 0.22 * S               # stroke length varies by this fraction
        s.land = 0.003 * S              # s of record, error coming back to the cue
        s.land_per = 0.05 * S           # plus this much per second of record travelled
        s.lvl = 0.06 * S
        s.fade = 0.0006 + 0.0016 * S    # s, fader cut time
        s.leak = 0.010 * S              # s, fader closes late after a sound, lets a bit through
        s.skew = 0.80 - 0.22 * W        # pushes peak early; heavier hand, earlier peak
        s.tau_rel = 0.006 + 0.022 * W   # s, platter pick-up after a let-go
        s.tau_stop = 0.003 + 0.010 * W
        s.tremor = 0.02 * S
        s.flub = 0.10 * R               # chance per word that something goes wrong


class Deck:
    def __init__(s, n, H, rng, cue, lo, hi):
        s.n = n; s.H = H; s.rng = rng; s.cue = cue; s.lo = lo; s.hi = hi
        s.v = np.zeros(n); s.t = 0; s.p = cue; s.p0 = cue
        s.opens = []; s.clicks = []
        s.late = 0.0

    def _put(s, c):
        m = min(len(c), s.n - s.t)
        if m <= 0: return
        s.v[s.t:s.t + m] = c[:m]; s.p += c[:m].sum() / SR; s.t += m

    def hold(s, t1):
        m = int(t1) - s.t
        if m <= 0: return
        v0 = s.v[s.t - 1] if s.t else 0.0
        c = v0 * np.exp(-(np.arange(m) + 1) / (s.H.tau_stop * SR)) if abs(v0) > 1e-3 else np.zeros(m)
        if s.H.tremor:
            k = np.arange(m) / SR
            c = c + s.H.tremor * 0.2 * np.sin(2 * np.pi * (7 + 3 * s.rng.random()) * k + 6.28 * s.rng.random())
        s._put(c)

    def stroke(s, t1, travel, kind='push', vcap=3.0):
        """Move the record by `travel` seconds of record, ending at sample t1, speed zero at both ends."""
        t1 = int(t1); v0 = s.v[s.t - 1] if s.t else 0.0
        if abs(v0) > 0.05:                                    # the hand lands on a moving record
            m = int(min((t1 - s.t) / 4, 4 * s.H.tau_stop * SR))
            if m > 0:
                p_before = s.p
                s._put(v0 * np.exp(-(np.arange(m) + 1) / (s.H.tau_stop * SR)))
                travel -= s.p - p_before
        tgt = np.clip(s.p + travel, s.lo, s.hi); travel = tgt - s.p
        if vcap: t1 = max(t1, s.t + int(abs(travel) / vcap * SR))  # a hand has a top speed: it is late instead
        m = t1 - s.t
        if m < 8: return
        u = (np.arange(m) + 0.5) / m
        k = {'push': s.H.skew, 'pull': 1.0, 'flick': s.H.skew * 0.75, 'even': 1.0}[kind]
        sh = np.sin(np.pi * u ** k)
        if kind == 'even': sh = sh ** 0.6                     # a drag is held at speed for longer
        s._put(sh * (travel * SR / sh.sum()))

    def to(s, t1, target, kind='pull', vcap=3.0):
        s.stroke(t1, target - s.p, kind, vcap)

    def play(s, t1, rate=1.0):
        m = int(t1) - s.t
        if m <= 0: return
        v0 = s.v[s.t - 1] if s.t else 0.0
        s._put(rate + (v0 - rate) * np.exp(-(np.arange(m) + 1) / (s.H.tau_rel * SR)))

    def wiggle(s, t1, f, amp):
        m = int(t1) - s.t
        if m <= 0: return
        cyc = max(1, round(m / SR * f)); k = np.arange(m) / m
        wob = 1 + s.H.S * 0.15 * np.sin(2 * np.pi * k * (1.3 + s.rng.random()))
        ph = 2 * np.pi * cyc * np.cumsum(wob) / wob.sum()
        c = amp * np.sin(ph) * np.minimum(1, np.minimum(k, 1 - k) * 12)
        c -= c.mean()
        s._put(c)

    def open(s, t0, t1, amp=1.0, exact=False):
        H = s.H
        if not exact:
            t0 += s.rng.normal(0, H.fad_jit) * SR
            t1 += (s.rng.normal(0, H.fad_jit) + H.leak * s.rng.random()) * SR
        if t1 - t0 > 0.004 * SR:
            s.opens.append((t0, t1, amp * min(1 + s.rng.normal(0, H.lvl), 1.12)))

    def click(s, t, closed=0.022):
        H = s.H
        if s.rng.random() < H.flub * 1.5: return               # a click that does not make it
        t += s.rng.normal(0, H.fad_jit) * SR
        w = closed * (1 + 0.3 * H.S * s.rng.normal()) * SR
        s.clicks.append((t - w / 2, t + w / 2))

    def gain(s):
        g = np.zeros(s.n)
        for a, b, amp in s.opens:
            a = max(int(a), 0); b = min(int(b), s.n)
            if b > a: g[a:b] = np.maximum(g[a:b], amp)
        for a, b in s.clicks:
            a = max(int(a), 0); b = min(int(b), s.n)
            if b > a: g[a:b] = 0
        m = max(2, int(s.H.fade * SR))
        return np.convolve(g, np.ones(m) / m, 'same')

    def audio(s, rec_cut):
        s.hold(s.n)
        p = s.p0 + (np.cumsum(s.v) - s.v) / SR
        k = np.arange(s.n * OS) / OS
        po = np.interp(k, np.arange(s.n), p); vo = np.interp(k, np.arange(s.n), s.v)
        idx = np.clip(po * SR * OS, 0, len(rec_cut) - 2)
        i = idx.astype(int); fr = idx - i
        y = (rec_cut[i] * (1 - fr) + rec_cut[i + 1] * fr) * vo
        y = resample_poly(phono(y), 1, OS)[:s.n]
        return y * s.gain(), p


class Scratch:
    """Places words on the beat grid. Beats count from 0; u is a stroke length in beats."""
    def __init__(s, rec, bpm, beats, hands=0.5, seed=1, cue=0.0, swing=0.5, **hk):
        s.rec = rec; s.cut = cut_record(rec); s.bpm = bpm; s.spb = 60.0 / bpm
        s.rng = np.random.default_rng(seed); s.H = Hands(hands, **hk); s.swing = swing
        s.n = int(beats * s.spb * SR) + SR // 2
        s.d = Deck(s.n, s.H, s.rng, cue, 0.0, len(rec) / SR - 0.002)
        s.log = []

    # ---- time
    def sec(s, b):
        w = int(np.floor(b * 2)); x = b * 2 - w               # swing the off 16ths
        x = x * s.swing / 0.5 if x < 0.5 else s.swing + (x - 0.5) * (1 - s.swing) / 0.5
        return (w + x) / 2 * s.spb

    def at(s, b, jit='grid'):
        d = s.d; H = s.H
        sd = H.grid_jit if jit == 'grid' else H.turn_jit
        d.late = H.drift * d.late + (1 - H.drift) * s.rng.normal(0, H.grid_jit * 1.5)
        return int((s.sec(b) + d.late + s.rng.normal(0, sd)) * SR)

    def cue(s, c):
        """Move the cue point (silently, between words)."""
        s.d.cue = c

    def start(s, b):
        d = s.d; tb = s.at(b)
        if abs(d.p - d.cue) > 0.08:                           # get to the cue first, fader closed
            dist = abs(d.p - d.cue); need = int(min(0.06 + dist / 4.0, 0.40) * SR)
            ta = d.t
            d.to(d.t + need, d.cue + s._err(dist), 'pull', None)
            d.clicks.append((ta - 0.004 * SR, d.t))               # fader shut (or needle up) for the move    # takes the time it takes; the word is late if there is none
        d.hold(tb); return d.t

    def _err(s, dist):
        H = s.H; e = s.rng.normal(0, H.land + H.land_per * dist)
        if s.rng.random() < H.flub * 0.5: e -= 0.03 + 0.03 * s.rng.random()   # pulled back too far
        return e

    def _trav(s, u, sp, cap=None):
        t = u * s.spb * sp * max(0.4, 1 + s.rng.normal(0, s.H.trav))
        return min(t, cap) if cap else t

    def _fwd(s, b1, travel, kind='push'):
        s.d.stroke(s.at(b1, 'turn'), travel, kind); return s.d.t

    def _back(s, b1, kind='pull'):
        d = s.d; s.d.to(s.at(b1), d.cue + s._err(abs(d.p - d.cue)), kind); return d.t

    def _silent_back(s, b1):
        """Pull back to the cue with the fader closed. Quick, but a hand has a top speed."""
        d = s.d; dist = abs(d.p - d.cue); t1 = s.at(b1)
        quick = d.t + int((0.06 + dist / 2.5) * SR); least = d.t + int((0.04 + dist / 5.0) * SR)
        d.to(max(min(t1, quick), least), d.cue + s._err(dist), 'pull', None)

    # ---- words
    def baby(s, b, u=0.25, n=1, sp=1.0, cap=None, kind='push'):
        t0 = s.start(b)
        for i in range(n):
            s._fwd(b + (2 * i + 1) * u, s._trav(u, sp, cap), kind); s._back(b + (2 * i + 2) * u, 'pull' if kind == 'push' else kind)
        s.d.open(t0 - 0.002 * SR, s.d.t); s.log.append(('baby', b, 2 * n * u))

    def drag(s, b, u=0.5, n=1, sp=0.35):
        s.baby(b, u, n, sp, kind='even'); s.log[-1] = ('drag', b, 2 * n * u)

    def tip(s, b, u=0.125, n=2):
        s.baby(b, u, n, 0.9, cap=0.045); s.log[-1] = ('tip', b, 2 * n * u)

    def forward(s, b, snd=0.25, ret=0.25, amp=1.0, frac=0.86):
        t0 = s.start(b); s.d.play(s.at(b + snd * frac)); t1 = s.d.t
        s.d.open(t0 - 0.002 * SR, t1, amp); s._silent_back(b + snd + ret); s.log.append(('forward', b, snd + ret))

    def reverse(s, b, snd=0.25, pre=0.25, sp=1.0):
        t0 = s.start(b - pre); s.d.stroke(s.at(b), s._trav(snd, sp), 'push'); t1 = s.d.t
        s._back(b + snd); s.d.open(t1, s.d.t); s.log.append(('reverse', b - pre, snd + pre))

    def stab(s, b, snd=0.125, ret=0.125, sp=1.7):
        t0 = s.start(b); s._fwd(b + snd, s._trav(snd, sp), 'flick'); t1 = s.d.t
        s.d.open(t0 - 0.002 * SR, t0 + 0.72 * (t1 - t0)); s._silent_back(b + snd + ret); s.log.append(('stab', b, snd + ret))

    def lazer(s, b, snd=0.125, ret=0.375, travel=0.26, tail=True):
        t0 = s.start(b); s.d.stroke(s.at(b + snd, 'turn'), travel * (1 + s.rng.normal(0, s.H.trav)), 'flick', 4.5); t1 = s.d.t
        if tail: s._back(b + snd + ret, 'even'); s.d.open(t0 - 0.002 * SR, s.d.t)
        else: s.d.open(t0 - 0.002 * SR, t1); s._silent_back(b + snd + ret)
        s.log.append(('lazer', b, snd + ret))

    def chirp(s, b, u=0.25, n=1, sp=1.1):
        t0 = s.start(b); a = t0 - 0.002 * SR
        for i in range(n):
            f0 = s.d.t; f1 = s._fwd(b + (2 * i + 1) * u, s._trav(u, sp)); b1 = s._back(b + (2 * i + 2) * u)
            s.d.open(a, f0 + 0.52 * (f1 - f0)); a = f1 + 0.50 * (b1 - f1)
        s.d.open(a, s.d.t); s.log.append(('chirp', b, 2 * n * u))

    def flare(s, b, u=0.5, n=1, clicks=1, sp=1.0):
        t0 = s.start(b)
        for i in range(2 * n):
            a = s.d.t
            if i % 2 == 0: z = s._fwd(b + (i + 1) * u, s._trav(u, sp))
            else: z = s._back(b + (i + 1) * u)
            for c in range(clicks): s.d.click(a + (c + 1) / (clicks + 1) * (z - a))
        s.d.open(t0 - 0.002 * SR, s.d.t); s.log.append(('flare%d' % clicks, b, 2 * n * u))

    def crab(s, b, u=0.5, taps=4, both=True, sp=0.9):
        t0 = s.start(b)
        for i in range(2 if both else 1):
            a = s.d.t
            z = s._fwd(b + u, s._trav(u, sp)) if i == 0 else s._back(b + 2 * u)
            gap = min((z - a) / taps, 0.060 * SR) * (1 + 0.1 * s.H.T * s.rng.normal())
            for k in range(taps): s.d.open(a + k * gap, a + (k + 0.52) * gap)
        if not both: s._silent_back(b + 2 * u)
        s.log.append(('crab', b, 2 * u))

    def tear(s, b, u=0.25, form='f-bb', sp=1.1):
        t0 = s.start(b); tr = s._trav(u, sp); c = s.d.cue
        if form == 'f-bb':
            s._fwd(b + u, tr); s.d.to(s.at(b + 2 * u, 'turn'), c + tr * 0.48, 'pull'); s._back(b + 3 * u)
        else:
            s._fwd(b + u, tr * 0.5); s._fwd(b + 2 * u, tr * 0.5); s._back(b + 3 * u)
        s.d.open(t0 - 0.002 * SR, s.d.t); s.log.append(('tear', b, 3 * u))

    def scribble(s, b, dur=1.0, f=15.0, amp=1.25):
        t0 = s.start(b); s.d.wiggle(s.at(b + dur), f, amp); s.d.open(t0 - 0.002 * SR, s.d.t)
        s.log.append(('scribble', b, dur))

    def transform(s, b, dur=2.0, step=0.25, duty=0.5, pat=None, mode='drag', sp=0.45):
        t0 = s.start(b)
        if mode == 'drag':
            s.d.stroke(s.at(b + dur / 2, 'turn'), s._trav(dur / 2, sp), 'even'); s._back(b + dur, 'even')
        else:
            s.d.play(s.at(b + dur))
        k = 0
        while k * step < dur - 1e-6:
            if pat is None or pat[k % len(pat)]:
                g0 = s.at(b + k * step); g1 = s.at(b + (k + duty) * step)
                if g1 > t0: s.d.open(max(g0, t0), g1)
            k += 1
        if mode != 'drag': s._silent_back(b + dur + 0.5)
        s.log.append(('transform', b, dur))

    def release(s, b, n=2.0, ret=0.5):
        t0 = s.start(b); s.d.play(s.at(b + n)); s.d.open(t0 - 0.002 * SR, s.d.t)
        s._silent_back(b + n + ret); s.log.append(('release', b, n + ret))

    def echo(s, b, u=0.5, n=4, k=0.6):
        for i in range(n): s.forward(b + i * u, u * 0.55, u * 0.45, amp=k ** i)

    def render(s):
        y, p = s.d.audio(s.cut); return y, p, s.d.v, s.d.gain()


def write_wav(path, x, peak=0.89):
    x = np.asarray(x); x = x / max(np.abs(x).max(), 1e-9) * peak
    w = wave.open(path, 'wb'); w.setnchannels(1 if x.ndim == 1 else 2); w.setsampwidth(2); w.setframerate(SR)
    w.writeframes((x * 32767).astype('<i2').tobytes()); w.close()
