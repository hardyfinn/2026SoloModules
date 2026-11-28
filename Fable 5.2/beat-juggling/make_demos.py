"""Test break + one demo per move. Run: python3 make_demos.py"""
import numpy as np, sys
from scipy.signal import lfilter, butter
from doubles_engine import *

BPM = 96.0; SPB = 60 / BPM * SR; rng = np.random.default_rng(7)

def env(n, a, d): t = np.arange(n) / SR; return (1 - np.exp(-t / a)) * np.exp(-t / d)
def kick(v=1.0):
    n = int(.35 * SR); t = np.arange(n) / SR
    f = 48 + 110 * np.exp(-t / .028); ph = 2 * np.pi * np.cumsum(f) / SR
    return v * (np.sin(ph) * np.exp(-t / .13) + .25 * rng.normal(0, 1, n) * np.exp(-t / .004))
def snare(v=1.0):
    n = int(.28 * SR); t = np.arange(n) / SR
    b, a = butter(2, [900 / (SR/2), 7000 / (SR/2)], 'band')
    nz = lfilter(b, a, rng.normal(0, 1, n)) * np.exp(-t / .075)
    body = np.sin(2*np.pi*185*t) * np.exp(-t / .05) + .6*np.sin(2*np.pi*330*t) * np.exp(-t / .03)
    return v * (1.6 * nz + .7 * body)
def hat(v=1.0, d=.03):
    n = int(max(d * 6, .05) * SR); t = np.arange(n) / SR
    b, a = butter(2, 6500 / (SR/2), 'high'); return v * .5 * lfilter(b, a, rng.normal(0, 1, n)) * np.exp(-t / d)
def bass(f, dur, v=1.0):
    n = int(dur * SR); t = np.arange(n) / SR
    x = np.sin(2*np.pi*f*t + 1.5*np.sin(2*np.pi*f*t) * np.exp(-t / .15))
    return v * .7 * x * env(n, .004, dur * .6) * np.minimum(1, (n - np.arange(n)) / (0.01 * SR))
def stab(fs, v=1.0):
    n = int(.5 * SR); t = np.arange(n) / SR; x = 0
    for f in fs: x = x + np.sin(2*np.pi*f*t + 1.2*np.sin(2*np.pi*f*2*t) * np.exp(-t / .08))
    return v * .16 * x * env(n, .003, .16)

def make_break():
    L = int(round(8 * SPB)); x = np.zeros(L + SR)
    def at(step, snd, swing=.58):
        b = step // 4; s16 = step % 4
        off = {0: 0, 1: swing / 2, 2: .5, 3: .5 + swing / 2}[s16]
        i = int((b + off) * SPB); x[i:i + len(snd)] += snd
    for st in (0, 7, 10, 16, 19, 23, 26): at(st, kick())
    for st in (4, 12, 20, 28): at(st, snare())
    for st, v in ((9, .25), (15, .3), (25, .25), (31, .45), (30, .3)): at(st, snare(v))
    for st in range(0, 32, 2): at(st, hat(.8 if st % 4 == 0 else .55))
    for st in (3, 11, 21): at(st, hat(.3))
    at(14, hat(.8, .12)); at(22, hat(.7, .12))
    A1, C2, D2, E2 = 55.0, 65.4, 73.4, 82.4
    for st, f, d in ((0, A1, .28), (7, A1, .15), (10, C2, .3), (14, D2, .14), (16, A1, .28), (19, A1, .12), (23, E2, .15), (26, D2, .3), (30, C2, .14)):
        at(st, bass(f, d))
    at(6, stab((220, 261.6, 329.6, 392))); at(22, stab((196, 246.9, 293.7, 370)), ); at(29, stab((220, 261.6, 329.6, 392), .7))
    x[:SR] += x[L:L + SR]; x = x[:L]                       # wrap tails so it loops
    ir = rng.normal(0, 1, int(.22 * SR)) * np.exp(-np.arange(int(.22 * SR)) / (.05 * SR))
    wet = np.real(np.fft.ifft(np.fft.fft(x) * np.fft.fft(ir, L)))
    x = x + .035 * wet; x = np.tanh(1.6 * x / np.abs(x).max()); return x / np.abs(x).max()

rec = make_break()
stk = find_stickers(rec)
print('stickers at 16ths:', np.round(stk / SPB * 4, 2))
out = 'demos/'
write_wav(out + '00-the-record.wav', np.tile(rec, 2))

def S(h=0.55, seed=3): return Seq(rec, BPM, hands=h, seed=seed, stickers=stk)
def done(name, s):
    x, info = s.render(); write_wav(out + name + '.wav', x)
    print(name, '%.1fs' % (len(x) / SR), 'late', [i['late'] for i in info], 'jump', [i['jump'] for i in info])
    return x

s = S(); s.straight(0, 8); s.loop(8, 4, 2, rb=0); s.loop(12, 4, 1, rb=0); s.loop(16, 4, 4, rb=4); s.loop(20, 2, .5, rb=4); s.loop(22, 2, .25, rb=4); s.straight(24, 4); done('01-loop', s)
s = S(); s.straight(0, 8); s.double(8, 8, .5); s.straight(16, 4); s.double(20, 4, .25); s.double(24, 4, 1.0); s.straight(28, 4); done('02-double', s)
s = S(); s.straight(0, 8)
for bar in (8, 12, 16, 20):
    s.straight(bar, 1); s.triple(bar + 1, 1); s.straight(bar + 2, 1); s.triple(bar + 3, 1)
s.straight(24, 4); done('03-triple', s)
s = S(); s.straight(0, 8); s.chase(8, 8, 1.0, rb=0); s.chase(16, 8, .5, rb=0); s.straight(24, 4); done('04-chase', s)
s = S(); s.straight(0, 8); s.chase(8, 8, .5, shuffle=.5, rb=0); s.chase(16, 4, .5, shuffle=1/3, rb=4); s.chase(20, 4, .25, shuffle=.0, rb=6); s.straight(24, 4); done('05-shuffle-strobe', s)
s = S(); s.straight(0, 8)
for bar, hb, hd in ((8, 2, .5), (12, 1, .25), (16, 2, .75), (20, 3, .5)):
    s.straight(bar, hb); s.pause(bar + hb, 4 - hb, hd)
s.straight(24, 4); done('06-pause', s)
s = S(); s.straight(0, 8)
for bar, pat in ((8, 'K.KS.K.SK.KS.SSS'), (12, 'KK.S.KKS.K.SKSKS'), (16, 'K..SK.S.KKSS.KSS'), (20, 'KSKKS.KSKSS.SSSS')):
    s.fill(bar, pat)
s.straight(24, 4); done('07-fill', s)
s = S(); s.straight(0, 8)
for bar in (8, 12, 16):
    s.straight(bar, 3); s.backspin(bar + 3, 1, back=4 if bar != 16 else 8)
s.straight(20, 4); done('08-backspin', s)
s = S(); s.straight(0, 4); s.phase(4, 16); s.straight(20, 4); done('09-phase', s)
s = S(); s.straight(0, 4)
X = 0
s.ev[0][-1]['gates'][-1] = (0, s.smp(4)); s.run(0, 4, 20, 4, snap=False, gates=[(4, 20, .72)], drags=[(8, .35, .30), (12, -.35, .30), (14, -.3, .25), (17, .3, .25)])
s.run(1, 4, 20, 4, snap=False, gates=[(4, 20, .72)]); s.end[1] -= 1e-6; s.live = 0
s.straight(20, 4); done('10-drag', s)

def routine(s):
    s.straight(0, 8)
    s.loop(8, 4, 2, rb=0)
    s.straight(12, 2); s.double(14, 2, .5)
    s.double(16, 4, .5); s.chase(20, 4, 1.0, rb=4)
    s.loop(24, 4, 1, rb=0)
    s.straight(28, 2); s.triple(30, 1, rb=5); s.fill(31, 'KSSS')
    s.chase(32, 8, .5, rb=0)
    s.chase(40, 4, .5, shuffle=.5, rb=4)
    s.fill(44, 'K.KS.K.SK.KS.SSS')
    s.straight(48, 2); s.pause(50, 2, .5)
    s.double(52, 2, .25); s.loop(54, 2, .5, rb=7)
    s.fill(56, 'KK.S.KKS.K.SKSKS')
    s.straight(60, 3); s.backspin(63, 1)
    s.straight(64, 8)
x = None
for h, nm in ((0.0, 'machine'), (0.55, 'dj'), (1.0, 'human')):
    s = S(h); routine(s); done('11-routine-hands-' + nm, s)

def instinct(s, bars, density=0.6, funk=0.5, seed=5, b_start=0):
    """Weighted choice per bar by place in a 4-bar phrase."""
    r = np.random.default_rng(seed); fills = ['K.KS.K.SK.KS.SSS', 'KK.S.KKS.K.SKSKS', 'K..SK.S.KKSS.KSS', 'KSKKS.KSKSS.SSSS', 'K.K.S..KK.S.KSSS']
    last_fill = None; log = []
    W = [dict(straight=6, loop=2.5, double=1.5), dict(straight=4, double=3, loop=1.5, chase=1.5),
         dict(straight=2.5, double=2.5, chase=2.5, loop=1.5, pause=1, shuffle=1.5 * funk * 2),
         dict(straight=1, double=2, chase=2, shuffle=2 * funk * 2, loop=1)]
    E = dict(fill=4, triple=2.5, backspin=2, chase8=1.5, straight=1)
    def pick(w):
        k = list(w); p = np.array([w[i] * (1 if i == 'straight' else 2 * density) for i in k]); return k[r.choice(len(k), p=p / p.sum())]
    def do(m, b, n):
        rb = b % 8
        if m == 'straight': s.straight(b, n)
        elif m == 'loop': s.loop(b, n, r.choice([.5, 1, 1, 2]) if n >= 2 else .5, rb=rb)
        elif m == 'double': s.double(b, n, r.choice([.25, .5, .5, .5]))
        elif m == 'chase': s.chase(b, n, r.choice([.5, 1.0]), rb=rb)
        elif m == 'shuffle': s.chase(b, n, .5, shuffle=r.choice([1/3, .5]), rb=rb)
        elif m == 'pause': s.straight(b, 2); s.pause(b + 2, n - 2, r.choice([.25, .5]))
    for bar in range(bars):
        b = b_start + bar * 4; ph = bar % 4
        if ph < 3:
            m = pick(W[ph]); do(m, b, 4); log.append(m)
        else:
            m = pick(W[3]); do(m if m != 'pause' else 'straight', b, 2); e = pick(E)
            if e == 'fill':
                c = [f for f in fills if f != last_fill]; f = c[r.integers(len(c))]; last_fill = f; s.fill(b + 2, f[8:])
            elif e == 'triple': s.triple(b + 2, 1, rb=(b + 3) % 8); s.triple(b + 3, 1, rb=(b + 3) % 8)
            elif e == 'backspin': s.straight(b + 2, 1); s.backspin(b + 3, 1)
            elif e == 'chase8': s.chase(b + 2, 2, .25, rb=(b + 3) % 8)
            else: s.straight(b + 2, 2)
            log.append(m + '+' + e)
    return log
s = S(); s.straight(0, 8); lg = instinct(s, 16, b_start=8); b = s.total
s.straight(b if b % 4 == 0 else np.ceil(b / 4) * 4, 4); done('12-instinct', s); print(lg)
