import numpy as np, soundfile as sf
from sketch_engine import Insect, env, SR
OUT = 'sketches/'
made = []
def done(I, name, **kw):
    y = I.finish(OUT+name+'.wav', **kw); made.append((name, y)); print(name, len(y)/SR)

def tymbal_run(I, t0, t1, rate, f, amp=1.0, loose=0.04, **kw):
    """alternating left/right tymbal cycles from t0 to t1. rate, f, amp may be callables of time."""
    c = lambda v, t: v(t) if callable(v) else v
    t = t0; k = 0
    while t < t1:
        I.tymbal(t, c(f, t), amp=c(amp, t)*(1+0.08*I.rng.standard_normal()), side=k % 2, loose=loose, **kw)
        t += (1/c(rate, t))*(1+loose*I.rng.standard_normal()); k += 1
    return t

# 01 field cricket, fitted to ESC-50 clip 5-216216: 4541 Hz, 5 pulses per chirp, 39 ms period, 20 ms pulses
I = Insect(12, 1); t = 0.3
while t < 11.5:
    for k in range(5 if I.rng.random() > 0.15 else 4):
        I.file(t, 0.0235*(1+0.08*I.rng.standard_normal()), 4541*(1+0.004*I.rng.standard_normal()), amp=[0.45, 0.8, 1, 1, 0.95][k], q=22, glide=-0.08, shape=0.7)
        t += 0.0393*(1+0.04*I.rng.standard_normal())
    t += 0.175*(1+0.10*I.rng.standard_normal())
done(I, '01-field-cricket-chirp')

# 02 tree cricket trill, fitted to clip 2-83270 (3116 Hz, 64.6 pulses/s, 7.6 ms pulses), then TEMP swept cool to warm
I = Insect(14, 2); t = 0.2
while t < 13.8:
    T = np.interp(t, [0, 4, 12, 14], [0, 0, 1, 1])      # 0 = cool night, 1 = warm night
    rate = 42+38*T; f = 2700+750*T
    I.file(t, 0.52/rate, f, q=22, glide=-0.065, shape=0.7)
    t += (1/rate)*(1+0.02*I.rng.standard_normal())
    if I.rng.random() < 0.004: t += 0.25
done(I, '02-tree-cricket-trill-cool-to-warm')

# 03 slow ground cricket, fitted to clip 5-216213: 2343 Hz, 24 pulses/s, 21 ms pulses, 21-pulse chirps
I = Insect(12, 3); t = 0.3
while t < 11:
    for k in range(21):
        I.file(t, 0.0245, 2343, amp=min(1, 0.4+0.15*k), q=20, glide=-0.10, shape=0.7); t += 0.0413*(1+0.035*I.rng.standard_normal())
    t += 0.6*(1+0.15*I.rng.standard_normal())
done(I, '03-ground-cricket-slow')

# 04 true katydid "ka-ty-did": harsh, broadband, 2 or 3 rasps per phrase
I = Insect(12, 4); t = 0.4
while t < 11.3:
    for k in range(3 if I.rng.random() < 0.6 else 2):
        d = 0.075*(1+0.1*I.rng.standard_normal())
        I.file(t, d, 3400, amp=1, lock=0, q=5.0, glide=0, tooth_rate=750, shape=0.5, pan=0.45)
        I.file(t, d, 5600, amp=0.7, lock=0, q=5, glide=0, tooth_rate=750, shape=0.5, pan=0.55)
        t += 0.185*(1+0.05*I.rng.standard_normal())
    t += 0.62*(1+0.12*I.rng.standard_normal())
done(I, '04-katydid-ka-ty-did')

# 05 meadow katydid: ticks speeding up, then a buzz
I = Insect(14, 5); t = 0.3
for rep in range(2):
    t0 = t
    while t-t0 < 2.6:
        x = (t-t0)/2.6
        for f, a in ((11500, 1), (15500, 0.6)): I.file(t, 0.012, f, amp=a*(0.5+0.5*x), lock=0, q=5, glide=0, tooth_rate=2500, shape=0.5)
        t += 1/(3.5+6*x)*(1+0.06*I.rng.standard_normal())
    t0 = t
    while t-t0 < 3.2:
        for f, a in ((11500, 1), (15500, 0.6)): I.file(t, 0.009, f, amp=a, lock=0, q=5, glide=0, tooth_rate=2500, shape=0.5)
        t += (1/62)*(1+0.03*I.rng.standard_normal())
    t += 0.9
done(I, '05-meadow-katydid-ticks-and-buzz', distance=0.08)

# 06 grasshopper: leg strokes against the wing, noisy phrases that swell
I = Insect(13, 6); t = 0.4
for rep in range(3):
    n = 30
    for k in range(n):
        a = 0.25+0.75*(k/n)**1.5
        I.file(t, 0.045, 6200, amp=a, lock=0, q=3.5, glide=0, tooth_rate=1500, shape=0.7, loose=0.2)
        I.file(t, 0.045, 9500, amp=a*0.6, lock=0, q=4, glide=0, tooth_rate=1500, shape=0.7, loose=0.2)
        t += 0.071*(1+0.05*I.rng.standard_normal())
    t += 1.8
done(I, '06-grasshopper-phrases')

# 07 dog-day cicada: 220 tymbal pulses/s, slow pulsation, swell and die-off
I = Insect(15, 7)
amp = lambda t: np.interp(t, [0.2, 5, 11, 14.5], [0.03, 1, 1, 0.0])*(1-0.45*np.interp(t, [0, 3, 6], [0, .3, 1])*(0.5+0.5*np.sin(2*np.pi*6.3*t)))
f = lambda t: 6500*np.interp(t, [0, 11, 14.5], [1, 1, 0.93])
rate = lambda t: 220*np.interp(t, [0, 11, 14.5], [1, 1, 0.75])
tymbal_run(I, 0.2, 14.5, rate, f, amp, ribs=3, q=8, out=0.35, out_at=0.0022)
n = I.n; I.abdomen(5200, 6800, 0.5+0.5*np.sin(2*np.pi*6.3*np.arange(n)/SR), q=3, mix=0.6)
done(I, '07-dog-day-cicada-swell')

# 08 periodical cicada "pharaoh": low pure tone, downward slur at the end
I = Insect(15, 8); t = 0.3
for rep in range(4):
    L = 2.4+0.4*I.rng.random(); t0 = t
    f = lambda x: 1340*np.interp(x-t0, [0, L, L+0.55], [1, 1, 0.74])
    amp = lambda x: np.interp(x-t0, [0, 0.4, L, L+0.55], [0.1, 1, 1, 0.15])
    tymbal_run(I, t0, t0+L+0.55, 132, f, amp, ribs=7, q=14, drop=0.0, out=0.15, out_at=0.004, loose=0.02)
    t = t0+L+0.55+0.75
done(I, '08-periodical-cicada-pharaoh')

# 09 periodical cicada, cassini type: ticks, then a shrill buzz that rises and falls
I = Insect(14, 9); t = 0.3
for rep in range(3):
    t0 = t
    while t-t0 < 1.9:
        I.tymbal(t, 5600, amp=0.6, ribs=3, q=6, side=int(I.rng.integers(2))); t += (1/17)*(1+0.07*I.rng.standard_normal())
    t0 = t
    f = lambda x: 5600*np.interp(x-t0, [0, 0.8, 2.0], [0.95, 1.05, 0.86])
    amp = lambda x: np.interp(x-t0, [0, 0.7, 1.4, 2.0], [0.4, 1, 0.9, 0.1])
    t = tymbal_run(I, t0, t0+2.0, 190, f, amp, ribs=3, q=6, out_at=0.0025)+0.5
done(I, '09-periodical-cicada-ticks-and-buzz')

# 10 evening cicada (higurashi style): "kana-kana-kana", speeding up, sinking and fading
I = Insect(13, 10); wah = np.zeros(I.n); t = 0.4
for rep in range(2):
    t0 = t; N = 30
    for k in range(N):
        x = k/N; d = 0.085-0.03*x
        a = np.interp(x, [0, 0.15, 0.6, 1], [0.5, 1, 0.8, 0.15]); fc = 4100*(1-0.10*x**1.5)
        tymbal_run(I, t, t+d, 160, lambda u: fc*(1-0.05*(u-t)/d), lambda u: a*np.interp((u-t)/d, [0, 0.15, 1], [0.6, 1, 0.35]), ribs=4, q=10, out_at=0.003)
        i0, i1 = int(t*SR), int((t+d*1.6)*SR); wah[i0:i1] = np.linspace(1, 0, i1-i0)
        t += (1/(4.6+3.8*x))*(1+0.03*I.rng.standard_normal())
    t += 1.6
I.abdomen(2900, 4300, wah, q=4, mix=0.7)
done(I, '10-evening-cicada-kana-kana', distance=0.3)

# 11 min-min cicada: the abdomen lifts and drops, so the body says "miiin-min-min-min-miii"
I = Insect(13, 11); pts = [(0, 0.2)]; t = 0.3; t0 = t
segs = [(1.4, 1)]+[(0.42, 1), ]*9+[(2.2, 1)]
aps = [(t, 0.1)]
for i, (L, _) in enumerate(segs):
    last = i == len(segs)-1
    pts += [(t+0.03, 0.15), (t+0.12, 1.0), (t+L-0.16, 1.0), (t+L-0.02, 0.0 if not last else 0.3)]
    aps += [(t+0.1, 1.0), (t+L-0.15, 1.0), (t+L, 0.25 if not last else 0.0)]
    t += L
wah = env(I.n, pts); A = lambda u: np.interp(u, [p[0] for p in aps], [p[1] for p in aps])
tymbal_run(I, t0, t, 175, lambda u: 4300*(1-0.04*(u > t-2.2)*(u-(t-2.2))/2.2), A, ribs=3, q=5, out_at=0.0027, loose=0.06)
I.abdomen(2500, 5200, wah, q=3.5, mix=0.85)
done(I, '11-min-min-cicada-talking-abdomen', distance=0.2)

# 12 Mediterranean cicada: dry rasping bursts, four or five a second, for as long as the sun is out
I = Insect(12, 12); t = 0.3
while t < 11.5:
    a = np.interp(t, [0, 2, 10, 11.7], [0.3, 1, 1, 0.3])
    tymbal_run(I, t, t+0.075, 210, 4900, lambda u: a*np.interp(u-t, [0, 0.02, 0.075], [0.5, 1, 0.5]), ribs=2, q=4.5, out=0.5, out_at=0.002, loose=0.14, drop=0.1)
    t += 0.215*(1+0.04*I.rng.standard_normal())
done(I, '12-mediterranean-cicada-rasp')

# 13 wing mechanism: a mosquito comes past, then a bumblebee (one impulse per wingbeat, soft body resonance)
I = Insect(12, 13)
def flyby(t0, t1, f0, body, q, rough, seed):
    r = np.random.default_rng(seed); t = t0; ph = r.random(3)*6.28
    while t < t1:
        x = (t-t0)/(t1-t0); near = np.exp(-((x-0.5)/0.22)**2)
        wob = 1+0.03*np.sin(2*np.pi*1.3*t+ph[0])+0.02*np.sin(2*np.pi*3.1*t+ph[1])+0.06*(0.5-x)*near  # hunting + doppler
        f = f0*wob; pan = np.clip(0.5+0.9*(x-0.5), 0, 1)
        I.grain(t, (0.05+near)*(1+rough*r.standard_normal()), body*wob, q, pan)
        I.grain(t+0.45/f, 0.5*(0.05+near)*(1+rough*r.standard_normal()), body*1.9*wob, q, pan)
        t += (1/f)*(1+0.3*rough*r.standard_normal())
flyby(0.2, 6.0, 520, 1040, 3.0, 0.03, 1); flyby(6.2, 11.8, 172, 520, 1.6, 0.12, 2)
done(I, '13-wing-mosquito-then-bumblebee', distance=0.05)

# ---- stretched past nature -----------------------------------------------------------------------------
BPM = 124; s16 = 60/BPM/4
def kick(y, t):
    n = int(0.25*SR); tt = np.arange(n)/SR; i = int(t*SR)
    k = np.sin(2*np.pi*(48*tt+ (160/30)*(1-np.exp(-30*tt))))*np.exp(-tt/0.09)*0.9
    if i+n < len(y): y[i:i+n] += k[:, None]

# 14 crickets as hi-hats: chirps triggered on a 124 BPM grid, pulses per chirp and pitch set per step
I = Insect(16*s16*4+0.5, 14)
pat = [1, 0, 2, 1, 1, 0, 3, 0, 1, 1, 2, 0, 1, 0, 4, 2]; acc = [1, .5, .8, .5, .9, .5, 1, .5, .9, .6, .8, .5, 1, .5, 1, .7]
for bar in range(4):
    for i in range(16):
        t = 0.05+(bar*16+i)*s16+(0.012 if i % 2 else 0)
        for k in range(pat[i]):
            I.file(t+k*s16/ (pat[i] if pat[i] > 2 else 4), 0.011 if i % 4 != 2 else 0.03, 7200 if i % 4 != 2 else 5200, amp=acc[i], q=14, glide=-0.05, lock=1 if bar < 2 else 0.55, pan=0.35+0.3*(i % 3)/2)
y = I.finish(OUT+'_tmp.wav', distance=0.1, level=0.35)
for b in range(16): kick(y, 0.05+b*4*s16)
y = np.clip(y, -1, 1); sf.write(OUT+'14-stretch-cricket-hats-124bpm.wav', y, SR, subtype='PCM_16'); print('14')

# 15 tymbal as a formant voice: pulse rate follows the notes, CARRIER is swept like a filter cutoff
I = Insect(16*s16*4+0.6, 15)
notes = [33, 33, 45, 33, 36, 33, 33, 43, 33, 45, 33, 31, 33, 33, 48, 36]; gate = [1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1]; slide = [0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1]
for bar in range(4):
    for i in range(16):
        if not gate[i]: continue
        t0 = 0.05+(bar*16+i)*s16; L = s16*(1.0 if slide[i] else 0.55)
        f0 = 440*2**((notes[i]-69)/12); f1 = 440*2**((notes[(i+1) % 16]-69)/12) if slide[i] else f0
        sweep = 350+ (500+700*bar)*(1.0 if i % 4 == 2 else 0.6)
        t = t0; k = 0
        while t < t0+L:
            x = (t-t0)/L; fn = f0*(f1/f0)**(max(0, x-0.5)*2)
            car = 300+sweep*np.exp(-x*3.0)
            I.tymbal(t, car, amp=np.exp(-x*1.2), ribs=3, rib_dt=1/car, q=6+2*bar, out=0.3, out_at=0.45/fn, side=k % 2, loose=0.004, spread=0.15, drop=0.15)
            t += 1/fn; k += 1
y = I.finish(OUT+'_tmp.wav', distance=0.0, level=0.5)
sosk = None
for b in range(16): kick(y, 0.05+b*4*s16)
y = np.clip(y*0.9, -1, 1); sf.write(OUT+'15-stretch-tymbal-formant-bassline.wav', y, SR, subtype='PCM_16'); print('15')
import os; os.remove(OUT+'_tmp.wav')

# 00 gallery: about five seconds of each natural sketch, in order
g = []
for name, y in made:
    n = int(5.5*SR); seg = y[:n].copy(); f = int(0.15*SR); seg[-f:] *= np.linspace(1, 0, f)[:, None]
    g += [seg, np.zeros((int(0.5*SR), 2))]
sf.write(OUT+'00-gallery.wav', np.concatenate(g), SR, subtype='PCM_16')
