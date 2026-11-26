"""Writes presets/Comper/*.vcvm from presets.py. Run from anywhere: python3 tools/proto/export_presets.py
Every preset uses seed 11, the seed of the golden preset cases, so a reset plays the take in test/out."""
import os, json, sys
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.join(HERE, '..', '..')
sys.path.insert(0, HERE)
from presets import PRESETS, PROGRESSIONS, INIT
from comper_ref import parse_chart
ver = json.load(open(os.path.join(ROOT, 'plugin.json')))['version']
IDS = ['STEPS', 'BEATS', 'KEY', 'STYLE', 'SPREAD', 'REGISTER', 'VOICES', 'LEAD', 'OUT', 'ROOTS', 'GROOVE', 'DENSITY', 'SWING',
       'REPEAT', 'PUSH', 'SEED', 'NEW', 'LOOSE', 'TIMING', 'ROLL', 'WEIGHT', 'SLOP', 'PEDAL', 'LISTEN', 'TEMPO', 'RUN']
DEFAULT = dict(STEPS=4, BEATS=2, KEY=0, STYLE=1, SPREAD=0.25, REGISTER=69, VOICES=5, LEAD=1, OUT=0, ROOTS=0, GROOVE=0, DENSITY=0.5,
               SWING=0, REPEAT=0.6, PUSH=0.85, SEED=11, NEW=0, LOOSE=1, TIMING=1, ROLL=1, WEIGHT=1, SLOP=1, PEDAL=0, LISTEN=0, TEMPO=120, RUN=1)
MAP = dict(style='STYLE', spread='SPREAD', register='REGISTER', voices='VOICES', lead='LEAD', out='OUT', roots='ROOTS', groove='GROOVE',
           density='DENSITY', swing='SWING', repeat='REPEAT', push='PUSH', loose='LOOSE', timing='TIMING', roll='ROLL', weight='WEIGHT',
           slop='SLOP', pedal='PEDAL', listen='LISTEN', tempo='TEMPO')
BEATS = {1: 0, 2: 1, 4: 2, 8: 3}
pdir = os.path.join(ROOT, 'presets', 'Comper'); os.makedirs(pdir, exist_ok=True)
for f in os.listdir(pdir):
    if f.endswith('.vcvm'): os.remove(os.path.join(pdir, f))
for name, d in PRESETS:
    v = dict(DEFAULT)
    prog = [p for p in PROGRESSIONS if p[0] == d['prog']][0]
    chords = [parse_chart(c)[0][0][1] for c in prog[2].split()]
    v['STEPS'] = len(chords); v['BEATS'] = BEATS[prog[1]]
    for k, x in d.items():
        if k != 'prog': v[MAP[k]] = x
    params = [{"value": float(v[k]), "id": j} for j, k in enumerate(IDS)]
    for i in range(16):
        r, q = chords[i] if i < len(chords) else (0, 0)
        params.append({"value": float(r), "id": 26 + i})
    for i in range(16):
        r, q = chords[i] if i < len(chords) else (0, 0)
        params.append({"value": float(q), "id": 42 + i})
    js = {"plugin": "ComperFable", "model": "Comper", "version": ver, "params": params,
          "data": {"ppb": 0, "channels": 12, "bassEveryBar": False, "resetReplays": True}}
    json.dump(js, open(os.path.join(pdir, name + '.vcvm'), 'w'), indent=1)
print('wrote', len(PRESETS), 'presets')
