"""Writes the golden cases the C++ engine must reproduce, and the reference demo MIDI.
  python3 make_reference.py            -> ../../test/golden.json, ../../test/detect.json, ref_midi/*.mid
"""
import json, os, struct
from comper_ref import *
from presets import PROGRESSIONS, PRESETS
HERE = os.path.dirname(os.path.abspath(__file__))

def prog(name):
    for n, bps, text in PROGRESSIONS:
        if n == name:
            ch = text.split(); out = []
            for i, c in enumerate(ch):
                c1, _ = parse_chart(c); out.append((i * float(bps), c1[0][1]))
            return out, len(ch) * bps
    raise KeyError(name)

def solo_line(chart, total, bpm, nbars, seed):
    """a made-up horn line with rests, for the listening cases (deterministic, uses Rng)"""
    r = Rng(seed); spb = 60.0 / bpm; f = auto_swing(bpm); f = f / (1 + f)
    out = []; e = 0; p = 70; d = 1
    def chord_at(b):
        b %= total; cur = chart[0][1]
        for x, c in chart:
            if x <= b + 1e-9: cur = c
        return cur
    while e < nbars * 8 - 4:
        n = 5 + r.n(9)
        for k in range(n):
            if e >= nbars * 8: break
            root, q = chord_at(e // 2)
            pcs = [(root + x) % 12 for x in SCALES[q]]
            if r.u() < 0.25: d = -d
            if p > 82: d = -1
            if p < 62: d = 1
            p += d
            while p % 12 not in pcs: p += d
            beat = e // 2 + (f if e % 2 else 0.0)
            dur = 1.6 if k == n - 1 else (1 - f if e % 2 else f) * 0.95
            out.append((beat * spb, (beat + dur) * spb, p))
            e += 1
        e += 6 + r.n(5)
    return out

CASES = []
def case(name, progname, bpm, nbars, seed, external=False, solo=False, **kw):
    chart, total = prog(progname)
    sol = solo_line(chart, total, bpm, nbars, seed + 7) if solo else None
    notes, pedal, log = simulate(chart, total, bpm, nbars, seed=seed, params=Params(**kw), solo=sol, external=external)
    CASES.append(dict(name=name, chart=[[b, c[0], c[1]] for b, c in chart], total_beats=total, bpm=bpm, nbars=nbars,
                      seed=seed, external=external, solo=[list(s) for s in sol] if sol else [], params=kw,
                      notes=[[round(a, 7), round(b, 7), p, v] for a, b, p, v in notes],
                      pedal=[[round(t, 7), v] for t, v in pedal]))
    return notes, pedal, sol

def vlq(n):
    b = [n & 0x7f]; n >>= 7
    while n: b.append((n & 0x7f) | 0x80); n >>= 7
    return bytes(reversed(b))
def write_midi(path, bpm, notes, solo=None):
    ppq = 480; tps = ppq * bpm / 60.0
    tr0 = b'\x00\xff\x51\x03' + struct.pack('>I', int(60e6 / bpm))[1:] + b'\x00\xff\x2f\x00'
    chunks = [tr0]
    for ch, prog_, ns in [(0, 0, notes)] + ([(1, 66, [(a, b, p, 90) for a, b, p in solo])] if solo else []):
        ev = [(0, 0, bytes([0xC0 | ch, prog_]))]
        for a, b, p, v in ns:
            ev.append((int(a * tps), 2, bytes([0x90 | ch, p, v]))); ev.append((max(int(a * tps) + 1, int(b * tps)), 1, bytes([0x80 | ch, p, 0])))
        ev.sort(key=lambda e: (e[0], e[1])); data = b''; last = 0
        for t, _, m in ev: data += vlq(t - last) + m; last = t
        chunks.append(data + vlq(960) + b'\xff\x2f\x00')
    with open(path, 'wb') as f:
        f.write(b'MThd' + struct.pack('>IHHH', 6, 1, len(chunks), ppq))
        for c in chunks: f.write(b'MTrk' + struct.pack('>I', len(c)) + c)

if __name__ == '__main__':
    os.makedirs(os.path.join(HERE, 'ref_midi'), exist_ok=True)
    # every preset, 16 bars
    for name, d in PRESETS:
        d = dict(d); pn = d.pop('prog'); bpm = d.pop('tempo')
        n, p, s = case('preset ' + name, pn, bpm, 16, 11, solo=(d.get('listen', 0) > 0), **d)
        write_midi(os.path.join(HERE, 'ref_midi', name + '.mid'), bpm, n, s)
    # every style x groove
    for g in range(5):
        for st in range(5):
            case('grid groove%d style%d' % (g, st), 'Falling fifths', [132, 126, 64, 96, 122][g], 8, 20 + g * 5 + st, groove=g, style=st)
    # single-control sweeps
    for k, vals in [('density', [0.0, 0.25, 0.75, 1.0]), ('loose', [0.0, 0.5, 2.0, 2.5]), ('lead', [0.0, 0.5]),
                    ('out', [0.3, 0.6, 1.0]), ('spread', [0.0, 0.5, 1.0]), ('register', [60.0, 64.0, 76.0, 79.0]),
                    ('voices', [2, 3, 4]), ('roots', [1, 2]), ('swing', [-1.0, 1.0]), ('repeat', [0.0, 1.0]),
                    ('push', [0.0, 1.0]), ('pedal', [0.5]), ('timing', [0.0, 2.0]), ('roll', [0.0, 2.0]),
                    ('weight', [0.0, 2.0]), ('slop', [0.0, 2.0])]:
        for i, v in enumerate(vals):
            case('sweep %s=%s' % (k, v), 'Jazz blues in F', 126, 8, 50 + i, **{k: v})
    for bpm in (60, 90, 180, 240): case('tempo %d' % bpm, 'Blues (12 steps)', bpm, 12, 70)
    for L in (0.3, 0.6, 1.0):
        case('listen %.1f' % L, 'Falling fifths', 132, 16, 80, solo=True, listen=L, density=0.6)
    case('listen out', 'ii-V-i minor', 132, 16, 81, solo=True, listen=0.8, out=0.8, density=0.7)
    for g in range(5):
        case('external groove%d' % g, 'Falling fifths', [132, 126, 64, 96, 122][g], 8, 90 + g, external=True, groove=g, density=0.6)
    case('external out roots', 'Jazz blues in F', 120, 8, 96, external=True, out=0.7, roots=2, pedal=0.5)
    for s in range(1, 9): case('seed %d' % s, 'Major then minor', 140, 8, s, out=0.4, style=4, density=0.6)
    os.makedirs(os.path.join(HERE, '..', '..', 'test'), exist_ok=True)
    json.dump(CASES, open(os.path.join(HERE, '..', '..', 'test', 'golden.json'), 'w'), separators=(',', ':'))
    # chord recognition table
    r = Rng(5); det = []
    for i in range(400):
        k = 1 + r.n(6); ns = sorted(set(40 + r.n(40) for _ in range(k)))
        lq = r.n(10); d = detect_chord(ns, lq); det.append([ns, lq, list(d)])
    for root in range(12):
        for q in range(10):
            for rec in RECIPES[q][ROOTLESS]:
                ns = [48 + root] + realize(root, rec, 55, 66)[0]
                d = detect_chord(ns, DOM); det.append([ns, DOM, list(d)])
    json.dump(det, open(os.path.join(HERE, '..', '..', 'test', 'detect.json'), 'w'), separators=(',', ':'))
    print(len(CASES), 'cases', sum(len(c['notes']) for c in CASES), 'notes;', len(det), 'detect cases')
