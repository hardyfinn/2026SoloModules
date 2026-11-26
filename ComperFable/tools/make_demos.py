"""Demo WAVs: Comper through Joanna (tools/render_demos.cpp), with a quiet band for context.
The band (walking or simple bass, ride or groove drums, and the horn line in the listening demos)
is General MIDI through fluidsynth, made with the sketch functions in tools/proto/sketch_band_v0.py.
   python3 tools/make_demos.py OUTDIR      (needs fluidsynth + fluid-soundfont-gm, numpy)"""
import os, sys, json, subprocess, random, wave
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.join(HERE, '..')
sys.path.insert(0, os.path.join(HERE, 'proto'))
import sketch_band_v0 as band
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else '/tmp/comper-demos'); os.makedirs(OUT, exist_ok=True)
SF = '/usr/share/sounds/sf2/FluidR3_GM.sf2'
QN = ['maj', 'maj6', 'm7', 'm6', 'mmaj', 'dom', 'alt', 'hdim', 'dim', 'sus']
G = {c['name']: c for c in json.load(open(os.path.join(ROOT, 'test', 'golden.json')))}

subprocess.run(['c++', '-O2', '-std=c++17', os.path.join(HERE, 'render_demos.cpp'), '-o', '/tmp/render_demos'], check=True)
PIANO = os.path.join(OUT, 'piano-only'); os.makedirs(PIANO, exist_ok=True)
subprocess.run(['/tmp/render_demos', PIANO], check=True)

JOBS = [(n, n[7:], False) for n in G if n.startswith('preset ')] + [
    ('sweep loose=0.0', 'demo-loose-0-machine', False), ('sweep density=0.75', 'demo-loose-1-player', False),
    ('sweep loose=2.0', 'demo-loose-2-too-loose', False), ('sweep lead=0.0', 'demo-lead-0-no-voice-leading', False),
    ('sweep lead=0.5', 'demo-lead-half', False), ('sweep out=1.0', 'demo-out-1', False),
    ('listen 1.0', 'demo-listen-0', True), ('listen 1.0', 'demo-listen-1', True)]

def read(p):
    w = wave.open(p); d = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float32) / 32768
    return d.reshape(-1, 2), w.getframerate()

for case, fname, solo in JOBS:
    c = G[case]; p = c['params']; groove = p.get('groove', 0); bpm = c['bpm']; nbars = c['nbars']; spb = 60.0 / bpm
    chart = [(b, (r, QN[q])) for b, r, q in c['chart']]; total = c['total_beats']
    rng = random.Random(5)
    sw = band.swing_ratio(bpm) if groove in (0, 2) else 1.0
    bass = drums = []
    if groove == 0:
        bass = band.walk_bass(chart, total, bpm, nbars, rng, sw); drums = band.swing_drums(bpm, nbars, rng, sw)
    elif groove in (1, 3, 4):
        kind = {1: 'bossa', 3: 'soul', 4: 'house'}[groove]
        bass = band.simple_bass(chart, total, bpm, nbars, rng, kind); drums = band.straight_drums(bpm, nbars, rng, kind)
    horn = [(a, b, pt, 92) for a, b, pt in c['solo']] if (solo or c['solo']) else []
    pre = 0.5 * spb
    sh = lambda ns: [(a + pre, b + pre, x, v) for a, b, x, v in ns]
    piano, sr = read(os.path.join(PIANO, fname + '.wav'))
    if bass or drums or horn:
        mid = '/tmp/ctx.mid'; wav = '/tmp/ctx.wav'
        band.write_midi(mid, bpm, [(1, 32, 100, sh(bass)), (2, 65, 96, sh(horn)), (9, 0, 70, sh(drums))], tail=3.0)
        subprocess.run(['fluidsynth', '-ni', '-g', '0.7', '-r', str(sr), '-F', wav, SF, mid], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        ctx, _ = read(wav)
        n = max(len(piano), len(ctx)); mix = np.zeros((n, 2), np.float32)
        mix[:len(piano)] += piano
        # balance by loudness: the band sits under the piano (horn a little more forward)
        rel = 0.8 if horn else 0.5
        mix[:len(ctx)] += ctx * rel * float(piano.std()) / max(1e-9, float(ctx.std()))
    else:
        mix = piano
    mix = mix / max(1e-6, float(np.abs(mix).max())) * 0.89
    w = wave.open(os.path.join(OUT, fname + '.wav'), 'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr)
    w.writeframes((mix * 32767).astype(np.int16).tobytes()); w.close()
    print(fname)
