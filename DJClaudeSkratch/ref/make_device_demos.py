"""Demos rendered by the device itself (ref/sim.js: the real planner and gen~ core), not by the
reference engine.   python3 ref/make_device_demos.py OUTDIR [record.wav cue cue ...]
Each word is held for two bars from its key at 96 BPM, then two Instinct runs of 8 bars."""
import json, os, subprocess, sys, wave, numpy as np
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
from sounds import beat
OUT = sys.argv[1]; os.makedirs(OUT, exist_ok=True)
REC = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, 'out', 'builtin.wav')
CUES = [float(c) for c in sys.argv[3:]] or None
WORDS = ['baby', 'forward', 'stab', 'chirp', 'flare1', 'flare2', 'crab', 'tear', 'scribble', 'transform', 'drag', 'tip', 'lazer', 'release', 'echo', 'reverse']
BPM = 96.0
def rd(p):
    w = wave.open(p); return np.frombuffer(w.readframes(w.getnframes()), '<i2').reshape(-1, 2).mean(1) / 32767
def wr(p, x):
    x = x / max(np.abs(x).max(), 1e-9) * 0.89
    w = wave.open(p, 'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(44100); w.writeframes((x * 32767).astype('<i2').tobytes()); w.close()
def render(name, script):
    script = dict({'record': REC, 'tempo': BPM, 'hands': 50, 'seed': 4}, **script)
    if CUES: script['cues'] = CUES
    js = os.path.join(OUT, '_s.json'); json.dump(script, open(js, 'w'))
    tmp = os.path.join(OUT, '_t.wav')
    subprocess.run(['node', os.path.join(HERE, 'sim.js'), js, tmp], check=True, capture_output=True)
    y = rd(tmp); os.remove(tmp); os.remove(js)
    bt = beat(BPM, 4 * int(np.ceil(script['beats'] / 4)) + 4, 0.58)[:len(y)]
    wr(os.path.join(OUT, name + '.wav'), 0.42 * bt / np.abs(bt).max() + 0.75 * y / max(np.abs(y).max(), 1e-9))
    return y
gal = []
for i, w in enumerate(WORDS):
    # hold from just before beat 1 to just before beat 9 (release: let go there, so it plays two bars)
    y = render('%02d-%s' % (i + 1, w), {'beats': 10, 'ctl': {'speed': 0 if w in ('flare1', 'flare2', 'crab', 'drag', 'transform') else 1},
                                         'events': [{'beat': 0.9, 'note': 36 + i, 'vel': 100}, {'beat': 8.6, 'note': 36 + i, 'vel': 0}]})
    gal.append(os.path.join(OUT, '%02d-%s.wav' % (i + 1, w)))
render('20-instinct-defaults', {'beats': 33, 'ctl': {'mode': 2}, 'seed': 5})
render('21-instinct-dense-walking', {'beats': 33, 'ctl': {'mode': 2, 'density': 85, 'skill': 100, 'walk': 80}, 'seed': 8})
render('22-routine-default', {'beats': 17, 'ctl': {'mode': 1}})
g = np.concatenate([np.concatenate([rd2[: int(44100 * 6.5)], np.zeros(22050)]) for rd2 in
                    [np.frombuffer(wave.open(p).readframes(10 ** 8), '<i2') / 32767 for p in gal]])
wr(os.path.join(OUT, '00-gallery-16-words.wav'), g)
print('demos in', OUT)
