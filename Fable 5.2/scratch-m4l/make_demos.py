"""Scratch machine demos. Run: python3 make_demos.py   (writes demos/ and charts/)"""
import numpy as np, os, wave, sys
from scratch_engine import *
from sounds import record, beat
import matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt

BPM = 96.0; SW = 0.58
FRESH = len(sys.argv) > 1 and sys.argv[1] == 'fresh'
if FRESH:
    _w = wave.open('source/ah-this-stuff-is-really-fresh.wav')
    REC = np.frombuffer(_w.readframes(_w.getnframes()), '<i2').reshape(-1, 2).mean(1) / 32768
    REC = np.concatenate([np.zeros(int(.03 * SR)), REC / np.abs(REC).max(), np.zeros(int(.05 * SR))])
    CUE = dict(ah=0.03, this=1.215, stuff=1.595, really=2.575, fresh=3.955, sh=4.30)
    CUE['word'] = CUE['fresh']; D, DS, CH = 'demos-fresh', 'demos-fresh-solo', 'charts-fresh'
else:
    REC, CUE = record(); D, DS, CH = 'demos', 'demos-solo', 'charts'
for _d in (D, DS, CH): os.makedirs(_d, exist_ok=True)
write_wav(D + '/00-the-record.wav', REC)
REPORT = []

def S(bars, hands=0.5, seed=3, cue='ah', rec=None, cues=None, **k):
    r = REC if rec is None else rec; c = (CUE if cues is None else cues)[cue]
    s = Scratch(r, BPM, bars * 4, hands=hands, seed=seed, cue=c, swing=SW, **k); s.cues = CUE if cues is None else cues
    return s

def done(name, s, chart=False, title=None):
    y, p, v, g = s.render(); bt = beat(BPM, (s.n - SR // 2) / SR * BPM / 60, SW)[:len(y)]
    y = y / max(np.abs(y).max(), 1e-9)
    write_wav(DS + '/%s.wav' % name, y); write_wav(D + '/%s.wav' % name, 0.42 * bt + 0.75 * y)
    aud = g > 0.5
    REPORT.append((name, len(y) / SR, np.abs(v[aud]).max() if aud.any() else 0, np.abs(v).max(), len(s.log), aud.mean()))
    if chart: ttm(name, s, p, g, title or name)
    return y

def ttm(name, s, p, g, title, beats=8):
    """Position against time, heavy where the fader is open: the way scratches are written down."""
    n = int(s.sec(beats) * SR); t = np.arange(n) / SR / s.spb; pp = (p[:n] - p[:n].min()) * 1000
    fig, ax = plt.subplots(figsize=(11, 2.6), dpi=110)
    ax.plot(t, pp, color='#bbbbbb', lw=1)
    q = np.where(g[:n] > 0.5, pp, np.nan); ax.plot(t, q, color='#111111', lw=2.6)
    for b in range(beats + 1): ax.axvline(b, color='#d33' if b % 4 == 0 else '#e9a', lw=0.8 if b % 4 else 1.3, zorder=0)
    for b in np.arange(0, beats, 0.25): ax.axvline(s.sec(b) / s.spb, color='#eee', lw=0.5, zorder=0)
    ax.set_xlim(0, beats); ax.set_xlabel('beats'); ax.set_ylabel('record (ms from cue)'); ax.set_title(title, loc='left', fontsize=10)
    fig.tight_layout(); fig.savefig(CH + '/%s.png' % name); plt.close(fig)

# ---------------------------------------------------------------- one demo per word
def words():
    s = S(4)
    for b in (0, 1, 2, 3): s.baby(b, .5)
    for b in (4, 4.5, 5.5, 6, 6.5, 7.5): s.baby(b, .25)
    s.baby(8, .5); s.baby(9, .25); s.baby(9.5, .25); s.baby(10.5, .25); s.baby(11, .25, 2)
    s.baby(12, .25, 2); s.baby(13, .25, 2); s.baby(14, .5)
    done('01-baby', s, True, 'BABY: push and pull, fader open. 8th strokes, then 16ths')

    s = S(4)
    for b in np.arange(0, 4, .5): s.forward(b, .25, .25)
    for b in (4, 4.75, 5.5, 6, 6.75, 7.5): s.forward(b, .25, .25)
    for b in (8, 9, 10, 11): s.forward(b, .5, .5)
    for b in (12, 12.25, 12.5, 12.75, 13, 14, 14.5, 15): s.forward(b, .125, .125)
    done('02-forward', s, True, 'FORWARD: let go, cut, pull back silently')

    s = S(4)
    for bar in (0, 4, 8, 12):
        for b in ((0, .25, .5, 1, 1.5, 1.75, 2, 2.5, 3, 3.25, 3.5) if bar % 8 == 0 else (0, .5, .75, 1.5, 2, 2.25, 2.5, 3.5)): s.stab(bar + b)
    done('03-stab', s, True, 'STAB: a fast push with a short fader opening')

    s = S(4)
    s.chirp(0, .25, 2); s.chirp(1, .25, 2); s.chirp(2, .25, 2); s.chirp(3, .25, 1)
    s.chirp(4, .25, 1); s.chirp(4.5, .25, 1); s.chirp(5.5, .25, 1); s.chirp(6, .25, 2); s.chirp(7.5, .25, 1)
    s.chirp(8, .125, 4); s.chirp(9, .25, 2); s.chirp(10, .125, 4); s.chirp(11, .25, 1)
    s.chirp(12, .25, 4); s.chirp(14, .125, 4)
    done('04-chirp', s, True, 'CHIRP: fader closes on the push and opens on the pull')

    s = S(4)
    s.flare(0, .5, 4, 1); s.flare(4, .5, 2, 1); s.flare(6.5, .5, 1, 1); s.flare(8, .5, 4, 1); s.flare(12, .25, 4, 1); s.flare(14, .5, 1, 1)
    done('05-flare-1-click', s, True, '1-CLICK FLARE (orbit): one click in the middle of each stroke')

    s = S(4)
    s.flare(0, .5, 4, 2); s.flare(4, .5, 3, 2); s.flare(8, 1.0, 2, 2); s.flare(12, .5, 3, 2)
    done('06-flare-2-click', s, True, '2-CLICK FLARE: two clicks in each stroke, three sounds')

    s = S(4)
    s.crab(0, .5); s.crab(2, .5); s.crab(4, .5); s.crab(5, .5); s.crab(6.5, .5)
    s.crab(8, .5, both=False); s.crab(9, .5, both=False); s.crab(10, .5); s.crab(12, .5, taps=3); s.crab(13, .5, taps=3); s.crab(14, .5)
    done('07-crab', s, True, 'CRAB: four fingers, four quick openings on one stroke')

    s = S(4)
    s.tear(0, .25); s.tear(1, .25); s.tear(2, .25); s.baby(3, .25)
    s.tear(4, .25); s.tear(4.75, .25); s.tear(5.5, .25); s.tear(6.5, .25); s.baby(7.5, .25)
    s.tear(8, 1 / 3); s.tear(9, 1 / 3); s.tear(10, 1 / 3); s.tear(11, 1 / 3)
    s.tear(12, .25, 'ff-b'); s.tear(13, .25, 'ff-b'); s.tear(14, .25); s.baby(15, .25)
    done('08-tear', s, True, 'TEAR: one push, the pull broken in two. No fader')

    s = S(4)
    s.scribble(0, .5); s.baby(1, .5); s.scribble(2, .5); s.baby(3, .25); s.scribble(4, 1.0); s.baby(5.5, .25); s.scribble(6, .5); s.scribble(7, .5)
    s.scribble(8, 2.0); s.baby(10.5, .25); s.baby(11, .5); s.scribble(12, .75); s.baby(13, .25); s.scribble(14, 1.0)
    done('09-scribble', s, True, 'SCRIBBLE: a tight shake on the front of the sound')

    s = S(4)
    s.transform(0, 2, .25); s.transform(2, 2, .25, pat=(1, 1, 0, 1)); s.transform(4, 2, 1 / 6); s.transform(6, 2, .125, duty=.55)
    s.transform(8, 2.5, .25, mode='play', pat=(1, 0, 1, 1, 0, 1, 1, 0)); s.transform(12, 2, .25, pat=(1, 1, 1, 0)); s.transform(14, 1.5, .125)
    done('10-transform', s, True, 'TRANSFORM: a slow drag, the fader tapped open in rhythm')

    s = S(4)
    s.drag(0, 1.0); s.drag(2, .5); s.baby(3, .25); s.drag(4, 1.0); s.baby(6, .25); s.drag(6.5, .5); s.drag(8, 2.0); s.drag(12, .5); s.drag(13, .5); s.drag(14, 1.0)
    done('11-drag', s, True, 'DRAG: a slow push and pull, low and growling')

    s = S(4)
    s.tip(0, .125, 2); s.baby(.5, .25); s.tip(1, .125, 4); s.tip(2.5, .125, 2); s.baby(3, .5)
    s.tip(4, .125, 4); s.tip(5.5, .125, 2); s.tip(6, .125, 4); s.baby(7.5, .25)
    s.tip(8, .125, 2); s.baby(8.5, .25); s.tip(9, .125, 2); s.baby(9.5, .25); s.tip(10, .125, 8)
    s.tip(12, .125, 4); s.tip(13, .125, 4); s.baby(14, .5)
    done('12-tip', s, True, 'TIP: tiny strokes on the very front of the sound')

    s = S(4)
    s.lazer(0); s.lazer(1); s.baby(2, .25); s.lazer(2.5); s.lazer(4, tail=False); s.lazer(4.5, tail=False); s.lazer(5); s.baby(6, .5); s.lazer(7)
    s.lazer(8, .125, .875); s.lazer(10); s.baby(11, .25, 2); s.lazer(12); s.lazer(12.5); s.lazer(13); s.lazer(14, .125, 1.375)
    done('13-lazer', s, True, 'LAZER: a hard flick, the record skids on, then a slow pull')

    s = S(4)
    s.release(0, 1.5, .5); s.echo(2, .5, 4); s.baby(4, .25, 2); s.release(5, 2.5, .5); s.echo(8, .5, 6, .7); s.baby(11.5, .25); s.release(12, 3.5, .5)
    done('14-release-and-echo', s, True, 'RELEASE: let the phrase play.  ECHO: forwards that fade')

    s = S(4)
    for b in (0.5, 1.5, 2.5, 3.5): s.reverse(b, .25, .25)
    for b in (4.5, 5.25, 6, 6.5, 7.5): s.reverse(b, .25, .25)
    s.reverse(8.5, .5, .5); s.baby(9.5, .25); s.reverse(10.5, .5, .5); s.baby(11.5, .25); s.reverse(12.5, .25, .25); s.forward(13, .25, .25); s.reverse(14, .25, .25); s.forward(14.5, .25, .25); s.baby(15, .5)
    done('15-reverse', s, True, 'REVERSE: push silently, open the fader for the pull')

# ---------------------------------------------------------------- cue points and sentences
def sentence_a(s, b, short=False):
    s.baby(b, .25); s.baby(b + .5, .25); s.forward(b + 1, .25, .25); s.stab(b + 1.5); s.stab(b + 1.75)
    s.chirp(b + 2, .25, 1); s.baby(b + 2.5, .25)
    if short: s.baby(b + 3, .25)
    else: s.tear(b + 3, .25); s.stab(b + 3.75)

def combos():
    s = S(4)
    for i, c in enumerate(('ah', 'stuff', 'really', 'fresh') if FRESH else ('ah', 'word', 'vowel', 'hiss')):
        s.cue(s.cues[c]); sentence_a(s, i * 4, True)
    done('20-same-bar-on-four-cue-points', s)

    s = S(4)
    for bar in (0, 4, 8, 12):
        s.flare(bar, .5, 1, 1); s.chirp(bar + 1, .25, 2); s.flare(bar + 2, .5, 1, 1); s.chirp(bar + 3, .25, 1); s.stab(bar + 3.5); s.stab(bar + 3.75)
    done('21-flare-and-chirp', s, True, 'Sentence: 1-click flare answered by chirps')

    s = S(4)
    for bar in (0, 8):
        s.forward(bar, .25, .25); s.forward(bar + .5, .25, .25); s.baby(bar + 1, .25, 2); s.forward(bar + 2, .25, .25); s.tear(bar + 2.5, .25); s.baby(bar + 3.5, .25)
        s.forward(bar + 4, .25, .25); s.baby(bar + 4.5, .25); s.tear(bar + 5, .25); s.stab(bar + 5.75); s.baby(bar + 6, .25, 2); s.forward(bar + 7, .25, .25); s.stab(bar + 7.5); s.stab(bar + 7.75)
    done('22-march-forwards-babies-tears', s, True, 'Sentence: forwards, babies and tears in a marching figure')

    s = S(4)
    s.transform(0, 2, .25, pat=(1, 1, 0, 1)); s.release(2, 1.5, .5); s.transform(4, 2, 1 / 6); s.baby(6, .25, 2); s.stab(7); s.stab(7.5)
    s.cue(s.cues['word']); s.transform(8, 1.5, .125, sp=.3); s.release(10, 1.75, .25); s.cue(s.cues['ah']); s.crab(12, .5); s.flare(13, .5, 1, 2); s.release(14, 2, .5)
    done('23-transform-into-release', s, True, 'Sentence: gated drags that open into the phrase')

    s = S(4)
    s.crab(0, .5); s.flare(1, .5, 1, 1); s.scribble(2, .5); s.lazer(2.5, .125, .375); s.chirp(3, .25, 2)
    s.tear(4, 1 / 3); s.tear(5, 1 / 3); s.flare(6, .5, 1, 2); s.drag(7, .5)
    s.tip(8, .125, 4); s.stab(9); s.stab(9.25); s.stab(9.5); s.crab(10, .5); s.reverse(11.5, .25, .25)
    s.flare(12, .5, 1, 1); s.chirp(13, .125, 4); s.release(14, 1.75, .25)
    done('24-everything', s, True, 'Sentence: a busy four bars using most of the words')
    if FRESH:
        s = S(8)
        s.baby(0, .25, 2); s.forward(1, .25, .25); s.stab(1.5); s.stab(1.75); s.chirp(2, .25, 2); s.baby(3, .5)
        s.flare(4, .5, 1, 1); s.tear(5, .25); s.stab(5.75); s.transform(6, 2, .25, pat=(1, 1, 0, 1))
        s.release(8, 7.6, .4)
        s.cue(s.cues['fresh']); s.baby(17, .25, 2); s.forward(18, .25, .25); s.stab(18.5); s.stab(18.75); s.chirp(19, .25, 2)
        s.tear(20, .25); s.baby(21, .25, 2); s.flare(22, .5, 1, 1); s.release(23, .9, .5)
        s.cue(s.cues['ah']); s.baby(26, .25, 2); s.crab(27, .5); s.transform(28, 2, .25); s.release(30, 1.5, .5)
        done('25-scratch-then-the-whole-phrase', s)

# ---------------------------------------------------------------- instinct
def cells():
    C = {}
    def cell(name, beats, w, dens):
        def deco(f): C[name] = (f, beats, w, dens); return f
        return deco
    @cell('rest', 1, 3, 0)
    def _(s, b): pass
    @cell('baby8', 1, 4, 1)
    def _(s, b): s.baby(b, .5)
    @cell('baby16', 1, 5, 2)
    def _(s, b): s.baby(b, .25); s.baby(b + .5, .25)
    @cell('pickup', 1, 4, 1)
    def _(s, b): s.baby(b + .5, .25)
    @cell('baby-rest', 1, 3, 1)
    def _(s, b): s.baby(b, .25)
    @cell('fwd-fwd', 1, 4, 2)
    def _(s, b): s.forward(b, .25, .25); s.forward(b + .5, .25, .25)
    @cell('fwd-baby', 1, 5, 2)
    def _(s, b): s.forward(b, .25, .25); s.baby(b + .5, .25)
    @cell('stabs', 1, 3, 3)
    def _(s, b): s.stab(b); s.stab(b + .25); s.stab(b + .5)
    @cell('stab-off', 1, 3, 2)
    def _(s, b): s.stab(b + .5); s.stab(b + .75)
    @cell('chirp', 1, 4, 3)
    def _(s, b): s.chirp(b, .25, 2)
    @cell('chirp-baby', 1, 3, 2)
    def _(s, b): s.chirp(b, .25, 1); s.baby(b + .5, .25)
    @cell('flare1', 1, 3, 3)
    def _(s, b): s.flare(b, .5, 1, 1)
    @cell('flare2', 1, 2, 4)
    def _(s, b): s.flare(b, .5, 1, 2)
    @cell('crab', 1, 2, 4)
    def _(s, b): s.crab(b, .5)
    @cell('tear', 1, 3, 3)
    def _(s, b): s.tear(b, .25); s.stab(b + .75)
    @cell('tear-trip', 1, 2, 3)
    def _(s, b): s.tear(b, 1 / 3)
    @cell('scribble', 1, 2, 3)
    def _(s, b): s.scribble(b, .5); s.baby(b + .5, .25)
    @cell('tips', 1, 2, 4)
    def _(s, b): s.tip(b, .125, 2); s.baby(b + .5, .25)
    @cell('lazer', 1, 1.5, 2)
    def _(s, b): s.lazer(b, .125, .375); s.baby(b + .5, .25)
    @cell('transform', 2, 2, 3)
    def _(s, b): s.transform(b, 2, .25, pat=(1, 1, 0, 1))
    @cell('transform-trip', 2, 1.2, 4)
    def _(s, b): s.transform(b, 2, 1 / 6)
    @cell('drag', 2, 1.5, 1)
    def _(s, b): s.drag(b, 1.0)
    @cell('echo', 2, 1.2, 2)
    def _(s, b): s.echo(b, .5, 4)
    @cell('release', 2, 0, 1)
    def _(s, b): s.release(b, 1.5, .5)
    return C

CELLS = cells()

def instinct(s, bars, density=0.5, skill=0.75, motif=0.6, release=0.6, seed=5, b0=0, cuewalk=0.0):
    """4-bar phrases: A, A again (sometimes changed), B, and an ending that often lets the phrase play."""
    r = np.random.default_rng(seed); names = list(CELLS)
    def weight(nm):
        f, beats, w, d = CELLS[nm]
        if nm == 'rest': return w * (1.3 - 1.25 * density) * 1.5
        if d > 1 + 3.2 * skill: return 0.0
        return w * np.exp(-abs(d - (1.6 + 2.6 * density)) * 0.6)
    def bar(maxd=9):
        out = []; left = 4
        while left > 0:
            ok = [n for n in names if CELLS[n][1] <= left and not (out and out[-1] == 'rest' and n == 'rest')]
            w = np.array([weight(n) for n in ok]); nm = ok[r.choice(len(ok), p=w / w.sum())]
            out.append(nm); left -= CELLS[nm][1]
        return out
    def vary(a):
        a = list(a); i = r.integers(len(a)); n = CELLS[a[i]][1]
        ok = [x for x in names if CELLS[x][1] == n and x != a[i] and weight(x) > 0]
        if not ok: return a
        w = np.array([weight(x) for x in ok]); a[i] = ok[r.choice(len(ok), p=w / w.sum())]; return a
    def play(a, b):
        for nm in a: CELLS[nm][0](s, b); b += CELLS[nm][1]
    plan = []
    for ph in range(bars // 4):
        b = b0 + ph * 16
        if cuewalk and r.random() < cuewalk:
            ks = [k for k in s.cues if k != 'word']; s.cue(s.cues[ks[r.integers(len(ks))]])
        A = bar(); A2 = A if r.random() < motif else vary(A); B = bar()
        if r.random() < release:
            C = vary(A)[:]; n = 0; keep = []
            for nm in C:
                if n + CELLS[nm][1] <= 2: keep.append(nm); n += CELLS[nm][1]
            while n < 2: keep.append('baby16'); n += 1
            C = keep + ['release']
        else: C = vary(B)
        for i, a in enumerate((A, A2, B, C)): play(a, b + 4 * i); plan.append(a)
    return plan

def instincts():
    for name, kw, hk in (('30-instinct-tight-dj', dict(), dict(hands=.5)),
                         ('31-instinct-machine', dict(), dict(hands=0.0)),
                         ('32-instinct-loose', dict(), dict(hands=.8)),
                         ('33-instinct-sparse-simple', dict(density=.2, skill=.3, seed=8), dict(hands=.5)),
                         ('34-instinct-dense-flashy', dict(density=.85, skill=1.0, seed=9), dict(hands=.5)),
                         ('35-instinct-walking-the-cue-points', dict(cuewalk=.9, seed=12), dict(hands=.5))):
        s = S(8, **hk); plan = instinct(s, 8, **kw); done(name, s, name.endswith('tight-dj'), 'INSTINCT, 8 bars, tight DJ hands')
        print(name); [print('   ', ' | '.join(a)) for a in plan]

def hands():
    for nm, h in (('1-machine', 0.0), ('2-tight-dj', .5), ('3-loose', .8), ('4-bedroom', 1.0)):
        s = S(4, hands=h, seed=4)
        for bar in (0, 4, 8, 12): sentence_a(s, bar)
        done('40-hands-' + nm, s, True, 'The same bar four times, hands = ' + nm[2:])
    for nm, k in (('timing', dict(timing=1.0)), ('weight', dict(weight=1.0)), ('slop', dict(slop=1.0)), ('risk', dict(risk=1.0))):
        s = S(4, hands=0.15, seed=4, **k)
        for bar in (0, 4, 8, 12): sentence_a(s, bar)
        done('41-only-' + nm, s)

def loop_phrase():
    w = wave.open('/mnt/project-files/beat-juggling/source/baby-cry-8bar.wav'); x = np.frombuffer(w.readframes(w.getnframes()), '<i2').reshape(-1, 2).mean(1) / 32768
    spb = 60 / 107.0 * SR; e = np.abs(x); hop = 256
    env = np.array([e[i:i + hop * 2].mean() for i in range(0, len(x) - hop * 2, hop)]); d = np.maximum(0, np.diff(np.log(env + 1e-4)))
    for nm, b0, nb in (('bar-1', 0, 4), ('bar-5', 16, 4)):
        seg = x[int(b0 * spb):int((b0 + nb) * spb)].copy(); seg /= np.abs(seg).max()
        i0 = int(b0 * spb / hop); dd = d[i0:i0 + int(nb * spb / hop)].copy(); cues = {}
        for k in range(4):
            j = int(dd.argmax()); cues['c%d' % k] = max(j * hop / SR - 0.004, 0); dd[max(0, j - 40):j + 40] = 0
        cues = dict(sorted(cues.items(), key=lambda kv: kv[1])); print('loop', nm, cues)
        first = list(cues)[0]
        s = S(8, rec=seg, cues=cues, cue=first, seed=6)
        s.release(0, 3.5, .5)
        for i, c in enumerate(cues): s.cue(cues[c]); sentence_a(s, 4 + i * 4, True)
        s.cue(cues[first]); instinct(s, 4, b0=20, seed=21)
        done('50-your-loop-%s-phrase-then-cues-then-instinct' % nm, s)
        s = S(8, rec=seg, cues=cues, cue=first, seed=7); instinct(s, 8, cuewalk=.8, seed=22); done('51-your-loop-%s-instinct' % nm, s)

if __name__ == '__main__':
    words(); combos(); instincts(); hands()
    if not FRESH: loop_phrase()
    print('%-52s %6s %8s %8s %5s %5s' % ('demo', 'sec', 'vmax-on', 'vmax', 'words', 'open'))
    for r in REPORT: print('%-52s %6.1f %8.2f %8.2f %5d %5.2f' % r)
