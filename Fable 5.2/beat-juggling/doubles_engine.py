"""Doubles: reference engine for the beat juggling device (Fable, 2026-11-21).

Offline Python model of what the Max for Live device should do. Two decks read
the same recorded buffer ("the record"). Each deck is a platter with a speed
and a position; a crossfader gates them. Moves are lists of timed events.
Times inside the engine are in samples; the Seq class works in beats.
"""
import numpy as np, wave

SR = 44100


class Hands:
    """How the DJ's hands behave. h = 0 machine, h = 1 loose human.
    Four parts can be set apart from h:
      timing  how far releases and fader cuts stray from the beat
      weight  how heavy the hand is on the record (slow starts, pushes, drags)
      slop    how untidy the cueing and fader work is (missed cue points, bits of
              pull-back let through, uneven levels, a shaky held record)
      risk    how often a move is fluffed outright
    """
    def __init__(s, h, timing=None, weight=None, slop=None, risk=None):
        T = h if timing is None else timing; W = h if weight is None else weight
        Sl = h if slop is None else slop; R = h if risk is None else risk
        s.h = h
        s.tau = SR * (0.002 + 0.018 * W)        # spin-up time constant after a release
        s.tau_stop = SR * (0.003 + 0.012 * W)   # stop time constant when a hand lands
        s.pre = int(s.tau * (3.2 - 1.2 * W))    # how early the record is let go before its cue
        s.push = 0.10 * W                       # release is pushed (fast) or dragged (slow), std dev
        s.rel_jit = SR * 0.007 * T              # release timing scatter (std dev)
        s.rel_corr = 0.6                        # a late hand tends to stay late for a few moves
        s.rush = SR * 0.002 * T                 # fast passages creep early
        s.fad_jit = SR * 0.003 * T              # fader timing scatter (std dev)
        s.fad_early = SR * 0.002 * Sl           # fader opens a touch before the hit
        s.fad_late = SR * 0.004 * Sl            # and closes a touch after the hand has grabbed
        s.cue_base = SR * 0.002 * Sl            # cue point error, fixed part
        s.cue_per = 0.012 * Sl                  # cue point error per second of record pulled back
        s.tremor = 0.03 * Sl                    # a held record is never quite still
        s.lvl = 0.07 * Sl                       # level differences from cut to cut
        s.flub_p = 0.05 * R                     # chance that a cued hit is fluffed
        s.flub_sd = SR * 0.035
        s.correct = 0.8                         # how much of a timing error is nudged out afterwards
        s.vmax = 6.0 / max(h, 0.05)             # fastest pull-back, x real time
        s.reach = SR * 0.045 * h                # fixed time for a hand to grab the record
        s.fade = int(SR * (0.0008 + 0.0012 * h))  # fader cut time
        s.mismatch = 0.0012 * h                 # deck B pitch error
        s.wear = h                              # deck B copy is duller / quieter


def _wrap(d, L):
    return (d + L / 2) % L - L / 2


def render_deck(events, n, L, H, rng, p_init=0.0):
    spd = np.zeros(n); pos = np.zeros(n); gain = np.zeros(n)
    st = dict(t=0, p=p_init, v=0.0, late=0, jump=0, e=0.0, flub=0)
    if events and events[0]['ts'] == 0 and events[0]['kind'] == 'play':
        st['v'] = events[0]['rate']            # transport starts with the platter already at speed

    def put(chunk):
        t = st['t']; m = len(chunk)
        if m <= 0: return
        m = min(m, n - t); chunk = chunk[:m]
        spd[t:t+m] = chunk
        c = np.cumsum(chunk)
        pos[t:t+m] = st['p'] + np.concatenate(([0.0], c[:-1]))
        st['p'] += c[-1]; st['v'] = chunk[-1]; st['t'] = t + m

    def gate(a, b, amp=1.0):
        a = int(a - H.fad_early + rng.normal(0, H.fad_jit)) if a > 0 else 0
        b = int(b + H.fad_late + rng.normal(0, H.fad_jit))
        a = max(a, 0); b = min(b, n)
        if b > a: gain[a:b] = amp * min(1 + rng.normal(0, H.lvl), 1.12)

    def stop_to(t1):
        m = t1 - st['t']
        if m > 0: put(st['v'] * np.exp(-(np.arange(m) + 1) / H.tau_stop))

    for e in events:
        ts, te = e['ts'], min(e['te'], n)
        if ts >= n: break
        k = e['kind']
        if k == 'hold':                      # hand stops the record, fader open
            stop_to(ts); m = te - st['t']
            if m > 0:
                tr_ = np.convolve(rng.normal(0, 1, m + 400), np.ones(400) / 20, 'valid')[:m] * H.tremor
                put(st['v'] * np.exp(-(np.arange(m) + 1) / H.tau_stop) + tr_); st['v'] = 0.0
            gate(ts, te)
        elif k == 'spin':                    # audible pull-back / backspin
            stop_to(ts)
            m = te - ts; u = (np.arange(m) + 0.5) / m
            w = np.exp(-3.0 * u) * (1 - np.exp(-60 * u)) * np.sqrt(1 - u)
            d = st['v'] * np.exp(-(np.arange(m) + 1) / H.tau_stop)
            put(d + (e['dist'] - d.sum()) * w / w.sum()); st['v'] = 0.0
            gate(ts, te)
        else:                                # play
            rate = e['rate']; cued = False
            if e['p0'] is None:              # let go from wherever it is, fader open
                stop_to(ts)
            else:
                p0 = e['p0']
                cont = (ts == st['t'] and abs(_wrap(p0 - st['p'], L)) < 64
                        and abs(st['v'] - rate) < 0.05)
                if not cont:
                    gap = ts - st['t']
                    pre = min(H.pre, gap // 2)
                    adv = rate * (pre - H.tau * (1 - np.exp(-pre / H.tau)))
                    cued = True
                    d0 = abs(_wrap(p0 - adv - st['p'], L))
                    prel = p0 - adv + rng.normal(0, min(H.cue_base + H.cue_per * d0, 0.03 * SR)) * (d0 > 8)
                    fast = min(0.25 * SR / max(gap, 1), 2.0)
                    st['e'] = H.rel_corr * st['e'] + rng.normal(0, H.rel_jit * 0.8)
                    jit = st['e'] * (1 + 0.5 * fast) - H.rush * fast
                    if rng.random() < H.flub_p:
                        jit += rng.normal(0, H.flub_sd); st['flub'] += 1
                    tr = int(ts - pre + jit)
                    tr = min(max(tr, st['t']), te - 1)
                    dist = _wrap(prel - st['p'], L)
                    need = int(H.reach + abs(dist) / H.vmax) if abs(dist) > 8 else 0
                    if tr - st['t'] < need:            # hand can't get there in time
                        tr = min(st['t'] + need, te - 1); st['late'] += 1
                    dc = min(need if need > 0 else 0, tr - st['t'])
                    if dc < 4:
                        if abs(dist) > 8 and gap == 0: st['jump'] += 1
                        st['p'] += dist; st['v'] = 0.0 if abs(dist) > 8 else st['v']
                    else:
                        w = np.hanning(dc + 2)[1:-1]
                        d = st['v'] * np.exp(-(np.arange(dc) + 1) / H.tau_stop)
                        put(d + (dist - d.sum()) * w / w.sum()); st['v'] = 0.0
                    stop_to(tr)
            m = te - st['t']
            if m > 0:
                kk = np.arange(m)
                chunk = rate - (rate - st['v']) * np.exp(-(kk + 1) / H.tau)
                if cued and H.push > 0:           # pushed or dragged release
                    chunk += rate * rng.normal(0, H.push) * (np.exp(-kk / (2.5 * H.tau)) - np.exp(-kk / (0.6 * H.tau)))
                if cued and m > 1.2 * SR:         # hears it is off the beat and nudges it back
                    err = _wrap(e['p0'] + rate * (te - ts) - (st['p'] + chunk.sum()), L)
                    a0 = ts - st['t'] + int(0.35 * SR); nb = int(0.6 * SR)
                    if a0 >= 0 and a0 + nb < m:
                        chunk[a0:a0 + nb] += H.correct * err * np.hanning(nb + 2)[1:-1] / np.hanning(nb + 2)[1:-1].sum()
                for (td, depth, dur) in e.get('drags', []):   # finger on the platter
                    a = td - st['t']; i0 = max(a, 0); i1 = min(a + dur, m)
                    if i1 > i0:
                        x = (np.arange(i0, i1) - a) / dur
                        chunk[i0:i1] *= 1 - depth * 0.5 * (1 - np.cos(2 * np.pi * x))
                put(chunk)
            for g in e['gates']:
                gate(g[0], g[1], g[2] if len(g) > 2 else 1.0)
    if H.fade > 1:
        w = np.hanning(H.fade + 2)[1:-1]; gain = np.convolve(gain, w / w.sum(), 'same')
    return pos, spd, gain, st


def read(rec, pos):
    L = len(rec); p = np.mod(pos, L); i = p.astype(int); f = p - i
    if rec.ndim == 2: f = f[:, None]
    return rec[i] * (1 - f) + rec[(i + 1) % L] * f


def find_stickers(rec, max_marks=32):
    """Mark the hits: positive jumps in short-time energy."""
    hop = 128
    if rec.ndim == 2: rec = rec.mean(1)
    x = np.diff(rec, prepend=0)                      # tilt towards attacks
    nfr = len(rec) // hop
    e = np.log1p(200 * np.sqrt((x[:nfr*hop].reshape(nfr, hop) ** 2).mean(1)))
    e2 = np.log1p(200 * np.sqrt((rec[:nfr*hop].reshape(nfr, hop) ** 2).mean(1)))
    flux = np.maximum(np.diff(e, prepend=e[-1]), 0) + np.maximum(np.diff(e2, prepend=e2[-1]), 0)
    thr = 0.25 * flux.max(); mind = int(0.07 * SR / hop); marks = []
    for i in np.argsort(-flux):
        if flux[i] < thr: break
        if all(min(abs(i - j), nfr - abs(i - j)) >= mind for j, _ in marks):
            marks.append((i, flux[i]))
        if len(marks) >= max_marks: break
    out = []
    for i, _ in sorted(marks):                       # refine to the sample where it rises
        a = max(i * hop - hop, 0); seg = np.abs(x[a:a + 3 * hop])
        out.append(a + int(np.argmax(seg > 0.35 * seg.max())))
    return np.array(sorted(out))


class Seq:
    """Builds a routine in beats. Record position 'rb' is also in beats."""
    def __init__(s, rec, bpm, hands=0.5, seed=1, stickers=None):
        s.rec = rec; s.L = len(rec); s.bpm = bpm; s.spb = 60.0 / bpm * SR
        s.Lb = s.L / s.spb; s.H = hands if isinstance(hands, Hands) else Hands(hands); s.rng = np.random.default_rng(seed)
        s.stk = find_stickers(rec) if stickers is None else stickers
        s.off = 0.0; s.ev = [[], []]; s.end = [0.0, 0.0]; s.rbend = [None, None]; s.live = 0; s.total = 0.0

    def smp(s, b): return int(round(b * s.spb))

    def snap(s, rb):
        p = (rb * s.spb) % s.L
        d = _wrap(s.stk - p, s.L); i = np.argmin(np.abs(d))
        if abs(d[i]) < 0.040 * SR: p = p + d[i]
        return p - 0.002 * SR                        # cue just ahead of the hit

    def free(s, b, rb=None):
        """Which deck takes the next play: the live one if it can simply carry on,
        otherwise the one whose hand has been free longest."""
        l = s.live; o = 1 - l
        for d in (l, o):
            if (rb is not None and abs(s.end[d] - b) < 1e-9 and s.rbend[d] is not None
                    and abs(_wrap(s.rbend[d] - rb, s.Lb)) < 1e-6):
                return d
        return o if s.end[o] < s.end[l] else l

    def run(s, deck, b0, b1, rb, snap=True, rate=1.0, gates=None, drags=None, p_none=False):
        rate_eff = rate * (1 + (s.H.mismatch if deck == 1 else 0))
        g = [(s.smp(a), s.smp(b)) + tuple(r) for a, b, *r in (gates or [(b0, b1)])]
        dr = [(s.smp(t), dep, int(du * SR)) for t, dep, du in (drags or [])]
        ev = s.ev[deck]
        if (ev and ev[-1]['kind'] == 'play' and abs(s.end[deck] - b0) < 1e-9 and not p_none
                and s.rbend[deck] is not None and abs(_wrap(s.rbend[deck] - rb, s.Lb)) < 1e-6
                and ev[-1]['rate'] == rate_eff):
            G = ev[-1]['gates']
            if G and g and G[-1][1] == g[0][0] and G[-1][2:] == g[0][2:]:   # fader simply stays open
                G[-1] = (G[-1][0],) + g[0][1:]; g = g[1:]
            ev[-1]['te'] = s.smp(b1); ev[-1]['gates'] += g; ev[-1]['drags'] += dr
        else:
            p0 = None if p_none else (s.snap(rb) if snap else (rb * s.spb) % s.L)
            ev.append(dict(kind='play', ts=s.smp(b0), te=s.smp(b1), p0=p0, rate=rate_eff, gates=g, drags=dr))
        s.end[deck] = b1; s.rbend[deck] = None if p_none else rb + (b1 - b0) * rate
        s.total = max(s.total, b1)

    # ---- moves. b0 = start beat, n = length in beats ----
    def straight(s, b0, n, drags=None):
        r = b0 + s.off; d = s.free(b0, r); s.run(d, b0, b0 + n, r, snap=False, drags=drags); s.live = d

    def loop(s, b0, n, unit, rb=None):
        rb = b0 + s.off if rb is None else rb; d = s.free(b0, rb); k = 0; b = b0
        while b < b0 + n - 1e-9:
            dk = (d + k) % 2; s.run(dk, b, min(b + unit, b0 + n), rb); s.live = dk; b += unit; k += 1

    def double(s, b0, n, d=0.5):
        """Echo double: deck Y runs d beats behind deck X, fader flicks every d."""
        r = b0 + s.off; X = s.free(b0, r); Y = 1 - X; m = int(round(n / d))
        s.run(X, b0, b0 + n, r, snap=False, gates=[(b0 + j*d, b0 + (j+1)*d) for j in range(0, m, 2)])
        s.run(Y, b0 + d, b0 + n, r, snap=False, gates=[(b0 + j*d, b0 + (j+1)*d) for j in range(1, m, 2)])
        s.live = X if m % 2 else Y

    def chase(s, b0, n, unit=0.5, shuffle=0.0, rb=None):
        """Strobe: decks alternate, each hit stated twice, record moves at half speed."""
        rb = b0 + s.off if rb is None else rb; d = s.free(b0, rb); b = b0; k = 0
        while b < b0 + n - 1e-9:
            dur = unit * (1 + shuffle) if k % 2 == 0 else unit * (1 - shuffle)
            dk = (d + k) % 2; s.run(dk, b, min(b + dur, b0 + n), rb + (k // 2) * unit)
            s.live = dk; b += dur; k += 1

    def triple(s, b0, n=1.0, rb=None):
        rb = b0 + s.off if rb is None else rb; d = s.free(b0, rb); u = n / 3
        for k in range(3):
            dk = (d + k) % 2; s.run(dk, b0 + k*u, b0 + (k+1)*u if k < 2 else b0 + n, rb); s.live = dk

    def pause(s, b0, n, hold=0.5):
        """Hand stops the live record on a hit, lets it go late; other deck rescues on the bar."""
        d = s.live
        a = b0 - 0.03
        if s.ev[d] and s.ev[d][-1]['kind'] == 'play' and abs(s.end[d] - b0) < 1e-9:
            e = s.ev[d][-1]; e['te'] = s.smp(a); e['gates'][-1] = (e['gates'][-1][0], s.smp(a))
        s.ev[d].append(dict(kind='hold', ts=s.smp(a), te=s.smp(b0 + hold)))
        s.end[d] = b0 + hold
        s.run(d, b0 + hold, b0 + n, None, p_none=True)

    def fill(s, b0, pattern, K=0.0, S=1.0, step=0.25):
        """Breakdown: K = kick sticker on one deck, S = snare sticker on the other."""
        hits = [(i, c) for i, c in enumerate(pattern) if c != '.']
        f = s.free(b0); X = f if hits[0][1] in 'Kk' else 1 - f; Y = 1 - X
        for j, (i, c) in enumerate(hits):
            deck = X if c in 'Kk' else Y; rb = K if c in 'Kk' else S
            nxt = hits[j + 1][0] if j + 1 < len(hits) else len(pattern)
            same = [h for h in hits[j + 1:] if (h[1] in 'Kk') == (c in 'Kk')]
            dur = min(nxt - i, 2) * step
            if same and same[0][0] - i <= 2: dur = min(dur, (same[0][0] - i) * step * 0.45)
            s.run(deck, b0 + i * step, b0 + i * step + dur, rb); s.live = deck
        s.total = max(s.total, b0 + len(pattern) * step)

    def backspin(s, b0, n=1.0, back=4.0):
        d = s.live; a = b0
        if s.ev[d] and s.ev[d][-1]['kind'] == 'play' and s.end[d] > a + 1e-9:
            e = s.ev[d][-1]; e['te'] = s.smp(a); e['gates'] = [g for g in e['gates'] if g[0] < s.smp(a)]
            e['gates'][-1] = (e['gates'][-1][0], s.smp(a))
        s.ev[d].append(dict(kind='spin', ts=s.smp(a), te=s.smp(b0 + n * 0.85), dist=-back * s.spb))
        s.end[d] = b0 + n; s.rbend[d] = None; s.total = max(s.total, b0 + n)

    def phase(s, b0, n, start_ms=-9.0, rate=1.0018):
        r = b0 + s.off; X = s.free(b0, r); Y = 1 - X
        s.run(X, b0, b0 + n, r, snap=False, gates=[(b0, b0 + n, 0.72)])
        s.run(Y, b0, b0 + n, r + start_ms / 1000 * s.bpm / 60, snap=False, rate=rate, gates=[(b0, b0 + n, 0.72)])
        s.live = X; s.end[Y] -= 1e-6

    def render(s, tail=0.3):
        n = s.smp(s.total) + int(tail * SR)
        recB = s.rec
        if s.H.wear > 0:                              # second copy: a little duller and quieter
            a = np.exp(-2 * np.pi * (20000 - 9000 * s.H.wear) / SR); y = np.zeros_like(recB); z = 0.0
            from scipy.signal import lfilter
            y = lfilter([1 - a], [1, -a], recB, axis=0); recB = y * (1 - 0.06 * s.H.wear)
        out = np.zeros((n,) + s.rec.shape[1:]); info = []
        for d, rec in ((0, s.rec), (1, recB)):
            pos, spd, gain, st = render_deck(s.ev[d], n, s.L, s.H, s.rng)
            out += read(rec, pos) * (gain if rec.ndim == 1 else gain[:, None]); info.append(st)
        k = 0.85; a = np.abs(out)                    # mixer output: clean below the knee, soft above
        out = np.where(a > k, np.sign(out) * (k + (1 - k) * np.tanh((a - k) / (1 - k))), out)
        return out, info


def write_wav(path, x, gain=0.89):
    x = np.clip(x * gain, -1, 1)
    with wave.open(path, 'wb') as w:
        w.setnchannels(1 if x.ndim == 1 else x.shape[1]); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((x * 32767).astype('<i2').tobytes())
