import numpy as np, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mosaic_proto import *
HERE = os.path.dirname(os.path.abspath(__file__)); OUT = os.path.join(HERE, 'sketches')
J = '/mnt/project-files/jazz-comping-vcv/sketches/'
house, sr1 = readwav(J + '11-house-stabs.wav'); ballad, sr2 = readwav(J + '08-ballad-solo-piano-with-roots.wav')
assert sr1 == SR and sr2 == SR
BPM = 122.0; beat = 60/BPM; bar = 4*beat
H = house[int(0.3*SR):int((0.3+8*bar)*SR)]            # 8 bars, A minor
Bl = ballad[int(0.3*SR):int((0.3+20)*SR)]             # 20 s solo piano, Eb major, 62 bpm
def tail(x, s=3.0): return np.vstack([x, np.zeros((int(s*SR), 2))])
def norm(y):
    p = np.abs(y).max(); return y*(0.7/p) if p > 0 else y
def lfo(hz, lo, hi, ph=0): return lambda t: lo + (hi-lo)*(0.5 - 0.5*np.cos(2*np.pi*(hz*t + ph)))
def ramp(t0, t1, a, b): return lambda t: a + (b-a)*min(1, max(0, (t-t0)/(t1-t0)))
log = []
def sk(name, x, **kw):
    y, s = process(x, **kw); y = norm(y)
    writewav(os.path.join(OUT, name + '.wav'), y)
    r = 20*np.log10(np.sqrt(np.mean(y**2))); 
    line = f'{name}: {len(y)/SR:.1f} s, rms {r:.1f} dB, max grains {s["max_grains"]}, grains {s["spawned"]}, shortened {s["shortened"]}'
    print(line); log.append(line); return y
G = []
G.append(sk('01-dry-house-chords', H, wet=0.0))
G.append(sk('02-grain-delay-eighths-octave-up', tail(H), scan_mode='delay', scan=1.5*beat, rate=4/beat, size=beat/4*1.5, pitch=12, wet=0.55, feedback=0.35))
G.append(sk('03-stretch-half-speed-cloud', tail(H), scan_mode='stretch', scan=0.5, rate=8/beat, size=beat, spray=0.06, wet=1.0))
G.append(sk('04-pattern-arpeggio-from-chords', tail(H), scan_mode='delay', scan=beat, rate=4/beat, size=beat/4, contour=-0.9, pattern=0.5, scale='pentatonic', wet=0.8))
G.append(sk('05-contour-sweep-pluck-to-swell', tail(H), scan_mode='delay', scan=beat/2, rate=2/beat, size=beat/2, contour=ramp(0, 14, -1, 1), wet=1.0))
G.append(sk('06-lock-then-scan-position', tail(H, 6), scan_mode='position', scan=lfo(0.08, 0.05, 0.95), rate=8/beat, size=beat/2, spray=0.01, lock=lambda t: t > 2*bar, wet=ramp(2*bar-0.2, 2*bar+0.2, 0.0, 1.0)))
G.append(sk('07-ballad-dry', Bl[:int(10*SR)], wet=0.0))
G.append(sk('08-ballad-reverse-and-random-pitch', tail(Bl), scan_mode='delay', scan=0.6, rate=7, size=0.45, randrev=0.6, randpitch_mode='random', randpitch=0.5, scale='major', pitch=0, spray=0.1, wet=0.6))
G.append(sk('09-ballad-feedback-shimmer', tail(Bl, 6), scan_mode='delay', scan=0.5, rate=12, size=0.3, pitch=12, scale='octaves', feedback=0.7, spray=0.05, wet=0.5))
G.append(sk('10-ballad-warp', tail(Bl), scan_mode='delay', scan=1.2, rate=10, size=0.6, spray_mode='warp', spray=0.15, randpitch=0.08, wet=0.75))
G.append(sk('11-fast-grains-pitched-buzz', tail(H), scan_mode='stretch', scan=0.25, rate=ramp(0, 15, 30, 220), size=0.03, contour=-0.5, wet=1.0))
G.append(sk('12-random-rate-size-amp-scatter', tail(H), scan_mode='delay', scan=beat, rate=4/beat, size=beat/2, randrate=1.0, randsize=0.8, randamp=0.8, randrev=0.3, spray=0.2, pitch=-12, wet=0.7))
gap = np.zeros((int(0.8*SR), 2))
writewav(os.path.join(OUT, '00-gallery.wav'), np.vstack([np.vstack([g, gap]) for g in G]))
open(os.path.join(OUT, 'sketch-log.txt'), 'w').write('\n'.join(log) + '\n')
