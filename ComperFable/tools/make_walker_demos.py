"""Walker demo WAVs: Comper through Joanna plus Walker's upright, linked as in Rack
(tools/render_trio.cpp), with quiet General MIDI drums for context (none on the ballads).
Trios are balanced by loudness: bass RMS equal to the piano's, drums at 0.4 of it.
The ACTIVITY, LOOSE, ATTACK and tone tours keep the gains of the Medium Swing trio, so level
changes from the controls are heard as they are.
   python3 tools/make_walker_demos.py OUTDIR      (needs fluidsynth + fluid-soundfont-gm, numpy, soundfile)"""
import os, sys, json, subprocess, random
import numpy as np, soundfile as sf
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.join(HERE, '..')
sys.path.insert(0, os.path.join(HERE, 'proto'))
import sketch_band_v0 as band
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else '/tmp/walker-demos'); os.makedirs(OUT, exist_ok=True)
STEMS = os.path.join(OUT, 'stems'); os.makedirs(STEMS, exist_ok=True)
SF = '/usr/share/sounds/sf2/FluidR3_GM.sf2'
G = {c['name']: c for c in json.load(open(os.path.join(ROOT, 'test', 'golden.json')))}
WG = {c['name']: c for c in json.load(open(os.path.join(ROOT, 'test', 'walker_golden.json')))}

subprocess.run(['c++', '-O3', '-funsafe-math-optimizations', '-std=c++17', os.path.join(HERE, 'render_trio.cpp'), '-o', '/tmp/render_trio'], check=True)
if not os.environ.get('SKIP_RENDER'): subprocess.run(['/tmp/render_trio', STEMS], check=True, stdout=subprocess.DEVNULL)

def drums_for(groove, bpm, nbars, sr):
    rng = random.Random(5); spb = 60.0 / bpm; pre = 0.5 * spb
    if groove == 0: d = band.swing_drums(bpm, nbars, rng, band.swing_ratio(bpm))
    elif groove in (1, 3, 4): d = band.straight_drums(bpm, nbars, rng, {1: 'bossa', 3: 'soul', 4: 'house'}[groove])
    else: return None
    mid = '/tmp/wdrums.mid'; wav = '/tmp/wdrums.wav'
    band.write_midi(mid, bpm, [(9, 0, 70, [(a + pre, b + pre, x, v) for a, b, x, v in d])], tail=3.0)
    subprocess.run(['fluidsynth', '-ni', '-g', '0.7', '-r', str(sr), '-F', wav, SF, mid], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    x, _ = sf.read(wav); return x.astype(np.float32)

def rms(x): return float(np.sqrt(np.mean(np.square(x)))) + 1e-12

def mix(name, case, groove, bpm, nbars, gains=None):
    bass, sr = sf.read(os.path.join(STEMS, name + '.bass.wav')); bass = bass.astype(np.float32)
    pp = os.path.join(STEMS, name + '.piano.wav')
    piano = sf.read(pp)[0].astype(np.float32) if os.path.exists(pp) else None
    drums = drums_for(groove, bpm, nbars, sr)
    if gains is None:
        ref = rms(piano) if piano is not None else rms(bass)
        gb = ref / rms(bass)
        gd = 0.4 * ref / rms(drums) if drums is not None else 0.0
        gains = (gb, gd)
    gb, gd = gains
    n = max(len(bass), len(drums) if drums is not None else 0)
    m = np.zeros((n, 2), np.float32)
    if piano is not None: m[:len(piano)] += piano
    m[:len(bass)] += (gb * bass)[:, None]
    if drums is not None: m[:len(drums)] += gd * drums
    return m, sr, gains

def write(name, m, sr, scale=None):
    s = scale if scale is not None else 0.89 / max(1e-6, float(np.abs(m).max()))
    sf.write(os.path.join(OUT, name + '.wav'), np.clip(m * s, -1, 1), sr, subtype='PCM_16')
    return s

names = sorted(n[7:] for n in WG if n.startswith('preset '))
refGains = refScale = None
for n in names:
    c = G['preset ' + n]; groove = c['params'].get('groove', 0)
    m, sr, g = mix('trio-' + n, c, groove, c['bpm'], c['nbars'])
    s = write('trio-' + n, m, sr)
    if n.startswith('01_'): refGains, refScale = g, s
    print('trio-' + n)
w = WG['feel WALK / Jazz blues in F / seed 1']
m, sr, _ = mix('bass-alone-blues', w, 0, w['bpm'], w['nbars']); write('bass-alone-blues', m, sr); print('bass-alone-blues')
c = G['preset 01_Medium Swing']
for n in ['activity-0.2', 'activity-0.5', 'activity-0.95', 'loose-0-machine', 'loose-1-player', 'loose-2-very-loose',
          'attack-0', 'attack-1-as-measured', 'attack-2.2', 'tone-gut-and-soft', 'tone-steel-and-ringing',
          'tone-growl-full', 'tone-pickup-only', 'tone-age-full']:
    m, sr, _ = mix(n, c, 0, c['bpm'], c['nbars'], refGains)
    pk = float(np.abs(m * refScale).max())
    write(n, m, sr, refScale if pk <= 0.99 else 0.98 / float(np.abs(m).max()))
    print(n)
