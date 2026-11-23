"""Check 1 of the build spec: with machine hands, every word played from its key in the device
(ref/sim.js running the real planner and core) moves the record and the fader as the reference
engine does (scratch_engine.py, hands = 0).   python3 ref/check_words.py"""
import json, os, subprocess, sys, numpy as np
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
from scratch_engine import Scratch, SR, write_wav
from sounds import record
OUT = os.path.join(HERE, 'out'); os.makedirs(OUT, exist_ok=True)
BPM, U = 96.0, 0.25
rec, cues = record()
write_wav(os.path.join(OUT, 'builtin.wav'), rec, peak=abs(rec).max())
KEYS = ['baby', 'forward', 'stab', 'chirp', 'flare1', 'flare2', 'crab', 'tear', 'scribble', 'transform', 'drag', 'tip', 'lazer', 'release', 'echo', 'reverse']
u = U
CALL = {
    'baby': lambda s, b: s.baby(b, u), 'forward': lambda s, b: s.forward(b, u, u), 'stab': lambda s, b: s.stab(b, u / 2, u / 2),
    'chirp': lambda s, b: s.chirp(b, u), 'flare1': lambda s, b: s.flare(b, 2 * u, 1, 1), 'flare2': lambda s, b: s.flare(b, 2 * u, 1, 2),
    'crab': lambda s, b: s.crab(b, 2 * u), 'tear': lambda s, b: s.tear(b, u), 'scribble': lambda s, b: s.scribble(b, 2 * u),
    'transform': lambda s, b: s.transform(b, 8 * u, u, pat=(1, 1, 0, 1)), 'drag': lambda s, b: s.drag(b, 2 * u),
    'tip': lambda s, b: s.tip(b, u / 2, 2), 'lazer': lambda s, b: s.lazer(b, u / 2, 1.5 * u),
    'release': lambda s, b: s.release(b, 1.25, 0.25), 'echo': lambda s, b: s.forward(b, 1.1 * u, 0.9 * u), 'reverse': lambda s, b: s.reverse(b + u, u, u)}
ok_all = True
print('%-10s %10s %10s %8s  %s' % ('word', 'pos err ms', 'edge err ms', 'edges', 'result'))
for i, w in enumerate(KEYS):
    s = Scratch(rec, BPM, 6, hands=0.0, seed=1, cue=cues['ah'], swing=0.58)
    CALL[w](s, 1.0)
    y, p, v, g = s.render()
    # device: press just before beat 1, let go after one unit (release: hold for 1 beat)
    off = 2.2 if w == 'release' else 1.05
    scr = {'record': os.path.join(OUT, 'builtin.wav'), 'cues': [cues['ah']], 'tempo': BPM, 'beats': 6, 'hands': 0,
           'events': [{'beat': 0.9, 'note': 36 + i, 'vel': 127 * 0.5 / 0.4 - 127 * 0.8 / 0.4 + 127 * 0.5 / 0.4}, {'beat': off, 'note': 36 + i, 'vel': 0}],
           'dump': os.path.join(OUT, 'dev_' + w)}
    scr['events'][0]['vel'] = 63.5            # travel x 1.0 at this velocity (0.8 + 0.4 * vel / 127)
    json.dump(scr, open(os.path.join(OUT, 'w.json'), 'w'))
    subprocess.run(['node', os.path.join(HERE, 'sim.js'), os.path.join(OUT, 'w.json'), os.path.join(OUT, 'dev_%s.wav' % w)], check=True, capture_output=True)
    dp = np.fromfile(os.path.join(OUT, 'dev_%s.pos.f32' % w), np.float32).astype(float)
    dg = np.fromfile(os.path.join(OUT, 'dev_%s.g.f32' % w), np.float32).astype(float)
    n = min(len(dp), len(p)); a = int(0.5 * 60 / BPM * SR)
    perr = np.abs(dp[a:n] - p[a:n]).max() * 1000
    e1 = np.flatnonzero(np.diff((g[:n] > 0.5).astype(int))); e2 = np.flatnonzero(np.diff((dg[:n] > 0.5).astype(int)))
    if len(e1) == len(e2): eerr = (np.abs(e1 - e2).max() / SR * 1000) if len(e1) else 0.0
    else: eerr = float('nan')
    good = perr < 1.0 and len(e1) == len(e2) and eerr < 1.0
    ok_all &= good
    print('%-10s %10.3f %10.3f %4d/%-3d  %s' % (w, perr, eerr, len(e2), len(e1), 'ok' if good else 'DIFFERENT'))
print('ALL OK' if ok_all else 'SOME DIFFER')
