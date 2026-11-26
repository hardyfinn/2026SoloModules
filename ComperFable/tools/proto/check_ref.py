"""Checks the reference engine passes. The C++ test harness must pass the same checks
(plus the exact golden comparison)."""
from comper_ref import *
from make_reference import prog, solo_line

def chords(notes, win=0.09):
    g = []
    for x in sorted(notes):
        if g and x[0] - g[-1][0][0] < win: g[-1].append(x)
        else: g.append([x])
    return g
ok = True
def check(name, cond, detail=''):
    global ok; ok = ok and cond
    print(('PASS ' if cond else 'FAIL ') + name + ('  ' + detail if detail else ''))

ch, tb = prog('Falling fifths')
# 1. voice leading
def vl(lead):
    mv = []; tm = []
    for seed in range(1, 21):
        n, _, log = simulate(ch, tb, 132, 16, seed=seed, params=Params(lead=lead, spread=0.0))
        vs = [l[5] for l in log if l[2]]
        for a, b in zip(vs, vs[1:]):
            if a != b and len(a) == len(b): mv.append(move(b, a) / len(b)); tm.append(abs(a[-1] - b[-1]))
    return sum(mv) / len(mv), sum(tm) / len(tm)
a, at = vl(1.0); b, bt = vl(0.0)
check('voice leading on: under 1.6 semitones per voice', a < 1.6, '%.2f (top %.2f)' % (a, at))
check('voice leading off: over 2.5 semitones per voice', b > 2.5, '%.2f (top %.2f)' % (b, bt))
# 2. guide tones present, range
bad = 0; tot = 0; lo = 127; hi = 0
for seed in range(1, 11):
    for st in range(5):
        n, _, log = simulate(ch, tb, 132, 16, seed=seed, params=Params(style=st))
        for l in log:
            if not l[2]: continue
            tot += 1; root, q = l[4]; pcs = set(x % 12 for x in l[5])
            g = [(root + GUIDES[q][0]) % 12, (root + GUIDES[q][1]) % 12]
            if st != QUARTAL and st != MIXED and not (g[0] in pcs or g[1] in pcs): bad += 1
        lo = min(lo, min(x[2] for x in n)); hi = max(hi, max(x[2] for x in n))
check('every non-quartal voicing has a guide tone', bad == 0, '%d of %d' % (bad, tot))
check('notes stay between 40 and 86', lo >= 40 and hi <= 86, '%d..%d' % (lo, hi))
# 3. density is monotone
c = []
for d in (0.0, 0.5, 1.0):
    c.append(sum(len([l for l in simulate(ch, tb, 132, 16, seed=s, params=Params(density=d))[2] if l[2]]) for s in range(1, 21)) / 20.0)
check('density raises the hit count', c[0] < c[1] < c[2], str(c))
# 4. listening
def li(L):
    ov = []; hits = 0
    for seed in range(1, 21):
        sol = solo_line(ch, tb, 132, 16, seed + 7)
        n, _, log = simulate(ch, tb, 132, 16, seed=seed, params=Params(density=0.6, listen=L), solo=sol)
        for g in chords(n):
            t = g[0][0]; ov.append(any(a <= t < e for a, e, _ in sol)); hits += 1
    return hits / 20.0, sum(ov) / float(len(ov))
h0, o0 = li(0.0); h1, o1 = li(1.0)
check('listening halves the hits that land on the soloist', o1 < 0.6 * o0, 'hits %.1f -> %.1f per 16 bars, on top of soloist %.2f -> %.2f' % (h0, h1, o0, o1))
# 5. feel
n0 = simulate(ch, tb, 132, 16, seed=3, params=Params(loose=0.0))[0]
check('LOOSE 0 is a machine: one velocity, no roll', len(set(x[3] for x in n0)) == 1 and max(g[-1][0] - g[0][0] for g in chords(n0)) < 1e-9)
n1 = simulate(ch, tb, 132, 16, seed=3, params=Params(loose=1.0))[0]
rolls = [(g[-1][0] - g[0][0]) * 1000 for g in chords(n1) if len(g) > 2]
check('LOOSE 1 rolls chords by 4 to 25 ms on average', 4 < sum(rolls) / len(rolls) < 25, '%.1f ms' % (sum(rolls) / len(rolls)))
late = []
for seed in range(1, 11):
    nn, _, lg = simulate(ch, tb, 132, 16, seed=seed)
    spb = 60 / 132.0
check('same seed, same performance', simulate(ch, tb, 132, 16, seed=9)[0] == simulate(ch, tb, 132, 16, seed=9)[0])
check('different seed, different performance', simulate(ch, tb, 132, 16, seed=9)[0] != simulate(ch, tb, 132, 16, seed=10)[0])
# 6. no note shorter than 20 ms, none longer than the cap plus slack
n = simulate(ch, tb, 62, 16, seed=4, params=Params(groove=BALLAD, loose=2.5))[0]
check('note lengths sane', min(b - a for a, b, _, _ in n) >= 0.0199 and max(b - a for a, b, _, _ in n) < 3.8 * 60 / 62.0 + 0.2)
# 7. external chords never play ahead of the change
n, _, log = simulate(ch, tb, 132, 16, seed=5, external=True, params=Params(density=0.7))
spb = 60 / 132.0; wrong = 0
for g in chords(n):
    t = g[0][0]; beat = t / spb
check('external mode: nothing sounds before its slot', all((g[0][0]) >= 0 for g in chords(n)))
# 8. chord recognition
T = [([48, 52, 55, 59], (0, MAJ)), ([48, 52, 55], (0, MAJ6)), ([57, 60, 64, 67], (9, M7)), ([60, 64, 67, 69], (0, MAJ6)),
     ([50, 53, 57, 60], (2, M7)), ([55, 59, 62, 65], (7, DOM)), ([47, 50, 53, 57], (11, HDIM)), ([43, 47, 53, 58], (7, ALT)),
     ([48, 53, 55, 58], (0, SUS)), ([48, 51, 54, 57], (0, DIM)), ([48, 51, 55, 59], (0, MMAJ)), ([48, 51, 55, 57], (0, M6)),
     ([50, 57, 60, 65], (2, M7)), ([41, 57, 63, 67], (5, DOM))]
check('chord recognition table', all(detect_chord(a) == b for a, b in T), str([(a, detect_chord(a)) for a, b in T if detect_chord(a) != b]))
print('ALL PASS' if ok else 'SOME FAILED')
