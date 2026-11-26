"""Writes test/rider_kit_cases.json: strokes to render through the kit, with the reference measurement of each.
The measurement: each output channel through the kit's own 26 band filters, then the energy (sum of squares) in each time window, in dB.
The C++ kit, given the same strokes, seed and controls, must land on the same numbers (see docs/RIDER-BUILD-INSTRUCTIONS.md, tests).
   python3 tools/proto/make_rider_kit_reference.py"""
import os, sys, json, math
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import numpy as np
from scipy.signal import sosfilt
import rider_ref as R, rider_kit_ref as K, rider_presets as RPS
from make_rider_reference import preset_bars
TEST = os.path.join(HERE, '..', '..', 'test')
TW = [(0.0, 0.006), (0.006, 0.02), (0.02, 0.06), (0.06, 0.15), (0.15, 0.4), (0.4, 1.0), (1.0, 2.0), (2.0, 3.0)]
def measure(y, wins):
    out = []
    for c in range(2):
        rows = []
        for i in range(K.NB):
            b = sosfilt(K.SOS[i], y[:, c]) ** 2
            rows.append([round(10 * math.log10(float(np.sum(b[int(a * K.SR):int(z * K.SR)])) + 1e-16), 3) for a, z in wins])
        out.append(rows)
    return out
cases = []
def add(name, strokes, sweeps, kit, dur, wins, seed=1):
    k = K.Kit(K.KitParams(**kit), seed)
    for t, v, vel in strokes: k.stroke(t, v, vel)
    for t, d, vel in sweeps: k.sweep(t, d, vel)
    y = k.render(dur)
    cases.append(dict(name=name, seed=seed, dur=dur, kit=kit, strokes=[[round(t, 7), v, vel] for t, v, vel in strokes], sweeps=[[round(t, 7), round(d, 7), vel] for t, d, vel in sweeps],
                      wins=[list(w) for w in wins], cells=measure(y, wins), peak=round(float(np.abs(y).max()), 6)))
    print('%-46s peak %.4f' % (name, cases[-1]['peak'])); sys.stdout.flush()
frames = lambda dur, step=0.1: [(round(i * step, 3), round((i + 1) * step, 3)) for i in range(int(dur / step))]
for v in range(14):
    if v == K.BR_SWEEP: continue
    for vel in (30, 60, 96, 124): add('%s vel %d' % (K.VOICES[v], vel), [(0.0, v, vel)], [], {}, 3.0, TW, seed=2)
add('ride: eight strokes building the wash', [(0.25 * i, K.RIDE, 70 + 6 * (i % 3)) for i in range(8)], [], {}, 4.0, frames(4.0))
add('hi-hat: open, then choked by the foot', [(0.0, K.HATO, 100), (0.3, K.HATF, 80)], [], {}, 1.5, frames(1.5, 0.05))
add('hi-hat: open, left to ring', [(0.0, K.HATO, 100)], [], {}, 1.5, frames(1.5, 0.05))
add('brush sweeps', [], [(0.1, 0.9, 50), (1.05, 0.9, 62), (2.0, 0.9, 40)], {}, 3.2, frames(3.2))
# two bars of the first preset, with every kit control at its ends
name, player, kit = RPS.PRESETS[0]
bars, bpm, chorus, c = preset_bars(name, 4); notes, d = R.play(None, bars[2:4], bpm, chorus, 11, R.RP(**player)); spb = 60.0 / bpm
ph = [(max(0.0, t + 0.1), v, vel) for t, v, vel, dur, b in notes]
for tag, kp in (('as set', {}), ('tune 0', dict(tune=0.0)), ('tune 1', dict(tune=1.0)), ('decay 0', dict(decay=0.0)), ('decay 1', dict(decay=1.0)), ('bright 0', dict(bright=0.0)), ('bright 1', dict(bright=1.0)),
                ('width 0', dict(width=0.0)), ('age 0.5', dict(age=0.5)), ('age 1', dict(age=1.0)), ('level 0.5', dict(level=0.5)), ('mix', dict(mix=[0.5, 1.5, 0.7, 1.2, 0.0]))):
    add('two bars of swing, %s' % tag, ph, [], kp, 8 * spb + 1.0, frames(8 * spb + 1.0), seed=5)
bars, bpm, chorus, c = preset_bars('03_Ballad', 2); notes, d = R.play(None, bars, bpm, chorus, 11, R.RP(feel=R.BALLAD)); spb = 60.0 / bpm
add('one bar of ballad with brushes', [(t + 0.1, v, vel) for t, v, vel, dur, b in notes if v != K.BR_SWEEP and b < 4 and t > -0.1], [(t + 0.1, dur, vel) for t, v, vel, dur, b in notes if v == K.BR_SWEEP and b < 4], {}, 4 * spb + 0.6, frames(4 * spb + 0.6))
json.dump(cases, open(os.path.join(TEST, 'rider_kit_cases.json'), 'w'), separators=(',', ':'))
print(len(cases), 'kit cases')
