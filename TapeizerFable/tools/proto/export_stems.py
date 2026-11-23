"""Writes the dry demo loops for the C++ demo renders: <song>-mix.wav (stereo, peak 1.0 = 5 V) and <song>-stems.wav
(drums, bass, keys L, keys R). python3 tools/proto/export_stems.py OUTDIR"""
import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, soundfile as sf
from songs import house, hiphop, techno, mix
out = sys.argv[1]; os.makedirs(out, exist_ok=True)
for name, f in (('house', house), ('hiphop', hiphop), ('techno', techno)):
    st = f(); m = mix(st); g = 1.0 / np.abs(m).max()
    sf.write(os.path.join(out, name + '-mix.wav'), (m * g).astype(np.float32), 48000, subtype='FLOAT')
    k = st['keys'] + st['extra']
    sf.write(os.path.join(out, name + '-stems.wav'), (np.stack([st['drums'], st['bass'], k[:, 0], k[:, 1]], 1) * g).astype(np.float32), 48000, subtype='FLOAT')
print('ok')
