"""Sound sketches for the kit: the recorded kit, then the model, playing the same thing."""
import os, sys, json
import numpy as np, soundfile as sf
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, os.path.dirname(HERE))
REPO = os.environ.get('COMPER', '/home/user/2026solomodules/ComperFable'); STEMS = os.environ.get('STEMS', '/tmp/walker-demos/stems')
from kit_play import *
from drummer import Drummer, DP
OUT = os.path.join(HERE, 'sketches'); os.makedirs(OUT, exist_ok=True)
G = {c['name']: c for c in json.load(open(os.path.join(REPO, 'test', 'golden.json')))}
WG = {c['name']: c for c in json.load(open(os.path.join(REPO, 'test', 'walker_golden.json')))}
def rms(x): return float(np.sqrt(np.mean(np.square(x)))) + 1e-12
def write(name, y): sf.write(os.path.join(OUT, name + '.wav'), np.clip(y, -1, 1), SR, subtype='PCM_16'); print(name, '%.1f s' % (len(y) / SR))
def take(preset, feel, p, bars=None, arc=None):
    c = G['preset ' + preset]; w = WG['preset ' + preset]; d = Drummer(c['bpm'], c['total_beats'], p)
    nb = bars or c['nbars']; notes = d.play(feel, nb, c['notes'], w['notes'], arc); spb = 60.0 / c['bpm']
    return [(a + 0.5 * spb, b + 0.5 * spb, n, v) for a, b, n, v in notes], nb * 4 * spb + 0.5 * spb, c
def trio(name, preset, notes, total, model=True, brushes=False, level=0.6):
    piano = sf.read(os.path.join(STEMS, 'trio-%s.piano.wav' % preset))[0]; bass = sf.read(os.path.join(STEMS, 'trio-%s.bass.wav' % preset))[0]
    n = int((total + 2.5) * SR); dr = render(notes, total + 2.5, model, brushes)
    m = np.zeros((n, 2)); k = min(n, len(piano)); m[:k] += piano[:k]; m[:k] += (bass[:k] * rms(piano) / rms(bass))[:, None]
    m += dr * level * rms(piano) / rms(dr); write(name, m * 0.89 / np.abs(m).max())
which = sys.argv[1:] or ['1', '2', '3', '4', '5']
M = '01_Medium Swing'
if '1' in which:
    notes, total, c = take(M, 'swing', DP(seed=3), bars=8)
    a = render(notes, total + 2.0, False); b = render(notes, total + 2.0, True); g = 0.8 / max(np.abs(a).max(), np.abs(b).max())
    write('k1_drums-alone_recorded-kit-then-model', np.concatenate([a, np.zeros((SR // 2, 2)), b]) * g)
    print('  level: recorded %.1f dB, model %.1f dB' % (20 * np.log10(rms(a)), 20 * np.log10(rms(b))))
if '2' in which:
    notes, total, c = take(M, 'swing', DP(seed=3)); trio('k2_trio-medium-swing_model-kit', M, notes, total)
    trio('k3_trio-medium-swing_recorded-kit', M, notes, total, model=False)
if '3' in which:
    spb = 0.5; seq = []; t = 0.3
    for note in (51, 53, 42, 44, 46, 38, 37, 36, 48, 41, 49):
        for v in (40, 70, 100, 124): seq.append((t, t + 0.1, note, v)); t += 0.45 if note not in (51, 53, 46, 49) else 0.9
        t += 0.6
    a = render(seq, t + 3, False); b = render(seq, t + 3, True); g = 0.8 / max(np.abs(a).max(), np.abs(b).max())
    write('k4_each-voice-soft-to-loud_recorded-kit-then-model', np.concatenate([a, b]) * g)
if '4' in which:
    notes, total, c = take('02_Fast Bebop Shells', 'swing', DP(seed=2, energy=0.6, comp=0.45)); trio('k5_trio-fast-bebop_model-kit', '02_Fast Bebop Shells', notes, total)
if '5' in which:
    Bd = '03_Ballad'; notes, total, c = take(Bd, 'ballad', DP(seed=4, energy=0.4), bars=4)
    a = render(notes, total + 2.0, False, brushes=True); b = render(notes, total + 2.0, True, brushes=True); g = 0.8 / max(np.abs(a).max(), np.abs(b).max())
    write('k6_brushes-alone_recorded-then-model', np.concatenate([a, np.zeros((SR // 2, 2)), b]) * g)
    print('  level: recorded %.1f dB, model %.1f dB' % (20 * np.log10(rms(a)), 20 * np.log10(rms(b))))
    notes, total, c = take(Bd, 'ballad', DP(seed=4, energy=0.4)); trio('k7_trio-ballad_model-brushes', Bd, notes, total, brushes=True, level=0.45)
