"""Rider demo WAVs from the C++ engines: tools/render_trio.cpp runs Comper (through Joanna), Walker and Rider linked
sample by sample as in Rack, and this mixes the three stems.
Trios: bass RMS equal to the piano's, drums at 0.6 of it (0.45 on the brush ballads).
The ENERGY, LISTEN and BAND tours keep the Medium Swing trio's gains, the building blues keeps the Blues Shells trio's,
and the kit tours (drums alone) keep the default kit's, so level changes from the controls are heard as they are.
   python3 tools/make_rider_demos_cpp.py OUTDIR      (needs numpy, soundfile, and the Joanna folder next to this one)"""
import os, sys, subprocess, zipfile
import numpy as np, soundfile as sf
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else '/tmp/rider-cpp'); os.makedirs(OUT, exist_ok=True)
STEMS = os.path.join(OUT, 'stems'); os.makedirs(STEMS, exist_ok=True)
subprocess.run(['c++', '-O3', '-funsafe-math-optimizations', '-std=c++17', os.path.join(HERE, 'render_trio.cpp'), '-o', '/tmp/render_trio'], check=True)
if not os.environ.get('SKIP_RENDER'): subprocess.run(['/tmp/render_trio', STEMS], check=True, stdout=subprocess.DEVNULL)

def rms(x): return float(np.sqrt(np.mean(np.square(x)))) + 1e-12
def load(name, part):
    p = os.path.join(STEMS, '%s.%s.wav' % (name, part))
    x, sr = sf.read(p); x = x.astype(np.float32)
    return (x[:, None] if x.ndim == 1 else x), sr

def mix(name, gains=None, level=0.6):
    piano, sr = load(name, 'piano'); bass, _ = load(name, 'bass'); drums, _ = load(name, 'drums')
    if gains is None:
        ref = rms(piano)
        gains = (ref / rms(bass), level * ref / rms(drums))
    gb, gd = gains
    n = max(len(piano), len(bass), len(drums))
    m = np.zeros((n, 2), np.float32)
    m[:len(piano)] += piano; m[:len(bass)] += gb * bass; m[:len(drums)] += gd * drums
    return m, sr, gains

def write(file, m, sr, scale=None):
    pk = float(np.abs(m).max())
    s = scale if (scale is not None and pk * scale <= 0.99) else 0.89 / max(1e-6, pk)
    sf.write(os.path.join(OUT, file + '.wav'), np.clip(m * s, -1, 1), sr, subtype='PCM_16')
    return s

written = []
names = sorted(f[len('trio-'):-len('.drums.wav')] for f in os.listdir(STEMS) if f.startswith('trio-') and f.endswith('.drums.wav'))
ref = {}
for n in names:
    m, sr, g = mix('trio-' + n, level=0.45 if n.startswith(('03_', '11_')) else 0.6)
    s = write('trio-' + n, m, sr); ref[n[:2]] = (g, s); written.append('trio-' + n); print('trio-' + n)
g01, s01 = ref['01']; g07, s07 = ref['07']
for n in ['rider-energy-0.1', 'rider-energy-0.9', 'rider-listen-0', 'rider-band-0-energy-ramp', 'rider-band-1-energy-ramp']:
    m, sr, _ = mix(n, g01); write(n, m, sr, s01); written.append(n); print(n)
m, sr, _ = mix('rider-building-blues', g07); write('rider-building-blues', m, sr, s07); written.append('rider-building-blues'); print('rider-building-blues')
d0, sr = load('rider-kit-default', 'drums'); ks = 0.89 / float(np.abs(d0).max())
for n in ['rider-kit-default', 'rider-kit-tune-0', 'rider-kit-tune-1', 'rider-kit-decay-0', 'rider-kit-decay-1', 'rider-kit-bright-0', 'rider-kit-bright-1', 'rider-kit-width-0', 'rider-kit-age-1']:
    d, sr = load(n, 'drums'); write(n, d, sr, ks); written.append(n); print(n)
with zipfile.ZipFile(os.path.join(OUT, 'rider-demos.zip'), 'w', zipfile.ZIP_DEFLATED) as z:
    for n in written: z.write(os.path.join(OUT, n + '.wav'), n + '.wav')
print('zip:', os.path.join(OUT, 'rider-demos.zip'))
