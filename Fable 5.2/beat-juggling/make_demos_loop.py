"""Every Doubles example on a real loop.
Run: python3 make_demos_loop.py source/baby-cry-8bar.wav 8 demos-baby-cry/ [hands]"""
import numpy as np, sys, wave, os
from doubles_engine import *

path, bars, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
HANDS = float(sys.argv[4]) if len(sys.argv) > 4 else 0.75
os.makedirs(out, exist_ok=True)
w = wave.open(path); rec = np.frombuffer(w.readframes(w.getnframes()), '<i2').reshape(-1, w.getnchannels()) / 32768
assert w.getframerate() == SR
BPM = bars * 4 * 60 / (len(rec) / SR); NB = bars * 4
stk = find_stickers(rec, max_marks=16 * bars)
print('%.2f BPM, %d beats, %d stickers' % (BPM, NB, len(stk)))
K, Sn = 0.0, 1.0            # kick on the one, snare on the two (checked by band energy)

def S(h=HANDS, seed=3): return Seq(rec, BPM, hands=h, seed=seed, stickers=stk)
def intro(s, n=8):
    """Two bars straight, then the second copy is dropped back at the top for the move."""
    s.straight(0, n); s.off = -n
def done(name, s):
    x, info = s.render(); write_wav(out + name + '.wav', x)
    print(name, '%.1fs' % (len(x) / SR), 'late', [i['late'] for i in info], 'fluffed', [i['flub'] for i in info])

s = S(); intro(s); s.loop(8, 4, 2, rb=0); s.loop(12, 4, 1, rb=0); s.loop(16, 4, 4, rb=4); s.loop(20, 2, .5, rb=4); s.loop(22, 2, .25, rb=4); s.off = -16; s.straight(24, 8); done('01-loop', s)
s = S(); intro(s); s.double(8, 8, .5); s.straight(16, 4); s.double(20, 4, .25); s.double(24, 4, 1.0); s.straight(28, 4); done('02-double', s)
s = S(); intro(s)
for bar in (8, 12, 16, 20):
    s.straight(bar, 1); s.triple(bar + 1, 1); s.straight(bar + 2, 1); s.triple(bar + 3, 1)
s.straight(24, 4); done('03-triple', s)
s = S(); intro(s); s.chase(8, 8, 1.0, rb=0); s.chase(16, 8, .5, rb=0); s.off = -24; s.straight(24, 8); done('04-chase', s)
s = S(); intro(s); s.chase(8, 8, .5, shuffle=.5, rb=0); s.chase(16, 4, .5, shuffle=1/3, rb=4); s.chase(20, 4, .25, rb=6); s.off = -24; s.straight(24, 8); done('05-shuffle-strobe', s)
s = S(); intro(s)
for bar, hb, hd in ((8, 2, .5), (12, 1, .25), (16, 2, .75), (20, 3, .5)):
    s.straight(bar, hb); s.pause(bar + hb, 4 - hb, hd)
s.straight(24, 4); done('06-pause', s)
s = S(); intro(s)
for bar, pat in ((8, 'K.KS.K.SK.KS.SSS'), (12, 'KK.S.KKS.K.SKSKS'), (16, 'K..SK.S.KKSS.KSS'), (20, 'KSKKS.KSKSS.SSSS')):
    s.fill(bar, pat, K=K, S=Sn)
s.off = -24; s.straight(24, 8); done('07-fill', s)
s = S(); intro(s)
for bar in (8, 12, 16):
    s.straight(bar, 3); s.backspin(bar + 3, 1, back=4 if bar != 16 else 8)
s.straight(20, 4); done('08-backspin', s)
s = S(); intro(s, 4); s.phase(4, 16); s.straight(20, 4); done('09-phase', s)
s = S(); s.straight(0, 4); s.ev[0][-1]['gates'][-1] = (0, s.smp(4))
s.run(0, 4, 20, 4, snap=False, gates=[(4, 20, .72)], drags=[(8, .35, .30), (12, -.35, .30), (14, -.3, .25), (17, .3, .25)])
s.run(1, 4, 20, 4, snap=False, gates=[(4, 20, .72)]); s.end[1] -= 1e-6; s.live = 0
s.straight(20, 4); done('10-drag', s)

def routine(s):
    intro(s)
    s.loop(8, 4, 2, rb=0)
    s.off = -12; s.straight(12, 2); s.double(14, 2, .5)
    s.double(16, 4, .5); s.chase(20, 4, 1.0, rb=4)
    s.loop(24, 4, 1, rb=0)
    s.off = -24; s.straight(28, 2); s.triple(30, 1, rb=5); s.fill(31, 'KSSS', K=K, S=Sn)
    s.chase(32, 8, .5, rb=0)
    s.chase(40, 4, .5, shuffle=.5, rb=4)
    s.fill(44, 'K.KS.K.SK.KS.SSS', K=K, S=Sn)
    s.off = -48; s.straight(48, 2); s.pause(50, 2, .5)
    s.double(52, 2, .25); s.loop(54, 2, .5, rb=7)
    s.fill(56, 'KK.S.KKS.K.SKSKS', K=K, S=Sn)
    s.off = -60; s.straight(60, 3); s.backspin(63, 1, back=3)
    s.off = -64; s.straight(64, 8)
for h, nm in ((0.0, '1-machine'), (0.5, '2-tight-dj'), (0.75, '3-loose-dj'), (1.0, '4-bedroom')):
    s = S(h); routine(s); done('11-routine-hands-' + nm, s)
# the four parts of HANDS on their own, everything else machine-tight
for nm, kw in (('timing', dict(timing=1.0)), ('weight', dict(weight=1.0)), ('slop', dict(slop=1.0)), ('risk', dict(timing=.4, risk=3.0))):
    base = dict(timing=0.0, weight=0.0, slop=0.0, risk=0.0); base.update(kw)
    H = Hands(0.0, **base); H.vmax = 12.0; H.reach = SR * 0.02
    s = S(H); routine(s); done('13-routine-only-' + nm, s)

def instinct(s, bars, density=0.6, funk=0.5, seed=5, b_start=0):
    """Weighted choice per bar by place in a 4-bar phrase. Like a DJ, it tends to say a
    thing twice before changing it, and it gets busier as the routine goes on."""
    r = np.random.default_rng(seed); fills = ['K.KS.K.SK.KS.SSS', 'KK.S.KKS.K.SKSKS', 'K..SK.S.KKSS.KSS', 'KSKKS.KSKSS.SSSS', 'K.K.S..KK.S.KSSS']
    last_fill = None; log = []; prev = None
    W = [dict(straight=6, loop=2.5, double=1.5), dict(straight=4, double=3, loop=1.5, chase=1.5),
         dict(straight=2.5, double=2.5, chase=2.5, loop=1.5, pause=1, shuffle=3 * funk),
         dict(straight=1, double=2, chase=2, shuffle=4 * funk, loop=1)]
    E = dict(fill=4, triple=2.5, backspin=2, chase8=1.5, straight=1)
    def pick(w, dens):
        k = list(w); p = np.array([w[i] * (1 if i == 'straight' else 2 * dens) for i in k]); return k[r.choice(len(k), p=p / p.sum())]
    def do(m, b, n):
        rb = (b + s.off) % NB
        if m == 'straight': s.straight(b, n)
        elif m == 'loop': s.loop(b, n, r.choice([.5, 1, 1, 2]) if n >= 2 else .5, rb=rb)
        elif m == 'double': s.double(b, n, r.choice([.25, .5, .5, .5]))
        elif m == 'chase': s.chase(b, n, r.choice([.5, 1.0]), rb=rb)
        elif m == 'shuffle': s.chase(b, n, .5, shuffle=r.choice([1/3, .5]), rb=rb)
        elif m == 'pause': s.straight(b, 2); s.pause(b + 2, n - 2, r.choice([.25, .5]))
    for bar in range(bars):
        b = b_start + bar * 4; ph = bar % 4; dens = density * (0.6 + 0.8 * bar / max(bars - 1, 1))
        if ph < 3:
            m = prev if (ph in (1, 2) and prev not in (None, 'straight', 'pause') and r.random() < 0.45) else pick(W[ph], dens)
            do(m, b, 4); log.append(m); prev = m
        else:
            m = pick(W[3], dens); do(m if m != 'pause' else 'straight', b, 2); e = pick(E, dens); rb = (b + 3 + s.off) % NB
            if e == 'fill':
                c = [f for f in fills if f != last_fill]; f = c[r.integers(len(c))]; last_fill = f; s.fill(b + 2, f[8:], K=K, S=Sn)
            elif e == 'triple': s.triple(b + 2, 1, rb=rb); s.triple(b + 3, 1, rb=rb)
            elif e == 'backspin': s.straight(b + 2, 1); s.backspin(b + 3, 1)
            elif e == 'chase8': s.chase(b + 2, 2, .25, rb=rb)
            else: s.straight(b + 2, 2)
            log.append(m + '+' + e); prev = None
    return log
s = S(); intro(s); lg = instinct(s, 16, b_start=8); s.straight(s.total, 4); done('12-instinct', s); print(lg)
