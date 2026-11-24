import numpy as np, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mosaic_proto as M, res_proto as R
SR = 44100
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'sketches')
J = '/mnt/project-files/jazz-comping-vcv/sketches/'
house, _ = M.readwav(J + '11-house-stabs.wav'); ballad, _ = M.readwav(J + '08-ballad-solo-piano-with-roots.wav')
beat = 60/122.0; bar = 4*beat
H = house[int(0.3*SR):int((0.3+4*bar)*SR)]; Bl = ballad[int(0.3*SR):int(12.3*SR)]
def tail(x, s=3.0): return np.vstack([x, np.zeros((int(s*SR), 2))])
def norm(y): return y*(0.7/np.abs(y).max())
log = []
def save(name, y):
    y = norm(y); M.writewav(os.path.join(OUT, name + '.wav'), y)
    line = f'{name}: {len(y)/SR:.1f} s, rms {20*np.log10(np.sqrt(np.mean(y**2))):.1f} dB'; print(line, flush=True); log.append(line)
def res(x, **kw):
    return np.stack([R.process(x[:, c], **kw)[0] for c in (0, 1)], 1)
# AGE on the grain module
for age in (0.0, 0.5, 1.0):
    y, s = M.process(tail(H), scan_mode='delay', scan=beat, rate=4/beat, size=beat/2, pitch=-12, spray=0.03, feedback=0.5, wet=0.8, age=age)
    save(f'13-age-{int(age*100):03d}-grains-octave-down', y)
# resonator alone
save('14-resonator-pentatonic-ring-on-house', res(tail(H, 4), scale='pentatonic', pitch=9, decay=0.7, wet=0.6))
save('15-resonator-fifths-long-ring-on-piano', res(tail(Bl, 6), scale='fifths', pitch=3, decay=0.85, wet=0.5))
save('16-resonator-as-filter-lowpass-resonant', res(H, cutoff=700.0, res=0.6, slope=-1.0, decay=0.0))
save('17-resonator-band-pass-chromatic-ring', res(tail(H, 3), cutoff=900.0, res=0.3, slope=0.0, decay=0.55))
# grains into resonator
y, s = M.process(tail(Bl, 2), scan_mode='stretch', scan=0.5, rate=9, size=0.5, spray=0.08, randrev=0.3, wet=1.0)
save('18-grains-into-resonator-major-on-piano', res(tail(y, 4), scale='major', pitch=3, decay=0.75, wet=0.55))
open(os.path.join(OUT, 'sketch-log-2.txt'), 'w').write('\n'.join(log) + '\n')
