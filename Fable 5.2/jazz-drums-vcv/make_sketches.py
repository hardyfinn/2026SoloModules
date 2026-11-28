"""Sketches of the drummer prototype against the Comper + Walker trio stems.
Piano (Comper through Joanna) and bass (Walker) are the real module engines; drums are General MIDI kits
played by drummer.py. Needs the stems from ComperFable/tools/make_walker_demos.py (STEMS), fluidsynth, numpy, soundfile."""
import os, sys, json, subprocess
import numpy as np, soundfile as sf
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.environ.get('COMPER', '/home/user/2026solomodules/ComperFable')
STEMS = os.environ.get('STEMS', '/tmp/walker-demos/stems')
sys.path.insert(0, HERE); sys.path.insert(0, os.path.join(REPO, 'tools', 'proto'))
import sketch_band_v0 as band
from drummer import Drummer, DP
OUT = os.path.join(HERE, 'sketches'); os.makedirs(OUT, exist_ok=True)
SF = '/usr/share/sounds/sf2/FluidR3_GM.sf2'
G = {c['name']: c for c in json.load(open(os.path.join(REPO, 'test', 'golden.json')))}
WG = {c['name']: c for c in json.load(open(os.path.join(REPO, 'test', 'walker_golden.json')))}
report = {}

def rms(x): return float(np.sqrt(np.mean(np.square(x)))) + 1e-12

def render_drums(notes, bpm, kit, sr, pre):
    mid = '/tmp/dr.mid'; wav = '/tmp/dr.wav'
    band.write_midi(mid, bpm, [(9, kit, 100, [(a + pre, b + pre, n, v) for a, b, n, v in notes if a + pre >= 0])], tail=3.0)
    subprocess.run(['fluidsynth', '-ni', '-g', '0.8', '-r', str(sr), '-F', wav, SF, mid], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return sf.read(wav)[0].astype(np.float32)

def sketch(name, preset, feel, p, kit=32, arc=None, alone=False, drums_level=0.6):
    c = G['preset ' + preset]; w = WG['preset ' + preset]
    bpm = c['bpm']; spb = 60.0 / bpm; nbars = c['nbars']
    d = Drummer(bpm, c['total_beats'], p)
    notes = d.play(feel, nbars, c['notes'], w['notes'], arc)
    piano, sr = sf.read(os.path.join(STEMS, 'trio-%s.piano.wav' % preset)); piano = piano.astype(np.float32)
    bass = sf.read(os.path.join(STEMS, 'trio-%s.bass.wav' % preset))[0].astype(np.float32)
    drums = render_drums(notes, bpm, kit, sr, 0.5 * spb)
    n = max(len(piano), len(drums)); m = np.zeros((n, 2), np.float32)
    ref = rms(piano)
    if not alone:
        m[:len(piano)] += piano; m[:len(bass)] += (bass * ref / rms(bass))[:, None]
    # one fixed drum gain per kit, set on a reference take, so ENERGY is heard as level too
    key = (kit, preset)
    if key not in GAIN: GAIN[key] = drums_level * ref / rms(drums)
    m[:len(drums)] += drums * GAIN[key]
    m *= 0.89 / max(1e-6, float(np.abs(m).max())) if name not in SCALE_FROM else SCALE[SCALE_FROM[name]]
    SCALE[name] = 0.89 / max(1e-6, float(np.abs(m).max())) if name not in SCALE_FROM else SCALE[SCALE_FROM[name]]
    sf.write(os.path.join(OUT, name + '.wav'), np.clip(m, -1, 1), sr, subtype='PCM_16')
    s = d.stats
    report[name] = dict(preset=preset, feel=feel, bpm=bpm, bars=nbars, drum_notes=len(notes), **s)
    print('%-44s %4d notes  pushes %2d caught %2d  comps %3d (%d on a piano hit)  fills %d' % (name, len(notes), s['pushes'], s['caught'], s['comps'], s['comps_on_piano'], s['fills']))
    return d, notes, c, w

GAIN = {}; SCALE = {}; SCALE_FROM = {}
M = '01_Medium Swing'
d, notes, c, w = sketch('01_trio-with-the-listening-drummer', M, 'swing', DP(seed=3))
keep = (d, notes, c, w)
sketch('02_same-take-drummer-not-listening', M, 'swing', DP(seed=3, listen=0.0))
sketch('03_energy-low', M, 'swing', DP(seed=3, energy=0.2))
sketch('04_energy-high', M, 'swing', DP(seed=3, energy=0.9))
sketch('05_drums-alone-medium-swing', M, 'swing', DP(seed=3), alone=True)
B = '07_Blues Shells'
sketch('06_blues-building-from-quiet-to-loud', B, 'swing', DP(seed=5, fills=0.7), arc=lambda bar: 0.2 + 0.7 * bar / 15.0)
sketch('07_fast-bebop', '02_Fast Bebop Shells', 'swing', DP(seed=2, energy=0.6, comp=0.45))
sketch('08_ballad-brushes', '03_Ballad', 'ballad', DP(seed=4, energy=0.4), kit=40)
sketch('09_bossa', '04_Bossa', 'bossa', DP(seed=6, energy=0.45))
json.dump(report, open(os.path.join(OUT, 'report.json'), 'w'), indent=1)

# picture: who plays where, first 8 bars of sketch 01
import matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
from drummer import cluster_piano, RIDE, HATF, SNARE, KICK, CRASH, TOM_HI, TOM_MID, TOM_LO
d, notes, c, w = keep; spb = 60.0 / c['bpm']
fig, ax = plt.subplots(figsize=(15, 4.6))
rows = {'piano': 6, RIDE: 5, CRASH: 5, HATF: 4, SNARE: 3, TOM_HI: 3, TOM_MID: 3, TOM_LO: 3, KICK: 2, 'bass': 1}
for h in cluster_piano(c['notes'], spb):
    if h['beat'] < 32: ax.plot([h['beat'], h['beat'] + max(0.1, min(2.0, h['long']))], [6, 6], color='#B4521F', lw=3 + 5 * h['w'], solid_capstyle='butt')
for n in w['notes']:
    if n[-1] < 32: ax.plot([n[-1], n[-1] + 0.6], [1, 1], color='#555', lw=4, solid_capstyle='butt')
for t, t1, note, v in notes:
    b = t / spb
    if b >= 32 or note not in rows: continue
    quiet = note == KICK and v < 40
    ax.scatter([b], [rows[note]], s=10 + v * (0.5 if quiet else 1.6), color={CRASH: '#D9A441'}.get(note, '#1b1b1b'), alpha=0.35 if quiet else 0.9, marker='o' if note not in (TOM_HI, TOM_MID, TOM_LO) else 'D', zorder=3)
for b in range(0, 33, 4): ax.axvline(b, color='#999', lw=0.8)
for b in range(33): ax.axvline(b, color='#ddd', lw=0.4, zorder=0)
ax.set_yticks([1, 2, 3, 4, 5, 6]); ax.set_yticklabels(['bass (Walker)', 'bass drum', 'snare / toms', 'hi-hat foot', 'ride / crash', 'piano (Comper)'])
ax.set_xticks(range(0, 33, 4)); ax.set_xticklabels(['bar %d' % (i + 1) for i in range(9)]); ax.set_xlim(-0.3, 32.3); ax.set_ylim(0.4, 6.6)
ax.set_title('Sketch 01, first 8 bars: the drummer against the piano and bass. Dot size is loudness; faint dots are the feathered bass drum.')
plt.tight_layout(); plt.savefig(os.path.join(OUT, 'who-plays-where.png'), dpi=130)
print('ok')
