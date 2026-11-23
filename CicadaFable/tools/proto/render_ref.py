import sys, numpy as np, soundfile as sf, os
from presets import *
out = sys.argv[1] if len(sys.argv) > 1 else 'out'; os.makedirs(out, exist_ok=True)
DUR = {'Dog-Day Cicada': 20, 'Greengrocer': 22, 'Meadow Katydid': 15, 'Pharaoh Cicada': 14, 'Min-Min Cicada': 16}
for i, (name, p) in enumerate(PRESETS.items()):
    if name in ('Cricket Hats', 'Tymbal Bass', 'Leaf Shaker'): continue
    y, pu, ch = render(p, DUR.get(name, 12), seed=i+1)
    act = np.abs(y).max(1) > 0.02*np.abs(y).max()
    print("%-18s peak %5.2f V  rms(active) %5.2f V  pulses/s %6.1f" % (name, np.abs(y).max(), np.sqrt(np.mean(y[act]**2)), pu.sum()/len(pu)*48000))
    sf.write('%s/%02d-%s.wav' % (out, i+1, name.lower().replace(' ', '-')), y/10.0, 48000, subtype='PCM_16')

# ---- patched demos: what the jacks do ---------------------------------------------------------------------
SR = 48000; BPM = 124; s16 = 60/BPM/4
def kick(y, t):
    n = int(0.25*SR); tt = np.arange(n)/SR; i = int(t*SR)
    k = np.sin(2*np.pi*(48*tt+(160/30)*(1-np.exp(-30*tt))))*np.exp(-tt/0.09)*7.0
    if i+n < len(y): y[i:i+n] += k[:, None]
def pulses(n, times, width=0.002):
    g = np.zeros(n)
    for t in times: g[int(t*SR):int((t+width)*SR)] = 10.0
    return g
def save(name, y): sf.write('%s/%s.wav' % (out, name), np.clip(y/10.0, -1, 1), SR, subtype='PCM_16'); print(name)

# TRIG: cricket hats on 16ths, CHIRP sequenced by CV (1, 2 or 3 pulses a step), PITCH dropped on the off-beat "open hat"
bars = 4; n = int((bars*16*s16+0.5)*SR); tr = []; ch = np.zeros(n); pk = np.zeros(n); pu = np.zeros(n)
pat = [1, 0, 2, 1, 1, 0, 3, 0, 1, 1, 2, 0, 1, 0, 4, 2]
p = PRESETS['Cricket Hats']
for b in range(bars):
    for i in range(16):
        if not pat[i]: continue
        t = 0.05+(b*16+i)*s16+(0.012 if i % 2 else 0); tr.append(t); a = int((t-0.01)*SR); e = int((t+s16)*SR)
        ch[a:e] = k_chirp(pat[i]); pk[a:e] = p['PITCH']-(0.07 if i % 4 == 2 else 0); pu[a:e] = p['PURE']-(0.25 if b >= 2 else 0)
y, _, _ = render(p, n/SR, auto=dict(TRIG=pulses(n, tr), CHIRP=ch, PITCH=pk, PURE=pu), flags=(0, 1, 0)); y = y*0.6
for b in range(bars*4): kick(y, 0.05+b*4*s16)
save('21-patched-cricket-hats-trig', y)

# V/OCT to RATE: the tymbal as a formant bass.  An envelope into PITCH sweeps the formant, an outside VCA shapes the notes.
notes = [33, 33, 45, 33, 36, 33, 33, 43, 33, 45, 33, 31, 33, 33, 48, 36]; gate = [1, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1]; slide = [0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1]
n = int((bars*16*s16+0.6)*SR); vo = np.zeros(n); pc = np.zeros(n); vca = np.zeros(n); pur = np.zeros(n); t_ = np.arange(n)/SR
p = PRESETS['Tymbal Bass']
for b in range(bars):
    for i in range(16):
        t0 = 0.05+(b*16+i)*s16; a = int(t0*SR); e = int((t0+s16)*SR); x = (t_[a:e]-t0)/s16
        v0 = (notes[i]-60)/12; v1 = (notes[(i+1) % 16]-60)/12 if slide[i] else v0
        vo[a:e] = v0+(v1-v0)*np.clip(x*2-1, 0, 1)
        if gate[i]:
            Lg = 1.0 if slide[i] else 0.55
            vca[a:e] = np.exp(-x*1.2)*(x < Lg)
            sweep = 350+(500+700*b)*(1.0 if i % 4 == 2 else 0.6)
            pc[a:e] = k_pitch(300+sweep*np.exp(-x*3.0))
        else: pc[a:e] = k_pitch(300)
        pur[a:e] = k_q(6+2*b)
vo[:int(0.05*SR)] = (notes[0]-60)/12; pc[pc == 0] = k_pitch(300)
y, _, _ = render(p, n/SR, auto=dict(VOCT=vo, PITCH=pc, PURE=pur, TRACK=np.ones(n), RATE=np.full(n, 0.5)), flags=(0, 0, 0))
from scipy import signal
vca = signal.sosfilt(signal.butter(1, 400, fs=SR, output='sos'), vca); y = y*vca[:, None]*1.6
for b in range(bars*4): kick(y, 0.05+b*4*s16)
save('22-patched-tymbal-bass-voct-to-rate', y)

# CLOCK: leaf shaker, one chirp per 8th note, then the song gesture SWELL shapes eight bars of it
n = int((8*16*s16)*SR); ck = pulses(n, [0.05+k*2*s16 for k in range(8*8)])
p = dict(PRESETS['Leaf Shaker']); p['SONG'] = SWELL; p['LENGTH'] = k_length(8.0)
y, _, _ = render(p, n/SR, auto=dict(CLOCK=ck), flags=(0, 0, 1)); y = y*0.7
for b in range(8*4): kick(y, 0.05+b*4*s16)
save('23-patched-leaf-shaker-clock', y)

# GATE: the pharaoh phrase held by a gate, three different lengths
n = 12*SR; g = np.zeros(n)
for a, e in ((0.3, 1.0), (2.5, 6.0), (8.0, 8.4)): g[int(a*SR):int(e*SR)] = 10
y, _, _ = render(PRESETS['Pharaoh Cicada'], 12, auto=dict(GATE=g), flags=(1, 0, 0)); save('24-patched-gate-held-phrases', y)

# TEMP swept from a cool night to a warm one on the tree cricket
n = 14*SR; y, _, _ = render(PRESETS['Tree Cricket'], 14, auto=dict(TEMP=np.interp(np.arange(n)/SR, [0, 3, 12, 14], [0.2, 0.2, 0.9, 0.9])))
save('25-temp-cool-to-warm', y)

# OPEN played by hand (a slow LFO) on the greengrocer held at full song
n = 12*SR; t_ = np.arange(n)/SR; p = dict(PRESETS['Greengrocer']); p['SONG'] = STEADY; p['BODY'] = 0.9
y, _, _ = render(p, 12, auto=dict(OPEN=0.45+0.3*np.sin(2*np.pi*0.9*t_)*np.sin(2*np.pi*0.13*t_)))
save('26-open-wah-by-cv', y)

# poly: six individuals each, a chorus without a chorus knob
y, _, _ = render(PRESETS['Field Cricket'], 14, voices=6); save('27-chorus-six-field-crickets', y*0.6)
y, _, _ = render(PRESETS['Evening Cicada'], 16, voices=5); save('28-chorus-five-evening-cicadas', y*0.6)
