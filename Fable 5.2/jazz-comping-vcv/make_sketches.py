import sys, os, subprocess, random
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from comp_engine import *
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'sketches'); os.makedirs(OUT, exist_ok=True)
os.makedirs(os.path.join(OUT, 'midi'), exist_ok=True)
SF = '/usr/share/sounds/sf2/FluidR3_GM.sf2'
STATS = {}

def shift(notes, t): return [(a+t, b+t, p, v) for a, b, p, v in notes]

def section(chart_text, bpm, style='swing', band=True, solo=False, seed=1, reps=1, **kw):
    chart, beats = parse_chart(chart_text); nbars = beats//4*reps
    rng = random.Random(seed+100)
    sw = swing_ratio(bpm) if style in ('swing','ballad') else 1.0
    sol = soloist(chart, beats, bpm, nbars, random.Random(seed+7), sw) if solo else []
    c = Comper(chart, beats, bpm, seed=seed, style=style, solo=sol, bars=nbars, **kw)
    piano = c.run()
    bass = drums = []
    if band:
        if style == 'swing':
            bass = walk_bass(chart, beats, bpm, nbars, rng, sw); drums = swing_drums(bpm, nbars, rng, sw)
        elif style != 'ballad':
            bass = simple_bass(chart, beats, bpm, nbars, rng, style); drums = straight_drums(bpm, nbars, rng, style)
    return dict(piano=piano, bass=bass, drums=drums, solo=sol, len=nbars*4*60/bpm)

def render(name, bpm, sections, gap=1.0):
    P, B, D, S = [], [], [], []; t = 0.3
    for s in sections:
        P += shift(s['piano'], t); B += shift(s['bass'], t); D += shift(s['drums'], t); S += shift(s['solo'], t)
        t += s['len'] + gap
    mid = os.path.join(OUT, 'midi', name + '.mid'); wav = os.path.join(OUT, name + '.wav')
    write_midi(mid, bpm, [(0, 0, 112, P), (1, 32, 84, B), (2, 66, 80, S), (9, 0, 62, D)])
    write_midi(os.path.join(OUT, 'midi', name + '-piano-only.mid'), bpm, [(0, 0, 112, P)])
    subprocess.run(['fluidsynth', '-ni', '-g', '0.9', '-r', '44100', '-F', wav, SF, mid], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    STATS[name] = (P, bpm)
    print(name, round(t, 1), 's', len(P), 'piano notes')

II_V = 'Dm7 | G7 | Cmaj7 | Cmaj7 | Dm7b5 | G7alt | Cm6 | Cm6'
BLUES = 'F7 | Bb7 | F7 | Cm7 F7 | Bb7 | Bdim7 | F7 | Am7 D7 | Gm7 | C7 | F7 D7 | Gm7 C7'
MINOR = 'Cm7 | F7 | Bbmaj7 | Ebmaj7 | Am7b5 | D7alt | Gm6 | Gm6'
BOSSA = 'Fmaj7 | Fmaj7 | G7 | G7 | Gm7 | C7 | Fmaj7 | Gm7 C7'
BALLAD = 'Ebmaj7 Cm7 | Fm7 Bb7 | Gm7 C7alt | Fm7 Bb7 | Ebmaj7 | Am7b5 D7alt | Gm7 C7 | Fm7 Bb7sus'
MODAL = 'Dm7 | Dm7 | Dm7 | Dm7 | Ebm7 | Ebm7 | Dm7 | Dm7'
SOUL = 'Em7 | A7 | Em7 | A7 | Cmaj7 | Bm7 | Am7 | B7alt'
HOUSE = 'Am9 | Am9 | Fmaj7 | Em7 | Dm9 | Dm9 | Fmaj7 | E7alt'

render('01-voicing-types-shell-rootless-quartal-spread', 132,
       [section(II_V, 132, voicing=v, seed=3, density=0.5) for v in ['shell','rootless','quartal','spread']])
render('02-feel-machine-then-player-then-loose', 126,
       [section(BLUES, 126, seed=5, density=0.55, loose=l) for l in [0.0, 1.0, 2.2]])
render('03-voice-leading-off-then-on', 132,
       [section(MINOR, 132, seed=8, density=0.5, lead=l, reps=2) for l in [False, True]])
render('04-density-sparse-medium-busy', 138,
       [section(BLUES, 138, seed=11, density=d) for d in [0.12, 0.5, 0.92]])
render('05-inside-then-outside', 132,
       [section(MINOR, 132, seed=14, density=0.55, out=o, reps=2, voicing=v) for o, v in [(0.0,'rootless'), (0.9,'rootless')]])
render('06-soloist-not-listening-then-listening', 132,
       [section(MINOR, 132, seed=21, density=0.6, solo=True, listen=l, reps=2) for l in [0.0, 1.0]])
render('07-bossa', 126, [section(BOSSA, 126, style='bossa', seed=4, density=0.5, reps=2, voicing='rootless')])
render('08-ballad-solo-piano-with-roots', 62, [section(BALLAD, 62, style='ballad', seed=6, density=0.45, band=False, roots=True, voicing='spread', loose=1.2)])
render('09-modal-quartal-fast', 184, [section(MODAL, 184, seed=9, density=0.75, voicing='quartal', reps=2)])
render('10-soul-sixteenths', 94, [section(SOUL, 94, style='soul', seed=12, density=0.55, reps=2, voicing='rootless')])
render('11-house-stabs', 122, [section(HOUSE, 122, style='house', seed=2, density=0.5, reps=2, voicing='spread', loose=0.5, repeat=0.9)])

import pickle; pickle.dump(STATS, open('/tmp/comp_stats.pkl', 'wb'))
