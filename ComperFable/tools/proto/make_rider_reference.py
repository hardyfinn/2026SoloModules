"""Writes test/rider_golden.json: drum parts the C++ player must reproduce (voices and velocities exact, times to 1e-6 s).
Each case carries the bar-by-bar input the drummer was given (Comper's figure for the bar, the bass notes off the beat).
   python3 tools/proto/make_rider_reference.py"""
import os, sys, json
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import comper_ref as C, rider_ref as R, presets as CP, rider_presets as RPS
from make_reference import prog
TEST = os.path.join(HERE, '..', '..', 'test')
WG = {c['name']: c for c in json.load(open(os.path.join(TEST, 'walker_golden.json')))}

def comper_bars(progname, bpm, nbars, seed, **kw):
    """run the Comper reference and note what it decides at each bar line: this is what the link carries"""
    chart, total = prog(progname); plans = []
    orig = C.Engine.begin_bar
    def spy(self, bar, bpm_):
        orig(self, bar, bpm_)
        kind = [-1] * 16
        for s, l in self.cellmap.items(): kind[s] = l
        plans.append((self.slots, kind, list(self.slot_beats) + [0.0] * (16 - self.slots), list(self.push_ok) + [False] * (16 - self.slots)))
    C.Engine.begin_bar = spy
    try: C.simulate(chart, total, bpm, nbars, seed=seed, params=C.Params(**kw))
    finally: C.Engine.begin_bar = orig
    return plans, total

def bars_in(plans, bass=None, nbars=None, force=None):
    out = []
    for bar in range(nbars or len(plans)):
        if plans is None: bi = R.BarIn()
        else:
            sl, kind, sb, push = plans[bar]; bi = R.BarIn(slots=sl, kind=kind, slot_beat=sb, push=push, has_piano=True)
        if bass: bi.bass_off = [round(b - 4 * bar, 7) for b in bass if 4 * bar <= b < 4 * bar + 4 and (b % 1.0) > 0.05]
        if force and bar in force: bi.force_fill = force[bar]
        out.append(bi)
    return out

cases = []
def add(name, bars, bpm, chorus_bars, seed, kw, energy=None):
    p = R.RP(**kw); en = None
    if energy: en = [round(energy(b), 7) for b in range(len(bars))]
    notes, d = R.play(None, bars, bpm, chorus_bars, seed, p, (lambda b: en[b]) if en else None)
    cases.append(dict(name=name, bpm=bpm, chorus_bars=chorus_bars, seed=seed, params=kw, energy=en or [],
                      bars=[dict(has_piano=int(b.has_piano), slots=b.slots, kind=b.kind, slot_beat=[round(x, 7) for x in b.slot_beat], push=[int(x) for x in b.push],
                                 bass_off=b.bass_off, force_fill=b.force_fill) for b in bars],
                      fields=['t', 'voice', 'vel', 'dur', 'beat'], notes=[[round(t, 7), v, vel, round(dur, 7), round(beat, 7)] for t, v, vel, dur, beat in notes], stats=d.stats))
    return notes, d

CPD = dict(CP.PRESETS)
def preset_bars(name, nbars=16, with_bass=True):
    c = dict(CPD[name]); pr = c.pop('prog'); bpm = c.pop('tempo')
    plans, total = comper_bars(pr, bpm, nbars, 11, **c)          # 11: the seed of Comper's own preset cases in golden.json
    bass = [n[-1] for n in WG['preset ' + name]['notes']] if with_bass else None
    return bars_in(plans, bass, nbars), bpm, max(1, int(round(total / 4))), c
if __name__ == '__main__':
    for name, player, kit in RPS.PRESETS:
        bars, bpm, chorus, c = preset_bars(name); kw = dict(player); kw['swing'] = c.get('swing', 0.0)
        add('preset ' + name, bars, bpm, chorus, 11, kw)
    # the same twelve as the host will see them next to a Comper with no Walker: the host test runs Comper's host and Rider's host
    # side by side (Comper seed 11, Rider seed 11) and must arrive at exactly these strokes
    for name, player, kit in RPS.PRESETS:
        bars, bpm, chorus, c = preset_bars(name, with_bass=False); kw = dict(player); kw['swing'] = c.get('swing', 0.0)
        add('linked ' + name, bars, bpm, chorus, 11, kw)
    M = '01_Medium Swing'
    for feel in range(5):
        for bpm in (96, 120, 132, 208):
            for seed in (1, 2): add('alone %s / %d bpm / seed %d' % (R.FEEL_NAMES[feel], bpm, seed), bars_in(None, nbars=12), bpm, 12, seed, dict(feel=feel))
    for feel, pname in ((R.SWING, '01_Medium Swing'), (R.BALLAD, '03_Ballad'), (R.BOSSA, '04_Bossa'), (R.SOUL, '08_Soul Sixteenths'), (R.HOUSE, '09_House Stabs')):
        bars, bpm, chorus, c = preset_bars(pname)
        for k, vals in (('energy', (0.0, 0.25, 0.75, 1.0)), ('comp', (0.0, 1.0)), ('listen', (0.0, 0.5)), ('fills', (0.0, 1.0)), ('feather', (0.0, 1.0)), ('repeat', (0.0, 1.0)),
                        ('loose', (0.0, 2.0)), ('ahead', (0.0, 1.0)), ('swing', (-0.5, 0.5))):
            for v in vals: add('sweep %s %s=%s' % (R.FEEL_NAMES[feel], k, v), bars, bpm, chorus, 5, {'feel': feel, k: v})
    bars, bpm, chorus, c = preset_bars(M)
    add('building from quiet to loud', bars, bpm, 12, 4, dict(feel=R.SWING, fills=0.7), energy=lambda b: 0.2 + 0.7 * b / 15.0)
    add('chorus of 12 bars', bars, bpm, 12, 6, dict(feel=R.SWING, fills=1.0))
    add('chorus of 2 bars', bars, bpm, 2, 6, dict(feel=R.SWING, fills=0.5))
    for feel in range(5):
        b2, bpm2, ch2, c2 = preset_bars(M)
        for b in b2: b.force_fill = 0
        b2[2].force_fill = 1; b2[5].force_fill = 2; b2[6].force_fill = 2
        add('fill input %s' % R.FEEL_NAMES[feel], b2, bpm2, ch2, 8, dict(feel=feel, fills=0.0, energy=0.7))
    # the drummer set to a different feel from the pianist: it still finds the hits by where they fall
    b8, bpm8, ch8, c8 = preset_bars('08_Soul Sixteenths')
    add('swing drummer over a soul pianist', b8, bpm8, ch8, 3, dict(feel=R.SWING, listen=1.0))
    add('soul drummer over a swing pianist', bars, bpm, chorus, 3, dict(feel=R.SOUL, listen=1.0))
    json.dump(cases, open(os.path.join(TEST, 'rider_golden.json'), 'w'), separators=(',', ':'))
    print(len(cases), 'cases,', sum(len(c['notes']) for c in cases), 'notes')
    for c in cases[:12]: print('  %-28s %4d notes' % (c['name'], len(c['notes'])), c['stats'])
    L1 = [c for c in cases if c['name'] == 'sweep SWING listen=0.0'][0]; print('  listen 0:', L1['stats'])
