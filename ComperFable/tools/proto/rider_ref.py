"""Rider: the reference player. An improvising jazz drummer that plans one bar at a time from what it is told at the bar line:
the tempo, where the chorus is, what the pianist is about to play in this bar (Comper's figure), and where the bass runs.
The C++ engine (src/dsp/RiderEngine.hpp) must reproduce plan_bar() exactly: same draws from the same two generators, in the same order.

Every random choice uses comper_ref.Rng. Two streams: r decides WHAT is played, h is the HANDS (timing and loudness scatter),
so changing LOOSE never changes the notes.
"""
from comper_ref import Rng, auto_swing

# voices (the kit engine's voice numbers, also the order of the trigger groups in the manual)
RIDE, BELL, CRASH, HATF, HATC, HATO, SNARE, STICK, KICK, TOM_HI, TOM_LO, BR_TAP, BR_SWEEP, BR_HATF = range(14)
VOICE_NAMES = ['ride', 'bell', 'crash', 'hat-foot', 'hat-closed', 'hat-open', 'snare', 'cross-stick', 'kick', 'tom-high', 'tom-low', 'brush-tap', 'brush-sweep', 'brush-hat-foot']
SWING, BOSSA, BALLAD, SOUL, HOUSE = range(5)          # same order as Comper's GROOVE
FEEL_NAMES = ['SWING', 'BOSSA', 'BALLAD', 'SOUL', 'HOUSE']
S_, L_ = 0, 1                                         # Comper's hit kinds: short, long
SUB0, SUBM, SUBA = 0, 1, 2                            # on the beat, the middle triplet, the swung "and"


class RP:
    def __init__(self, **kw):
        self.feel = SWING
        self.energy = 0.5    # how hard the band is playing: level, busyness, bombs, crashes
        self.comp = 0.5      # how much snare and bass drum conversation
        self.listen = 1.0    # 0 plays its own figures; 1 catches the piano's pushes, leaves its other hits alone, answers in its gaps
        self.fills = 0.5     # set-ups at the ends of phrases and into the top of the chorus
        self.feather = 0.5   # quiet bass drum on all four beats (swing, ballad)
        self.repeat = 0.6    # keep the two-bar comping figure
        self.loose = 1.0     # human hands: scales every timing and loudness scatter
        self.ahead = 0.5     # 0 lays back 12 ms, 1 plays 12 ms on top
        self.swing = 0.0     # Comper's SWING, so the "and" lands where the pianist's does
        self.__dict__.update(kw)


class BarIn:
    """what the drummer is told at the bar line"""
    def __init__(self, slots=8, kind=None, slot_beat=None, push=None, bass_off=None, has_piano=False, force_fill=0):
        self.has_piano = has_piano
        self.slots = slots
        self.kind = kind if kind is not None else [-1] * 16          # -1 no hit, S_, L_ per slot
        self.slot_beat = slot_beat if slot_beat is not None else [0.0] * 16   # beats from the bar line
        self.push = push if push is not None else [False] * 16      # this upbeat anticipates the next chord
        self.bass_off = bass_off or []                               # beats (from the bar line) of bass notes off the beat
        self.force_fill = force_fill                                 # 0 no, 1 small, 2 big (the FILL input)


class Rider:
    def __init__(self, seed=1):
        self.reset(seed)

    def reset(self, seed):
        self.r = Rng(seed * 5 + 1); self.h = Rng(seed * 5 + 2)
        self.figure = None          # swing comping figure: list of (bar parity, beat, sub)
        self.kfig = None            # soul bass-drum figure: list of (bar parity, slot)
        self.landing = 0            # 0 none, 1 bass drum on the next 1, 2 bass drum and crash
        self.drift = 0.0
        self.prev_hits = []         # last bar's piano hits, beats from THIS bar line (negative)
        self.ev = []
        self.stats = dict(pushes=0, caught=0, comps=0, fills=0)

    # ---- helpers
    def swing_f(self, bpm, p):
        ratio = max(1.0, min(3.0, auto_swing(bpm) * 2.0 ** (p.swing * 0.6)))
        return ratio / (1.0 + ratio)

    def hit(self, beat, voice, vel, late=0.0, jit=1.0, dur=0.0):
        """beat: beats from the bar line. late: seconds. The hands draw exactly two gaussians per hit, time then loudness."""
        L = self.p.loose
        offs = (late + self.drift) * L + self.h.g() * 0.004 * jit * L - (self.p.ahead - 0.5) * 0.024
        v = int(max(1.0, min(127.0, vel + self.h.g() * 4.0 * L)))
        self.ev.append((beat, offs, voice, v, dur))

    def piano_at(self, beat, win):
        """the piano hit nearest to beat (beats from the bar line) within win, as its slot number, or -1; ties go to the earlier slot"""
        best = -1; bd = 1e9
        if not self.bi.has_piano: return -1
        for s in range(self.bi.slots):
            if self.bi.kind[s] < 0: continue
            d = abs(self.bi.slot_beat[s] - beat)
            if d <= win and d < bd - 1e-12: best = s; bd = d
        return best

    def strong(self, s): return self.bi.kind[s] == L_ or self.bi.push[s]

    def quiet(self, beat):
        """no piano hit from 1.6 beats before to 0.4 after"""
        if not self.bi.has_piano: return True
        for s in range(self.bi.slots):
            if self.bi.kind[s] >= 0 and beat - 1.6 <= self.bi.slot_beat[s] <= beat + 0.4: return False
        for b in self.prev_hits:
            if beat - 1.6 <= b <= beat + 0.4: return False
        return True

    def pick_fill(self, bar, chorus_bars, e):
        """0 none, 1 small, 2 big. Draw order is fixed: at most two draws."""
        p = self.p; r = self.r
        if self.bi.force_fill: return self.bi.force_fill
        if (bar + 1) % chorus_bars == 0:
            return 2 if r.u() < min(1.0, p.fills * 1.5) else 0
        if (bar + 1) % 8 == 0:
            if r.u() < p.fills * 0.9: return 2 if r.u() < 0.3 + 0.5 * e else 1
            return 0
        if (bar + 1) % 4 == 0:
            return 1 if r.u() < p.fills * 0.45 else 0
        return 0

    def land(self, e):
        if self.landing:
            self.hit(0.0, KICK, 76 + 34 * e)
            if self.landing == 2 and e > 0.4: self.hit(0.0, CRASH, 76 + 34 * e, late=0.002)
            self.landing = 0

    # ---- one bar
    def plan_bar(self, bar, bpm, chorus_bars, p, bi):
        """returns this bar's hits: (beat from the bar line, offset in seconds, voice, velocity 1..127, length in beats (sweeps only))"""
        self.p = p; self.bi = bi; self.ev = []; self.bpm = bpm
        e = max(0.0, min(1.0, p.energy))
        if p.feel == SWING: self.swing_bar(bar, chorus_bars, e)
        elif p.feel == BALLAD: self.ballad_bar(bar, chorus_bars, e)
        elif p.feel == BOSSA: self.bossa_bar(bar, chorus_bars, e)
        elif p.feel == SOUL: self.soul_bar(bar, chorus_bars, e)
        else: self.house_bar(bar, chorus_bars, e)
        self.prev_hits = [bi.slot_beat[s] - 4.0 for s in range(bi.slots) if bi.has_piano and bi.kind[s] >= 0]
        return self.ev

    # ------------------------------------------------------------ swing, sticks
    FILL_SMALL = [[(3, SUB0), (3, SUBM), (3, SUBA)], [(2, SUBA), (3, SUB0), (3, SUBA)], [(3, SUBA)], [(2, SUBA), (3, SUBM), (3, SUBA)]]
    FILL_BIG = [[(2, SUB0), (2, SUBM), (2, SUBA), (3, SUB0), (3, SUBM), (3, SUBA)], [(2, SUB0), (2, SUBA), (3, SUB0), (3, SUBM), (3, SUBA)],
                [(1, SUBA), (2, SUB0), (2, SUBA), (3, SUB0), (3, SUBM), (3, SUBA)]]
    RIDE_PATS = [[(0, SUB0), (1, SUB0), (1, SUBA), (2, SUB0), (3, SUB0), (3, SUBA)],
                 [(0, SUB0), (1, SUB0), (1, SUBA), (2, SUB0), (3, SUB0)],
                 [(0, SUB0), (1, SUB0), (2, SUB0), (2, SUBA), (3, SUB0), (3, SUBA)],
                 [(0, SUB0), (1, SUB0), (2, SUB0), (3, SUB0)],
                 [(0, SUB0), (0, SUBA), (1, SUB0), (1, SUBA), (2, SUB0), (3, SUB0), (3, SUBA)]]

    def swing_bar(self, bar, chorus_bars, e):
        p = self.p; r = self.r; bi = self.bi
        f = self.swing_f(self.bpm, p)
        def pos(b, sub): return b + (0.0 if sub == SUB0 else f / 2.0 if sub == SUBM else f)
        self.drift = 0.6 * self.drift + self.h.g() * 0.0015
        # --- where is the phrase? fills and landings
        fill = self.pick_fill(bar, chorus_bars, e)
        fill_pos = []; land_on_push = False
        if fill:
            self.stats['fills'] += 1
            T = self.FILL_SMALL if fill == 1 else self.FILL_BIG
            fill_pos = list(T[r.n(len(T))])
            s = self.piano_at(pos(3, SUBA), 0.18)
            if s >= 0 and self.strong(s):
                if r.u() < p.listen:
                    land_on_push = True
                    fill_pos = [q for q in fill_pos if q != (3, SUBA)]
        fill_beats = sorted(set(q[0] for q in fill_pos)) if (fill == 2 or len(fill_pos) > 1) else []
        # --- ride cymbal: the time
        fast = self.bpm > 190
        w = [5.0, 1.2, 0.6 + e, 0.5 + (2.5 if fast else 0.0), 0.3 + 1.2 * e]
        x = r.u() * (w[0] + w[1] + w[2] + w[3] + w[4]); k = 0
        while k < 4 and x >= w[k]: x -= w[k]; k += 1
        base = 50 + 34 * e
        for b, sub in self.RIDE_PATS[k]:
            if b in fill_beats: continue                      # both hands are on the drums
            v = base + (7 if (b in (1, 3) and sub == SUB0) else 0) - (13 if sub == SUBA else 0)
            self.hit(pos(b, sub), RIDE, v, late=0.0, jit=0.8)
        for b in (1, 3): self.hit(float(b), HATF, 58 + 16 * e, late=-0.003, jit=0.7)
        if p.feather > 0.05 and e < 0.85:
            for b in range(4): self.hit(float(b), KICK, 18 + 14 * p.feather, late=-0.002, jit=0.8)
        # --- comping: snare and bass drum, a kept two-bar figure
        par = bar % 2
        if par == 0 or self.figure is None:
            if self.figure is None or r.u() > p.repeat:
                fig = []
                for bb in range(2):
                    for b in range(4):
                        for sub, wt in ((SUBA, 1.0 if b in (1, 3) else 0.7), (SUB0, 0.4 if b == 3 else 0.22), (SUBM, 0.18 * e)):
                            if r.u() < p.comp * (0.22 + 0.5 * e) * wt: fig.append((bb, b, sub))
                self.figure = fig
        fig = self.figure
        run = 0
        for b in range(4):
            for sub in (SUB0, SUBM, SUBA):
                if (b, sub) in fill_pos or b in fill_beats: continue
                beat = pos(b, sub)
                want = (par, b, sub) in fig
                s = self.piano_at(beat, 0.17) if sub != SUBM else -1
                caught = False
                if s >= 0:
                    if sub == SUBA and self.strong(s):
                        self.stats['pushes'] += 1
                        if r.u() < p.listen * (0.55 + 0.4 * e): want = True; caught = True
                    elif want:
                        if r.u() < 0.65 * p.listen: want = False      # leave the pianist's hit alone
                elif sub == SUBA and not want and bi.has_piano:
                    if self.quiet(beat):                               # the piano is resting: answer in the gap
                        if r.u() < 0.28 * p.listen * p.comp * (0.5 + e): want = True
                if want and not caught:                                # stay out of the way of a bass run
                    near = False
                    for bb in bi.bass_off:
                        if abs(bb - beat) < 0.12: near = True
                    if near:
                        if r.u() < 0.5 * p.listen: want = False
                if want and run >= (2 if e < 0.7 else 3): want = False
                run = run + 1 if want else 0
                if not want: continue
                self.stats['comps'] += 1
                if caught: self.stats['caught'] += 1
                bomb = r.u() < (0.16 + 0.26 * e) * (1.5 if (sub == SUBA and b in (1, 3)) else 1.0)
                if caught and e > 0.55 and bi.kind[s] == L_:
                    with_snare = r.u() < 0.5
                    self.hit(beat, KICK, 78 + 30 * e, late=0.0)
                    self.hit(beat, SNARE if with_snare else RIDE, 80 + 25 * e, late=0.003)
                elif bomb:
                    self.hit(beat, KICK, 62 + 40 * e, late=0.001)
                else:
                    ghost = (r.u() < 0.5 - 0.3 * e) and not caught
                    self.hit(beat, SNARE, (36 + 14 * e) if ghost else (62 + 34 * e), late=0.005)
        # --- the fill, and where it lands
        n = len(fill_pos)
        toms = e > 0.45 and fill == 2
        for i, (b, sub) in enumerate(fill_pos):
            x = (i + 1) / float(n)
            voice = SNARE
            if toms and i >= n - 3:
                if r.u() < 0.75: voice = (TOM_HI, TOM_HI, TOM_LO)[i - (n - 3)]
            self.hit(pos(b, sub), voice, 48 + 30 * e + 26 * x, late=-0.004 * x, jit=0.9)
        self.land(e)
        if fill:
            if land_on_push:
                self.hit(pos(3, SUBA), KICK, 84 + 30 * e)
                self.hit(pos(3, SUBA), CRASH if (fill == 2 and e > 0.4) else RIDE, 82 + 30 * e, late=0.002)
                self.stats['caught'] += 1
            else:
                self.landing = 2 if fill == 2 else 1

    # ------------------------------------------------------------ ballad, brushes
    def ballad_bar(self, bar, chorus_bars, e):
        p = self.p; r = self.r; bi = self.bi
        f = self.swing_f(self.bpm, p)
        self.drift = 0.6 * self.drift + self.h.g() * 0.002
        self.landing = 0
        for b in range(4):
            # the left hand sweeps a circle per beat; the right hand taps
            self.hit(float(b), BR_SWEEP, 40 + 22 * e + (5 if b in (1, 3) else 0), late=0.0, jit=1.2, dur=0.95)
            if b in (1, 3): self.hit(float(b), BR_TAP, 44 + 24 * e, late=0.006)
            if r.u() < 0.35 + 0.4 * p.comp: self.hit(b + f, BR_TAP, 30 + 18 * e, late=0.008)
            if b in (1, 3): self.hit(float(b), BR_HATF, 40 + 14 * e, late=-0.003)
        if p.feather > 0.05: self.hit(0.0, KICK, 26 + 12 * p.feather)
        if bi.has_piano:
            for s in range(bi.slots):
                if bi.kind[s] < 0 or not self.strong(s): continue
                self.stats['pushes'] += 1
                if r.u() < 0.5 * p.listen:
                    self.hit(bi.slot_beat[s], BR_TAP, 68 + 26 * e, late=0.004); self.stats['caught'] += 1
        fill = self.pick_fill(bar, chorus_bars, e)
        if fill:
            for i, sub in enumerate((0.0, f / 2.0, f)): self.hit(3 + sub, BR_TAP, 40 + 10 * i + 20 * e, late=0.004)
            self.stats['fills'] += 1

    # ------------------------------------------------------------ bossa
    CLAVE = [[(0, 0), (1, 1), (3, 0)], [(1, 0), (2, 1)]]            # (beat, 0 on / 1 the "and"): cross-stick, two-bar figure

    def bossa_bar(self, bar, chorus_bars, e):
        p = self.p; r = self.r
        self.drift = 0.6 * self.drift + self.h.g() * 0.0012
        top = RIDE if e > 0.55 else HATC
        for b in range(4):
            self.hit(float(b), top, 50 + 22 * e, jit=0.7)
            self.hit(b + 0.5, top, 40 + 20 * e, jit=0.7)
            if b in (1, 3): self.hit(float(b), HATF, 54 + 12 * e, late=-0.003)
        for b, half in ((0, 0), (1, 1), (2, 0), (3, 1)): self.hit(b + 0.5 * half, KICK, (70 if half == 0 else 54) + 16 * e, late=-0.001)
        extra = r.u() < 0.25 * (1 - p.repeat) + 0.1
        for b, half in self.CLAVE[bar % 2]: self.hit(b + 0.5 * half, STICK, 60 + 18 * e, late=0.004)
        if extra: self.hit(3.5, STICK, 60 + 18 * e, late=0.004)
        self.land(e)
        fill = self.pick_fill(bar, chorus_bars, e)
        if fill:
            for half in (0, 1): self.hit(3 + 0.5 * half, SNARE, 60 + 24 * e)
            self.stats['fills'] += 1
            if fill == 2: self.landing = 2

    # ------------------------------------------------------------ soul: sixteenths with a backbeat
    def grid16(self, soul):
        sw = max(0.5, min(0.67, (0.54 if soul else 0.5) + 0.12 * self.p.swing))
        return [(s // 4) + (0.0, sw / 2, 0.5, 0.5 + sw / 2)[s % 4] for s in range(16)]

    KCAND = [(3, 0.30), (6, 0.45), (7, 0.25), (8, 0.35), (10, 0.5), (11, 0.3), (14, 0.3)]     # bass-drum slots and how likely

    def soul_bar(self, bar, chorus_bars, e):
        p = self.p; r = self.r; bi = self.bi
        g = self.grid16(True)
        self.drift = 0.6 * self.drift + self.h.g() * 0.0012
        fill = self.pick_fill(bar, chorus_bars, e)
        fill_from = 16
        if fill: fill_from = 12 if fill == 1 else 8; self.stats['fills'] += 1
        par = bar % 2
        if par == 0 or self.kfig is None:
            if self.kfig is None or r.u() > p.repeat:
                kf = []
                for bb in range(2):
                    for s, wt in self.KCAND:
                        if r.u() < wt * (0.35 + 0.9 * p.comp) * (0.6 + 0.6 * e): kf.append((bb, s))
                self.kfig = kf
        # the time: hi-hat eighths, sixteenths as it heats up; ride bell when it is really cooking
        bell = e > 0.72
        open_at = 14 if (par == 1 and r.u() < 0.15 + 0.3 * e) else -1
        for s in range(16):
            if s >= fill_from: break
            if s % 2 == 1:
                if r.u() < max(0.0, e - 0.45) * 1.4 and not bell: self.hit(g[s], HATC, 34 + 18 * e, jit=0.6)
                continue
            if bell: self.hit(g[s], BELL if s % 4 == 0 else RIDE, (58 if s % 4 == 0 else 46) + 26 * e, jit=0.6)
            elif s == open_at: self.hit(g[s], HATO, 60 + 24 * e, jit=0.6)
            else: self.hit(g[s], HATC, (56 if s % 4 == 0 else 44) + 24 * e, jit=0.6)
        if bell:
            for b in (1, 3): self.hit(float(b), HATF, 58 + 14 * e, late=-0.003)
        # backbeat
        for s in (4, 12):
            if s >= fill_from: continue
            self.hit(g[s], STICK if e < 0.3 else SNARE, (64 if e < 0.3 else 80) + 30 * e, late=0.004)
        # bass drum: the 1, the kept figure, and the pianist's strong hits
        landed = self.landing != 0
        self.land(e)
        kicks = [0] + [s for (bb, s) in self.kfig if bb == par]
        if bi.has_piano:
            for s in range(16):
                beat = g[s]
                if s in (4, 12) or s in kicks: continue
                ps = self.piano_at(beat, 0.07)
                if ps >= 0 and self.strong(ps):
                    self.stats['pushes'] += 1
                    if r.u() < p.listen * (0.4 + 0.4 * e): kicks.append(s); self.stats['caught'] += 1
        for s in sorted(set(kicks)):
            if s >= fill_from: continue
            if s == 0 and landed: continue                         # the landing already played the 1
            self.hit(g[s], KICK, (88 if s == 0 else 70) + 24 * e, late=-0.001)
        # ghost notes
        for s in (7, 9, 15, 3):
            if s >= fill_from: continue
            if r.u() < p.comp * (0.2 + 0.35 * e): self.hit(g[s], SNARE, 28 + 10 * e, late=0.005); self.stats['comps'] += 1
        # the fill: sixteenths on the snare, down the toms if it is a big one
        n = 16 - fill_from
        for i in range(n):
            s = fill_from + i; x = (i + 1) / float(n)
            if fill == 1 and i % 2 == 1:
                if r.u() < 0.5: continue
            voice = SNARE
            if fill == 2 and e > 0.45 and i >= n - 4: voice = TOM_HI if i < n - 2 else TOM_LO
            self.hit(g[s], voice, 50 + 30 * e + 24 * x, late=-0.003 * x, jit=0.8)
        if fill: self.landing = 2 if fill == 2 else 1

    # ------------------------------------------------------------ house: four on the floor
    def house_bar(self, bar, chorus_bars, e):
        p = self.p; r = self.r; bi = self.bi
        g = self.grid16(False)
        self.drift = 0.6 * self.drift + self.h.g() * 0.0006
        fill = self.pick_fill(bar, chorus_bars, e)
        fill_from = 16
        if fill: fill_from = 12 if fill == 1 else 8; self.stats['fills'] += 1
        crash = self.landing == 2 and e > 0.4
        self.landing = 0
        for s in range(16):
            beat = g[s]
            if s % 4 == 0:
                if s < fill_from or fill == 1: self.hit(beat, KICK, 98 + 18 * e, late=-0.001, jit=0.4)
                if s in (4, 12) and e > 0.25 and s < fill_from: self.hit(beat, SNARE, 70 + 26 * e, late=0.003, jit=0.5)
            elif s % 4 == 2:
                if s < fill_from: self.hit(beat, HATO if e > 0.35 else HATC, 58 + 30 * e, jit=0.5)
            else:
                if s < fill_from:
                    if r.u() < max(0.0, e - 0.4) * 1.5: self.hit(beat, HATC, 32 + 20 * e, jit=0.5)
                    ps = self.piano_at(beat, 0.07)
                    if ps >= 0 and self.strong(ps):
                        self.stats['pushes'] += 1
                        if r.u() < p.listen * p.comp: self.hit(beat, SNARE, 34 + 16 * e, late=0.004); self.stats['caught'] += 1
        if crash: self.hit(0.0, CRASH, 80 + 30 * e, late=0.002)
        n = 16 - fill_from
        for i in range(n):
            s = fill_from + i; x = (i + 1) / float(n)
            self.hit(g[s], SNARE, 44 + 30 * e + 36 * x, late=-0.002 * x, jit=0.6)
        if fill: self.landing = 2 if fill == 2 else 1


def play(feel_or_none, bars_in, bpm, chorus_bars, seed=1, p=None, energy_fn=None):
    """bars_in: one BarIn per bar. Returns (notes, rider). notes: (t seconds from beat 0, voice, vel, dur seconds, beat)"""
    p = p or RP(); d = Rider(seed); spb = 60.0 / bpm; out = []
    if feel_or_none is not None: p.feel = feel_or_none
    for bar, bi in enumerate(bars_in):
        if energy_fn: p.energy = energy_fn(bar)
        for beat, offs, voice, vel, dur in d.plan_bar(bar, bpm, chorus_bars, p, bi):
            out.append(((bar * 4 + beat) * spb + offs, voice, vel, dur * spb, bar * 4 + beat))
    return out, d
