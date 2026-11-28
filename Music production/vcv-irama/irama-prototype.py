"""Irama prototype: gamelan sequencing engine (Java + Bali kebyar) rendered to WAV.
Sequencer logic mirrors what the VCV module would do; the voices here are only
for listening (in Rack you'd use Bell Foundry / Panorama / your own voices)."""
import numpy as np, wave, sys

SR = 48000
rng = np.random.default_rng(7)

# ---------------- tunings ----------------
SLENDRO = [0, 240, 475, 720, 955]            # 1 2 3 5 6 (approx Central Java)
SELISIR = [0, 125, 270, 680, 795]            # ding dong deng dung dang (pelog subset, Bali)

def freq(scale, idx, base):
    o, d = divmod(idx, len(scale))
    return base * 2 ** (o + scale[d] / 1200)

# ---------------- seleh weights (from the research PDF) ----------------
def weights_slendro(stressed, strong, avoided, n=5):
    w = np.full(n, .18); w[stressed] = .30; w[strong] = .24; w[avoided] = .10
    return w / w.sum()

def pick(w, focus=1.0, exclude=None):
    w = np.array(w) ** focus
    if exclude is not None: w[exclude] *= .25
    w /= w.sum(); return int(rng.choice(len(w), p=w))

# ---------------- balungan / pokok ----------------
def make_gatra(seleh_deg, n=5, lo=3, hi=9):
    """Build a 4-beat gatra backwards from its landing tone (stepwise, mostly
    approaching from above = descending cadences)."""
    # place seleh in middle octave
    cur = seleh_deg + n
    notes = [cur]
    for _ in range(3):
        step = rng.choice([1, -1, 2, -2, 0], p=[.42, .26, .14, .1, .08])
        nxt = int(np.clip(cur + step, lo, hi))
        notes.append(nxt); cur = nxt
    return notes[::-1]

def make_cycle(n_gatra, w, stressed, n=5):
    g = []
    for k in range(n_gatra):
        last = k == n_gatra - 1
        s = stressed if last else pick(w, 2.0 if k % 2 else 1.0)
        g.append(make_gatra(s, n))
    return g

# ---------------- voices (listening only) ----------------
events = []   # (time, voice, freq, vel, extra)
def ev(t, v, f, vel=1.0, **kw): events.append((t, v, f, vel, kw))

def bar_tone(f, dur, partials, decays, amps, ombak=0.0, click=.0):
    n = int(SR * dur); t = np.arange(n) / SR
    y = np.zeros(n)
    for r, d, a in zip(partials, decays, amps):
        for det, g in ((0, 1.0), (ombak, 1.0)) if ombak else ((0, 1.0),):
            ph = rng.uniform(0, 6.28)
            y += a * g * np.exp(-t / d) * np.sin(2 * np.pi * (f * r + det * (r ** .5)) * t + ph)
    att = np.minimum(1, t / .0015); y *= att
    if click:
        k = int(.004 * SR); y[:k] += click * rng.standard_normal(k) * np.linspace(1, 0, k)
    return y

def gong(f, dur):
    n = int(SR * dur); t = np.arange(n) / SR
    fdrop = f * (1 + .012 * np.exp(-t / .4))
    ph = 2 * np.pi * np.cumsum(fdrop) / SR
    hum = np.sin(ph) + .97 * np.sin(ph * 1.0 + 2 * np.pi * 1.1 * t)  # built-in beating (gong ombak)
    y = .9 * hum * np.exp(-t / 5.5)
    y += .35 * np.sin(2.01 * ph) * np.exp(-t / 2.5) + .22 * np.sin(2.93 * ph) * np.exp(-t / 1.4)
    y += .12 * np.sin(4.1 * ph) * np.exp(-t / .5)
    swell = 1 - .6 * np.exp(-t / .08)
    return y * swell * np.minimum(1, t / .01)

def drum(f, dur, slap=.5):
    n = int(SR * dur); t = np.arange(n) / SR
    pf = f * (1 + .8 * np.exp(-t / .015)); ph = 2 * np.pi * np.cumsum(pf) / SR
    y = np.sin(ph) * np.exp(-t / .12) + slap * rng.standard_normal(n) * np.exp(-t / .02)
    return y

def render(events, total):
    L = int(SR * (total + 8)); outL = np.zeros(L); outR = np.zeros(L)
    for t0, v, f, vel, kw in events:
        om = kw.get('ombak', 0); pan = kw.get('pan', 0)
        damp = kw.get('damp', None)
        if v == 'saron':   y = bar_tone(f, 2.5, [1, 2.76, 5.2], [1.2, .35, .12], [1, .25, .08], om, .15)
        elif v == 'peking': y = bar_tone(f, 1.8, [1, 2.76, 5.2], [.8, .25, .1], [1, .2, .06], om, .2)
        elif v == 'gangsa': y = bar_tone(f, 1.6, [1, 2.71, 5.1], [.9, .25, .1], [1, .22, .07], om, .25)
        elif v == 'bonang' or v == 'reyong':
            y = bar_tone(f, 2.2, [1, 1.5, 2.0, 3.0, 4.15], [1.0, .3, .6, .25, .12], [1, .12, .3, .1, .05], om, .1)
        elif v == 'kenong': y = bar_tone(f, 4.5, [1, 2.0, 3.0, 4.2], [2.2, 1.0, .5, .2], [1, .3, .12, .05], om, .05)
        elif v == 'kempul': y = bar_tone(f, 4, [1, 2.02, 2.95], [2.0, .9, .4], [1, .35, .15], 1.6, .03)
        elif v == 'kethuk': y = bar_tone(f, .5, [1, 2.4, 3.9], [.09, .05, .03], [1, .3, .1], 0, .1)
        elif v == 'kempli': y = bar_tone(f, .3, [1, 2.3, 3.6], [.06, .04, .02], [1, .4, .2], 0, .3)
        elif v == 'gong':   y = gong(f, 9)
        elif v == 'kendang': y = drum(f, .5, kw.get('slap', .5))
        if damp is not None:   # Balinese damping: cut the ring after `damp` seconds
            k = int(damp * SR)
            if k < len(y): y[k:] *= np.exp(-np.arange(len(y) - k) / (SR * .03))
        i = int(t0 * SR); j = min(L, i + len(y)); y = y[:j - i] * vel
        gl, gr = np.cos((pan + 1) * np.pi / 4), np.sin((pan + 1) * np.pi / 4)
        outL[i:j] += y * gl; outR[i:j] += y * gr
    return outL, outR

def reverb(x, secs=2.2, mix=.22, seed=1):
    r = np.random.default_rng(seed); n = int(SR * secs); t = np.arange(n) / SR
    ir = r.standard_normal(n) * np.exp(-t / (secs / 6.9)); ir[:int(.012 * SR)] = 0
    ir /= np.sqrt((ir ** 2).sum())
    N = 1 << int(np.ceil(np.log2(len(x) + n)))
    wet = np.fft.irfft(np.fft.rfft(x, N) * np.fft.rfft(ir, N), N)[:len(x)]
    return x + mix * wet

# =====================================================================
# PART 1: Java, lancaran, slendro pathet manyura, irama 1 -> 2 -> 4
# =====================================================================
def java_part(t0):
    stressed, strong, avoided = 4, 1, 3        # 6, 2, avoided 5
    w = weights_slendro(stressed, strong, avoided)
    cyc = make_cycle(4, w, stressed)             # lancaran: 16 beats = 4 gatra
    base_sar = 262.0                              # saron demung-ish tone 1
    t = t0
    pulse = .16                                   # fastest elaboration step (fixed in traditional mode)
    plan = [(1, 2), (2, 2), (4, 2)]              # (irama: notes per balungan beat, gong cycles)
    for irama, reps in plan:
        for rep in range(reps):
            if rep == 1:   # MUTATE: rewrite one non-gong gatra per gong cycle
                k = int(rng.integers(0, 3)); cyc[k] = make_gatra(pick(w), 5)
            beat = pulse * max(irama, 2) * (1.6 if irama == 1 else 1)   # traditional: beat stretches with irama
            flat = [n for g in cyc for n in g]
            for b, note in enumerate(flat):
                tb = t + b * beat; pos = b + 1     # 1..16
                ev(tb, 'saron', freq(SLENDRO, note, base_sar / 2), .55, pan=-.2)
                nxt = flat[(b + 1) % 16]
                # peking: "a a b b" on the next pair, density = irama
                sub = max(irama, 2); pair = (flat[b - b % 2], flat[(b - b % 2 + 1) % 16])
                for s in range(sub):
                    k = ((b % 2) * sub + s) // max(1, sub // 2) % 4
                    nn = pair[0] if k < 2 else pair[1]
                    ev(tb + s * beat / sub, 'peking', freq(SLENDRO, nn, base_sar), .22, pan=.45)
                # bonang: mipil (alternate a,b) at irama>=2; imbal interlock at 4
                if irama == 2:
                    for s in range(2):
                        nn = pair[s % 2]
                        ev(tb + s * beat / 2, 'bonang', freq(SLENDRO, nn, base_sar / 2), .2, pan=-.5)
                if irama == 4:
                    sel = cyc[b // 4][3]
                    comp = [sel + 1, sel, sel + 2, sel + 1, sel, sel + 2, sel + 1, sel][(b % 4) * 2:(b % 4) * 2 + 2] if False else None
                    for s in range(4):
                        part = 'A' if s % 2 == 0 else 'B'
                        nn = (sel + (1 if part == 'A' else 2) + (s // 2 + b) % 2)
                        ev(tb + s * beat / 4, 'bonang', freq(SLENDRO, nn, base_sar / 2), .18,
                           pan=-.6 if part == 'A' else .6)
                # colotomy (lancaran)
                if pos % 2 == 1: ev(tb, 'kethuk', 520, .25, pan=.2)
                if pos in (6, 10, 14): ev(tb, 'kempul', freq(SLENDRO, cyc[(pos - 1) // 4][3], 131 / 2), .45)
                if pos % 4 == 0 and pos != 16:
                    ev(tb, 'kenong', freq(SLENDRO, note, 131), .38, pan=.3)
                if pos == 16:
                    ev(tb, 'kenong', freq(SLENDRO, note, 131), .38, pan=.3)
                    ev(tb, 'gong', 48.0, .9)
                # kendhang on gatra ends
                if pos % 4 == 3: ev(tb + beat * .5, 'kendang', 120, .25, slap=.3)
            t += 16 * beat
    return t

# =====================================================================
# PART 2: Bali gong kebyar: kebyar opening, gilak cycle, kotekan, angsel
# =====================================================================
def bali_part(t0):
    base_pem = 300.0   # pemade ding
    OMB = 6.5          # ombak: pengumbang/pengisep beat rate (Hz)
    t = t0
    # --- kebyar opening: unison tutti strikes, irregular, with gong ---
    hits = [0, .19, .38, 1.1, 1.3, 1.42, 1.54, 2.4, 3.3, 3.42, 3.54, 3.66, 3.78]
    chordset = [[0, 2, 3, 5], [1, 3, 5, 6], [0, 2, 3, 5]]
    for i, h in enumerate(hits):
        ch = chordset[i % 3]
        for c in ch:
            ev(t + h, 'gangsa', freq(SELISIR, c + 5, base_pem / 2), .2, ombak=OMB, pan=rng.uniform(-.7, .7))
            ev(t + h, 'reyong', freq(SELISIR, c + 5, base_pem / 2), .12, ombak=OMB * .7, pan=rng.uniform(-.4, .4))
        ev(t + h, 'kendang', 90, .4, slap=.6)
    ev(t, 'gong', 52.0, 1.0); ev(t + 3.78, 'gong', 52.0, .9)
    t += 5.0
    # --- gilak-style 8-beat gong cycle; pokok built like a balungan ---
    w = np.array([.28, .18, .2, .14, .2])           # selisir: designed, landing on ding/deng
    stressed = 0
    cyc = make_cycle(2, w, stressed)                 # 8 beats = 2 gatra
    sections = [('telu', .5, 3), ('nyogcag', .5, 2), ('empat', .44, 3), ('empat', .38, 2)]
    for si, (kot, beat, reps) in enumerate(sections):
        for rep in range(reps):
            if rep > 0: k = int(rng.integers(0, 2)); cyc[k][int(rng.integers(0, 3))] += int(rng.choice([-1, 1]))
            flat = [n for g in cyc for n in g]
            angsel = (si >= 2 and rep == reps - 1)   # break at the end of the last cycle
            for b, P in enumerate(flat):
                tb = t + b * beat; pos = b + 1
                nxt = flat[(b + 1) % 8]
                stop = angsel and pos >= 6
                if not stop:
                    # calung / jublag: pokok
                    ev(tb, 'saron', freq(SELISIR, P, base_pem / 4), .5, ombak=OMB * .5, pan=-.1)
                    # kotekan: 4 notes per beat leading to next pokok tone
                    x = nxt + 5 - 5   # target in gangsa register (index)
                    x = nxt
                    for s in range(4):
                        ts = tb + s * beat / 4
                        if kot == 'telu':
                            comp = [x + 1, x, x + 2, x + 1][s] if s < 3 else x
                            comp = [x + 1, x + 2, x, x + 1][s]
                            pol = comp in (x, x + 1); san = comp in (x + 1, x + 2)
                            if pol: ev(ts, 'gangsa', freq(SELISIR, comp + 5, base_pem / 2), .2, ombak=OMB, pan=-.55, damp=beat * .5)
                            if san: ev(ts, 'gangsa', freq(SELISIR, comp + 5, base_pem / 2), .2, ombak=OMB, pan=.55, damp=beat * .5)
                        elif kot == 'nyogcag':
                            comp = [x + 2, x + 1, x + 2, x + 1][s] if (b % 2) else [x - 1, x, x + 1, x][s]
                            side = -.6 if s % 2 == 0 else .6
                            ev(ts, 'gangsa', freq(SELISIR, comp + 5, base_pem / 2), .22, ombak=OMB, pan=side, damp=beat * .4)
                        else:  # empat: polos line, sangsih offbeats + kempyung (3 steps up) on shared hits
                            pol_line = [x + 1, x, x + 1, x]
                            if s % 2 == 0:
                                ev(ts, 'gangsa', freq(SELISIR, pol_line[s] + 5, base_pem / 2), .2, ombak=OMB, pan=-.55, damp=beat * .4)
                                if s == 0: ev(ts, 'gangsa', freq(SELISIR, pol_line[s] + 8, base_pem / 2), .17, ombak=OMB, pan=.55, damp=beat * .4)
                            else:
                                ev(ts, 'gangsa', freq(SELISIR, x + 2 + 5 + (s == 3), base_pem / 2), .2, ombak=OMB, pan=.55, damp=beat * .4)
                        # kantilan doubles an octave up
                        if kot != 'telu' and s % 2 == 0:
                            ev(ts, 'peking', freq(SELISIR, x + 10 + (s // 2) % 2, base_pem / 2), .07, ombak=OMB * 1.3, pan=.2, damp=beat * .3)
                    # reyong: norot-ish on offbeats
                    ev(tb + beat / 2, 'reyong', freq(SELISIR, nxt + 5, base_pem / 2), .12, ombak=OMB * .7, pan=.3)
                    # kempli keeps the beat; kajar-ish
                    ev(tb, 'kempli', 700, .2, pan=.1)
                    if b % 2: ev(tb + beat * .75, 'kendang', 140, .18, slap=.2)
                else:
                    # angsel: unison accents then a held breath
                    if pos == 6:
                        for k, off in enumerate([0, .25, .75]):
                            for c in (nxt, nxt + 2, nxt + 3):
                                ev(tb + off * beat, 'gangsa', freq(SELISIR, c + 5, base_pem / 2), .2, ombak=OMB, pan=rng.uniform(-.7, .7), damp=beat * .3)
                            ev(tb + off * beat, 'kendang', 100, .45, slap=.7)
                # colotomy (gilak-like): gong on 8, kempur on 4, kemong on 2/6
                if pos == 8: ev(tb, 'gong', 52.0, .9)
                if pos == 4: ev(tb, 'kempul', freq(SELISIR, P, 73), .5)
                if pos in (2, 6) and not stop: ev(tb, 'kenong', freq(SELISIR, 3, 196), .25, pan=-.3)
            t += 8 * beat
    # final gong
    ev(t, 'gong', 52.0, 1.0); ev(t, 'gangsa', freq(SELISIR, cyc[-1][-1] + 5, base_pem / 2), .25, ombak=OMB)
    return t

t = java_part(0.2)
t = bali_part(t + 1.0)
L, R = render(events, t)
L, R = reverb(L, seed=1), reverb(R, seed=2)
m = max(np.abs(L).max(), np.abs(R).max()); L, R = L / m * .89, R / m * .89
n = int(SR * (t + 6)); L, R = L[:n], R[:n]
fade = int(SR * 2); L[-fade:] *= np.linspace(1, 0, fade); R[-fade:] *= np.linspace(1, 0, fade)
pcm = (np.stack([L, R], 1) * 32767).astype('<i2')
out = sys.argv[1] if len(sys.argv) > 1 else 'irama-demo.wav'
with wave.open(out, 'wb') as f:
    f.setnchannels(2); f.setsampwidth(2); f.setframerate(SR); f.writeframes(pcm.tobytes())
print(out, round(t, 1), 's', len(events), 'events')
