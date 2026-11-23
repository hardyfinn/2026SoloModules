"""Writes presets/Sawstack/*.vcvm. Run from anywhere: python3 tools/proto/export_presets.py
Times are given in seconds and converted to knob positions (time = 0.002 * 5000^knob)."""
import os, json, math
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.join(HERE, '..', '..')
ver = json.load(open(os.path.join(ROOT, 'plugin.json')))['version']
IDS = ['FREQ', 'SPREAD', 'TIMBRE', 'RICH', 'AMP', 'OUTPUT', 'FREQ_ATT', 'SPREAD_ATT', 'TIMBRE_ATT', 'RICH_ATT', 'AMP_ATT',
       'RISE1', 'FALL1', 'RISE2', 'FALL2', 'FALL1_ATT', 'FALL2_ATT', 'AGE', 'DRIVE']
T = lambda s: math.log(s / 0.002) / math.log(5000.0)
DEFAULT = dict(FREQ=0, SPREAD=0.25, TIMBRE=1, RICH=0, AMP=0, OUTPUT=1, FREQ_ATT=0, SPREAD_ATT=0, TIMBRE_ATT=0, RICH_ATT=0, AMP_ATT=1,
               RISE1=0, FALL1=0.55, RISE2=0, FALL2=0.6, FALL1_ATT=0, FALL2_ATT=0, AGE=0.6, DRIVE=0.35)
PRESETS = [
    ('Init Swarm', {}, 0),
    ('Tight Unison', dict(SPREAD=0.0), 0),
    ('Supersaw Pad', dict(SPREAD=0.3, RISE1=T(1.0), FALL1=T(1.5), RICH_ATT=0.2, RISE2=T(0.8), FALL2=T(1.5)), 0),
    ('Octave Organ', dict(TIMBRE=0.0, SPREAD=1.0, RISE2=T(0.01), FALL2=T(0.4)), 0),
    ('Sync Sweep Bass', dict(FREQ=-2, SPREAD=0.26, RICH=0.1, RICH_ATT=0.6, FALL1=T(0.11), FALL2=T(0.16)), 0),
    ('Fifths Stack', dict(SPREAD=1.0, TIMBRE=0.6, RISE2=T(0.02), FALL2=T(0.8)), 1),
    ('Harmonic Bells', dict(FREQ=1, SPREAD=1.0, TIMBRE=0.0, FALL2=T(0.6)), 2),
    ('Tired Unit', dict(AGE=1.0, DRIVE=0.75), 0),
    ('Factory Fresh', dict(AGE=0.0, DRIVE=0.0), 0),
    ('Driven Drone', dict(FREQ=-2, SPREAD=0.35, AMP=0.8, AMP_ATT=0.0, DRIVE=0.8), 0),
    ('Sine Swarm', dict(TIMBRE=0.0, SPREAD=0.25, RISE2=T(0.05), FALL2=T(0.9)), 0),
    ('Looper', dict(SPREAD=0.15, SPREAD_ATT=0.85, TIMBRE=0.55, RISE1=T(2.0), FALL1=T(2.0), AMP=0.9, AMP_ATT=0.0), 0),
]
pdir = os.path.join(ROOT, 'presets', 'Sawstack'); os.makedirs(pdir, exist_ok=True)
for f in os.listdir(pdir):
    if f.endswith('.vcvm'): os.remove(os.path.join(pdir, f))
for i, (name, ch, voicing) in enumerate(PRESETS):
    v = dict(DEFAULT); v.update(ch)
    js = {"plugin": "SawstackFable", "model": "Sawstack", "version": ver,
          "params": [{"value": round(float(v[k]), 4), "id": j} for j, k in enumerate(IDS)],
          "data": {"voicing": voicing, "snapFreq": False}}
    json.dump(js, open(os.path.join(pdir, '%02d_%s.vcvm' % (i + 1, name)), 'w'), indent=1)
print('wrote', len(PRESETS), 'presets')
