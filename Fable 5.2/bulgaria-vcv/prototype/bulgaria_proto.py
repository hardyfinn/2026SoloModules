"""Bulgaria harmoniser prototype (design sketch, not the module code).
Control rate 1 kHz rules engine -> per-voice pitch (cents), gate, push curves.
A plain saw synth renders the result so the rub can be heard."""
import numpy as np, wave, sys, os
SR = 48000; CR = 1000
rng = np.random.default_rng(7)

# mode: (12-TET degrees, village-tuned degrees) in cents
MODES = {
 'SHOPE':    ([0,200,300,500,700,900,1000], [0,170,310,500,700,870,1010]),
 'AEOLIAN':  ([0,200,300,500,700,800,1000], [0,190,305,500,700,810,1000]),
 'PHRYGIAN': ([0,100,300,500,700,800,1000], [0,125,305,500,700,815,1000]),
 'HIJAZ':    ([0,100,400,500,700,800,1000], [0,130,385,500,700,810,1000]),
}
def semi_to_deg(semi, mode):
    tet = MODES[mode][0]
    o, s = divmod(semi, 12)
    d = int(np.argmin([abs(s*100 - c) for c in tet]))
    return o*7 + d
def deg_to_cents(deg, mode, tune):
    tet, vil = MODES[mode]
    o, d = divmod(deg, 7)
    return o*1200 + tet[d] + tune*(vil[d]-tet[d])

def harmonise(notes, total, P):
    """notes: list of (start_s, dur_s, semitone_rel_root, cadence_bool).
    Returns pitch[v] (cents rel root), gate[v], push[v] at CR, and lead-dry pitch."""
    n = int(total*CR); NV = P['voices']
    tgt = np.full((NV, n), np.nan); gate = np.zeros((NV, n)); push = np.zeros((NV, n))
    orn = np.zeros((NV, n)); rubflag = np.zeros((NV, n))
    dry = np.full(n, np.nan); dgate = np.zeros(n)
    mode, tune = P['mode'], P['tune']
    stag = rng.uniform(0, 0.035*P['loose'], NV); stag[0] = 0
    for (t0, dur, semi, cad) in notes:
        L = semi_to_deg(semi, mode)
        a, b = int(t0*CR), int((t0+dur)*CR)
        dry[a:b] = semi*100; dgate[a:b] = 1
        lead = deg_to_cents(L, mode, tune)
        use_rub = True if P['rule'] else (rng.random() < P['chance'])
        # --- tradition rules, in scale degrees ---
        D = -1 if (L == 0 and not cad) else 0           # drone dodges to keep the second
        if not use_rub:  R = ('deg', L-2 if L >= 2 else 0)
        elif L >= 2:     R = ('rub', -1)                 # a second under the lead
        elif L == 1:     R = ('uni', 0)                  # lead already rubs the drone: double the drone
        elif L == 0:     R = ('uni', D)
        else:            R = ('rub', +1)
        plan = [('lead', None), R, ('deg', D), ('deg', -7), ('rub', +1), ('deg', 4), ('rub2', -2), ('deg', 7)][:NV]
        seg = np.arange(b-a)/CR
        lean = 1-np.exp(-seg/0.12)                       # rub leans in after the onset
        for v, (kind, x) in enumerate(plan):
            va, vb = a+int(stag[v]*CR), b
            if kind == 'lead': c = np.full(b-a, lead)
            elif kind == 'deg': c = np.full(b-a, deg_to_cents(x, mode, tune))
            elif kind == 'uni':
                c = np.full(b-a, deg_to_cents(x, mode, tune)) + (6 if P['rubmode']=='cents' else 0)
                if P['rubmode']=='hz':
                    f = P['root']*2**(c/1200); c = 1200*np.log2((f+2.0)/P['root'])
            else:
                k = abs(x); sgn = np.sign(x)
                scale_iv = deg_to_cents(L+x, mode, tune) - lead
                w = P['width']*(0.6 if (cad and P['cadence']=='tight') else 1.0)
                if P['rubmode'] == 'cents':
                    iv = scale_iv + lean*(sgn*k*w - scale_iv)
                else:
                    fl = P['root']*2**(lead/1200)
                    hz = 1200*np.log2(np.maximum(fl + sgn*k*P['beat'], 20)/fl)
                    iv = scale_iv + lean*(hz - scale_iv)
                if cad and P['cadence'] == 'resolve':
                    iv = iv*np.clip(1-(seg-0.35)/0.3, 0, 1)   # close to unison
                c = lead + iv
                rubflag[v, a:b] = 1
            tgt[v, a:b] = c
            gate[v, va:vb] = 1
            base = 0.55 + 0.45*lean if kind in ('rub','rub2') else (0.75 if kind=='lead' else 0.6)
            if kind == 'lead' and R[0]=='rub': base = 0.7+0.3*lean
            push[v, a:b] = base[:b-a] if hasattr(base,'__len__') else base
        # --- ornaments (lead) ---
        if P['slide'] > 0 and rng.random() < P['slide']:
            orn[0, a:b] += -160*np.exp(-seg/0.045)
            for v in range(1, NV):
                if rubflag[v, a]: orn[v, a:b] += -90*np.exp(-seg/0.07)
        if P['shake'] > 0 and dur > 0.45 and rng.random() < P['shake']:
            for k in range(3):
                s = a + int((0.30 + k*0.09)*CR); e = min(s+28, b)
                if e > s: 
                    orn[0, s:e] += 190*np.sin(np.pi*np.arange(e-s)/(e-s))**2
                    push[0, s:e] *= 0.55
        if cad and P['whoop'] > 0 and rng.random() < P['whoop']:
            wl = int(0.17*CR); e = min(b+wl, n); x = np.arange(e-b)/wl
            for v in ([0] if NV < 5 else [0, 4]):
                tgt[v, b:e] = tgt[v, b-1]; gate[v, b:e] = 1
                orn[v, b:e] += (1000 + 150*v)*x**1.6
                push[v, b:e] = 1.0*(1-x)**0.7
    # hold last target, smooth (glide), add looseness drift
    pitch = np.zeros((NV, n))
    for v in range(NV):
        last = 0.0; y = 0.0; g = np.exp(-1/(CR*(0.012 if v == 0 else 0.022)))
        drift = np.zeros(n)
        if P['loose'] > 0:
            w = rng.standard_normal(n); d = 0
            for i in range(n):
                d = 0.9985*d + w[i]; drift[i] = d
            drift *= 9*P['loose']/ (np.std(drift)+1e-9)
        first = True
        for i in range(n):
            if not np.isnan(tgt[v, i]):
                last = tgt[v, i]
                if first: y = last; first = False
            y = last + g*(y-last)
            pitch[v, i] = y + orn[v, i] + drift[i]
    return pitch, gate, push, dry, dgate

def up(x): return np.interp(np.arange(len(x)*SR//CR)/SR, np.arange(len(x))/CR, x)
def saw(f):
    dt = f/SR; p = np.cumsum(dt) % 1.0; s = 2*p-1
    m = p < dt; t = p[m]/dt[m]; s[m] -= (t+t-t*t-1)
    m = p > 1-dt; t = (p[m]-1)/dt[m]; s[m] -= (t*t+t+t+1)
    return s
def shape(x, kind):
    X = np.fft.rfft(x); f = np.fft.rfftfreq(len(x), 1/SR)
    if kind == 'dull': H = 1/(1+(f/900)**2)
    else:
        H = 1/(1+(f/7000)**4)
        for fc, q, gdb in ((1150, 4, 9), (2900, 5, 12)):
            H = H*(1 + (10**(gdb/20)-1)/(1+(q*(f/fc - fc/np.maximum(f,1)))**2))
    return np.fft.irfft(X*H, len(x))
def env(g, a=0.012, r=0.07):
    y = np.zeros(len(g)); e = 0; ka = np.exp(-1/(CR*a)); kr = np.exp(-1/(CR*r))
    for i in range(len(g)):
        k = ka if g[i] > e else kr; e = g[i] + k*(e-g[i]); y[i] = e
    return y
PAN = [0.5, 0.3, 0.7, 0.5, 0.15, 0.85, 0.4, 0.6]
def render(pitch, gate, push, root):
    NV, n = pitch.shape; N = n*SR//CR; out = np.zeros((2, N))
    for v in range(NV):
        s = saw(root*2**(up(pitch[v])/1200))
        pu = up(env(push[v]*gate[v], 0.03, 0.06)); a = up(env(gate[v]))
        y = (shape(s, 'dull')*(1-pu)*1.2 + shape(s, 'bright')*pu*0.5)*a*(0.35+0.65*pu)
        out[0] += y*np.cos(PAN[v]*np.pi/2); out[1] += y*np.sin(PAN[v]*np.pi/2)
    return out
def reverb(x, mix=0.16, t60=1.6):
    L = int(SR*t60); r = np.random.default_rng(3)
    out = np.zeros((2, x.shape[1]+L))
    for c in range(2):
        ir = r.standard_normal(L)*np.exp(-6.9*np.arange(L)/L); ir[:int(.012*SR)] = 0
        nfft = 1 << int(np.ceil(np.log2(x.shape[1]+L)))
        wet = np.fft.irfft(np.fft.rfft(x[c], nfft)*np.fft.rfft(ir, nfft), nfft)[:x.shape[1]+L]
        out[c, :x.shape[1]] += x[c]; out[c] += mix*wet/np.sqrt(L)*8
    return out
def write(path, x):
    x = x/np.max(np.abs(x))*0.89
    d = (x.T*32767).astype('<i2')
    with wave.open(path, 'wb') as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR); w.writeframes(d.tobytes())
BASE = dict(voices=3, mode='SHOPE', tune=1.0, rule=True, chance=0.5, rubmode='cents', width=170, beat=30,
            cadence='tight', slide=0.0, shake=0.0, whoop=0.0, loose=0.3, root=329.63)
def dry_params(P): 
    Q = dict(P); Q.update(voices=1, tune=0.0, slide=0, shake=0, whoop=0, loose=0); return Q
