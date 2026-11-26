"""Comper: reference engine. Design by Fable, 2026-11-23.

THIS FILE IS THE DEFINITION OF THE MODULE'S BEHAVIOUR. The C++ engine is a line-by-line
port: same tables, same order of random draws, doubles everywhere, so that for the same
seed and inputs it produces the same notes (make_reference.py writes the golden cases).

Three parts:
  Rng      deterministic generator both languages can reproduce exactly
  Engine   the player. Called once per bar (begin_bar) and once per rhythmic slot (decide).
           It never looks at the clock itself; the host tells it where it is.
  simulate an offline host: walks a chord chart, feeds the engine, resolves note lengths.
           The Rack module is the real-time version of this function.
"""
import math

# ------------------------------------------------------------------ random numbers
class Rng:
    def __init__(self, seed):
        self.s = (int(seed) * 2654435761 + 12345) & 0xFFFFFFFF
        for _ in range(4): self.u()
    def u(self):                               # uniform [0,1), 24 bits
        self.s = (self.s * 1664525 + 1013904223) & 0xFFFFFFFF
        return (self.s >> 8) / 16777216.0
    def uni(self, a, b): return a + (b - a) * self.u()
    def n(self, k): return int(self.u() * k)   # integer 0..k-1
    def g(self):                               # near-gaussian, unit variance, 4 draws
        return (self.u() + self.u() + self.u() + self.u() - 2.0) * 1.7320508075688772

# ------------------------------------------------------------------ chords
# quality index order is also the QUAL knob / QUAL CV order (1 V per step)
Q_NAMES = ['maj7', '6', 'm7', 'm6', 'mMaj7', '7', '7alt', 'm7b5', 'dim7', '7sus']
MAJ, MAJ6, M7, M6, MMAJ, DOM, ALT, HDIM, DIM, SUS = range(10)
MINOR_FAMILY = (M7, M6, MMAJ, HDIM)
NOTE_NAMES = ['C', 'Db', 'D', 'Eb', 'E', 'F', 'Gb', 'G', 'Ab', 'A', 'Bb', 'B']

def chord_name(ch): return NOTE_NAMES[ch[0]] + Q_NAMES[ch[1]]

# voicing styles
SHELL, ROOTLESS, QUARTAL, SPREADV, MIXED, COLOR = range(6)
STYLE_NAMES = ['SHELL', 'ROOTLESS', 'QUARTAL', 'SPREAD', 'MIXED']

# Recipes: semitones above the root, bottom to top. Each next note is the nearest one above
# the previous. No roots (except where a root is a deliberate colour): the bass has them.
RECIPES = {
 MAJ:  {SHELL: [[4,11],[11,4],[4,11,2],[11,4,7]],
        ROOTLESS: [[4,7,11,2],[11,2,4,7],[4,9,11,2]],
        QUARTAL: [[4,9,2],[9,2,7],[11,4,9]],
        SPREADV: [[11,4,9,2,7],[4,9,2,7,11],[4,11,2,7]],
        COLOR: [[4,11,2,6],[11,2,4,6,9]]},
 MAJ6: {SHELL: [[4,9],[9,4],[4,9,2]],
        ROOTLESS: [[4,7,9,2],[9,2,4,7]],
        QUARTAL: [[4,9,2],[9,2,7]],
        SPREADV: [[4,9,2,7],[9,4,7,2]]},
 M7:   {SHELL: [[3,10],[10,3],[3,10,2],[10,3,7]],
        ROOTLESS: [[3,7,10,2],[10,2,3,7]],
        QUARTAL: [[5,10,3],[7,0,5],[2,7,0]],
        SPREADV: [[0,5,10,3,7],[3,10,2,5,7],[10,3,7,2,5]]},
 M6:   {SHELL: [[3,9],[9,3],[3,9,2]],
        ROOTLESS: [[3,7,9,2],[9,2,3,7]],
        QUARTAL: [[3,9,2],[9,2,7]],
        SPREADV: [[3,9,2,7],[9,3,7,2]]},
 MMAJ: {SHELL: [[3,11],[11,3]],
        ROOTLESS: [[3,7,11,2],[11,2,3,7]],
        QUARTAL: [[3,7,11,2]],
        SPREADV: [[3,11,2,7]]},
 DOM:  {SHELL: [[4,10],[10,4],[4,10,2],[10,4,9]],
        ROOTLESS: [[4,9,10,2],[10,2,4,9]],
        QUARTAL: [[10,4,9],[4,9,2]],
        SPREADV: [[10,4,9,2,7],[4,10,2,7,9]],
        COLOR: [[4,10,2,6,9],[10,4,9,2,6]]},
 ALT:  {SHELL: [[4,10,3],[10,4,8]],
        ROOTLESS: [[4,8,10,3],[10,1,4,8]],
        QUARTAL: [[10,3,8],[4,10,3]],
        SPREADV: [[4,10,8,0,3],[10,4,8,1,3]]},
 HDIM: {SHELL: [[3,10],[10,3],[6,10,3]],
        ROOTLESS: [[3,6,10,0],[10,0,3,6],[6,10,3,5]],
        QUARTAL: [[5,10,3],[6,10,3,5]],
        SPREADV: [[6,10,3,5],[3,10,5,6,0]]},
 DIM:  {SHELL: [[3,9],[9,3],[6,0]],
        ROOTLESS: [[3,6,9,0],[9,0,3,6],[6,9,0,3]],
        QUARTAL: [[3,6,9,0]],
        SPREADV: [[3,9,0,6],[9,3,6,0]]},
 SUS:  {SHELL: [[5,10],[10,5,2]],
        ROOTLESS: [[5,9,10,2],[10,2,5,9]],
        QUARTAL: [[10,2,5],[2,5,10]],
        SPREADV: [[10,5,9,2,7],[5,10,2,7,9]]},
}
# lowest note may start anywhere in this window (MIDI), before REGISTER shifts it
WINDOW = {SHELL: (48, 64), ROOTLESS: (50, 67), QUARTAL: (52, 68), SPREADV: (44, 58), COLOR: (46, 60)}
# the two notes that define each chord (kept when VOICES is turned down)
GUIDES = {MAJ: (4, 11), MAJ6: (4, 9), M7: (3, 10), M6: (3, 9), MMAJ: (3, 11), DOM: (4, 10),
          ALT: (4, 10), HDIM: (3, 10), DIM: (3, 9), SUS: (5, 10)}
# chord scales for the SCALE output
SCALES = {MAJ: [0,2,4,6,7,9,11], MAJ6: [0,2,4,7,9], M7: [0,2,3,5,7,9,10], M6: [0,2,3,5,7,9,11],
          MMAJ: [0,2,3,5,7,9,11], DOM: [0,2,4,7,9,10], ALT: [0,1,3,4,6,8,10], HDIM: [0,1,3,5,6,8,10],
          DIM: [0,2,3,5,6,8,9,11], SUS: [0,2,5,7,9,10]}

def realize(root, recipe, lo, hi):
    out = []
    pc0 = (root + recipe[0]) % 12
    for b in range(lo, hi + 1):
        if b % 12 != pc0: continue
        v = [b]
        for d in recipe[1:]:
            n = v[-1] + 1
            while n % 12 != (root + d) % 12: n += 1
            v.append(n)
        out.append(v)
    return out

def mud(v, top_soft):
    """penalty for close intervals low down, and for a top note far out of range"""
    p = 0.0
    for i in range(len(v) - 1):
        a = v[i]; d = v[i+1] - a
        if d <= 2 and a < 55: p += 6
        if d <= 4 and a < 47: p += 6
        if d == 1: p += 2
    if v[-1] > top_soft + 10: p += 3 * (v[-1] - top_soft - 10)
    if v[-1] < top_soft - 10: p += 2 * (top_soft - 10 - v[-1])
    return p

def move(v, p):
    """how far the hand travels from voicing p to voicing v, in semitones"""
    if p is None: return 0.0
    if len(v) == len(p):
        s = 0
        for i in range(len(v)): s += abs(v[i] - p[i])
        return float(s)
    s = 0
    for a in v:
        m = 999
        for b in p:
            if abs(a - b) < m: m = abs(a - b)
        s += m
    return s * float(max(len(v), len(p))) / len(v)

def limit_voices(v, root, q, nmax):
    """VOICES: drop notes until nmax remain. Guide tones go last, inner notes first."""
    v = list(v)
    g = [(root + GUIDES[q][0]) % 12, (root + GUIDES[q][1]) % 12]
    while len(v) > nmax:
        idx = -1
        for i in range(1, len(v) - 1):               # inner, non-guide, lowest first
            if v[i] % 12 not in g: idx = i; break
        if idx < 0:
            for i in (len(v) - 1, 0):                # then top, then bottom, non-guide
                if v[i] % 12 not in g: idx = i; break
        if idx < 0: idx = len(v) - 1                 # only guides left: drop the top
        v.pop(idx)
    return v

# ------------------------------------------------------------------ rhythm cells
# (slot, long?) ; 8 slots = swung eighths, 16 slots = sixteenths
S_, L_ = 0, 1
SWING, BOSSA, BALLAD, SOUL, HOUSE = range(5)
GROOVE_NAMES = ['SWING', 'BOSSA', 'BALLAD', 'SOUL', 'HOUSE']
GROOVE_SLOTS = [8, 8, 8, 16, 16]
CELLS = {
 SWING: [
  [[], [(0,L_)], [(7,L_)], [(3,S_)], [(3,L_)]],
  [[(0,S_),(3,L_)], [(1,S_),(4,L_)], [(3,L_),(7,L_)], [(2,S_),(5,L_)], [(1,L_),(5,S_)], [(3,S_),(7,L_)], [(0,L_),(7,L_)]],
  [[(0,S_),(3,S_),(6,S_)], [(3,S_),(5,S_),(7,L_)], [(1,S_),(3,L_),(7,L_)], [(0,S_),(3,L_),(5,S_),(7,L_)], [(1,S_),(2,S_),(5,L_)]]],
 BOSSA: [
  [[(0,L_)], [(0,L_),(5,S_)], [(3,L_)]],
  [[(0,S_),(3,L_),(6,S_)], [(1,L_),(4,S_),(7,L_)], [(0,L_),(3,S_),(5,L_)], [(2,S_),(5,L_)]],
  [[(0,S_),(3,S_),(5,S_),(6,S_)], [(1,S_),(3,S_),(4,S_),(7,L_)]]],
 BALLAD: [
  [[(0,L_)], [(0,L_)], [(0,L_),(4,L_)]],
  [[(0,L_),(5,S_)], [(0,L_),(4,L_)], [(0,L_),(7,L_)], [(0,L_),(3,S_),(4,L_)]],
  [[(0,L_),(3,S_),(5,S_),(7,L_)], [(0,L_),(2,S_),(4,L_),(7,L_)]]],
 SOUL: [
  [[(0,L_)], [(3,L_)], [(6,S_),(10,L_)]],
  [[(0,S_),(3,S_),(10,L_)], [(3,S_),(6,L_),(14,S_)], [(0,L_),(7,S_),(10,S_)], [(2,S_),(6,S_),(11,L_)]],
  [[(0,S_),(3,S_),(6,S_),(10,S_),(13,S_)], [(2,S_),(3,S_),(7,S_),(10,L_),(15,S_)]]],
 HOUSE: [
  [[(2,S_),(10,S_)], [(0,L_)], [(6,S_),(14,S_)]],
  [[(2,S_),(6,S_),(10,S_),(14,S_)], [(0,S_),(3,S_),(6,S_),(10,S_),(13,S_)], [(3,S_),(6,S_),(11,S_),(14,S_)], [(0,S_),(3,S_),(6,L_)]],
  [[(0,S_),(3,S_),(6,S_),(8,S_),(11,S_),(14,S_)], [(2,S_),(5,S_),(6,S_),(10,S_),(13,S_),(14,S_)]]],
}
UPBEATS = {8: [1, 3, 5, 7], 16: [2, 6, 10, 14]}      # where listening may add an answer
ADD_SLOTS = {8: [1, 3, 5, 7], 16: [3, 6, 10, 14]}    # where a varied cell may gain a hit

def auto_swing(bpm):
    """long:short ratio of a swung pair. Triplet-ish at medium tempo, flatter when fast."""
    return max(1.15, min(2.2, 2.05 - (bpm - 120.0) / 170.0))

# ------------------------------------------------------------------ parameters
class Params:
    def __init__(self, **kw):
        self.style = ROOTLESS     # 0..4
        self.spread = 0.25        # 0..1   close .. open (drop-2)
        self.register = 69.0      # 60..79 where the top note likes to sit (MIDI)
        self.voices = 5           # 2..5
        self.lead = 1.0           # 0..1   voice-leading strength
        self.out = 0.0            # 0..1   altered dominants, substitutes, side-slips
        self.roots = 0            # 0 off, 1 sometimes, 2 always
        self.groove = SWING       # 0..4
        self.density = 0.5        # 0..1
        self.swing = 0.0          # -1..1  offset from the automatic swing
        self.repeat = 0.6         # 0..1   keep the two-bar figure
        self.push = 0.85          # 0..1   upbeat hits take the coming chord
        self.loose = 1.0          # 0..2.5 master feel
        self.timing = 1.0         # 0..2   trims (multiply LOOSE)
        self.roll = 1.0
        self.weight = 1.0
        self.slop = 1.0
        self.pedal = 0.0          # 0..1
        self.listen = 0.0         # 0..1
        for k, v in kw.items():
            if not hasattr(self, k): raise KeyError(k)
            setattr(self, k, v)

class Ctx:
    """What the host tells the engine at each decision."""
    def __init__(self):
        self.spb = 0.5                 # seconds per beat
        self.lead_s = 0.06             # how long before the slot's nominal time we are being asked
        self.chord_at = None           # function(beat) -> (root, quality)
        self.next_change = None        # function(beat) -> next different chord after beat, or None
        self.solo_connected = False
        self.solo_sounding = False
        self.solo_gap = 99.0           # beats since the soloist stopped
        self.solo_recent = False       # a solo note started within the last 2.5 beats
        self.solo_low = None           # lowest recent solo pitch (MIDI) or None

# ------------------------------------------------------------------ the player
class Engine:
    def __init__(self, seed=1):
        self.p = Params()
        self.reset(seed)

    def reset(self, seed):
        self.rr = Rng(seed * 3 + 0)    # rhythm
        self.rv = Rng(seed * 3 + 1)    # voicing
        self.rf = Rng(seed * 3 + 2)    # feel
        self.prev = None               # last voicing
        self.prev_chord = None
        self.target = None             # wandering top-note target
        self.motif = None
        self.cur_style = ROOTLESS
        self.cellmap = {}
        self.push_ok = []
        self.slot_beats = []
        self.slots = 8
        self.dyn = 0.0
        self.last_hit_beat = -99.0
        self.pending = None            # (bar, slot, voicing, chord) promised by a side-slip
        self.root_chord = None
        self.first_bar = True

    # ---- rhythm
    def pick_cell(self):
        C = CELLS[self.p.groove]; d = self.p.density
        w0 = max(0.0, 1 - 2*d) + 0.08; w1 = 1 - abs(2*d - 1) + 0.08; w2 = max(0.0, 2*d - 1) + 0.08
        r = self.rr.u() * (w0 + w1 + w2)
        pool = C[0] if r < w0 else (C[1] if r < w0 + w1 else C[2])
        return list(pool[self.rr.n(len(pool))])

    def mutate(self, cell):
        cell = list(cell); r = self.rr.u()
        if cell and r < 0.35:
            cell.pop(self.rr.n(len(cell)))
        elif cell and r < 0.7:
            i = self.rr.n(len(cell)); s, l = cell[i]
            s2 = (s + (-1 if self.rr.u() < 0.5 else 1)) % self.slots
            if all(s2 != x[0] for x in cell): cell[i] = (s2, l)
        else:
            a = ADD_SLOTS[self.slots]; s2 = a[self.rr.n(len(a))]
            if all(s2 != x[0] for x in cell): cell.append((s2, S_))
        cell.sort()
        return cell

    def pick_style(self):
        if self.p.style != MIXED: return self.p.style
        r = self.rr.u()
        return ROOTLESS if r < 0.4 else SHELL if r < 0.55 else QUARTAL if r < 0.75 else SPREADV

    def begin_bar(self, bar, bpm):
        p = self.p
        slots = GROOVE_SLOTS[p.groove]
        if self.first_bar or slots != self.slots or self.motif is None or p.groove != self.motif_groove:
            self.slots = slots
            self.motif = [self.pick_cell(), self.pick_cell()]
            self.motif_groove = p.groove
            self.cur_style = self.pick_style()
            self.first_bar = False
        elif bar % 2 == 0:
            if self.rr.u() > p.repeat:
                self.motif = [self.pick_cell(), self.pick_cell()]
            elif self.rr.u() < 0.6:
                k = self.rr.n(2); self.motif[k] = self.mutate(self.motif[k])
            self.cur_style = self.pick_style()
        if p.style != MIXED: self.cur_style = p.style
        self.cellmap = {}
        for s, l in self.motif[bar % 2]: self.cellmap[s] = l
        self.push_ok = [self.rr.u() < p.push for _ in range(slots)]
        # slot positions in beats within the bar
        self.slot_beats = []
        if slots == 8:
            base = auto_swing(bpm) if p.groove in (SWING, BALLAD) else 1.0
            ratio = max(1.0, min(3.0, base * 2.0 ** (p.swing * 0.6)))
            f = ratio / (1.0 + ratio)
            for s in range(8): self.slot_beats.append((s // 2) + (f if s % 2 else 0.0))
        else:
            sw = max(0.5, min(0.67, (0.54 if p.groove == SOUL else 0.5) + 0.12 * p.swing))
            for s in range(16): self.slot_beats.append((s // 4) + [0.0, sw / 2, 0.5, 0.5 + sw / 2][s % 4])

    def on_up(self, slot):
        return (slot % 2 == 1) if self.slots == 8 else (slot % 4 != 0)

    # ---- harmony
    def chord_for(self, bar, slot, ctx):
        beat = bar * 4 + self.slot_beats[slot]
        up = self.on_up(slot)
        look = math.floor(beat) + 1 if (up and self.push_ok[slot]) else beat
        chord = ctx.chord_at(look)
        antic = up and chord != ctx.chord_at(beat)
        u1 = self.rv.u(); u2 = self.rv.u(); out = self.p.out
        if chord[1] in (DOM, ALT):
            nxt = ctx.next_change(look) if ctx.next_change else None
            resolves = nxt is not None and (nxt[0] - chord[0]) % 12 == 5
            p_tri = 0.3 * out if resolves else (0.15 * out if nxt is None else 0.0)
            if u1 < p_tri:
                chord = ((chord[0] + 6) % 12, DOM)                    # tritone substitute
            elif chord[1] == DOM:
                if resolves and nxt[1] in MINOR_FAMILY: p_alt = min(1.0, 1.5 * out)
                elif resolves or nxt is None: p_alt = 0.8 * out
                else: p_alt = 0.3 * out
                if u2 < p_alt: chord = (chord[0], ALT)
        return chord, antic

    def pick_voicing(self, chord, ctx):
        p = self.p; rv = self.rv
        root, q = chord
        centre = p.register
        if self.target is None: self.target = centre
        self.target += rv.g() * 0.8
        self.target = min(centre + 7, max(centre - 5, self.target))
        if ctx.solo_low is not None and p.listen > 0:
            self.target = min(self.target, max(58.0, ctx.solo_low - 4.0))   # stay under the soloist
        shift = int(math.floor(centre - 69.0 + 0.5))
        tab = RECIPES[q]; style = self.cur_style
        recs = [(r, style) for r in tab[style]]
        uc = rv.u()
        if COLOR in tab and uc < p.out:
            recs += [(r, COLOR) for r in tab[COLOR]]
        cands = []                                   # (voicing, opened?)
        for r, st in recs:
            lo, hi = WINDOW[st]
            for v in realize(root, r, lo + shift, hi + shift):
                cands.append((limit_voices(v, root, q, p.voices), False))
                if p.spread > 0.05 and len(v) >= 4:  # drop-2: second voice from the top down an octave
                    d = list(v); x = d.pop(len(d) - 2) - 12
                    if x >= 43 + shift:
                        d.append(x); d.sort()
                        cands.append((limit_voices(d, root, q, p.voices), True))
        same = (chord == self.prev_chord)
        stay = rv.u() < 0.7
        bias = 8.0 * (p.spread - 0.5)
        best = None; bestk = 1e18
        for v, opened in cands:
            k = p.lead * (move(v, self.prev) + (1.5 * abs(v[-1] - self.prev[-1]) if self.prev else 0.0))
            k += 1.1 * abs(v[-1] - self.target) + mud(v, centre)
            if same and v == self.prev: k += -4.0 if stay else 6.0
            k += (max(0.0, -bias) if opened else max(0.0, bias))
            k += rv.u() * (1.8 + 10.0 * (1.0 - p.lead))
            if k < bestk: bestk = k; best = v
        self.prev = best; self.prev_chord = chord
        return best

    # ---- one chord, as a hand would play it
    def emit(self, ev, v, kind, acc, boost, up, ctx, low=False, dur_beats=None):
        p = self.p; rf = self.rf
        T = p.loose * p.timing; R = p.loose * p.roll; W = p.loose * p.weight; S = p.loose * p.slop
        Wc = min(W, 1.0)
        lay = T * (0.006 if up else 0.012) + rf.g() * 0.006 * T          # a touch behind the beat
        roll = rf.uni(0.004, 0.018) * R * (3.5 if p.groove == BALLAD else 1.0)
        mode = rf.u()
        n = len(v)
        for k in range(n):
            if n == 1: off = 0.0
            elif mode < 0.7: off = roll * k / (n - 1)                    # bottom to top
            elif mode < 0.85: off = roll * (n - 1 - k) / (n - 1)         # top first
            else: off = rf.uni(0.0, roll)
            off += rf.g() * 0.002 * R
            shape = 0.0 if n == 1 else (7.0 if k == n - 1 else 2.0 if k == 0 else -5.0)
            vel = (76.0 - 12.0 * Wc) + Wc * (acc + shape) + boost + rf.g() * 4.0 * min(W, 1.5)
            if low: off -= 0.004 * T
            vel = int(max(18.0, min(120.0, vel)))
            extra = rf.uni(0.0, 0.02) * S
            t = max(-ctx.lead_s + 0.001, lay + off)
            ev.append({'t': t, 'pitch': v[k], 'vel': vel, 'long': kind == L_ and dur_beats is None,
                       'dur': (dur_beats * ctx.spb if dur_beats is not None else 0.0), 'extra': extra})

    def strike(self, bar, slot, kind, boost, ctx, follow=False):
        p = self.p; rf = self.rf
        beat = bar * 4 + self.slot_beats[slot]
        up = False if follow else self.on_up(slot)
        if self.pending is not None and self.pending[0] == bar and self.pending[1] == slot:
            v, chord = self.pending[2], self.pending[3]; antic = False
        elif follow:
            chord = ctx.chord_at(beat); antic = False; v = self.pick_voicing(chord, ctx)
        else:
            chord, antic = self.chord_for(bar, slot, ctx)
            v = self.pick_voicing(chord, ctx)
        self.pending = None
        S = p.loose * p.slop
        dur = rf.uni(0.28, 0.45)
        self.dyn = max(-10.0, min(10.0, self.dyn + rf.g() * 2.5))
        acc = (12.0 if (kind == L_ and up) else 4.0 if antic else 0.0) + self.dyn
        if rf.u() < 0.06 * S:                                            # ghosted chord
            acc -= 25.0; kind = S_
        u = rf.u(); u2 = rf.u()
        v = list(v)
        if kind == S_ and len(v) >= 4 and u < 0.25 * min(1.0, S):        # lighter stab: lose an inner note
            v.pop(1 + int(u2 * (len(v) - 2)))
        ev = []
        self.emit(ev, v, kind, acc, boost, up, ctx, dur_beats=(dur if kind == S_ else None))
        ur = rf.u()
        want_root = p.roots == 2 or (p.roots == 1 and ur < 0.5)
        if want_root and (slot == 0 or chord != self.root_chord):
            r = 36 + chord[0]
            if r > 43: r -= 12
            self.emit(ev, [r], kind, acc - 4.0, boost, up, ctx, low=True, dur_beats=(dur if kind == S_ else None))
            self.root_chord = chord
        pedal = []
        if p.pedal > 0:
            t0 = min(e['t'] for e in ev)
            pedal.append((t0, 0.0))                                      # lift as the hands land
            if kind == L_: pedal.append((t0 + 0.07, p.pedal))            # catch it again
        self.last_hit_beat = beat
        return {'notes': ev, 'pedal': pedal, 'hit': True, 'kind': kind, 'chord': chord}

    def decide(self, bar, slot, ctx):
        """Called once per slot, ctx.lead_s seconds before its nominal time."""
        p = self.p
        kind = self.cellmap.get(slot)
        beat = bar * 4 + self.slot_beats[slot]
        promised = self.pending is not None and self.pending[0] == bar and self.pending[1] == slot
        extra = kind is None and p.listen > 0 and ctx.solo_connected and slot in UPBEATS[self.slots]
        hit = False; boost = 0.0
        if promised:
            hit = True
            if kind is None: kind = S_
        elif kind is not None or extra:
            if p.listen > 0 and ctx.solo_connected:
                if ctx.solo_sounding:                                    # stay out of the way
                    if not extra:
                        hit = not (self.rr.u() < 0.8 * p.listen)
                        if hit: kind = S_; boost = -12.0
                elif ctx.solo_recent and ctx.solo_gap < 2.5:             # answer in the gap
                    if extra:
                        hit = not (self.rr.u() > 0.55 * p.listen)
                        if hit: kind = S_
                    else: hit = True
                    if hit: boost = 8.0
                else: hit = not extra
            else: hit = not extra
        if hit:
            return self.strike(bar, slot, kind, boost, ctx)
        # chromatic side-slip: a quick soft chord a semitone off, one slot before a fresh chord
        nxt = slot + 1
        if (p.out > 0 and nxt < self.slots and self.cellmap.get(nxt) is not None and self.pending is None
                and bar * 4 + self.slot_beats[nxt] - self.last_hit_beat > 0.9
                and not (p.listen > 0 and ctx.solo_connected and ctx.solo_sounding)):
            if self.rv.u() < 0.35 * p.out:
                chord, _ = self.chord_for(bar, nxt, ctx)
                v = self.pick_voicing(chord, ctx)
                sh = 1 if self.rv.u() < 0.5 else -1
                self.pending = (bar, nxt, v, chord)
                ev = []
                self.emit(ev, [x + sh for x in v], S_, -10.0 + self.dyn, 0.0, self.on_up(slot), ctx, dur_beats=0.3)
                return {'notes': ev, 'pedal': [], 'hit': False, 'kind': S_, 'chord': chord}
        return {'notes': [], 'pedal': [], 'hit': False, 'kind': None, 'chord': None}

    def cap_beats(self): return 3.8 if self.p.groove == BALLAD else 2.6

# ------------------------------------------------------------------ chord recognition (POLY IN)
#           guides   other    allowed tensions
TEMPL = [(MAJ6, (4, 9),  (0, 7), (2,)),
         (MAJ,  (4, 11), (0, 7), (2, 6, 9)),
         (M7,   (3, 10), (0, 7), (2, 5)),
         (M6,   (3, 9),  (0, 7), (2, 5)),
         (MMAJ, (3, 11), (0, 7), (2,)),
         (DOM,  (4, 10), (0, 7), (2, 9, 6)),
         (ALT,  (4, 10), (0,),   (1, 3, 8, 6)),
         (HDIM, (3, 10), (0, 6), (5, 8, 2)),
         (DIM,  (3, 9),  (0, 6), (11, 2)),
         (SUS,  (5, 10), (0, 7), (2, 9))]

def detect_chord(notes, last_quality=DOM):
    """notes: MIDI numbers sounding at the POLY input. Returns (root, quality) or None."""
    if not notes: return None
    low = min(notes) % 12
    pcs = sorted(set(n % 12 for n in notes))
    if len(pcs) == 1: return (low, last_quality)
    if len(pcs) == 2:
        iv = [(x - low) % 12 for x in pcs if x != low][0]
        q = {3: M7, 4: MAJ6, 10: DOM, 11: MAJ, 9: MAJ6, 6: DIM, 5: SUS, 8: ALT}.get(iv, last_quality)
        return (low, q)
    best = None; bests = -1e9
    for r in pcs:
        rel = set((x - r) % 12 for x in pcs)
        for i, (q, guides, other, allowed) in enumerate(TEMPL):
            s = 3.0 * len(rel & set(guides)) + 1.0 * len(rel & set(other))
            s -= 3.0 * len(rel - set(guides) - set(other) - set(allowed))
            if q == ALT: s += 2.0 if rel & {1, 3, 8} else -5.0
            if q in (HDIM, DIM) and 6 not in rel: s -= 4.0
            if r == low: s += 1.5
            s -= 0.01 * i
            if s > bests: bests = s; best = (r, q)
    return best

# ------------------------------------------------------------------ offline host
def parse_chart(text):
    """'Dm7 G7 | Cmaj7' -> [(beat, (root, quality)), ...], total_beats. Bars of 4, chords share a bar equally."""
    import re
    N = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}
    table = [('maj7', MAJ), ('M7', MAJ), ('6/9', MAJ6), ('m7b5', HDIM), ('mMaj7', MMAJ), ('m6', M6), ('m9', M7),
             ('m11', M7), ('m7', M7), ('dim7', DIM), ('dim', DIM), ('7alt', ALT), ('7b9', ALT), ('7#9', ALT),
             ('7sus', SUS), ('sus', SUS), ('13', DOM), ('9', DOM), ('7', DOM), ('6', MAJ6), ('m', M7), ('', MAJ6)]
    out = []
    bars = [b.strip() for b in text.split('|') if b.strip()]
    for i, b in enumerate(bars):
        ch = b.split()
        for j, c in enumerate(ch):
            m = re.match(r'^([A-G])([#b]?)(.*)$', c)
            root = (N[m.group(1)] + {'#': 1, 'b': -1, '': 0}[m.group(2)]) % 12
            q = [v for k, v in table if k == m.group(3)]
            if not q: raise ValueError(c)
            out.append((i * 4 + j * 4.0 / len(ch), (root, q[0])))
    return out, len(bars) * 4

LEAD_BEATS = 0.12          # chart mode: decisions are taken this long before the slot
EXT_LATE_S = 0.004         # CV / POLY mode: decisions are taken this long AFTER the slot

def simulate(chart, total_beats, bpm, nbars, seed=1, params=None, solo=None, external=False, param_fn=None):
    """Returns (notes, pedal, log). notes: (on_s, off_s, pitch, vel). solo: [(on_s, off_s, pitch)]."""
    eng = Engine(seed)
    if params is not None: eng.p = params
    spb = 60.0 / bpm
    solo = solo or []
    def chord_at(beat):
        b = beat % total_beats; cur = chart[0][1]
        for x, c in chart:
            if x <= b + 1e-9: cur = c
        return cur
    def next_change(beat):
        cur = chord_at(beat); b = beat % total_beats
        order = sorted(chart)
        for x, c in order:
            if x > b + 1e-9 and c != cur: return c
        for x, c in order:
            if c != cur: return c
        return None
    notes = []; pedal = []; log = []
    held = []                                # long notes waiting for their release: [on_nominal_s, cap_s, pitch, vel, on_s, extra]
    def release(now_s, t_next_nominal):
        for h in held:
            off = min(h[0] + h[1], max(h[0] + 0.9 * spb, t_next_nominal - LEAD_BEATS * spb))
            off = max(off, min(now_s, h[0] + h[1])) + h[5]   # never before now, never past the cap
            notes.append((h[4], max(off, h[4] + 0.02), h[2], h[3]))
        del held[:]
    last_ext_chord = None; last_hit_s = -9.0
    for bar in range(nbars):
        if param_fn: param_fn(eng.p, bar)
        eng.begin_bar(bar, bpm)
        for slot in range(eng.slots):
            beat = bar * 4 + eng.slot_beats[slot]
            nominal = beat * spb
            ctx = Ctx(); ctx.spb = spb
            if external:
                ctx.lead_s = -EXT_LATE_S
                now = nominal + EXT_LATE_S
                cur = chord_at(beat)
                ctx.chord_at = lambda b, cur=cur: cur
                ctx.next_change = None
            else:
                ctx.lead_s = LEAD_BEATS * spb
                now = nominal - ctx.lead_s
                ctx.chord_at = chord_at; ctx.next_change = next_change
            if solo:
                ctx.solo_connected = True
                ctx.solo_sounding = any(a <= now < b for a, b, _ in solo)
                ends = [b for a, b, _ in solo if b <= now]
                ctx.solo_gap = (now - max(ends)) / spb if ends else 99.0
                ctx.solo_recent = any(now - 2.5 * spb < a <= now for a, b, _ in solo)
                rec = [p for a, b, p in solo if now - 2.0 * spb < b and a <= now]
                ctx.solo_low = float(min(rec)) if rec else None
            res = None
            if external and last_ext_chord is not None and cur != last_ext_chord:
                # the chord changed under us: let go, and maybe catch the new chord straight away
                release(now, nominal + LEAD_BEATS * spb)
                u = eng.rr.u()
                has_cell = eng.cellmap.get(slot) is not None
                if not has_cell and now - last_hit_s > 0.15 * spb and u < 0.5 + 0.5 * eng.p.density:
                    res = eng.strike(bar, slot, L_, -4.0, ctx, follow=True)
            if external: last_ext_chord = cur
            if res is None: res = eng.decide(bar, slot, ctx)
            if res['hit']:
                release(now, nominal); last_hit_s = now
            for e in res['notes']:
                on = nominal + e['t']
                if e['long']: held.append([nominal, eng.cap_beats() * spb, e['pitch'], e['vel'], on, e['extra']])
                else: notes.append((on, on + e['dur'] + e['extra'], e['pitch'], e['vel']))
            for t, v in res['pedal']: pedal.append((nominal + t, v))
            if res['notes']:
                log.append((bar, slot, res['hit'], res['kind'], res['chord'], sorted(set(e['pitch'] for e in res['notes']))))
    release(0.0, 1e9)
    notes.sort()
    return notes, pedal, log
