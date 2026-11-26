"""Rider demos from the reference engines: the drummer with the Comper + Walker trio stems (tools/make_walker_demos.py writes those).
   STEMS=/tmp/walker-demos/stems python3 tools/make_rider_demos.py OUTDIR [name filter]"""
import os, sys, json
import numpy as np, soundfile as sf
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, os.path.join(HERE, 'proto'))
import rider_ref as R, rider_kit_ref as KR, rider_presets as RPS
from make_rider_reference import preset_bars
STEMS = os.environ.get('STEMS', '/tmp/walker-demos/stems'); OUT = sys.argv[1] if len(sys.argv) > 1 else '/tmp/rider-demos'; os.makedirs(OUT, exist_ok=True)
only = sys.argv[2] if len(sys.argv) > 2 else None
def rms(x): return float(np.sqrt(np.mean(np.square(x)))) + 1e-12
def drums(name, player, kit, nbars=16, alone=False, energy=None, seed=11, level=0.6):
    bars, bpm, chorus, c = preset_bars(name, nbars); kw = dict(player); kw['swing'] = c.get('swing', 0.0)
    notes, d = R.play(None, bars, bpm, chorus, seed, R.RP(**kw), energy); spb = 60.0 / bpm; pre = 0.5 * spb
    total = nbars * 4 * spb + pre + 3.0
    y = KR.play_notes([(t + pre, v, vel, dur, b) for t, v, vel, dur, b in notes], total, KR.KitParams(**kit), seed)
    return y, total, d
def mix(file, name, y, total, level=0.6):
    piano = sf.read(os.path.join(STEMS, 'trio-%s.piano.wav' % name))[0]; bass = sf.read(os.path.join(STEMS, 'trio-%s.bass.wav' % name))[0]
    n = len(y); m = np.zeros((n, 2)); k = min(n, len(piano)); m[:k] += piano[:k]; m[:k] += (bass[:k] * rms(piano) / rms(bass))[:, None]
    m += y * level * rms(piano) / rms(y); sf.write(os.path.join(OUT, file + '.wav'), np.clip(m * 0.89 / np.abs(m).max(), -1, 1), KR.SR, subtype='PCM_16')
for name, player, kit in RPS.PRESETS:
    if only and only not in name: continue
    y, total, d = drums(name, player, kit)
    mix('trio-' + name, name, y, total, 0.45 if player.get('feel') == R.BALLAD else 0.6)
    sf.write(os.path.join(OUT, 'drums-' + name + '.wav'), np.clip(y * 0.8 / np.abs(y).max(), -1, 1), KR.SR, subtype='PCM_16')
    print('%-24s %4.1f s  %s' % (name, total, d.stats)); sys.stdout.flush()
if not only:
    M = '01_Medium Swing'
    for tag, e in (('energy-0.1', 0.1), ('energy-0.9', 0.9)):
        y, total, d = drums(M, dict(feel=R.SWING, energy=e), {}); mix(tag, M, y, total)
    y, total, d = drums('07_Blues Shells', dict(feel=R.SWING, fills=0.7), {}, energy=lambda b: 0.2 + 0.7 * b / 15.0, seed=4); mix('building-from-quiet-to-loud', '07_Blues Shells', y, total)
    y, total, d = drums(M, dict(feel=R.SWING, listen=0.0), {}); mix('not-listening', M, y, total)
    for tag, kit in (('age-full', dict(age=1.0)), ('tune-low-decay-short', dict(tune=0.15, decay=0.15)), ('bright-full-decay-long', dict(bright=1.0, decay=1.0))):
        y, total, d = drums(M, dict(feel=R.SWING), kit, nbars=8); sf.write(os.path.join(OUT, 'kit-' + tag + '.wav'), np.clip(y * 0.8 / np.abs(y).max(), -1, 1), KR.SR, subtype='PCM_16')
