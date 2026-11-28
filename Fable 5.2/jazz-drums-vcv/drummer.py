"""Prototype of the improvising jazz drummer (the third player with Comper and Walker).
First sketch of the PLAYER only: what to hit and when. Sounds here are General MIDI kits.

The drummer is given what a real drummer hears and knows:
  - the tempo, the bar, the length of the chorus (from Comper's chart),
  - the piano's hits (time, weight, whether it is a push into the next chord),
  - the bass notes (to stay out of the way of its runs).
It returns drum notes (t0, t1, gm_note, vel) in seconds from beat 0.
"""
import math, random

RIDE, BELL, CRASH, HATF, HATC, HATO, SNARE, STICK, KICK = 51, 53, 49, 44, 42, 46, 38, 37, 36
TOM_HI, TOM_MID, TOM_LO = 48, 45, 41
BR_TAP, BR_SLAP, BR_SWIRL = 38, 39, 40


def auto_swing(bpm):
    return max(1.15, min(2.2, 2.05 - (bpm - 120.0) / 170.0))


class DP:
    def __init__(self, **kw):
        self.energy = 0.5    # how hard the band is playing: level, busyness, bombs, crashes
        self.comp = 0.5      # how much snare and bass drum conversation
        self.listen = 1.0    # 0 plays its own figures; 1 catches the piano's pushes and answers in its gaps
        self.fills = 0.5     # set-ups at the ends of phrases and into the top of the chorus
        self.feather = 0.5   # quiet bass drum on all four beats
        self.repeat = 0.6    # keep the two-bar comping figure
        self.loose = 1.0     # human hands
        self.seed = 1
        self.__dict__.update(kw)


def cluster_piano(notes, spb):
    """piano notes (on, off, pitch, vel) -> hits: dict(beat, t, w 0..1, n, long)"""
    hits = []
    for on, off, pitch, vel in sorted(notes):
        if hits and on - hits[-1]['t'] < 0.045:
            h = hits[-1]; h['n'] += 1; h['v'] = max(h['v'], vel); h['off'] = max(h['off'], off)
        else:
            hits.append(dict(t=on, v=vel, n=1, off=off))
    for h in hits:
        h['beat'] = h['t'] / spb
        h['w'] = min(1.0, h['v'] / 100.0)
        h['long'] = (h['off'] - h['t']) / spb
    return hits


class Drummer:
    def __init__(self, bpm, total_beats, p):
        self.bpm = bpm; self.spb = 60.0 / bpm; self.p = p
        self.chorus_bars = max(1, int(round(total_beats / 4)))
        self.r = random.Random(p.seed)          # what to play
        self.h = random.Random(p.seed * 7919 + 17)   # the hands
        self.sw = auto_swing(bpm); self.f = self.sw / (1 + self.sw)
        self.out = []
        self.figure = None      # the kept two-bar comping figure: set of (bar parity, beat, sub)
        self.stats = dict(pushes=0, caught=0, comps=0, comps_on_piano=0, fills=0)
        self.drift = 0.0

    # grid position -> beat offset in the bar
    def pos(self, b, sub):
        return b + {0: 0.0, 'a': self.f, 'm': self.f / 2.0}[sub]

    def hit(self, bar, b, sub, note, vel, late=0.0, jit=1.0, dur=0.12):
        beat = bar * 4 + self.pos(b, sub)
        L = self.p.loose
        t = beat * self.spb + (late + self.drift) * L + self.h.gauss(0, 0.004 * jit) * L
        v = int(max(1, min(127, vel + self.h.gauss(0, 4.0) * L)))
        self.out.append((t, t + dur, note, v))

    # ------------------------------------------------------------ swing, sticks
    def swing_bar(self, bar, piano, bass, energy=None):
        p = self.p; r = self.r
        e = p.energy if energy is None else energy
        self.drift = 0.6 * self.drift + self.h.gauss(0, 0.0015)
        b0 = bar * 4.0
        ph = [x for x in piano if b0 - 0.3 <= x['beat'] < b0 + 4.2]

        def piano_at(beat, win=0.2):
            best = None
            for x in ph:
                if abs(x['beat'] - beat) <= win and (best is None or x['w'] > best['w']): best = x
            return best

        # --- where is the phrase? fills and landings
        last_of_chorus = (bar + 1) % self.chorus_bars == 0
        last_of_8 = (bar + 1) % 8 == 0
        last_of_4 = (bar + 1) % 4 == 0
        fill = None
        if last_of_chorus and r.random() < min(1.0, p.fills * 1.5): fill = 'big'
        elif last_of_8 and r.random() < p.fills * 0.9: fill = 'big' if r.random() < 0.3 + 0.5 * e else 'small'
        elif last_of_4 and r.random() < p.fills * 0.45: fill = 'small'
        fill_pos = []
        land_on_push = False
        if fill:
            self.stats['fills'] += 1
            T = {'small': [[(3, 0), (3, 'm'), (3, 'a')], [(2, 'a'), (3, 0), (3, 'a')], [(3, 'a')], [(2, 'a'), (3, 'm'), (3, 'a')]],
                 'big': [[(2, 0), (2, 'm'), (2, 'a'), (3, 0), (3, 'm'), (3, 'a')], [(2, 0), (2, 'a'), (3, 0), (3, 'm'), (3, 'a')],
                         [(1, 'a'), (2, 0), (2, 'a'), (3, 0), (3, 'm'), (3, 'a')]]}[fill]
            fill_pos = list(r.choice(T))
            # a push on the "and of 4": the fill lands there with the piano, not on the next 1
            push = piano_at(b0 + self.pos(3, 'a'), 0.18)
            if push and push['w'] > 0.5 and r.random() < p.listen:
                land_on_push = True
                fill_pos = [q for q in fill_pos if q != (3, 'a')]
        fill_beats = sorted(set(q[0] for q in fill_pos)) if fill == 'big' or len(fill_pos) > 1 else []

        # --- ride cymbal: the time
        fast = self.bpm > 190
        pats = [([(0, 0), (1, 0), (1, 'a'), (2, 0), (3, 0), (3, 'a')], 5.0),
                ([(0, 0), (1, 0), (1, 'a'), (2, 0), (3, 0)], 1.2),
                ([(0, 0), (1, 0), (2, 0), (2, 'a'), (3, 0), (3, 'a')], 0.6 + e),
                ([(0, 0), (1, 0), (2, 0), (3, 0)], 0.5 + (2.5 if fast else 0.0)),
                ([(0, 0), (0, 'a'), (1, 0), (1, 'a'), (2, 0), (3, 0), (3, 'a')], 0.3 + 1.2 * e)]
        tot = sum(w for _, w in pats); x = r.random() * tot
        for pat, w in pats:
            x -= w
            if x <= 0: break
        base = 50 + 34 * e
        for b, sub in pat:
            if b in fill_beats: continue                      # both hands are on the drums
            v = base + (7 if b in (1, 3) and sub == 0 else 0) - (13 if sub == 'a' else 0)
            self.hit(bar, b, sub, RIDE, v, late=0.0, jit=0.8)
        # hi-hat with the foot on 2 and 4
        for b in (1, 3): self.hit(bar, b, 0, HATF, 58 + 16 * e, late=-0.003, jit=0.7)
        # feathered bass drum
        if p.feather > 0.05 and e < 0.85:
            for b in range(4): self.hit(bar, b, 0, KICK, 18 + 14 * p.feather, late=-0.002, jit=0.8)

        # --- comping: snare and bass drum
        par = bar % 2
        if par == 0 and (self.figure is None or r.random() > p.repeat):
            fig = set()
            for bb in range(2):
                for b in range(4):
                    for sub, w in (('a', 1.0 if b in (1, 3) else 0.7), (0, 0.4 if b == 3 else 0.22), ('m', 0.18 * e)):
                        if r.random() < p.comp * (0.22 + 0.5 * e) * w: fig.add((bb, b, sub))
            self.figure = fig
        fig = self.figure or set()
        run = 0
        for b in range(4):
            for sub in (0, 'm', 'a'):
                if (b, sub) in fill_pos: continue
                if b in fill_beats: continue
                beat = b0 + self.pos(b, sub)
                want = (par, b, sub) in fig
                px = piano_at(beat, 0.17)
                is_push = bool(px and sub == 'a' and px['w'] > 0.45 and (px['long'] > 0.5 or px['w'] > 0.7))
                caught = False
                if px is not None and sub != 'm':
                    if is_push:
                        self.stats['pushes'] += 1
                        if r.random() < p.listen * (0.55 + 0.4 * e): want = True; caught = True
                    elif want and r.random() < 0.65 * p.listen: want = False      # leave the pianist's hit alone
                elif sub == 'a' and not want:
                    # the piano is resting: answer in the gap
                    quiet = not any(beat - 1.6 <= x['beat'] <= beat + 0.4 for x in ph)
                    if quiet and r.random() < 0.28 * p.listen * p.comp * (0.5 + e): want = True
                # stay out of the way of a bass run
                if want and not caught and any(abs(n[-1] - beat) < 0.12 and (n[-1] % 1.0) > 0.05 for n in bass) and r.random() < 0.5 * p.listen: want = False
                if want and run >= (2 if e < 0.7 else 3): want = False
                run = run + 1 if want else 0
                if not want: continue
                self.stats['comps'] += 1
                if px is not None: self.stats['comps_on_piano'] += 1
                if caught: self.stats['caught'] += 1
                bomb = r.random() < (0.16 + 0.26 * e) * (1.5 if sub == 'a' and b in (1, 3) else 1.0)
                if caught and e > 0.55 and px['w'] > 0.6:
                    self.hit(bar, b, sub, KICK, 78 + 30 * e, late=0.0)
                    self.hit(bar, b, sub, SNARE if r.random() < 0.5 else RIDE, 80 + 25 * e, late=0.003)
                elif bomb:
                    self.hit(bar, b, sub, KICK, 62 + 40 * e, late=0.001)
                else:
                    ghost = (not caught) and r.random() < 0.5 - 0.3 * e
                    self.hit(bar, b, sub, SNARE, (36 + 14 * e) if ghost else (62 + 34 * e), late=0.005)

        # --- the fill, and where it lands
        if fill_pos:
            n = len(fill_pos)
            toms = e > 0.45 and fill == 'big'
            for i, (b, sub) in enumerate(fill_pos):
                x = (i + 1) / n
                v = 48 + 30 * e + 26 * x
                note = SNARE
                if toms and i >= n - 3: note = [TOM_HI, TOM_MID, TOM_LO][i - (n - 3)] if r.random() < 0.75 else SNARE
                self.hit(bar, b, sub, note, v, late=-0.004 * x, jit=0.9)
        if fill:
            big = fill == 'big'
            if land_on_push:
                self.hit(bar, 3, 'a', KICK, 84 + 30 * e)
                self.hit(bar, 3, 'a', CRASH if (big and e > 0.4) else RIDE, 82 + 30 * e, late=0.002)
                self.stats['caught'] += 1; self.stats['pushes'] += 0
            else:
                self.landing = (bar + 1, big)
        if getattr(self, 'landing', None) and self.landing[0] == bar:
            big = self.landing[1]; self.landing = None
            self.hit(bar, 0, 0, KICK, 76 + 34 * e)
            if big and e > 0.4: self.hit(bar, 0, 0, CRASH, 76 + 34 * e, late=0.002)

    # ------------------------------------------------------------ ballad, brushes
    def ballad_bar(self, bar, piano, bass):
        p = self.p; r = self.r; e = p.energy
        self.drift = 0.6 * self.drift + self.h.gauss(0, 0.002)
        for b in range(4):
            # the left hand sweeps a circle per beat; the right hand taps
            self.hit(bar, b, 0, BR_SWIRL, 40 + 22 * e + (5 if b in (1, 3) else 0), late=0.0, jit=1.2, dur=self.spb * 0.95)
            if b in (1, 3): self.hit(bar, b, 0, BR_TAP, 44 + 24 * e, late=0.006)
            if r.random() < 0.35 + 0.4 * p.comp: self.hit(bar, b, 'a', BR_TAP, 30 + 18 * e, late=0.008)
            if b in (1, 3): self.hit(bar, b, 0, HATF, 40 + 14 * e, late=-0.003)
        if p.feather > 0.05: self.hit(bar, 0, 0, KICK, 26 + 12 * p.feather)
        b0 = bar * 4.0
        for x in piano:
            if b0 <= x['beat'] < b0 + 4 and x['w'] > 0.62 and r.random() < 0.5 * p.listen:
                bb = x['beat'] - b0; b = int(bb); sub = 'a' if bb - b > 0.4 else 0
                self.hit(bar, b, sub, BR_SLAP, 46 + 26 * e, late=0.004); self.stats['caught'] += 1
        if (bar + 1) % 4 == 0 and r.random() < p.fills:
            for i, sub in enumerate((0, 'm', 'a')): self.hit(bar, 3, sub, BR_TAP, 40 + 10 * i + 20 * e, late=0.004)
            self.stats['fills'] += 1

    # ------------------------------------------------------------ bossa
    def bossa_bar(self, bar, piano, bass):
        p = self.p; r = self.r; e = p.energy
        self.sw, self.f = 1.0, 0.5
        self.drift = 0.6 * self.drift + self.h.gauss(0, 0.0012)
        top = RIDE if e > 0.55 else HATC
        for b in range(4):
            self.hit(bar, b, 0, top, 50 + 22 * e, jit=0.7)
            self.hit(bar, b, 'a', top, 40 + 20 * e, jit=0.7)
            if b in (1, 3): self.hit(bar, b, 0, HATF, 54 + 12 * e, late=-0.003)
        for b, sub in ((0, 0), (1, 'a'), (2, 0), (3, 'a')): self.hit(bar, b, sub, KICK, (70 if sub == 0 else 54) + 16 * e, late=-0.001)
        clave = [[(0, 0), (1, 'a'), (3, 0)], [(1, 0), (2, 'a')]][bar % 2]      # cross-stick, two-bar bossa figure
        if r.random() < 0.25 * (1 - p.repeat) + 0.1: clave = clave + [(3, 'a')]
        for b, sub in clave: self.hit(bar, b, sub, STICK, 60 + 18 * e, late=0.004)
        if (bar + 1) % 8 == 0 and r.random() < p.fills:
            for sub in (0, 'a'): self.hit(bar, 3, sub, SNARE, 60 + 24 * e)
            self.stats['fills'] += 1

    def play(self, feel, nbars, piano_notes, bass_notes, arc=None):
        piano = cluster_piano(piano_notes, self.spb)
        for bar in range(nbars):
            if feel == 'swing': self.swing_bar(bar, piano, bass_notes, arc(bar) if arc else None)
            elif feel == 'ballad': self.ballad_bar(bar, piano, bass_notes)
            elif feel == 'bossa': self.bossa_bar(bar, piano, bass_notes)
        self.out.sort()
        return self.out
