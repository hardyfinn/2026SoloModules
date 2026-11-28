"""Bass player: design prototype of the line engine (Fable, 2026-11-23).
Reads the same chart as Comper (beat -> (root, quality)) and writes one bass line: walking four, two-feel,
ballad, bossa, soul and house. Uses Comper's random generator so a C++ port can reproduce it note for note."""
import sys, math
sys.path.insert(0, '/home/user/2026solomodules/ComperFable/tools/proto')
from comper_ref import Rng, SCALES, MAJ, MAJ6, M7, M6, MMAJ, DOM, ALT, HDIM, DIM, SUS, auto_swing, parse_chart
CHORD = {MAJ: [0, 4, 7, 11], MAJ6: [0, 4, 7, 9], M7: [0, 3, 7, 10], M6: [0, 3, 7, 9], MMAJ: [0, 3, 7, 11], DOM: [0, 4, 7, 10],
         ALT: [0, 4, 8, 10], HDIM: [0, 3, 6, 10], DIM: [0, 3, 6, 9], SUS: [0, 5, 7, 10]}
LO, HI = 28, 55                         # E1 .. G3
WALK, TWO, BALLAD, BOSSA, SOUL, HOUSE = range(6)
FEEL_NAMES = ['WALK', 'TWO', 'BALLAD', 'BOSSA', 'SOUL', 'HOUSE']
STEP_COST = {0: 6.0, 1: 0.6, 2: 0.0, 3: 0.45, 4: 0.55, 5: 1.2, 6: 2.4, 7: 1.5, 8: 2.6, 9: 2.6, 10: 2.8, 11: 3.5, 12: 2.0}
# sixteenth-note cells: (slot, role, length in sixteenths, weight). R root, O octave, 5 fifth, 7 seventh, A approach, G ghost
SOUL_CELLS = [
    [(0, 'R', 2.5, 1.0), (3, 'R', 1, 0.6), (6, '5', 1.5, 0.75), (8, 'R', 2, 0.9), (11, 'G', 1, 0.4), (12, '7', 1.5, 0.7), (14, 'A', 1.5, 0.8)],
    [(0, 'R', 3, 1.0), (4, 'G', 1, 0.4), (6, 'O', 1, 0.8), (7, 'R', 2, 0.7), (10, 'R', 1.5, 0.85), (13, '5', 1, 0.6), (15, 'A', 1, 0.75)],
    [(0, 'R', 1.5, 1.0), (2, 'O', 1, 0.7), (4, 'G', 1, 0.4), (6, 'R', 2, 0.85), (10, '7', 1.5, 0.7), (12, '5', 2, 0.75), (15, 'A', 1, 0.7)]]
HOUSE_CELLS = [
    [(2, 'R', 1.6, 0.9), (6, 'R', 1.6, 0.85), (10, 'R', 1.6, 0.9), (14, 'O', 1.6, 0.8)],
    [(0, 'R', 1.5, 1.0), (3, 'R', 1, 0.7), (6, 'O', 1.5, 0.8), (10, 'R', 1.5, 0.9), (14, '7', 1.5, 0.75)],
    [(0, 'R', 1, 1.0), (2, 'R', 1, 0.7), (4, 'R', 1, 0.8), (6, 'O', 1, 0.75), (8, 'R', 1, 0.95), (10, 'R', 1, 0.7), (12, '5', 1, 0.8), (14, 'A', 1, 0.8)]]

class BP:
    def __init__(self, **kw):
        self.feel = WALK
        self.activity = 0.5      # 0 bare .. 1 busy (skips, ghost notes, runs, pickups)
        self.register = 40.0     # where the line likes to sit (MIDI); 40 = E2
        self.roam = 0.5          # how far the line travels up and down the neck
        self.out = 0.2           # chromatic passing notes and approaches
        self.repeat = 0.7        # soul / house / bossa: keep the bar figure
        self.loose = 1.0         # master feel
        self.timing = 1.0; self.weight = 1.0; self.slop = 1.0; self.pitch = 1.0   # trims
        self.ahead = 0.5         # 0 on the beat .. 1 well on top of it
        self.swing = 0.0
        for k, v in kw.items():
            if not hasattr(self, k): raise KeyError(k)
            setattr(self, k, v)

def near(pc, ref, lo=LO, hi=HI):
    c = [n for n in range(lo, hi + 1) if n % 12 == pc % 12]
    return min(c, key=lambda n: (abs(n - ref), n))

class Player:
    def __init__(self, chart, total_beats, bpm, seed=1, p=None):
        self.chart = chart; self.total = total_beats; self.bpm = bpm; self.p = p or BP(); self.r = Rng(seed)
        self.last = None; self.dirn = 1; self.cell = None; self.stats = dict(app={}, steps=[], root1=0, bars=0)
        self.cperiod = 5 + self.r.n(5); self.cphase = self.r.u()
    def chord_at(self, beat):
        b = beat % self.total; cur = self.chart[0][1]
        for x, c in self.chart:
            if x <= b + 1e-6: cur = c
        return cur
    def centre(self, beat):
        return self.p.register + 6.5 * self.p.roam * math.sin(2 * math.pi * (beat / 4.0 / self.cperiod + self.cphase))
    def target(self, beat, ref, same_as_before):
        """the note on a chord arrival: the root, unless the chord is only continuing"""
        root, q = self.chord_at(beat); pcs = [root]
        if same_as_before and self.r.u() < 0.25 + 0.45 * self.p.roam:
            pcs = [root + CHORD[q][1], root + CHORD[q][2], root] if self.r.u() < 0.6 else [root]
            pcs = [pcs[self.r.n(len(pcs))]]
        cen = self.centre(beat); best = None
        for n in range(LO, HI + 1):
            if n % 12 != pcs[0] % 12: continue
            d = abs(n - ref)
            c = 0.5 * abs(n - cen) + (2.5 if d == 0 else 0.0) + (0.25 * (d - 7) if d > 7 else 0.0) + self.r.u() * (0.6 + 2.0 * self.p.roam)
            if best is None or c < best[0]: best = (c, n)
        return best[1]
    def approach(self, t2, prev, beat):
        """the note before an arrival"""
        root, q = self.chord_at(beat); nroot, nq = self.chord_at(beat + 1)
        sc = set((root + x) % 12 for x in SCALES[q]) | set((nroot + x) % 12 for x in SCALES[nq])
        o = self.p.out; opts = []
        for d, kind, w in ((-1, 'chromatic below', 0.9 + o), (1, 'chromatic above', 0.75 + o), (-2, 'scale below', 1.0), (2, 'scale above', 0.9),
                           (7, 'fifth above', 0.45), (-5, 'fifth below', 0.6)):
            n = t2 + d
            if n < LO or n > HI or n == prev: continue
            if abs(d) == 2 and n % 12 not in sc: continue
            # reward arriving in the direction the line is already going
            w *= 1.35 if (n - prev) * (t2 - n) > 0 else 1.0
            w *= 0.5 if abs(n - prev) > 7 else 1.0
            opts.append((w, n, kind))
        if not opts: return t2 + (1 if t2 < HI else -1), 'chromatic above'
        x = self.r.u() * sum(w for w, _, _ in opts)
        for w, n, kind in opts:
            x -= w
            if x <= 0: return n, kind
        return opts[-1][1], opts[-1][2]
    def passing(self, a, b, beat0, count):
        """count notes between a (already played) and b (the approach note): cheapest path through chord and scale tones"""
        if count <= 0: return []
        cen = self.centre(beat0); o = self.p.out; cands = []
        for i in range(count):
            root, q = self.chord_at(beat0 + i); ch = set((root + x) % 12 for x in CHORD[q]); sc = set((root + x) % 12 for x in SCALES[q])
            strong = ((beat0 + i) % 2 == 0)
            row = []
            for n in range(max(LO, min(a, b) - 9), min(HI, max(a, b) + 9) + 1):
                pc = n % 12
                c = 0.0 if pc in ch else ((2.2 if strong else 0.9) if pc in sc else (5.0 if strong else 3.4 - 2.6 * o))
                c += 0.035 * (n - cen) ** 2 / 4.0 + self.r.u() * (0.5 + 1.4 * self.p.roam)
                row.append((n, c))
            cands.append(row)
        def st(x, y): return STEP_COST.get(abs(x - y), 4.5)
        best = None
        if count == 1:
            for n, c in cands[0]:
                t = c + st(a, n) + st(n, b) + (0.4 if (n - a) * (b - n) < 0 else 0)
                if best is None or t < best[0]: best = (t, [n])
        else:
            for n1, c1 in cands[0]:
                s1 = c1 + st(a, n1)
                for n2, c2 in cands[1]:
                    t = s1 + c2 + st(n1, n2) + st(n2, b) + (0.4 if (n1 - a) * (n2 - n1) < 0 else 0) + (0.4 if (n2 - n1) * (b - n2) < 0 else 0)
                    if n2 == a: t += 1.5
                    if best is None or t < best[0]: best = (t, [n1, n2])
        return best[1]
    # ---------------------------------------------------------------- feels
    def walk(self, nbars):
        p = self.p; r = self.r; sw = max(1.0, auto_swing(self.bpm) + p.swing); f = sw / (1 + sw); ev = []
        nb = nbars * 4; starts = [b for b in range(nb + 1) if b % 4 == 0 or self.chord_at(b) != self.chord_at(b - 1)]
        t1 = self.target(0, self.centre(0), False)
        for si in range(len(starts) - 1):
            b0, b1 = starts[si], starts[si + 1]; n = b1 - b0
            same = self.chord_at(b1) == self.chord_at(b0)
            t2 = self.target(b1, t1, same)
            line = [t1]
            if n >= 2:
                ap, kind = self.approach(t2, t1, b1 - 1); self.stats['app'][kind] = self.stats['app'].get(kind, 0) + 1
                line += self.passing(t1, ap, b0 + 1, n - 2) + [ap]
            for i, pitch in enumerate(line):
                b = b0 + i; e = dict(beat=float(b), len=0.94, pitch=pitch, w=1.0 + (0.07 if b % 2 else 0.0) + (0.04 if b % 4 == 0 else 0.0), kind='walk')
                ev.append(e)
                nxt = line[i + 1] if i + 1 < len(line) else t2
                u = r.u(); act = p.activity
                strong_next = (b + 1) % 2 == 0
                if u < (0.02 + 0.42 * max(0.0, act - 0.3)) * (1.3 if strong_next else 0.8):
                    # a skip: a short note on the swung upbeat, ghosted, repeated or leaning into the next note
                    v = r.u()
                    if v < 0.45: ev.append(dict(beat=b + f, len=(1 - f) * 0.8, pitch=pitch, w=0.5, kind='skip', dead=True)); e['len'] = f * 0.95
                    elif v < 0.75: ev.append(dict(beat=b + f, len=(1 - f) * 0.9, pitch=pitch, w=0.62, kind='skip')); e['len'] = f * 0.9
                    else:
                        lean = nxt - 1 if nxt > pitch else nxt + 1
                        if LO <= lean <= HI and lean != pitch: ev.append(dict(beat=b + f, len=(1 - f) * 0.95, pitch=lean, w=0.66, kind='skip')); e['len'] = f * 0.95
                elif act > 0.62 and u > 1 - 0.3 * (act - 0.62) and abs(nxt - pitch) in (3, 4):
                    # a triplet run filling a third
                    d = 1 if nxt > pitch else -1; e['len'] = 0.32
                    for j in (1, 2): ev.append(dict(beat=b + j / 3.0, len=0.32, pitch=pitch + d * j, w=0.7, kind='run'))
            t1 = t2
        return ev
    def two(self, nbars, ballad=False):
        p = self.p; r = self.r; sw = max(1.0, auto_swing(self.bpm) + p.swing); f = sw / (1 + sw); ev = []
        last = self.target(0, self.centre(0), False)
        for bar in range(nbars):
            b = bar * 4; c1 = self.chord_at(b); c3 = self.chord_at(b + 2); cn = self.chord_at(b + 4)
            n1 = near(c1[0], last if bar else self.centre(b), LO, 47)
            if abs(n1 - self.centre(b)) > 9: n1 = near(c1[0], self.centre(b), LO, 47)
            if c3 != c1: n3 = near(c3[0], n1, LO, 47)
            else:
                fifth = c1[0] + CHORD[c1[1]][2]; third = c1[0] + CHORD[c1[1]][1]
                pc = fifth if r.u() > 0.22 * (0.5 + p.roam) else third
                n3 = near(pc, n1 - 3 if r.u() < 0.6 else n1 + 4, LO, 50)
                if n3 == n1: n3 = near(pc, n1 + 6, LO, 50)
            whole = ballad and c3 == c1 and r.u() < 0.55 - 0.6 * p.activity
            ev.append(dict(beat=float(b), len=3.8 if whole else 1.85, pitch=n1, w=1.0, kind='two', vib=ballad))
            if not whole: ev.append(dict(beat=b + 2.0, len=1.85, pitch=n3, w=0.9, kind='two', vib=ballad))
            last = n3 if not whole else n1
            nn = near(cn[0], last, LO, 47)
            if r.u() < (0.12 if ballad else 0.22) + 0.6 * p.activity:
                ap, kind = self.approach(nn, last, b + 3); self.stats['app'][kind] = self.stats['app'].get(kind, 0) + 1
                late = r.u() < (0.3 if ballad else 0.5)
                if not whole: ev[-1]['len'] = 1.45 if late else 0.95
                else: ev[-1]['len'] = 3.4 if late else 2.9
                ev.append(dict(beat=b + (3 + f if late else 3.0), len=(1 - f) if late else 0.95, pitch=ap, w=0.7, kind='pickup')); last = ap
            if not ballad and p.activity > 0.5 and r.u() < 0.5 * (p.activity - 0.5):
                ev.append(dict(beat=b + 1 + f, len=(1 - f) * 0.8, pitch=n1, w=0.45, kind='skip', dead=True))
        return ev
    def bossa(self, nbars):
        p = self.p; r = self.r; ev = []; last = self.target(0, self.centre(0), False)
        for bar in range(nbars):
            b = bar * 4; c1 = self.chord_at(b); c3 = self.chord_at(b + 2); cn = self.chord_at(b + 4)
            n1 = near(c1[0], self.centre(b) - 2)
            fpc = (c3[0] if c3 != c1 else c1[0] + CHORD[c1[1]][2])
            n3 = near(fpc, n1 - 4 if c3 == c1 and r.u() < 0.65 else n1 + 3)
            nn = near(cn[0], self.centre(b + 4) - 2)
            ev.append(dict(beat=b + 0.0, len=1.4, pitch=n1, w=1.0, kind='bossa'))
            if r.u() < 0.35 + 0.6 * p.activity: ev.append(dict(beat=b + 1.5, len=0.45, pitch=n3 if r.u() < 0.6 else n1, w=0.62, kind='pickup'))
            ev.append(dict(beat=b + 2.0, len=1.4, pitch=n3, w=0.88, kind='bossa'))
            if r.u() < 0.5 + 0.5 * p.activity: ev.append(dict(beat=b + 3.5, len=0.5, pitch=nn, w=0.72, kind='pickup')); ev[-2]['len'] = 1.4
            last = nn
        return ev
    def cells(self, nbars, bank):
        p = self.p; r = self.r; ev = []; cell = bank[r.n(len(bank))]; kept = None
        for bar in range(nbars):
            if bar and bar % 2 == 0 and r.u() > p.repeat: cell = bank[r.n(len(bank))]; kept = None
            if kept is None or r.u() > 0.5 + 0.5 * p.repeat:
                kept = [h for h in cell if h[0] == cell[0][0] or r.u() < min(1.0, 0.45 + p.activity)]
            for slot, role, ln, w in kept:
                beat = bar * 4 + slot / 4.0; root, q = self.chord_at(beat); base = near(root, self.centre(bar * 4) - 3, LO, 40)
                if role == 'R' or role == 'G': n = base
                elif role == 'O': n = base + 12
                elif role == '5': n = base + CHORD[q][2] - (12 if base + CHORD[q][2] > base + 6 and r.u() < 0.5 and base + CHORD[q][2] - 12 >= LO else 0)
                elif role == '7': n = base + CHORD[q][3] - (12 if base + CHORD[q][3] - 12 >= LO and r.u() < 0.4 else 0)
                else:
                    nroot = self.chord_at(bar * 4 + 4)[0]; tgt = near(nroot, base, LO, 40); n = tgt + (-1 if r.u() < 0.6 else 2)
                    if n == tgt + 2 and n % 12 not in [(nroot + x) % 12 for x in SCALES[self.chord_at(bar * 4 + 4)[1]]]: n = tgt - 2
                    n = max(LO, n)
                ev.append(dict(beat=beat, len=ln / 4.0 * 0.9, pitch=n, w=w, kind='cell', dead=(role == 'G')))
        return ev
    def line(self, nbars):
        f = self.p.feel
        ev = (self.walk(nbars) if f == WALK else self.two(nbars) if f == TWO else self.two(nbars, True) if f == BALLAD else
              self.bossa(nbars) if f == BOSSA else self.cells(nbars, SOUL_CELLS if f == SOUL else HOUSE_CELLS))
        ev.sort(key=lambda e: e['beat']); return ev
    def perform(self, ev):
        """the hands: timing on top of the beat, weight, intonation, slides, dead notes. Returns notes for upright.play"""
        p = self.p; r = self.r; spb = 60.0 / self.bpm; L = p.loose; out = []; drift = 0.0; phrase = 0.0; prev = None
        for i, e in enumerate(ev):
            drift = 0.9 * drift + 0.0016 * r.g() * L * p.timing
            if int(e['beat']) % 8 == 0 and e['beat'] == int(e['beat']): phrase = 0.05 * r.g() * L * p.weight
            t = e['beat'] * spb - 0.016 * p.ahead * min(1.5, L + 0.25) + drift + 0.0045 * r.g() * L * p.timing
            vel = 0.74 * e['w'] + phrase + 0.045 * r.g() * L * p.weight
            dur = e['len'] * spb
            dead = e.get('dead', False)
            leap = abs(e['pitch'] - prev) if prev is not None else 0
            if leap >= 5 and not dead and i: out[-1]['dur'] = max(0.06, out[-1]['dur'] - min(0.05, 0.12 * spb))      # the hand has to travel
            if not dead and e['kind'] == 'walk' and r.u() < 0.018 * L * p.slop: dead = True                              # a note that did not speak
            scoop = 0.0
            if not dead and (leap >= 5 or r.u() < 0.1 * L): scoop = (18 + 40 * r.u()) * min(1.5, L) * p.pitch
            cents = 4.5 * r.g() * L * p.pitch + (3.0 * L * p.pitch if e['pitch'] > 47 else 0.0)                          # high notes run a touch sharp
            vib = (5 + 5 * r.u()) * min(1.5, L) if (e.get('vib') and dur > 0.8) or dur > 1.3 else 0.0
            out.append(dict(t=t, pitch=e['pitch'], vel=max(0.12, min(1.0, vel)), dur=dur, dead=dead, scoop=scoop, cents=cents, vib=vib, kind=e['kind'], beat=e['beat']))
            prev = e['pitch']
        return out

def play_line(chart, total_beats, bpm, nbars, seed=1, p=None):
    pl = Player(chart, total_beats, bpm, seed, p); ev = pl.line(nbars); return pl.perform(ev), pl

def describe(notes, pl, nbars):
    """numbers to check a line by"""
    w = [n for n in notes if n['kind'] == 'walk']; res = {}
    if w:
        root1 = sum(1 for n in w if n['beat'] % 4 == 0 and n['pitch'] % 12 == pl.chord_at(n['beat'])[0]); b1 = sum(1 for n in w if n['beat'] % 4 == 0)
        steps = [abs(a['pitch'] - b['pitch']) for a, b in zip(w, w[1:])]
        ct = sum(1 for n in w if (n['pitch'] - pl.chord_at(n['beat'])[0]) % 12 in CHORD[pl.chord_at(n['beat'])[1]])
        res = dict(beat1_root='%d/%d' % (root1, b1), chord_tones='%d%%' % round(100.0 * ct / len(w)),
                   steps_1_2='%d%%' % round(100.0 * sum(1 for s in steps if s in (1, 2)) / len(steps)),
                   thirds='%d%%' % round(100.0 * sum(1 for s in steps if s in (3, 4)) / len(steps)),
                   leaps_5plus='%d%%' % round(100.0 * sum(1 for s in steps if s >= 5) / len(steps)),
                   repeats=sum(1 for s in steps if s == 0), mean_step='%.2f' % (sum(steps) / len(steps)))
    res.update(notes=len(notes), low=min(n['pitch'] for n in notes), high=max(n['pitch'] for n in notes),
               extras=sum(1 for n in notes if n['kind'] in ('skip', 'run', 'pickup')), dead=sum(1 for n in notes if n['dead']), approaches=dict(pl.stats['app']))
    return res

if __name__ == '__main__':
    NN = ['C', 'Db', 'D', 'Eb', 'E', 'F', 'Gb', 'G', 'Ab', 'A', 'Bb', 'B']
    chart, total = parse_chart('F7 | Bb7 | F7 | F7 | Bb7 | Bdim7 | F7 | D7 | Gm7 | C7 | F7 D7 | Gm7 C7')
    for act in (0.2, 0.5, 0.9):
        notes, pl = play_line(chart, total, 120, 24, seed=3, p=BP(activity=act))
        print('activity', act, describe(notes, pl, 24))
    notes, pl = play_line(chart, total, 120, 12, seed=3)
    for bar in range(12):
        print('%-10s' % '/'.join(NN[pl.chord_at(bar * 4 + k)[0]] for k in (0, 2)), ' '.join('%s%d%s' % (NN[n['pitch'] % 12], n['pitch'] // 12 - 1, {'skip': "'", 'run': '~'}.get(n['kind'], '')) for n in notes if bar * 4 <= n['beat'] < bar * 4 + 4))
    for feel in (TWO, BALLAD, BOSSA, SOUL, HOUSE):
        notes, pl = play_line(chart, total, 120, 12, seed=3, p=BP(feel=feel)); print(FEEL_NAMES[feel], describe(notes, pl, 12))
