"""Jazz piano comping engine: design prototype (Fable, 2026-11-23).
Notes out only. Everything is causal except chord lookahead of one beat
(needed for anticipations; the module gets it from its own chord chart).
"""
import random, math, re, struct

NOTE = {'C':0,'D':2,'E':4,'F':5,'G':7,'A':9,'B':11}

def parse_chord(s):
    m = re.match(r'^([A-G])([#b]?)(.*)$', s)
    root = (NOTE[m.group(1)] + {'#':1,'b':-1,'':0}[m.group(2)]) % 12
    q = m.group(3)
    table = [('maj7','maj'),('M7','maj'),('6/9','maj6'),('m7b5','hdim'),('mMaj7','mmaj'),
             ('m6','m6'),('m9','m7'),('m11','m7'),('m7','m7'),('dim7','dim'),('dim','dim'),
             ('7alt','alt'),('7b9','alt'),('7#9','alt'),('7sus','sus'),('sus','sus'),
             ('13','dom'),('9','dom'),('7','dom'),('6','maj6'),('m','m7'),('','maj')]
    for k, v in table:
        if q == k: return (root, v)
    raise ValueError(s)

def parse_chart(text, beats_per_bar=4):
    """'Dm7 G7 | Cmaj7' -> list of (beat, (root, quality)), total beats"""
    out = []
    bars = [b.strip() for b in text.split('|') if b.strip()]
    for i, b in enumerate(bars):
        ch = b.split()
        for j, c in enumerate(ch):
            out.append((i*beats_per_bar + j*beats_per_bar/len(ch), parse_chord(c)))
    return out, len(bars)*beats_per_bar

# ---------------------------------------------------------------- voicings
# Semitones above the root, bottom to top. No roots: the bass has them.
R = {
 'maj': dict(shell=[[4,11],[11,4],[4,11,2],[11,4,7]],
             rootless=[[4,7,11,2],[11,2,4,7],[4,9,11,2]],
             quartal=[[4,9,2],[9,2,7],[11,4,9]],
             spread=[[11,4,9,2,7],[4,9,2,7,11],[4,11,2,7]],
             color=[[4,11,2,6],[11,2,4,6,9]]),          # #11
 'maj6':dict(shell=[[4,9],[9,4],[4,9,2]],
             rootless=[[4,7,9,2],[9,2,4,7]],
             quartal=[[4,9,2],[9,2,7]],
             spread=[[4,9,2,7],[9,4,7,2]]),
 'm7':  dict(shell=[[3,10],[10,3],[3,10,2],[10,3,7]],
             rootless=[[3,7,10,2],[10,2,3,7]],
             quartal=[[5,10,3],[7,0,5],[2,7,0]],
             spread=[[0,5,10,3,7],[3,10,2,5,7],[10,3,7,2,5]]),
 'm6':  dict(shell=[[3,9],[9,3],[3,9,2]],
             rootless=[[3,7,9,2],[9,2,3,7]],
             quartal=[[3,9,2],[9,2,7]],
             spread=[[3,9,2,7],[9,3,7,2]]),
 'mmaj':dict(shell=[[3,11],[11,3]],rootless=[[3,7,11,2],[11,2,3,7]],
             quartal=[[3,7,11,2]],spread=[[3,11,2,7]]),
 'dom': dict(shell=[[4,10],[10,4],[4,10,2],[10,4,9]],
             rootless=[[4,9,10,2],[10,2,4,9]],
             quartal=[[10,4,9],[4,9,2]],
             spread=[[10,4,9,2,7],[4,10,2,7,9]],
             color=[[4,10,2,6,9],[10,4,9,2,6]]),         # upper-structure II triad
 'alt': dict(shell=[[4,10,3],[10,4,8]],
             rootless=[[4,8,10,3],[10,1,4,8]],
             quartal=[[10,3,8],[4,10,3]],
             spread=[[4,10,8,0,3],[10,4,8,1,3]]),         # bVI triad on top
 'hdim':dict(shell=[[3,10],[10,3],[6,10,3]],
             rootless=[[3,6,10,0],[10,0,3,6],[6,10,3,5]],
             quartal=[[5,10,3],[6,10,3,5]],
             spread=[[6,10,3,5],[3,10,5,6,0]]),
 'dim': dict(shell=[[3,9],[9,3],[6,0]],
             rootless=[[3,6,9,0],[9,0,3,6],[6,9,0,3]],
             quartal=[[3,6,9,0]],spread=[[3,9,0,6],[9,3,6,0]]),
 'sus': dict(shell=[[5,10],[10,5,2]],
             rootless=[[5,9,10,2],[10,2,5,9]],
             quartal=[[10,2,5],[2,5,10]],
             spread=[[10,5,9,2,7],[5,10,2,7,9]]),
}
WINDOW = dict(shell=(48,64), rootless=(50,67), quartal=(52,68), spread=(44,58), color=(46,60))

def realize(root, recipe, lo, hi):
    out = []
    pc0 = (root + recipe[0]) % 12
    for b in range(lo, hi+1):
        if b % 12 != pc0: continue
        v = [b]
        for d in recipe[1:]:
            n = v[-1] + 1
            while n % 12 != (root + d) % 12: n += 1
            v.append(n)
        out.append(v)
    return out

def mud(v):
    """low interval limits: penalty for close intervals low down"""
    p = 0
    for a, b in zip(v, v[1:]):
        if b-a <= 2 and a < 55: p += 6
        if b-a <= 4 and a < 47: p += 6
        if b-a == 1: p += 2
    if v[-1] > 79: p += 3*(v[-1]-79)
    if v[-1] < 59: p += 2*(59-v[-1])
    return p

def move(v, p):
    if p is None: return 0
    if len(v) == len(p): return sum(abs(a-b) for a, b in zip(v, p))
    return sum(min(abs(a-b) for b in p) for a in v) * max(len(v),len(p))/len(v)

class Voicer:
    def __init__(self, rng, style='rootless', lead=True, out=0.0, vary=0.3):
        self.rng, self.style, self.lead, self.out, self.vary = rng, style, lead, out, vary
        self.prev = None; self.prev_key = None; self.target = 69.0
    def candidates(self, chord):
        root, q = chord
        tab = R[q]
        recs = [(r, self.style) for r in tab[self.style]]
        if 'color' in tab and self.rng.random() < self.out:
            recs += [(r, 'color') for r in tab['color']]
        c = []
        for r, st in recs:
            c += realize(root, r, *WINDOW[st])
        return c
    def pick(self, chord, shift=0):
        """shift: chromatic side-slip in semitones (voiced as the real chord, then moved)"""
        rng = self.rng
        self.target += rng.gauss(0, 0.8)
        self.target = min(76, max(64, self.target))
        c = self.candidates(chord)
        if not self.lead:
            v = min(c, key=lambda v: abs(v[0]-WINDOW[self.style][0]-2) + (0 if v == c[0] else 0))
            v = realize(chord[0], R[chord[1]][self.style][0], WINDOW[self.style][0]-3, WINDOW[self.style][0]+8)[0]
        else:
            same = (chord == self.prev_key)
            def cost(v):
                k = move(v, self.prev) + 1.5*(abs(v[-1]-self.prev[-1]) if self.prev else 0)
                k += 1.1*abs(v[-1]-self.target) + mud(v)
                if same and v == self.prev: k -= 4 if rng.random() < 0.7 else -6
                return k + rng.random()*6*self.vary
            v = min(c, key=cost)
        self.prev, self.prev_key = v, chord
        return [n+shift for n in v]

# ---------------------------------------------------------------- rhythm
# cells: list of (slot, 'S' short | 'L' long). 8 slots = eighths, 16 = sixteenths.
CELLS = {
 'swing': dict(slots=8, sparse=[[],[(0,'L')],[(7,'L')],[(3,'S')],[(3,'L')]],
               medium=[[(0,'S'),(3,'L')],[(1,'S'),(4,'L')],[(3,'L'),(7,'L')],[(2,'S'),(5,'L')],
                       [(1,'L'),(5,'S')],[(3,'S'),(7,'L')],[(0,'L'),(7,'L')]],
               busy=[[(0,'S'),(3,'S'),(6,'S')],[(3,'S'),(5,'S'),(7,'L')],[(1,'S'),(3,'L'),(7,'L')],
                     [(0,'S'),(3,'L'),(5,'S'),(7,'L')],[(1,'S'),(2,'S'),(5,'L')]]),
 'bossa': dict(slots=8, sparse=[[(0,'L')],[(0,'L'),(5,'S')],[(3,'L')]],
               medium=[[(0,'S'),(3,'L'),(6,'S')],[(1,'L'),(4,'S'),(7,'L')],[(0,'L'),(3,'S'),(5,'L')],[(2,'S'),(5,'L')]],
               busy=[[(0,'S'),(3,'S'),(5,'S'),(6,'S')],[(1,'S'),(3,'S'),(4,'S'),(7,'L')]]),
 'ballad':dict(slots=8, sparse=[[(0,'L')],[(0,'L')],[(0,'L'),(4,'L')]],
               medium=[[(0,'L'),(5,'S')],[(0,'L'),(4,'L')],[(0,'L'),(7,'L')],[(0,'L'),(3,'S'),(4,'L')]],
               busy=[[(0,'L'),(3,'S'),(5,'S'),(7,'L')],[(0,'L'),(2,'S'),(4,'L'),(7,'L')]]),
 'soul':  dict(slots=16, sparse=[[(0,'L')],[(3,'L')],[(6,'S'),(10,'L')]],
               medium=[[(0,'S'),(3,'S'),(10,'L')],[(3,'S'),(6,'L'),(14,'S')],[(0,'L'),(7,'S'),(10,'S')],[(2,'S'),(6,'S'),(11,'L')]],
               busy=[[(0,'S'),(3,'S'),(6,'S'),(10,'S'),(13,'S')],[(2,'S'),(3,'S'),(7,'S'),(10,'L'),(15,'S')]]),
 'house': dict(slots=16, sparse=[[(2,'S'),(10,'S')],[(0,'L')],[(6,'S'),(14,'S')]],
               medium=[[(2,'S'),(6,'S'),(10,'S'),(14,'S')],[(0,'S'),(3,'S'),(6,'S'),(10,'S'),(13,'S')],
                       [(3,'S'),(6,'S'),(11,'S'),(14,'S')],[(0,'S'),(3,'S'),(6,'L')]],
               busy=[[(0,'S'),(3,'S'),(6,'S'),(8,'S'),(11,'S'),(14,'S')],[(2,'S'),(5,'S'),(6,'S'),(10,'S'),(13,'S'),(14,'S')]]),
}

def swing_ratio(bpm):
    # triplet-ish at medium tempo, flattening as it gets fast (measured behaviour of real players)
    return max(1.15, min(2.2, 2.05 - (bpm-120)/170.0))

class Comper:
    def __init__(self, chart, beats, bpm, seed=1, style='swing', voicing='rootless', density=0.5,
                 loose=1.0, lead=True, out=0.0, repeat=0.6, listen=0.0, solo=None, roots=False,
                 swing=None, bars=None):
        self.rng = random.Random(seed)
        self.chart, self.beats, self.bpm = chart, beats, bpm
        self.style, self.density, self.loose, self.out = style, density, loose, out
        self.repeat, self.listen, self.solo, self.roots = repeat, listen, solo or [], roots
        self.voicer = Voicer(self.rng, voicing, lead, out)
        self.spb = 60.0/bpm
        self.sw = swing if swing is not None else (swing_ratio(bpm) if style in ('swing','ballad') else 1.0)
        self.nbars = bars or beats//4
    def chord_at(self, beat):
        beat = beat % self.beats
        cur = self.chart[0][1]
        for b, c in self.chart:
            if b <= beat + 1e-6: cur = c
        return cur
    def slot_beat(self, bar, slot, slots):
        if slots == 8:
            b, off = divmod(slot, 2)
            return bar*4 + b + (self.sw/(1+self.sw) if off else 0)
        b, s = divmod(slot, 4)
        sw16 = 0.54 if self.style == 'soul' else 0.5      # light sixteenth swing
        return bar*4 + b + [0, sw16/2, 0.5, 0.5+sw16/2][s]
    def cell(self):
        C = CELLS[self.style]; d = self.density; r = self.rng.random()
        w = [max(0, 1-2*d), 1-abs(2*d-1), max(0, 2*d-1)]
        w = [x+0.08 for x in w]; t = sum(w); r *= t
        pool = C['sparse'] if r < w[0] else C['medium'] if r < w[0]+w[1] else C['busy']
        return list(self.rng.choice(pool))
    def mutate(self, cell, slots):
        cell = list(cell); r = self.rng.random()
        if cell and r < 0.35: cell.pop(self.rng.randrange(len(cell)))
        elif cell and r < 0.7:
            i = self.rng.randrange(len(cell)); s, l = cell[i]
            s2 = (s + self.rng.choice([-1,1])) % slots
            if all(s2 != x[0] for x in cell): cell[i] = (s2, l)
        else:
            s2 = self.rng.choice([1,3,5,7] if slots == 8 else [3,6,10,14])
            if all(s2 != x[0] for x in cell): cell.append((s2,'S'))
        return sorted(cell)
    # soloist activity, causal: only looks at what has already been played
    def solo_state(self, t):
        sounding = any(a <= t and t < b for a, b, *_ in self.solo)
        ends = [b for a, b, *_ in self.solo if b <= t]
        gap = t - max(ends) if ends else 99
        recent = any(t - 2.5*self.spb < a <= t for a, b, *_ in self.solo)
        return sounding, gap, recent
    def run(self):
        rng = self.rng; slots = CELLS[self.style]['slots']
        hits = []                                   # (beat, len, bar, slot)
        motif = [self.cell(), self.cell()]
        for bar in range(self.nbars):
            if bar % 2 == 0 and bar > 0:
                if rng.random() > self.repeat: motif = [self.cell(), self.cell()]
                elif rng.random() < 0.6:
                    k = rng.randrange(2); motif[k] = self.mutate(motif[k], slots)
            cell = list(motif[bar % 2])
            for s, l in cell: hits.append([self.slot_beat(bar, s, slots), l, bar, s, 0])
            if self.listen > 0:                     # candidate answer slots on the upbeats
                for s in ([1,3,5,7] if slots == 8 else [2,6,10,14]):
                    if all(s != x[0] for x in cell):
                        hits.append([self.slot_beat(bar, s, slots), 'S', bar, s, 1])
        hits.sort()
        notes = []; kept = []
        # listening pass (causal)
        for h in hits:
            t = h[0]*self.spb; extra = h[4]; boost = 0
            if self.listen > 0 and self.solo:
                sounding, gap, recent = self.solo_state(t)
                if sounding:
                    if extra or rng.random() < 0.8*self.listen: continue
                    h[1] = 'S'; boost = -12
                elif recent and gap < 2.5*self.spb:
                    if extra and rng.random() > 0.55*self.listen: continue
                    boost = 8
                elif extra: continue
            elif extra: continue
            kept.append(h + [boost])
        dyn = 0.0
        for i, h in enumerate(kept):
            beat, ln, bar, slot, extra, boost = h
            nxt = kept[i+1][0] if i+1 < len(kept) else beat + 4
            on_up = (slot % 2 == 1) if slots == 8 else (slot % 4 != 0)
            # anticipation: an upbeat hit takes the chord of the beat it leads into
            look = math.floor(beat) + 1 if on_up else beat
            chord = self.chord_at(look)
            antic = on_up and chord != self.chord_at(beat)
            # substitutions
            if chord[1] in ('dom','alt') and rng.random() < 0.3*self.out:
                chord = ((chord[0]+6) % 12, 'dom')              # tritone substitute
            elif chord[1] == 'dom' and rng.random() < 0.8*self.out:
                chord = (chord[0], 'alt')
            v = self.voicer.pick(chord)
            # chromatic side-slip: a quick soft hit a semitone away, one eighth before a fresh chord
            prevbeat = kept[i-1][0] if i else -9
            if self.out > 0 and beat - prevbeat > 0.9 and rng.random() < 0.35*self.out and beat > 0.7:
                sh = rng.choice([1,-1])
                pb = self.slot_beat(bar, slot-1, slots) if slot > 0 else self.slot_beat(bar-1, slots-1, slots)
                self.emit(notes, pb, 0.3, [n+sh for n in v], -10, not on_up)
            if ln == 'S': dur = rng.uniform(0.28, 0.45)
            else: dur = max(0.9, min(nxt - beat - 0.12, 2.6 if self.style != 'ballad' else 3.8))
            dyn = max(-10, min(10, dyn + rng.gauss(0, 2.5)))
            acc = (12 if (ln == 'L' and on_up) else 4 if antic else 0) + boost + dyn
            if ln == 'S' and len(v) >= 4 and rng.random() < 0.25*min(1,self.loose):
                v = v[:1] + rng.sample(v[1:-1], len(v)-3) + v[-1:]; v.sort()   # lighter stab
            self.emit(notes, beat, dur, v, acc, on_up)
            if self.roots and (slot == 0 or (i == 0)):
                r = 36 + (chord[0] - 0) % 12
                if r > 43: r -= 12
                self.emit(notes, beat, dur, [r], acc-4, on_up, low=True)
        return notes
    def emit(self, notes, beat, dur, v, acc, on_up, low=False):
        rng, L = self.rng, self.loose
        t = beat*self.spb
        t += L*(0.012 if not on_up else 0.006) + rng.gauss(0, 0.006)*L   # sit a touch behind the beat
        roll = rng.uniform(0.004, 0.018)*L*(3.5 if self.style == 'ballad' else 1)
        mode = rng.random()
        n = len(v)
        for k, p in enumerate(v):
            if n == 1: off = 0
            elif mode < 0.7: off = roll*k/(n-1)                 # bottom to top
            elif mode < 0.85: off = roll*(n-1-k)/(n-1)          # top first
            else: off = rng.uniform(0, roll)
            off += rng.gauss(0, 0.002)*L
            vel = 64 + acc + (0 if L == 0 else (7 if k == n-1 else 2 if k == 0 else -5) + rng.gauss(0, 4)*min(L,1.5))
            if L == 0: vel = 76
            if low: off -= 0.004*L
            vel = int(max(18, min(120, vel)))
            end = t + dur*self.spb + rng.uniform(0, 0.02)*L
            notes.append((max(0, t+off), end, p, vel))

# ---------------------------------------------------------------- band (context only)
def walk_bass(chart, beats, bpm, nbars, rng, sw):
    spb = 60/bpm; out = []; last = 40
    def chord_at(b):
        b %= beats; cur = chart[0][1]
        for x, c in chart:
            if x <= b+1e-6: cur = c
        return cur
    T = {'maj':[0,4,7,9],'maj6':[0,4,7,9],'m7':[0,3,7,10],'m6':[0,3,7,9],'mmaj':[0,3,7,11],'dom':[0,4,7,10],
         'alt':[0,4,8,10],'hdim':[0,3,6,10],'dim':[0,3,6,9],'sus':[0,5,7,10]}
    for b in range(nbars*4):
        root, q = chord_at(b); nroot, _ = chord_at(b+1)
        newch = chord_at(b) != chord_at(b-1) or b % 4 == 0
        def near(pc, ref):
            c = [n for n in range(29, 53) if n % 12 == pc]
            return min(c, key=lambda n: abs(n-ref))
        if newch: n = near(root, last if b else 38)
        elif chord_at(b+1) != chord_at(b) or b % 4 == 3:
            tgt = near(nroot, last); n = tgt + rng.choice([-1, 1, 1, -2])
            if n == last: n = tgt + 2
        else:
            pcs = [(root+x) % 12 for x in T[q]]
            c = [m for m in range(29, 53) if m % 12 in pcs and m != last and abs(m-last) <= 5]
            n = rng.choice(c) if c else near(root, last)
        out.append((b*spb + rng.gauss(0, .004), (b+0.92)*spb, n, 84 + (6 if b % 2 else 0) + int(rng.gauss(0, 4))))
        last = n
    return out

def swing_drums(bpm, nbars, rng, sw):
    spb = 60/bpm; o = []; f = sw/(1+sw)
    for b in range(nbars*4):
        o.append((b*spb, b*spb+.1, 51, 62 + (10 if b % 2 else 0) + int(rng.gauss(0, 4))))
        if b % 2: 
            o.append(((b+f)*spb, (b+f)*spb+.1, 51, 50 + int(rng.gauss(0, 4))))
            o.append((b*spb, b*spb+.1, 44, 70))
    return o

def straight_drums(bpm, nbars, rng, kind):
    spb = 60/bpm; o = []
    for b in range(nbars*4):
        t = b*spb
        if kind == 'house':
            o.append((t, t+.1, 36, 100)); o.append((t+spb/2, t+spb/2+.05, 46 if b % 4 == 3 else 42, 70))
            if b % 2: o.append((t, t+.1, 39, 64))
        elif kind == 'bossa':
            for k in range(2): o.append((t+k*spb/2, t+k*spb/2+.05, 42, 48 + int(rng.gauss(0, 4))))
            bb = b % 8
            for x in ([0, 1.5, 3, 5, 6.5]):
                if abs(bb - x) < 1e-6 or abs(bb + 0.5 - x) < 1e-6:
                    tt = (b - bb + x)*spb; o.append((tt, tt+.05, 37, 62))
            if b % 2 == 0: o.append((t, t+.1, 36, 60)); o.append((t+1.5*spb, t+1.5*spb+.1, 36, 50))
        elif kind == 'soul':
            for k in range(4): o.append((t+k*spb/4, t+k*spb/4+.04, 42, [70,40,58,42][k] + int(rng.gauss(0, 4))))
            if b % 4 in (0,): o.append((t, t+.1, 36, 100)); o.append((t+.75*spb, t+.75*spb+.1, 36, 70))
            if b % 4 == 2: o.append((t+.5*spb, t+.5*spb+.1, 36, 88))
            if b % 2: o.append((t, t+.1, 38, 96))
    return o

def simple_bass(chart, beats, bpm, nbars, rng, kind):
    spb = 60/bpm; o = []
    def chord_at(b):
        b %= beats; cur = chart[0][1]
        for x, c in chart:
            if x <= b+1e-6: cur = c
        return cur
    for bar in range(nbars):
        for b in range(4):
            root, q = chord_at(bar*4+b); r = 36 + root % 12
            if r > 43: r -= 12
            t = (bar*4+b)*spb
            if kind == 'bossa':
                if b in (0, 2): o.append((t, t+1.4*spb, r if b == 0 else r+7, 86))
                if b in (1, 3): o.append((t+.5*spb, t+.95*spb, r+7 if b == 1 else r, 70))
            elif kind == 'house':
                o.append((t+.5*spb, t+.9*spb, r+12, 92))
                if b == 3 and rng.random() < .5: o.append((t+.75*spb, t+.95*spb, r+10, 70))
            elif kind == 'soul':
                if b == 0: o.append((t, t+.7*spb, r, 98)); o.append((t+.75*spb, t+spb, r, 80))
                if b == 2: o.append((t+.5*spb, t+.9*spb, r+12, 84))
                if b == 3: o.append((t+.5*spb, t+.95*spb, r+10, 80))
    return o

SCALE = {'maj':[0,2,4,7,9,11],'maj6':[0,2,4,7,9],'m7':[0,2,3,5,7,10],'m6':[0,2,3,5,7,9],'mmaj':[0,2,3,7,11],
         'dom':[0,2,4,7,9,10],'alt':[1,3,4,8,10],'hdim':[0,3,5,6,10],'dim':[0,2,3,6,9],'sus':[0,2,5,7,10]}
def soloist(chart, beats, bpm, nbars, rng, sw):
    """a made-up horn line with clear rests, to show listening"""
    spb = 60/bpm; f = sw/(1+sw); o = []; e = 0; p = 70; total = nbars*8
    def chord_at(b):
        b %= beats; cur = chart[0][1]
        for x, c in chart:
            if x <= b+1e-6: cur = c
        return cur
    while e < total - 4:
        n = rng.randint(5, 13); d = rng.choice([-1, 1])
        for k in range(n):
            if e >= total: break
            beat = e//2 + (f if e % 2 else 0)
            root, q = chord_at(e//2)
            pcs = [(root+x) % 12 for x in SCALE[q]]
            if rng.random() < 0.25: d = -d
            if p > 82: d = -1
            if p < 62: d = 1
            p += d
            while p % 12 not in pcs: p += d
            last = (k == n-1)
            dur = (1.6 if last else (1-f if e % 2 else f)*0.95)
            o.append((beat*spb, (beat+dur)*spb, p, 88 + (8 if e % 2 else 0) + int(rng.gauss(0, 5))))
            e += 1
        e += 3 + rng.randint(3, 7)
    return o

# ---------------------------------------------------------------- MIDI out
def vlq(n):
    b = [n & 0x7f]; n >>= 7
    while n: b.append((n & 0x7f) | 0x80); n >>= 7
    return bytes(reversed(b))

def write_midi(path, bpm, tracks, tail=2.0):
    """tracks: list of (channel, program, volume, notes[(t0,t1,pitch,vel)]) times in seconds"""
    ppq = 480; tps = ppq*bpm/60.0
    chunks = []
    ev0 = b'\x00\xff\x51\x03' + struct.pack('>I', int(60e6/bpm))[1:] + b'\x00\xff\x2f\x00'
    chunks.append(ev0)
    for ch, prog, vol, notes in tracks:
        ev = [(0, 0, bytes([0xC0|ch, prog])), (0, 0, bytes([0xB0|ch, 7, vol])), (0, 0, bytes([0xB0|ch, 91, 30]))]
        for t0, t1, p, v in notes:
            ev.append((int(t0*tps), 2, bytes([0x90|ch, p, v])))
            ev.append((max(int(t0*tps)+1, int(t1*tps)), 1, bytes([0x80|ch, p, 0])))
        ev.sort(key=lambda e: (e[0], e[1]))
        data = b''; last = 0
        for t, _, m in ev: data += vlq(t-last) + m; last = t
        data += vlq(int(tail*tps)) + b'\xff\x2f\x00'
        chunks.append(data)
    with open(path, 'wb') as f:
        f.write(b'MThd' + struct.pack('>IHHH', 6, 1, len(chunks), ppq))
        for c in chunks: f.write(b'MTrk' + struct.pack('>I', len(c)) + c)
