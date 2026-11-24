import numpy as np, sys
sys.path.insert(0, '.')
from mosaic_proto import *
t = np.arange(SR)/SR
burst = np.repeat((0.25*np.sin(2*np.pi*330*t)*np.hanning(SR))[:, None], 2, 1)
x = np.vstack([burst, np.zeros((11*SR, 2))])
r = lambda v: 20*np.log10(np.sqrt(np.mean(v**2))+1e-12)
print('FEEDBACK, GRAIN mode (1 s tone burst at -12 dB peak, then silence). Output level in dB at 1-2 s, 5-6 s, 11-12 s, and overall peak. WET 1.')
for label, kw in (('coherent, overlap 4', dict(rate=16, size=0.25, scan=0.25)), ('sprayed, overlap 4', dict(rate=16, size=0.25, scan=0.25, spray=0.2)), ('sparse, overlap 1', dict(rate=4, size=0.25, scan=0.5)), ('dense sprayed, overlap 16', dict(rate=40, size=0.4, scan=0.5, spray=0.3)), ('octave up, overlap 4', dict(rate=16, size=0.25, scan=0.25, pitch=12))):
    for F in (0.5, 0.8, 1.0):
        y, s = process(x, feedback=F, **kw)
        print(f'  {label}, FEEDBACK {F:.1f}: {r(y[SR:2*SR]):6.1f}, {r(y[5*SR:6*SR]):6.1f}, {r(y[11*SR:]):6.1f}, peak {20*np.log10(np.abs(y).max()):.1f}')
