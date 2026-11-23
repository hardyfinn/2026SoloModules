"""Sound check for Walker. The C++ test program renders every case of test/upright_cases.json to DIR/NNN.wav
(mono, 48 kHz, 32-bit float, NNN = case index with three digits); this compares each with the reference measurements.
   python3 tools/check_upright.py DIR          compare a C++ render
   python3 tools/check_upright.py --self       render with the Python reference and compare (must print 0.00)"""
import os, sys, json
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, os.path.join(HERE, 'proto'))
import numpy as np, soundfile as sf
from upright_features import features, compare
cases = json.load(open(os.path.join(HERE, '..', 'test', 'upright_cases.json')))
MEAN_TOL, WORST_TOL, RMS_TOL = 0.75, 3.0, 0.5
bad = 0; means = []
for i, c in enumerate(cases):
    if sys.argv[1] == '--self':
        import upright_ref as U; x = U.play(c['notes'], U.Tone(**c['tone']), seed=c['seed'], total=c['total']); sr = U.SR
    else:
        x, sr = sf.read(os.path.join(sys.argv[1], '%03d.wav' % i)); x = x if x.ndim == 1 else x[:, 0]
    assert sr == 48000
    m, w, r = compare(c['features'], features(x, sr)); means.append(m)
    ok = m <= MEAN_TOL and w <= WORST_TOL and r <= RMS_TOL; bad += not ok
    print('%s %-28s mean %.2f dB  worst %.2f dB  level %.2f dB' % ('ok  ' if ok else 'FAIL', c['name'], m, w, r))
print('%d cases, %d failed, mean difference %.2f dB (allowed: mean %.2f, worst %.1f, level %.1f)' % (len(cases), bad, float(np.mean(means)), MEAN_TOL, WORST_TOL, RMS_TOL))
sys.exit(1 if bad else 0)
