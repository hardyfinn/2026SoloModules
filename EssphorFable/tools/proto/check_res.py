import numpy as np, sys
sys.path.insert(0,'.')
from res_proto import *
def resp(n=1<<15, **kw):
    x = np.zeros(n); x[0] = 1
    y, f = process(x, **kw)
    return np.fft.rfftfreq(n, 1/SR), 20*np.log10(np.abs(np.fft.rfft(y))+1e-12), f, y
def at(fr, H, f0):
    m = (fr > f0/1.12) & (fr < f0*1.12); return H[m].mean()
print('FLATNESS (DECAY 0, filter open, chromatic)')
fr, H, f, y = resp()
for a, b in ((20, 60), (60, 6000), (6000, 18000)):
    m = (fr > a) & (fr < b); print(f'  {a}-{b} Hz: mean {H[m].mean():+.1f} dB, min {H[m].min():+.1f}, max {H[m].max():+.1f}')
print('BANDS PER SCALE (count, lowest and highest in Hz)')
for s in ('chromatic','major','pentatonic','whole_tone','tetratonic','fifths','octaves'):
    n = band_notes(s); ff = C1*2**(n/12); print(f'  {s}: {len(n)} bands, {ff[0]:.1f} to {ff[-1]:.0f} Hz')
print('FILTER CURVE (DECAY 0, cutoff 1 kHz, resonance 0): level in dB at 125, 250, 500, 1k, 2k, 4k, 8k')
for name, sl in (('low-pass', -1.0), ('band-pass', 0.0), ('high-pass', 1.0)):
    fr, H, f, y = resp(cutoff=1000.0, slope=sl)
    print(f'  {name}: ' + ', '.join(f'{at(fr, H, q):+.1f}' for q in (125, 250, 500, 1000, 2000, 4000, 8000)))
fr, H, f, y = resp(cutoff=1000.0, slope=-1.0, res=0.7)
print('  low-pass, resonance 70%: ' + ', '.join(f'{at(fr, H, q):+.1f}' for q in (125, 250, 500, 1000, 2000, 4000, 8000)))
print('TUNING AND RING TIME (octaves scale, impulse in; band at C4 = 261.6 Hz)')
for dec in (0.25, 0.5, 0.75, 1.0):
    n = int(SR*min(30, 0.05*400**dec*1.6 + 0.5))
    x = np.zeros(n); x[0] = 1
    y, f = process(x, scale='octaves', decay=dec, cutoff=262.0, slope=0.0, res=0.6)
    # isolate C4 by its narrow band-pass gain curve; measure pitch and T60
    Y = np.abs(np.fft.rfft(y*np.hanning(n))); frq = np.fft.rfftfreq(n, 1/SR)
    m = (frq > 200) & (frq < 330); pk = frq[m][np.argmax(Y[m])]
    env = np.sqrt(np.convolve(y**2, np.ones(441)/441, 'same')); e0 = env[int(0.02*SR)]
    below = np.flatnonzero(env[int(0.02*SR):] < e0*10**(-30/20))
    t60 = 2*below[0]/SR if len(below) else float('nan')
    print(f'  DECAY {dec:.2f}: design T60 {0.05*400**dec:.2f} s, measured {t60:.2f} s, peak at {pk:.1f} Hz')
