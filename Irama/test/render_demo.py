#!/usr/bin/env python3
"""Render test/events.csv (written by test/render from the real engines) to a WAV.
The voices are rough listening stand-ins; in Rack use Bell Foundry, Panorama,
your own voices and drum modules.  python3 test/render_demo.py events.csv out.wav"""
import sys, wave, numpy as np
SR = 48000
rng = np.random.default_rng(3)

def bar_tone(f, dur, partials, decays, amps, ombak=0.0, click=0.0):
    n = int(SR * dur); t = np.arange(n) / SR; y = np.zeros(n)
    pairs = ((0.0, 1.0), (ombak, 1.0)) if ombak > 0 else ((0.0, 1.0),)
    for r, d, a in zip(partials, decays, amps):
        for det, g in pairs:
            y += a * g * np.exp(-t / d) * np.sin(2 * np.pi * (f * r + det * r ** .5) * t + rng.uniform(0, 6.28))
    y *= np.minimum(1, t / .0015)
    if click:
        k = int(.004 * SR); y[:k] += click * rng.standard_normal(k) * np.linspace(1, 0, k)
    return y

def gong(f, dur=9):
    n = int(SR * dur); t = np.arange(n) / SR
    ph = 2 * np.pi * np.cumsum(f * (1 + .012 * np.exp(-t / .4))) / SR
    y = .9 * (np.sin(ph) + .97 * np.sin(ph + 2 * np.pi * 1.1 * t)) * np.exp(-t / 5.5)
    y += .35 * np.sin(2.01 * ph) * np.exp(-t / 2.5) + .22 * np.sin(2.93 * ph) * np.exp(-t / 1.4)
    return y * (1 - .6 * np.exp(-t / .08)) * np.minimum(1, t / .01)

def hp_noise(n, k=2):
    x = rng.standard_normal(n)
    for _ in range(k): x = np.diff(x, prepend=0)
    return x / 2 ** k

def kick(dur=.5):
    n = int(SR * dur); t = np.arange(n) / SR
    f = 48 + 140 * np.exp(-t / .03); ph = 2 * np.pi * np.cumsum(f) / SR
    return np.tanh(1.6 * np.sin(ph) * np.exp(-t / .22)) + .3 * rng.standard_normal(n) * np.exp(-t / .002)

def noise_hit(dur, dec, k=2):
    n = int(SR * dur); t = np.arange(n) / SR
    return hp_noise(n, k) * np.exp(-t / dec)

def clap():
    n = int(SR * .35); t = np.arange(n) / SR; env = np.zeros(n)
    for o in (0, .011, .022): env += (t >= o) * np.exp(-np.maximum(t - o, 0) / (.008 if o < .02 else .09))
    return hp_noise(n, 1) * env * .8

def drum(f, slap=.4):
    n = int(SR * .5); t = np.arange(n) / SR
    ph = 2 * np.pi * np.cumsum(f * (1 + .8 * np.exp(-t / .015))) / SR
    return np.sin(ph) * np.exp(-t / .12) + slap * rng.standard_normal(n) * np.exp(-t / .02)

def hz(v): return 261.63 * 2 ** v

ev = []
for line in open(sys.argv[1]):
    t, v, volts, om = line.strip().split(',')
    ev.append((float(t), v, float(volts), float(om)))
total = max(e[0] for e in ev) + 8
L = np.zeros(int(SR * total)); R = np.zeros_like(L)
bass_ev = [e for e in ev if e[1] == 'bass']

def put(t0, y, pan=0.0, g=1.0):
    i = int(t0 * SR); j = min(len(L), i + len(y)); y = y[:j - i] * g
    L[i:j] += y * np.cos((pan + 1) * np.pi / 4); R[i:j] += y * np.sin((pan + 1) * np.pi / 4)

for t, v, volts, om in ev:
    f = hz(volts); bali = om >= 6
    if v == 'saron':   put(t, bar_tone(f / 2 if bali else f, 2.5, [1, 2.76, 5.2], [1.2, .35, .12], [1, .25, .08], om * .5, .15), -.2, .5)
    elif v == 'peking': put(t, bar_tone(f, 1.6, [1, 2.76, 5.2], [.8, .25, .1], [1, .2, .06], om * 1.2, .2), .45, .16)
    elif v == 'bonang': put(t, bar_tone(f, 2.0, [1, 1.5, 2.0, 3.0, 4.15], [1.0, .3, .6, .25, .12], [1, .12, .3, .1, .05], om * .7, .1), -.45, .17)
    elif v in ('polos', 'sangsih'):
        y = bar_tone(f, 1.4, [1, 2.71, 5.1], [.9, .25, .1], [1, .22, .07], om, .25)
        if bali:
            k = int(.16 * SR); y[k:] *= np.exp(-np.arange(len(y) - k) / (SR * .03))
        put(t, y, -.6 if v == 'polos' else .6, .17)
    elif v == 'gong':   put(t, gong(48.0), 0, .9)
    elif v == 'kenong': put(t, bar_tone(f, 4.0, [1, 2.0, 3.0, 4.2], [2.2, 1.0, .5, .2], [1, .3, .12, .05], 0, .05), .3, .32)
    elif v == 'kempul': put(t, bar_tone(f / 2, 3.5, [1, 2.02, 2.95], [2.0, .9, .4], [1, .35, .15], 1.6, .03), -.1, .4)
    elif v == 'kethuk': put(t, bar_tone(520 if not bali else 700, .4, [1, 2.4, 3.9], [.08, .05, .03], [1, .3, .1], 0, .2), .2, .2)
    elif v == 'kempyang': put(t, bar_tone(880, .3, [1, 2.3], [.05, .03], [1, .3], 0, .1), .35, .1)
    elif v == 'kendang': put(t, drum(130), -.15, .22)
    elif v == 'kick':   put(t, kick(), 0, .7)
    elif v == 'dgong':  put(t, gong(46.0), 0, .55); put(t, noise_hit(1.5, .45, 1), .1, .12)
    elif v == 'clap':   put(t, clap(), .05, .35)
    elif v == 'ohat':   put(t, noise_hit(.4, .1, 3), .3, .16)
    elif v == 'ohat2':  put(t, noise_hit(.25, .05, 3), -.3, .10)
    elif v == 'rim':    put(t, bar_tone(1700, .06, [1, 1.7], [.012, .008], [1, .5], 0, .3), .4, .08)
    elif v == 'hat':    put(t, noise_hit(.06, .012, 3), -.5, .22)
    elif v == 'shaker': put(t, noise_hit(.09, .025, 2), .5, .12)
# seleh bass: held saw-ish sine until the next seleh
for i, (t, v, volts, om) in enumerate(bass_ev):
    t1 = bass_ev[i + 1][0] if i + 1 < len(bass_ev) else t + 2
    n = int((t1 - t) * SR); tt = np.arange(n) / SR; f = hz(volts) / 2
    y = np.tanh(2.2 * (np.sin(2 * np.pi * f * tt) + .3 * np.sin(4 * np.pi * f * tt))) * np.minimum(1, tt / .01) * np.minimum(1, (n - np.arange(n)) / (SR * .02))
    put(t, y, 0, .28)

def reverb(x, secs=2.0, mix=.18, seed=1):
    r = np.random.default_rng(seed); n = int(SR * secs); t = np.arange(n) / SR
    ir = r.standard_normal(n) * np.exp(-t / (secs / 6.9)); ir[:int(.012 * SR)] = 0; ir /= np.sqrt((ir ** 2).sum())
    N = 1 << int(np.ceil(np.log2(len(x) + n)))
    return x + mix * np.fft.irfft(np.fft.rfft(x, N) * np.fft.rfft(ir, N), N)[:len(x)]
L, R = reverb(L, seed=1), reverb(R, seed=2)
m = max(np.abs(L).max(), np.abs(R).max()); L, R = L / m * .89, R / m * .89
n = int(SR * (total - 4)); L, R = L[:n], R[:n]
fade = SR * 2; L[-fade:] *= np.linspace(1, 0, fade); R[-fade:] *= np.linspace(1, 0, fade)
with wave.open(sys.argv[2], 'wb') as w:
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
    w.writeframes((np.stack([L, R], 1) * 32767).astype('<i2').tobytes())
print(sys.argv[2], round(total - 4, 1), 's')
