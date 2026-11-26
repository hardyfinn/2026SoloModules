"""Writes the Walker test references:
   test/walker_golden.json    bass lines the C++ engine must reproduce (notes exact, times to 1e-6 s)
   test/upright_cases.json    notes to render for the sound check, with the reference measurements
   python3 tools/proto/make_walker_reference.py"""
import os, sys, json
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import numpy as np
import walker_ref as W, upright_ref as U, presets as CP, walker_presets as WP
from upright_features import features
TEST = os.path.join(HERE, '..', '..', 'test')
PROG = {n: (b, t) for n, b, t in CP.PROGRESSIONS}
def chart_of(name):
    bps, txt = PROG[name]; ch = txt.split(); bars = []
    if bps == 4: bars = ch
    else: bars = [' '.join(ch[i:i + 2]) for i in range(0, len(ch), 2)]
    return W.parse_chart(' | '.join(bars))
FIELDS = ('t', 'pitch', 'vel', 'dur', 'dead', 'scoop', 'cents', 'vib', 'string', 'fret', 'stopped', 'beat')
def pack(notes): return [[round(float(n[f]), 7) if f in ('t', 'vel', 'dur', 'scoop', 'cents', 'vib', 'beat') else int(n[f]) for f in FIELDS] for n in notes]
cases = []
def add(name, prog, bpm, nbars, seed, kw, blind=False):
    chart, total = chart_of(prog); p = W.BP(**kw)
    notes, pl = (W.play_blind if blind else W.play_line)(chart, total, bpm, nbars, seed, p)
    cases.append(dict(name=name, chart=[[b, r, q] for b, (r, q) in chart], total_beats=total, bpm=bpm, nbars=nbars, seed=seed, blind=blind, params=kw, fields=FIELDS, notes=pack(notes)))
CPD = dict(CP.PRESETS)
for (name, line, tone) in WP.PRESETS:
    c = CPD[name]; add('preset ' + name, c['prog'], c['tempo'], 16, 11, line)
for feel in range(6):
    for prog, bpm in (('Jazz blues in F', 120), ('Falling fifths', 152), ('Turnaround I-vi-ii-V', 96), ('One chord', 132)):
        for seed in (1, 2): add('feel %s / %s / seed %d' % (W.FEEL_NAMES[feel], prog, seed), prog, bpm, 12, seed, dict(feel=feel))
for k, vals in (('activity', (0.0, 0.25, 0.75, 1.0)), ('roam', (0.0, 1.0)), ('out', (0.0, 1.0)), ('register', (34.0, 46.0)), ('loose', (0.0, 2.0)),
                ('ahead', (0.0, 1.0)), ('timing', (0.0, 2.0)), ('weight', (0.0, 2.0)), ('slop', (0.0, 2.0)), ('pitch', (0.0, 2.0)), ('swing', (-0.5, 0.5))):
    for v in vals: add('sweep %s=%s' % (k, v), 'Blues (12 steps)', 126, 12, 5, {k: v})
for feel, k, v in ((W.TWO, 'activity', 0.9), (W.BALLAD, 'activity', 0.0), (W.BOSSA, 'activity', 1.0), (W.SOUL, 'repeat', 0.0), (W.SOUL, 'activity', 0.0), (W.HOUSE, 'repeat', 0.2)):
    add('sweep %s %s=%s' % (W.FEEL_NAMES[feel], k, v), 'Soul vamp', 100, 12, 4, {'feel': feel, k: v})
for bpm in (60, 90, 208, 280): add('tempo %d' % bpm, 'Rhythm bridge', bpm, 8, 9, {})
for prog in ('Jazz blues in F', 'Falling fifths', 'One chord'): add('blind / ' + prog, prog, 120, 8, 6, {}, blind=True)
json.dump(cases, open(os.path.join(TEST, 'walker_golden.json'), 'w'), separators=(',', ':'))
print(len(cases), 'line cases,', sum(len(c['notes']) for c in cases), 'notes')

# ---- sound cases: single notes over the range, loudness and every tone control, and one short phrase
snd = []
def one(name, pitch, vel, dur, tone_kw, string=None, fret=None, **nk):
    n = dict(t=0.01, pitch=pitch, vel=vel, dur=dur, dead=False, scoop=0.0, cents=0.0, vib=0.0, stopped=False); n.update(nk)
    if string is None: string, fret = W.note_string(pitch, 2)
    n['string'] = string; n['fret'] = fret
    snd.append(dict(name=name, tone=tone_kw, seed=1, total=dur + 0.9, notes=[n]))
for pitch in (28, 31, 33, 36, 40, 43, 45, 48, 52, 55):
    one('note %d loud' % pitch, pitch, 0.9, 1.0, {}); one('note %d soft' % pitch, pitch, 0.4, 1.0, {})
for k, vals in (('attack', (0.0, 2.0)), ('pluck', (0.08, 0.3)), ('bright', (0.0, 1.0)), ('sustain', (0.0, 1.0)), ('growl', (0.0, 1.0)), ('body', (0.0, 1.0)),
                ('pickup', (0.0, 1.0)), ('thump', (0.0, 1.0)), ('age', (0.0, 1.0))):
    for v in vals: one('tone %s=%s' % (k, v), 38, 0.8, 0.9, {k: v}); one('tone %s=%s high' % (k, v), 47, 0.8, 0.9, {k: v})
one('dead note', 36, 0.6, 0.2, {}, dead=True); one('stopped note', 41, 0.8, 0.35, {}, stopped=True)
one('scoop and vibrato', 45, 0.8, 1.6, {}, scoop=50.0, vib=9.0, cents=6.0)
chart, total = chart_of('Jazz blues in F')
for nm, kw, tk in (('phrase walk', {}, {}), ('phrase soul pickup', dict(feel=W.SOUL), dict(pickup=0.6, bright=0.65)), ('phrase ballad old', dict(feel=W.BALLAD), dict(age=0.8, sustain=0.8))):
    notes, pl = W.play_line(chart, total, 120, 4, 3, W.BP(**kw)); shift = 0.05 - min(n['t'] for n in notes)
    ns = [{f: (n[f] + shift if f == 't' else n[f]) for f in FIELDS[:-1]} for n in notes]
    snd.append(dict(name=nm, tone=tk, seed=3, total=8.0 + 1.0, notes=ns))
for c in snd:
    x = U.play(c['notes'], U.Tone(**c['tone']), seed=c['seed'], total=c['total']); c['features'] = features(x, U.SR); c['peak'] = float(np.abs(x).max())
json.dump(snd, open(os.path.join(TEST, 'upright_cases.json'), 'w'), separators=(',', ':'))
print(len(snd), 'sound cases')
