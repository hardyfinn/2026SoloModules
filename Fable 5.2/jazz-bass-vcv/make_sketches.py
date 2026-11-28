"""Sketches for the bass player proposal: the upright model against real samples, the line engine alone,
and trios with Comper (through Joanna) and quiet General MIDI drums.   python3 make_sketches.py [only-prefix]"""
import os, sys, json, random, subprocess, wave
import numpy as np, soundfile as sf
import upright as U, walker as W, upright_v1 as U1
sys.path.insert(0, '/home/user/2026solomodules/ComperFable/tools/proto')
import sketch_band_v0 as band
OUT = 'sketches'; os.makedirs(OUT, exist_ok=True)
PIANO = '/tmp/comper-demos/piano-only/'; SF2 = '/usr/share/sounds/sf2/FluidR3_GM.sf2'; SR = U.SR
G = {c['name']: c for c in json.load(open('/home/user/2026solomodules/ComperFable/test/golden.json'))}
ONLY = sys.argv[1] if len(sys.argv) > 1 else ''
REPORT = {}
def save(name, x):
    x = np.asarray(x, dtype=np.float64); x = x / max(1e-9, np.abs(x).max()) * 0.89
    sf.write(os.path.join(OUT, name + '.wav'), x, SR, subtype='PCM_16'); print(name, '%.1f s' % (len(x) / SR), flush=True)
def drums_wav(bpm, nbars, kind, pre):
    rng = random.Random(5)
    d = band.swing_drums(bpm, nbars, rng, band.swing_ratio(bpm)) if kind == 'swing' else band.straight_drums(bpm, nbars, rng, kind)
    d = [(a + pre, b + pre, x, v) for a, b, x, v in d]
    band.write_midi('/tmp/bd.mid', bpm, [(9, 0, 70, d)], tail=3.0)
    subprocess.run(['fluidsynth', '-ni', '-g', '0.7', '-r', str(SR), '-F', '/tmp/bd.wav', SF2, '/tmp/bd.mid'], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    x, _ = sf.read('/tmp/bd.wav'); return x
def bass(chart, total, bpm, nbars, seed, bp, tone, pre):
    notes, pl = W.play_line(chart, total, bpm, nbars, seed, bp)
    for n in notes: n['t'] += pre
    notes = [n for n in notes if n['t'] >= 0]
    return U.play(notes, tone, seed=seed, total=nbars * 4 * 60.0 / bpm + pre + 2.5), W.describe(notes, pl, nbars)
def mix(parts):
    """parts: (signal mono or stereo, level relative to the first part's loudness)"""
    n = max(len(p) for p, _ in parts); m = np.zeros((n, 2)); ref = None
    for p, lvl in parts:
        p = p if p.ndim == 2 else np.stack([p, p], 1)
        s = float(p.std()); ref = ref or s
        m[:len(p)] += p * lvl * ref / max(1e-9, s)
    return m
FEEL = {0: W.WALK, 1: W.BOSSA, 2: W.BALLAD, 3: W.SOUL, 4: W.HOUSE}; DR = {0: 'swing', 1: 'bossa', 3: 'soul', 4: 'house'}
def trio(name, preset, bp_kw=None, tone_kw=None, drums=True, nb=None):
    if ONLY and not name.startswith(ONLY): return
    c = G['preset ' + preset]; bpm = c['bpm']; nbars = nb or c['nbars']; pre = 0.5 * 60.0 / bpm; g = c['params'].get('groove', 0)
    chart = [(b, (r, q)) for b, r, q in c['chart']]
    bp = W.BP(feel=FEEL[g], **(bp_kw or {})); tone = U.Tone(**(tone_kw or {}))
    b, d = bass(chart, c['total_beats'], bpm, nbars, 7, bp, tone, pre); REPORT[name] = d
    piano, sr = sf.read(PIANO + preset + '.wav'); assert sr == SR
    parts = [(piano, 1.0), (b, 1.0)]
    if drums and g in DR: parts.append((drums_wav(bpm, nbars, DR[g], pre), 0.4))
    save(name, mix(parts))

# 01: real plucks, then the model playing the same notes
if not ONLY or '01'.startswith(ONLY):
    D = '/tmp/bassref/mb/Samples/pizz/'; segs = []
    for nm, midi in (('gb1', 30), ('a1', 33), ('c2', 36), ('eb2', 39), ('a2', 45), ('c3', 48)):
        f = [x for x in sorted(os.listdir(D)) if x.lower().startswith(nm + '_vl4_rr1')]
        if not f: continue
        x, sr = sf.read(D + f[0]); x = x if x.ndim == 1 else x.mean(1)
        if sr != SR:
            from scipy.signal import resample_poly; x = resample_poly(x, SR, sr)
        pk = np.argmax(np.abs(x) > 0.1 * np.abs(x).max()); x = x[max(0, pk - 300):][:int(1.3 * SR)]; x[-2400:] *= np.linspace(1, 0, 2400)
        m = U.play([dict(t=0.006, pitch=midi, vel=0.9, dur=1.15)], U.Tone(growl=0.1, age=0.0, pickup=0.1), total=1.3)
        m0 = U1.play([dict(t=0.006, pitch=midi, vel=0.9, dur=1.15)], U1.Tone(growl=0.1, age=0.0, pickup=0.1), total=1.3)
        segs += [x / np.abs(x).max(), m0 / np.abs(m0).max(), m / np.abs(m).max(), np.zeros(int(0.4 * SR))]
    save('01_real-pluck-then-first-model-then-new-model-six-notes', np.concatenate(segs))

BLUES = 'F7 | Bb7 | F7 | F7 | Bb7 | Bdim7 | F7 | D7 | Gm7 | C7 | F7 D7 | Gm7 C7'
chart, total = W.parse_chart(BLUES)
def alone(name, bpm, nbars, bp, tone, ride=False, seed=3):
    if ONLY and not name.startswith(ONLY): return None
    b, d = bass(chart, total, bpm, nbars, seed, bp, tone, 0.25); REPORT[name] = d
    if ride: return mix([(b, 1.0), (drums_wav(bpm, nbars, 'swing', 0.25), 0.3)])
    return b
x = alone('02_bass-alone-walking-blues', 120, 24, W.BP(), U.Tone())
if x is not None: save('02_bass-alone-walking-blues', x)
trio('03_trio-medium-swing', '01_Medium Swing')
trio('04_trio-fast-bebop', '02_Fast Bebop Shells', dict(activity=0.35))
trio('05_duo-ballad', '03_Ballad', dict(activity=0.4), dict(sustain=0.75), drums=False)
trio('06_trio-bossa', '04_Bossa')
trio('07_trio-blues', '07_Blues Shells', dict(activity=0.65))
trio('08_trio-modal', '05_Modal Quartal', dict(roam=0.8))
trio('09_trio-soul', '08_Soul Sixteenths', None, dict(bright=0.65, pickup=0.5))
trio('10_trio-house', '09_House Stabs', None, dict(bright=0.6, pickup=0.6, sustain=0.35))
def sweep(name, variants, bpm=126, nbars=8, ride=True):
    if ONLY and not name.startswith(ONLY): return
    segs = []
    for bp, tone in variants:
        b, d = bass(chart, total, bpm, nbars, 3, bp, tone, 0.25); REPORT.setdefault(name, []).append(d)
        m = mix([(b, 1.0), (drums_wav(bpm, nbars, 'swing', 0.25), 0.3)]) if ride else np.stack([b, b], 1)
        segs.append(m[:int((nbars * 4 * 60.0 / bpm + 0.25 + 0.6) * SR)])
    save(name, np.concatenate(segs))
sweep('11_activity-two-feel-then-walk-then-busy', [(W.BP(feel=W.TWO, activity=0.3), U.Tone()), (W.BP(activity=0.3), U.Tone()), (W.BP(activity=0.95), U.Tone())])
sweep('12_loose-0-machine-then-1-player-then-2-too-loose', [(W.BP(loose=0.0), U.Tone(age=0.0)), (W.BP(loose=1.0), U.Tone()), (W.BP(loose=2.2), U.Tone(age=0.6))])
sweep('13_tone-gut-then-steel-then-growl-then-pickup-then-old', [(W.BP(), U.Tone(bright=0.15, sustain=0.25, growl=0.1)), (W.BP(), U.Tone(bright=0.9, sustain=0.9, growl=0.2)),
      (W.BP(), U.Tone(growl=1.0)), (W.BP(), U.Tone(pickup=1.0, growl=0.2)), (W.BP(), U.Tone(age=1.0))], nbars=4, ride=False)
sweep('14_pluck-none-then-as-measured-then-exaggerated', [(W.BP(), U.Tone(attack=0.0)), (W.BP(), U.Tone(attack=1.0)), (W.BP(), U.Tone(attack=2.2))], nbars=4, ride=False)
json.dump(REPORT, open(os.path.join(OUT, 'report.json'), 'w'), indent=1)
