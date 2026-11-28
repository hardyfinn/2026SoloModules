"""Prototype of the proposed grain engine (design stage, Fable 2026-11-24).
Circular 4 s stereo buffer, up to 64 grains, block-rendered (32 samples).
Not the build code: the C++ version is per-sample with 4-point interpolation."""
import numpy as np, wave

SR = 44100
BUF_S = 4.0
MAXG = 64
N = 32
GUARD = N + 8
FBCAL = 2.0      # FEEDBACK 1.0 = loop gain about 1 for plain overlapping grains

SCALES = {
 'chromatic':[0,1,2,3,4,5,6,7,8,9,10,11],'major':[0,2,4,5,7,9,11],'minor':[0,2,3,5,7,8,10],
 'dorian':[0,2,3,5,7,9,10],'lydian':[0,2,4,6,7,9,11],'super_locrian':[0,1,3,4,6,8,10],
 'mixolydian':[0,2,4,5,7,9,10],'hex_aeolian':[0,3,5,7,8,10],'hex_dorian':[0,2,3,5,7,10],
 'blues':[0,3,5,6,7,10],'pentatonic':[0,3,5,7,10],'hirajoshi':[0,2,3,7,8],'kumoi':[0,2,3,7,9],
 'iwato':[0,1,5,6,10],'whole_tone':[0,2,4,6,8,10],'pelog':[0,1,3,7,8],'tetratonic':[0,4,7,11],
 'fifths':[0,7],'octaves':[0]}

def quantise(semi, scale):
    if scale is None: return semi
    deg = SCALES[scale]
    best = None
    o = np.floor(semi/12.0)
    for oo in (o-1, o, o+1):
        for d in deg:
            c = oo*12 + d
            if best is None or abs(c-semi) < abs(best-semi): best = c
    return best

def readwav(path):
    w = wave.open(path); n = w.getnframes(); ch = w.getnchannels(); sr = w.getframerate()
    a = np.frombuffer(w.readframes(n), dtype='<i2').astype(np.float64)/32768.0
    a = a.reshape(-1, ch)
    if ch == 1: a = np.repeat(a, 2, axis=1)
    return a, sr

def writewav(path, x, sr=SR):
    x = np.clip(x, -1, 1)
    w = wave.open(path, 'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr)
    w.writeframes((x*32767).astype('<i2').tobytes()); w.close()

def val(p, t):
    return p(t) if callable(p) else p

DEFAULTS = dict(
    pitch=0.0, scale='chromatic', rate=8.0, size=0.25, contour=0.0,
    scan_mode='delay', scan=0.25,          # stretch: speed (1 = with write head); position: 0..1 of buffer; delay: seconds
    spray_mode='random', spray=0.0,        # random / random_mono / warp / warp_mono; amount 0..1 (1 = +-1 s)
    pattern=0.0, wet=1.0, wet_mode='mix',
    fb_mode='grain', feedback=0.0,
    randpitch_mode='detune', randpitch=0.0, randrate=0.0, randsize=0.0, randamp=0.0, randrev=0.0,
    lock=False, seed=1, age=0.0,
)

def process(x, **kw):
    """x: (n,2) input. Returns (out, stats)."""
    P = dict(DEFAULTS); P.update(kw)
    rng = np.random.default_rng(P['seed'])
    pat = np.random.default_rng(P['seed']+1000).uniform(-1, 1, 16)   # fixed per instance
    n = len(x); nb = n//N
    Bn = int(BUF_S*SR)
    buf = np.zeros((Bn, 2))
    out = np.zeros((nb*N, 2))
    wetprev = np.zeros((N, 2))
    # grain state
    act = np.zeros(MAXG, bool); age = np.zeros(MAXG); glen = np.ones(MAXG)
    pos = np.zeros(MAXG); inc = np.ones(MAXG); gl = np.zeros(MAXG); gr = np.zeros(MAXG)
    peakpos = np.full(MAXG, 0.5); lin = np.zeros(MAXG)
    w = 0                      # write head (absolute sample in buffer)
    play = 0.0                 # stretch playhead (absolute)
    next_spawn = 0.0           # in samples
    grid = 0.0
    step = 0
    warp = np.zeros(2); warp_t = np.zeros(2)
    ar = np.arange(N)
    maxact = 0; nspawn = 0; shortened = 0
    locked_prev = False
    fbenv = 0.0; hpz = np.zeros(2); a_hp = 1 - np.exp(-2*np.pi*30.0/SR); fbsig = np.zeros((N, 2))
    AGE = float(P['age'])
    a_lp = 1 - np.exp(-2*np.pi*(18000.0*(5000.0/18000.0)**AGE)/SR)   # record bandwidth 18 kHz -> 5 kHz
    lpz = np.zeros(2); lpz2 = np.zeros(2)
    bits = 16 - 7*AGE                                              # 16 -> 9 bits
    qstep = 2.0**(1-bits)
    hiss = 10**((-84 + 34*AGE)/20)                                 # -84 -> -50 dB
    drift = 0.0; drift_t = 0.0
    for b in range(nb):
        t = b*N/SR
        lock = bool(val(P['lock'], t))
        xin = x[b*N:(b+1)*N]
        rec = xin
        if AGE > 0:
            rec = np.empty_like(xin)
            for i in range(N):                       # 2-pole record filter, then drive, hiss and word length
                lpz += a_lp*(xin[i] - lpz); lpz2 += a_lp*(lpz - lpz2); rec[i] = lpz2
            dr = 1 + 2*AGE
            rec = np.tanh(rec*dr)/dr + rng.normal(0, hiss, (N, 2))
            rec = np.round(rec/qstep + rng.uniform(-0.5, 0.5, (N, 2))*(1-AGE))*qstep
            ad = np.exp(-N/(SR*1.5)); drift_t = drift_t*ad + rng.normal()*np.sqrt(1-ad*ad); drift = drift*ad + drift_t*(1-ad)
        fbamt = val(P['feedback'], t)
        if fbamt > 0 and P['fb_mode'] == 'grain':
            # feedback path: 30 Hz high-pass, then a limiter that holds the grains at or below half scale
            pk = float(np.abs(wetprev).max())
            fbenv = max(pk, fbenv*np.exp(-N/(SR*0.2)))
            lim = min(1.0, 0.5/max(fbenv, 1e-9))
            for i in range(N):
                hpz += a_hp*(wetprev[i] - hpz); fbsig[i] = wetprev[i] - hpz
            fbsig = np.tanh(fbsig*FBCAL*fbamt*lim)
        else:
            fbsig[:] = 0
        if not lock:
            idx = (w + ar) % Bn
            if P['fb_mode'] == 'grain':
                buf[idx] = rec + fbsig
            else:   # legacy sound-on-sound: keep part of what was there
                buf[idx] = rec + fbamt*buf[idx]
            w = (w + N) % Bn
        # smooth random for warp (two independent ~0.7 Hz walks)
        a = np.exp(-N/(SR*0.35))
        warp_t = warp_t*a + rng.normal(0, 1, 2)*np.sqrt(1-a*a)
        warp = warp*a + warp_t*(1-a)
        # stretch playhead
        mode = P['scan_mode']; scan = val(P['scan'], t)
        if mode == 'stretch':
            play = (play + scan*N) % Bn
        # spawn
        rate = max(0.05, val(P['rate'], t))
        while next_spawn < (b+1)*N:
            off = next_spawn - b*N
            size = val(P['size'], t)*2.0**(-rng.uniform(0, 1)*3*val(P['randsize'], t))
            L = max(64.0, size*SR)
            scale = P['scale'] if P['scale'] != 'free' else None
            semi = val(P['pitch'], t) + pat[step % 16]*24.0*val(P['pattern'], t)
            rp = val(P['randpitch'], t); cents = 0.0; panr = 0.0
            if P['randpitch_mode'] == 'detune':
                cents = rng.uniform(-1, 1)*100*rp
            else:
                semi += rng.uniform(-1, 1)*24*rp
                if P['randpitch_mode'] == 'random': panr = rng.uniform(-1, 1)*min(1.0, rp*2)
            semi = quantise(semi, scale) + cents/100.0
            semi = float(np.clip(semi, -36, 36)) + AGE*0.35*float(np.clip(drift*3, -1, 1))   # clock drift, up to +-35 cents
            r = 2.0**(semi/12.0)
            rev = rng.uniform() < val(P['randrev'], t)
            spray = val(P['spray'], t); sm = P['spray_mode']; pan = panr
            if sm.startswith('random'):
                so = rng.uniform(-1, 1)*spray*SR
                if sm == 'random': pan += rng.uniform(-1, 1)*min(1.0, spray*2)
            else:
                so = warp[0]*spray*SR*3
                if sm == 'warp': pan += float(np.clip(warp[1]*3, -1, 1))*min(1.0, spray*2)
            pan = float(np.clip(pan, -1, 1))
            wsp = (w - N + int(off)) % Bn if not lock else w   # write head at spawn time
            if mode == 'stretch': start = play
            elif mode == 'position': start = scan*Bn
            else: start = wsp - scan*SR
            start = (start + so) % Bn
            if not lock:
                d0 = (wsp - start) % Bn
                while True:
                    if not rev: lo = GUARD + max(0.0, (r-1)*L); hi = Bn - GUARD - max(0.0, (1-r)*L)
                    else:       lo = GUARD;                     hi = Bn - GUARD - (1+r)*L   # reads backwards from start
                    if lo <= hi: break
                    L *= 0.5; shortened += 1
                d0 = min(max(d0, lo), hi)
                start = (wsp - d0) % Bn
            # allocate
            free = np.flatnonzero(~act)
            g = free[0] if len(free) else int(np.argmax(age/glen))
            act[g] = True; age[g] = -off; glen[g] = L
            if rev: pos[g] = start; inc[g] = -r
            else:   pos[g] = start; inc[g] = r
            ga = 1.0 - rng.uniform(0, 1)*val(P['randamp'], t)
            # level: identical overlapping grains add in amplitude, scattered ones add in power
            decor = min(1.0, 8*spray + 4*abs(val(P['pattern'], t)) + 4*rp + 2*val(P['randrev'], t) + abs(semi))
            comp = max(1.0, rate*size)**(-(1.0 - 0.5*decor))
            th = (pan+1)*np.pi/4
            gl[g] = ga*comp*np.cos(th)*np.sqrt(2); gr[g] = ga*comp*np.sin(th)*np.sqrt(2)
            c = float(np.clip(val(P['contour'], t), -1, 1))
            edge = min(0.45, 0.001*SR/L)
            peakpos[g] = min(max(0.5 + 0.5*c, edge), 1-edge); lin[g] = abs(c)
            step += 1; nspawn += 1
            # next spawn time: jitter around the grid, never accumulating
            grid += SR/rate
            jit = rng.uniform(-0.5, 0.5)*val(P['randrate'], t)*SR/rate + rng.normal()*AGE*0.004*SR   # loose timing, 4 ms rms
            next_spawn = max(next_spawn + 1, grid + jit)
        # render
        ia = np.flatnonzero(act)
        wet = np.zeros((N, 2))
        if len(ia):
            maxact = max(maxact, len(ia))
            k = age[ia][:, None] + ar[None, :]
            Lg = glen[ia][:, None]
            m = (k >= 0) & (k < Lg)
            ph = np.clip(k/Lg, 0, 1)
            pk = peakpos[ia][:, None]
            u = np.where(ph < pk, ph/pk, (1-ph)/(1-pk))
            lg = lin[ia][:, None]
            win = ((1-lg)*(0.5-0.5*np.cos(np.pi*u)) + lg*u)*m
            rpos = (pos[ia][:, None] + inc[ia][:, None]*np.maximum(k, 0)) % Bn
            i0 = np.floor(rpos).astype(int); fr = (rpos - i0)[..., None]
            i1 = (i0 + 1) % Bn
            s = buf[i0]*(1-fr) + buf[i1]*fr           # (G,N,2)
            wet[:, 0] = np.sum(s[..., 0]*win*gl[ia][:, None], axis=0)
            wet[:, 1] = np.sum(s[..., 1]*win*gr[ia][:, None], axis=0)
            age[ia] += N
            act[ia] = age[ia] < glen[ia]
        wetprev = wet
        wm = val(P['wet'], t)
        if P['wet_mode'] == 'mix':
            o = xin*np.cos(wm*np.pi/2) + wet*np.sin(wm*np.pi/2)
        else:
            o = xin + wet*wm
        out[b*N:(b+1)*N] = o
    return out, dict(max_grains=maxact, spawned=nspawn, shortened=shortened)
