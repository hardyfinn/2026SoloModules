"""Writes presets/Tapeizer/*.vcvm (section 10 of docs/BUILD-INSTRUCTIONS.md). python3 tools/proto/export_presets.py"""
import os, json, math
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.join(HERE, '..', '..')
ver = json.load(open(os.path.join(ROOT, 'plugin.json')))['version']
IDS = ['STRENGTH', 'AGE', 'MIX', 'OUTPUT', 'TRIM', 'LOW', 'LOW_FREQ', 'HIGH', 'HIGH_FREQ', 'DESK_ON',
       'NR_TYPE', 'NR_ENC', 'NR_DEC', 'CAL', 'MULTI_MACHINE', 'REC', 'TAPE', 'BIAS', 'BOUNCE', 'MULTI_ON',
       'WOW', 'FLUTTER', 'DROPOUT', 'HISS', 'AZIMUTH', 'WEAR', 'THRESH', 'RATIO', 'BUS_ON',
       'MASTER_MACHINE', 'MASTER_REC', 'MASTER_TAPE', 'MASTER_NR', 'MASTER_ON']
LF = lambda hz: math.log(hz / 80.0) / math.log(15.0)
HF = lambda hz: math.log(hz / 700.0) / math.log(10000.0 / 700.0)
DEFAULT = dict(STRENGTH=0.4, AGE=0.2, MIX=1, OUTPUT=0, TRIM=0, LOW=0, LOW_FREQ=LF(100), HIGH=0, HIGH_FREQ=1, DESK_ON=1,
               NR_TYPE=3, NR_ENC=1, NR_DEC=1, CAL=0, MULTI_MACHINE=0, REC=0, TAPE=1, BIAS=0, BOUNCE=0, MULTI_ON=1,
               WOW=0.5, FLUTTER=0.5, DROPOUT=0.5, HISS=0.5, AZIMUTH=0, WEAR=0, THRESH=-8, RATIO=4, BUS_ON=0,
               MASTER_MACHINE=0, MASTER_REC=0, MASTER_TAPE=1, MASTER_NR=1, MASTER_ON=1)
# machines: 0 4TR, 1 4TR HI, 2 4TR C, 3 8TR CASS, 4 8TR 7.5, 5 8TR 15. master: 0 DECK, 1 BOOMBOX, 2 1/4 INCH, 3 DAT. NR: 0 OFF, 1 B, 2 C, 3 2:1
PRESETS = [
    ('Four-Track To Cassette', {}),
    ('Two Decks 1985', dict(STRENGTH=0.5, MULTI_MACHINE=0, TAPE=0, NR_TYPE=0, REC=3, MASTER_MACHINE=0, MASTER_TAPE=0, MASTER_NR=0, MASTER_REC=3)),
    ('Clean Four-Track', dict(STRENGTH=0.25, MULTI_MACHINE=1, NR_TYPE=3, MASTER_MACHINE=3)),
    ('In The Red', dict(STRENGTH=0.5, TRIM=8, REC=9, MASTER_REC=6)),
    ('260 Session', dict(STRENGTH=0.45, MULTI_MACHINE=2, NR_TYPE=2, REC=4, LOW=2, LOW_FREQ=LF(100), HIGH=2, HIGH_FREQ=HF(8000), MASTER_MACHINE=3)),
    ('Eight On A Cassette', dict(STRENGTH=0.5, MULTI_MACHINE=3, NR_TYPE=3, REC=2)),
    ('Studio Eight', dict(STRENGTH=0.55, MULTI_MACHINE=4, NR_TYPE=3, REC=6, MASTER_MACHINE=2, MASTER_NR=0, MASTER_REC=5)),
    ('Budget Reel', dict(STRENGTH=0.5, MULTI_MACHINE=5, NR_TYPE=2, REC=6, MASTER_MACHINE=2, MASTER_NR=0, MASTER_REC=4)),
    ('Third Bounce', dict(STRENGTH=0.45, MULTI_MACHINE=1, BOUNCE=2, REC=3)),
    ('Boombox Dub', dict(STRENGTH=0.5, MULTI_ON=0, MASTER_MACHINE=1, MASTER_TAPE=0, MASTER_NR=0, MASTER_REC=5)),
    ('Bright Trick', dict(STRENGTH=0.4, NR_TYPE=1, NR_ENC=1, NR_DEC=0, MASTER_NR=0)),
    ('Neglected', dict(STRENGTH=0.7, AGE=1.0, WEAR=0.5)),
]
pdir = os.path.join(ROOT, 'presets', 'Tapeizer'); os.makedirs(pdir, exist_ok=True)
for f in os.listdir(pdir):
    if f.endswith('.vcvm'): os.remove(os.path.join(pdir, f))
for i, (name, ch) in enumerate(PRESETS):
    v = dict(DEFAULT); v.update(ch)
    js = {"plugin": "TapeizerFable", "model": "Tapeizer", "version": ver,
          "params": [{"value": round(float(v[k]), 4), "id": j} for j, k in enumerate(IDS)], "data": {"humHz": 50.0}}
    json.dump(js, open(os.path.join(pdir, '%02d_%s.vcvm' % (i + 1, name)), 'w'), indent=1)
print('wrote', len(PRESETS), 'presets')
