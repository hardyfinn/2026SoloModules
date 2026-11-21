"""Clapfall reference prototype (design by Fable). Numpy/scipy only.
Everything the C++ needs is expressed with: white noise, exponential envelopes,
RBJ band-pass biquads, one-pole low/high-pass, linear chirps.  Tables are built
once at 48 kHz and then played back by a simple resampling voice player."""
import numpy as np
from scipy import signal
SR = 48000
NVAR = 24          # variants per surface

# ---------------------------------------------------------------- primitives
def onepole_lp(x, fc):
    a = np.exp(-2*np.pi*fc/SR); return signal.lfilter([1-a], [1, -a], x)
def onepole_hp(x, fc):
    return x - onepole_lp(x, fc)
def bp(x, f, q):                       # RBJ band-pass, constant 0 dB peak gain
    w = 2*np.pi*min(f, SR*0.45)/SR; al = np.sin(w)/(2*q)
    b = [al, 0, -al]; a = [1+al, -2*np.cos(w), 1-al]
    return signal.lfilter(b, a, x)

# ---------------------------------------------------------------- recipes
# contact = (delay_ms, gain, attack_ms, decay_ms)   noise burst hitting the body
# mode    = (freq_hz, Q, gain_db)                   resonances of the body
# Fields:  air = (gain_db, hp_hz, lp_hz) un-resonated part of the burst
#          click = (gain_db, lp_hz) single band-limited impulse at t=0
#          var = (freq_pct, q_pct, gain_db, time_pct, delay_pct) per-variant scatter
#          chirp = (f0, f1, dur_ms, gain_db) optional sine sweep (bubbles)
#          ring = extra multiplier on table length
def R(length_ms, contacts, modes, air=None, click=None, var=(8, 25, 3, 25, 40), chirp=None, exc_lp=None):
    # exc_lp: two cascaded one-pole low-passes on the noise burst (soft things do not make white noise)
    return dict(len=length_ms, contacts=contacts, modes=modes, air=air, click=click, var=var, chirp=chirp, exc_lp=exc_lp)

HANDS = {
 # flat palm on flat palm: bright, hard, almost no cavity
 'PALMS':   R(45, [(0, 1.0, 0.15, 2.2)], [(1950, 3.2, 0), (2900, 3.0, -6), (760, 2.0, -7), (4800, 2.5, -10)], air=(-20, 1500, 9000), click=(-20, 6000), exc_lp=4200),
 # ordinary clap, palms at an angle: the "standard" 1.5 kHz clap
 'CLAP':    R(50, [(0, 1.0, 0.2, 2.8), (1.6, 0.35, 0.2, 2.0)], [(1600, 3.4, 0), (640, 1.8, -2), (2450, 3.0, -6), (4600, 2.2, -9), (980, 2.0, -5)], air=(-19, 1200, 9000), click=(-24, 5000), exc_lp=3000),
 # cupped hands: air cavity, low and round
 'CUPPED':  R(60, [(0, 1.0, 0.35, 3.6)], [(690, 3.8, 0), (1240, 3.0, -5), (330, 1.8, -7), (2600, 2.5, -12)], air=(-28, 800, 5000), click=(-28, 3000), exc_lp=1800),
 # fingers on palm: thin and high
 'FINGERS': R(40, [(0, 1.0, 0.12, 1.6), (0.9, 0.5, 0.12, 1.4)], [(2450, 3.0, 0), (1360, 2.6, -5), (3900, 3.0, -6), (6200, 2.5, -10)], air=(-18, 2500, 11000), click=(-18, 8000), exc_lp=5500),
}
SURF = dict(HANDS)
SURF.update({
 'SNAP':    R(50, [(0, 0.25, 0.1, 0.8), (3.2, 1.0, 0.08, 1.3)], [(2750, 5.0, 0), (3600, 5.0, -4), (1500, 3.0, -9), (5600, 4.0, -8)], air=(-20, 2500, 12000), click=(-16, 9000), var=(7, 20, 3, 20, 25), exc_lp=6000),
 'SLAP':    R(70, [(0, 1.0, 0.4, 4.5)], [(240, 2.2, 0), (520, 2.0, -4), (1150, 2.0, -8), (2700, 2.0, -12)], air=(-24, 600, 6000), click=(-26, 3000), exc_lp=1500),
 'STOMP':   R(160, [(0, 1.0, 0.6, 7.0)], [(92, 5.0, 0), (158, 5.0, -4), (265, 4.0, -8), (710, 2.0, -12)], air=(-26, 400, 3500), click=(-26, 2500), var=(10, 25, 3, 25, 0), exc_lp=900),
 'STICKS':  R(90, [(0, 1.0, 0.05, 0.5)], [(1830, 28, 0), (2710, 30, -5), (4650, 35, -6), (7300, 30, -12), (980, 12, -14)], air=(-18, 3000, 14000), click=(-6, 12000), var=(5, 20, 3, 20, 0)),
 'BLOCK':   R(110, [(0, 1.0, 0.08, 0.7)], [(860, 22, 0), (1420, 18, -9), (2350, 20, -12), (3900, 15, -18)], air=(-24, 1500, 9000), click=(-12, 8000), var=(6, 20, 3, 20, 0)),
 'RIM':     R(80, [(0, 1.0, 0.05, 0.6)], [(470, 9, -3), (1720, 16, 0), (2480, 20, -4), (3850, 18, -7), (6100, 14, -12)], air=(-14, 3000, 13000), click=(-5, 12000), var=(4, 20, 3, 20, 0)),
 'PING':    R(70, [(0, 1.0, 0.05, 0.45)], [(5800, 14, 0), (1080, 7, -5), (8700, 12, -8), (2950, 8, -12)], air=(-20, 3000, 14000), click=(-6, 14000), var=(4, 20, 3, 20, 0)),
 'SWITCH':  R(45, [(0, 0.7, 0.04, 0.35), (2.6, 1.0, 0.04, 0.45), (7.5, 0.2, 0.04, 0.4)], [(3350, 12, 0), (5200, 14, -3), (7900, 12, -6), (1450, 6, -12)], air=(-12, 3500, 15000), click=(-3, 15000), var=(8, 20, 3, 20, 25)),
 'COINS':   R(140, [(0, 1.0, 0.04, 0.4), (4.5, 0.6, 0.04, 0.4)], [(8500, 60, 0), (10900, 70, -2), (12700, 60, -5), (6300, 40, -6), (4100, 30, -14)], air=(-22, 5000, 16000), click=(-10, 15000), var=(9, 25, 4, 30, 60)),
 'GLASS':   R(260, [(0, 1.0, 0.05, 0.5)], [(4880, 140, 0), (7900, 120, -8), (2040, 60, -14), (11300, 90, -15)], air=(-24, 3000, 14000), click=(-10, 12000), var=(12, 25, 3, 30, 0)),
 'TIN':     R(150, [(0, 1.0, 0.06, 0.7)], [(492, 14, 0), (926, 18, -8), (2210, 25, -10), (3990, 30, -6), (6400, 25, -14)], air=(-18, 2000, 12000), click=(-8, 10000), var=(10, 25, 3, 30, 0)),
 'PEBBLE':  R(40, [(0, 1.0, 0.04, 0.5), (1.1, 0.4, 0.04, 0.4)], [(3900, 9, 0), (5600, 9, -3), (2500, 6, -8), (8800, 8, -9)], air=(-12, 3000, 15000), click=(-4, 14000), var=(14, 25, 4, 25, 60)),
 'DROP':    R(60, [(0, 0.15, 0.1, 0.6)], [(2400, 3, -6)], air=(-20, 2000, 9000), click=(-14, 6000), chirp=(1150, 2300, 14, 0), var=(22, 20, 3, 30, 0)),
 'CRACKLE': R(12, [(0, 1.0, 0.02, 0.18)], [(4200, 1.2, 0)], air=(-3, 1200, 16000), click=(0, 16000), var=(40, 20, 6, 60, 0)),
 # drum machine style: these are NOT hands but the classic filtered-noise pulses
 '808':     R(170, [(0, 1.0, 0.05, 4.0), (9.0, 0.7, 1.0, 30.0)], [(1020, 1.5, 0)], air=(-24, 1500, 9000), click=None, var=(0.5, 3, 0.3, 4, 0)),
 '909':     R(220, [(0, 1.0, 0.05, 4.5), (10.0, 0.6, 1.0, 42.0)], [(1100, 1.2, 0), (2300, 1.2, -1), (4600, 1.2, -4)], air=(-9, 2500, 14000), click=None, var=(0.5, 3, 0.3, 4, 0)),
 'NOISE':   R(60, [(0, 1.0, 0.05, 6.0)], [], air=(0, 20, 20000), click=None, var=(0, 0, 0.5, 30, 0)),
})
CHOKE = {'808', '909', 'NOISE'}   # a new particle cuts the previous one off (1.5 ms fade), like a retriggered analog envelope
ORDER = ['CLAP','PALMS','CUPPED','FINGERS','GROUP','SNAP','SLAP','STOMP','STICKS','BLOCK','RIM','PING','SWITCH','COINS','GLASS','TIN','PEBBLE','DROP','CRACKLE','808','909','NOISE']

def render_variant(rec, rng):
    fv, qv, gv, tv, dv = rec['var']
    n = int(rec['len']*SR/1000); t = np.arange(n)/SR
    tscale = 2**(rng.uniform(-1, 1)*tv/100)
    exc = np.zeros(n)
    for (dly, g, att, dec) in rec['contacts']:
        d = dly*(1+rng.uniform(-1, 1)*dv/100); i0 = int(d*SR/1000)
        if i0 >= n: continue
        tt = t[:n-i0]; a = att/1000; dc = dec/1000*tscale
        env = (1-np.exp(-tt/a))*np.exp(-tt/dc)
        env /= env.max()
        gg = g*10**(rng.uniform(-1, 1)*gv/20) if dly > 0 else g
        exc[i0:] += gg*env*rng.standard_normal(n-i0)
    if rec['exc_lp']:
        exc = onepole_lp(onepole_lp(exc, rec['exc_lp']), rec['exc_lp'])
    out = np.zeros(n)
    fshift = 2**(rng.uniform(-1, 1)*fv/100*1.0)   # whole body moves together (hand size)
    for (f, q, g) in rec['modes']:
        ff = f*fshift*2**(rng.uniform(-1, 1)*fv/100*0.35)
        qq = q*2**(rng.uniform(-1, 1)*qv/100)
        gg = 10**((g+rng.uniform(-1, 1)*gv)/20)
        # band-pass has 0 dB peak; scale by sqrt(q) so narrow modes keep their energy
        out += gg*np.sqrt(qq)*bp(exc, ff, qq)
    if rec['air']:
        g, hp, lp = rec['air']
        out += 10**(g/20)*3.0*onepole_lp(onepole_hp(exc, hp), lp)
    if rec['click']:
        g, lp = rec['click']
        imp = np.zeros(n); imp[0] = 1.0
        c = onepole_lp(onepole_lp(imp, lp), lp); c /= np.abs(c).max()
        out += 10**(g/20)*np.abs(out).max()*c if np.abs(out).max() > 0 else c
    if rec['chirp']:
        f0, f1, dur, g = rec['chirp']; k = fshift*2**(rng.uniform(-1,1)*fv/100)
        d = dur/1000*tscale; tt = np.minimum(t, d)
        ph = 2*np.pi*k*(f0*tt + (f1-f0)*tt*tt/(2*d))
        out += 10**(g/20)*np.sin(ph)*np.exp(-t/(d*0.45))*(t < d*3)*max(np.abs(out).max(), 1e-3)*4
    # fade the last 3 ms, remove DC, normalise to equal energy-ish loudness
    fade = int(0.003*SR); out[-fade:] *= np.linspace(1, 0, fade)
    out = onepole_hp(out, 30)
    rms = np.sqrt(np.sum(out**2)); pk = np.abs(out).max()
    out = out/max(pk, 1e-9)                    # peak 1.0 ...
    # ... then pull loud-and-long variants down a little so sets are even
    return out.astype(np.float32)

def build_tables(seed=1):
    tabs = {}
    for si, name in enumerate(ORDER):
        rng = np.random.default_rng(seed*1000+si)
        vs = []
        for v in range(NVAR):
            if name == 'GROUP':
                rec = HANDS[['CLAP','PALMS','CUPPED','FINGERS','CLAP','CUPPED'][v % 6]]
            else:
                rec = SURF[name]
            vs.append(render_variant(rec, rng))
        tabs[name] = vs
    return tabs

# ---------------------------------------------------------------- engine
def density_hz(k):   return 1.5*400**k                 # 1.5 .. 600 particles/s
def decay_s(k):      return 0.012*500**k               # 12 ms .. 6 s  (time constant = this/3 => about -26 dB at decay_s)
def freq_hz(k):      return 150*(10000/150)**k
def spread_oct(k):   return 1.25*k*k + 0.25*k          # 0 .. 1.5 octaves either side

def burst(tabs, surface, dur, density=0.6, decay=0.35, loose=0.5, freq=0.5, spread=0.3,
          env_mode=1, filt_mode=0, accent=False, free=False, trigs=(0.0,), ones=(), pitch_v=None,
          reverb=0.0, size=0.3, seed=7, level=1.0):
    """Offline model of one module instance. env_mode 0=AMP 1=BOTH 2=CHANCE; filt_mode 0=BAND 1=BAND+PITCH 2=PARALLEL."""
    rng = np.random.default_rng(seed); n = int(dur*SR)
    L = np.zeros(n+SR); Rr = np.zeros(n+SR)
    vs = tabs[surface]; last = -1
    rate = density_hz(density); tau = decay_s(decay)/3.0
    centre = 1.0
    if pitch_v is not None: centre = 2**pitch_v
    elif filt_mode >= 1:    centre = 2**((freq-0.5)*4)        # +-2 octaves
    sp = spread_oct(spread)
    le = min(1.0, loose*4)          # how much chance is allowed in; 0 = fully repeatable machine
    choke = surface in CHOKE
    comp = 1.0 if choke else (1+rate/80)**-0.5
    def fire(t, amp):
        nonlocal last
        v = rng.integers(NVAR-1); v = v if v < last else v+1
        if last < 0: v = rng.integers(NVAR)
        last = v
        ratio = centre*2**(rng.uniform(-1, 1)*sp)
        pan = np.clip(rng.uniform(-1, 1)*min(1.0, spread*1.6), -1, 1)
        amp = comp*amp*10**(le*rng.uniform(-2.5, 2.5)/20)
        x = vs[v]; m = int(len(x)/ratio)
        idx = np.arange(m)*ratio; i0 = idx.astype(int); fr = idx-i0
        xx = np.append(x, 0.0); y = xx[i0]*(1-fr)+xx[np.minimum(i0+1, len(x))]*fr
        i = int(t*SR)
        if i >= n: return
        th = (pan+1)*np.pi/4
        if choke:
            f = int(0.0015*SR); ramp = np.linspace(1, 0, f)
            for B in (L, Rr): B[i:i+f] *= ramp; B[i+f:] = 0
        L[i:i+m] += amp*np.cos(th)*y; Rr[i:i+m] += amp*np.sin(th)*y
    for t0 in ones: fire(t0, 1.0)
    trigs = sorted(trigs)
    # particle clock: one jittered clock; a trigger restarts it so a particle lands on the beat
    t = trigs[0] if (trigs and not free) else 0.0
    ti = 0
    while t < dur:
        # which trigger is current?
        while ti+1 < len(trigs) and trigs[ti+1] <= t: ti += 1
        if free:
            e = (decay)                                     # knob stands in for the envelope
            boost = 1.0
            for tr in trigs:
                if tr <= t: boost += 3.0*np.exp(-(t-tr)/0.12)
            r = rate*boost
        else:
            if not trigs or t < trigs[0]: break
            age = t-trigs[ti]
            acc = 1.6 if accent else 1.0
            e = np.exp(-age/(tau*acc)); r = rate
            if e < 1e-3:
                if ti+1 < len(trigs): t = trigs[ti+1]; continue
                break
        if env_mode == 0:   amp, prob = e, 1.0
        elif env_mode == 1: amp, prob = np.sqrt(e), np.sqrt(e)
        else:               amp, prob = 1.0 - le*rng.uniform(0, 0.65), e
        if free and env_mode != 0 and e <= 0: prob = 0
        if free and env_mode == 0 and e <= 0: prob = 0
        first = (not free) and abs(t-trigs[ti]) < 1e-9
        u = 0.5 + (rng.random()-0.5)*le   # at LOOSE 0 a particle fires exactly while prob > 0.5
        if first or u < prob:
            fire(t, amp*(1.5 if (accent and not free) else 1.0))
        T = 1.0/r
        nt = t + T*((1-loose) + loose*rng.exponential(1.0))
        if (not free) and ti+1 < len(trigs) and nt >= trigs[ti+1]: nt = trigs[ti+1]
        t = nt
    L = L[:n]; Rr = Rr[:n]
    # filter section
    fc = freq_hz(freq); Q = 1.8
    def filt(x):
        b = bp(x, fc, Q)*2.0
        return b if filt_mode < 2 else x + b*1.5
    L, Rr = filt(L), filt(Rr)
    g = 0.5*level
    L, Rr = L*g, Rr*g
    if reverb > 0:
        irL, irR = reverb_ir(size)
        wl = signal.fftconvolve(L, irL)[:n]; wr = signal.fftconvolve(Rr, irR)[:n]
        L, Rr = L + reverb*wl, Rr + reverb*wr
    out = np.stack([L, Rr], 1)
    return np.tanh(out*0.9)/0.9

_ir = {}
def reverb_ir(size):
    """Stand-in for the FDN: decaying shaped noise with a few early taps. size 0=small room .. 1=hall"""
    key = round(size, 3)
    if key in _ir: return _ir[key]
    rt = 0.25*(3.2/0.25)**size; pre = 0.004+0.02*size
    n = int((rt*1.3+0.1)*SR); t = np.arange(n)/SR; out = []
    for ch in range(2):
        rng = np.random.default_rng(100+ch)
        x = rng.standard_normal(n)*np.exp(-6.91*t/rt)
        hi = onepole_lp(x, 9000-5000*size)
        lo = onepole_lp(x, 2200)
        # high end dies faster than the low end
        mix = np.exp(-t/(rt*0.25)); x = hi*mix + lo*(1-mix)*1.4
        x = onepole_hp(x, 180)
        build = 1-np.exp(-t/(0.004+0.03*size)); x *= build
        d = int(pre*SR); ir = np.zeros(n+d); ir[d:] = x
        for k in range(6):                                   # early reflections
            tk = pre*0.4 + rng.uniform(0.003, 0.012+0.05*size)*(k+1)/3
            i = int(tk*SR)
            if i < len(ir): ir[i] += rng.choice([-1, 1])*8*0.75**k
        ir *= 0.5/np.sqrt(np.sum(ir**2))
        out.append(ir)
    _ir[key] = out; return out
