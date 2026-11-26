"""Rider: the reference kit, written the way the module runs it (src/dsp/RiderKit.hpp must reproduce it).
Fixed 48 kHz. Every voice is three things, fitted to recordings of a real jazz kit (tools/proto/rider_kit.json):
  partials  decaying sine modes: the pitches that ring (drum heads, the clear tones of a cymbal)
  wash      noise in 26 bands. Each band's level is the square root of a set of ENERGY STORES that only decay.
            A stroke tops the stores up; they are shared by a family of voices, so strokes build on one another
            and closing the hi-hat can choke them.
  click     the stick: the same stores, at two very fast decay rates.
The brush sweep is the wash held open by the hand.
All randomness is comper_ref.Rng, so the C++ output can be compared with this sample for sample.
"""
import os, json, math
import numpy as np
from scipy.signal import butter, sosfilt
from comper_ref import Rng
HERE = os.path.dirname(os.path.abspath(__file__))
SR = 48000
CTRL = 16                                             # the stores and band gains move every 16 samples
D = json.load(open(os.path.join(HERE, 'rider_kit.json')))
EDGES = np.array(D['edges']); NB = len(EDGES) - 1
RATES = D['rates']; CLICK = D['click']; TAUS = D['taus']         # dB per second (power), dB per second, seconds
TAPS = D['taps']                                      # the wash's fixed fine colour: (delay in samples, gain)
VOICES = ['ride', 'bell', 'crash', 'hat-foot', 'hat-closed', 'hat-open', 'snare', 'cross-stick', 'kick', 'tom-high', 'tom-low', 'brush-tap', 'brush-sweep', 'brush-hat-foot', 'snare-edge']
RIDE, BELL, CRASH, HATF, HATC, HATO, SNARE, STICK, KICK, TOM_HI, TOM_LO, BR_TAP, BR_SWEEP, BR_HATF, SNARE_EDGE = range(15)
CYM, HAT, DRUM = 0, 1, 2                              # families: each has its own energy stores
FAMILY = [CYM, CYM, CYM, HAT, HAT, HAT, DRUM, DRUM, DRUM, DRUM, DRUM, DRUM, DRUM, HAT, DRUM]
PAN = [0.35, 0.35, -0.3, -0.45, -0.45, -0.45, -0.1, -0.1, 0.0, 0.1, 0.4, -0.1, -0.1, -0.45, -0.1]
MIX = [0, 0, 0, 1, 1, 1, 2, 2, 3, 4, 4, 2, 2, 1, 2]   # which level knob: 0 RIDE (cymbals) 1 HATS 2 SNARE 3 KICK 4 TOMS
TUNED = [0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1] # TUNE moves these voices' partials
CLOSES = [0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0]  # these strokes choke the hi-hat family
# decay constants of the stores, in dB per second of POWER: the plain ones, then for each (rate, tau) the two extra ones a swell needs
def store_rates():
    out = [float(r) for r in RATES] + [float(r) for r in CLICK]
    for r in RATES:
        for tau in TAUS:
            k = 10.0 / math.log(10.0) / tau           # 1/tau as dB per second of power
            out += [r + k, r + 2 * k]
    return out
SRATE = store_rates(); NS = len(SRATE)                # 8 + 2 + 8*3*2 = 58
def swell_index(ri, ti): return len(RATES) + len(CLICK) + (ri * len(TAUS) + ti) * 2

SOS = np.array([butter(2, [EDGES[i], EDGES[i + 1]], btype='band', fs=SR, output='sos') for i in range(NB)])   # two biquads per band
def band_power():
    x = np.zeros(1 << 16); x[0] = 1.0
    return np.array([np.sum(sosfilt(SOS[i], x) ** 2) for i in range(NB)])
BP = band_power()                                     # power of each band's filtered unit white noise


class KitParams:
    def __init__(self, **kw):
        self.level = 1.0
        self.mix = [1.0, 1.0, 1.0, 1.0, 1.0]          # RIDE HATS SNARE KICK TOMS, linear
        self.tune = 0.5                               # drums: 0 = 5 semitones down, 1 = 5 up
        self.decay = 0.5                              # cymbals and hi-hats: 0 short .. 1 long
        self.bright = 0.5                             # tilt of wash and click around 1 kHz
        self.width = 1.0
        self.age = 0.0
        self.__dict__.update(kw)


def vel_db(v): return 32.0 * math.log10(max(v, 1) / 96.0)

def noise(seed, n):
    """n uniform samples of unit variance from Rng(seed): (u - 0.5) * sqrt(12). Vectorised, identical to calling u() n times."""
    s0 = Rng(seed).s
    a = np.full(n, 1664525, dtype=np.uint64); A = np.cumprod(a)                 # wraps mod 2^64, and 2^32 divides that
    G = np.cumsum(np.concatenate([np.array([1], dtype=np.uint64), A[:-1]]))
    s = (A * np.uint64(s0) + np.uint64(1013904223) * G) & np.uint64(0xFFFFFFFF)
    return ((s >> np.uint64(8)).astype(np.float64) / 16777216.0 - 0.5) * math.sqrt(12.0)


class Kit:
    def __init__(self, kp=None, seed=1):
        self.kp = kp or KitParams(); self.seed = seed
        self.strokes = []          # (tick, voice, velocity 1..127)
        self.sweeps = []           # (start sample, length samples, velocity)

    def stroke(self, t, voice, vel): self.strokes.append((int(math.ceil(t * SR / CTRL)), voice, vel))
    def sweep(self, t, dur, vel): self.sweeps.append((int(round(t * SR)), max(1, int(round(dur * 1.08 * SR))), vel))

    # ---- what the knobs do
    def rate_mult(self, fam):
        kp = self.kp
        if fam == DRUM: return 1.0 + 0.8 * kp.age
        return 2.0 ** ((0.5 - kp.decay) * 2.0) * (1.0 + 0.6 * kp.age)
    def tilt_db(self, i):
        kp = self.kp; fc = math.sqrt(EDGES[i] * EDGES[i + 1])
        return max(-9.0, min(9.0, ((kp.bright - 0.5) * 12.0 - 6.0 * kp.age) * math.log2(fc / 1000.0) / 4.0))
    def curve(self, d, tab, level_db):
        x = d['lx']; T = d[tab]; n = len(x)
        if level_db <= x[0]: return [T[0][i] + (T[1][i] - T[0][i]) / (x[1] - x[0]) * (level_db - x[0]) for i in range(NB)]
        if level_db >= x[n - 1]: return [T[n - 1][i] + (T[n - 1][i] - T[n - 2][i]) / (x[n - 1] - x[n - 2]) * (level_db - x[n - 1]) for i in range(NB)]
        k = 0
        while level_db > x[k + 1]: k += 1
        u = (level_db - x[k]) / (x[k + 1] - x[k])
        return [T[k][i] + (T[k + 1][i] - T[k][i]) * u for i in range(NB)]

    def render(self, dur):
        """stereo, dur seconds"""
        kp = self.kp; n = int(dur * SR) // CTRL * CTRL; nt = n // CTRL; out = np.zeros((n, 2))
        t = np.arange(n) / SR
        store = np.zeros((3, 2, NB, NS))                                    # family, channel, band, store
        dec = np.array([[10 ** (-r * self.rate_mult(f) * (CTRL / SR) / 10.0) if k < len(RATES) or k >= len(RATES) + len(CLICK) else 10 ** (-r * (CTRL / SR) / 10.0)
                         for k, r in enumerate(SRATE)] for f in range(3)])   # the click's two rates do not follow DECAY or AGE
        # swell stores: only the slow part follows DECAY/AGE, 1/tau does not
        for f in range(3):
            m = self.rate_mult(f)
            for ri, r in enumerate(RATES):
                for ti, tau in enumerate(TAUS):
                    k = 10.0 / math.log(10.0) / tau; j = swell_index(ri, ti)
                    dec[f][j] = 10 ** (-(r * m + k) * (CTRL / SR) / 10.0); dec[f][j + 1] = 10 ** (-(r * m + 2 * k) * (CTRL / SR) / 10.0)
        gain2 = np.zeros((nt + 1, 2, NB))                                   # band power at each tick
        by_tick = {}
        for i, s in enumerate(sorted(self.strokes, key=lambda s: s[0])): by_tick.setdefault(s[0], []).append((i, s))
        part = np.zeros((n, 2)); hat_open = []                              # hat-open partial strokes, so a closing stroke can choke them
        # the sweeping hand: level in dB at each tick, -200 where it rests
        sw = np.full(nt + 1, 0.0); swd = D['sweep']; top = swd['lx'][-1]
        for (a, m, v) in self.sweeps:
            for k in range(int(math.ceil(a / CTRL)), min(nt, (a + m) // CTRL) + 1):
                u = (k * CTRL - a) / float(m)
                if u < 0 or u > 1: continue
                g = (0.42 + 0.58 * math.sin(math.pi * u ** 0.7) ** 2) * min(1.0, min(u, 1 - u) / 0.06) * 10 ** ((top - 3 + 0.5 * vel_db(v)) / 20.0)
                sw[k] = max(sw[k], g)
        drift = 0.0; rd = Rng(self.seed * 7 + 3)
        for k in range(nt + 1):
            for (idx, (tick, voice, vel)) in by_tick.get(k, []):
                rs = Rng(self.seed * 7919 + 31 * idx + 5)
                if voice == SNARE and vel < 42: voice = SNARE_EDGE
                d = D['voices'][VOICES[voice]]; fam = FAMILY[voice]
                x = d['lx']; db = max(min(x[0], -12.0) - 6.0, min(max(x[-1], 4.0) + 3.0, vel_db(vel)))
                db += rs.g() * 1.5 * kp.age                                  # an old kit answers less evenly
                tl = self.curve(d, 'ltail', db); cl = self.curve(d, 'lclick', db)
                lin = kp.level * kp.mix[MIX[voice]]; pan = PAN[voice] * kp.width
                ch = (math.sqrt(0.5 * (1 - pan)), math.sqrt(0.5 * (1 + pan)))
                if CLOSES[voice]:
                    store[HAT] *= 0.03
                    for (a0, sig) in hat_open: part[k * CTRL:, :] -= 0.9 * sig[k * CTRL - a0:][:n - k * CTRL]
                    hat_open = [(a0, 0.1 * sig) for (a0, sig) in hat_open]
                # wash and click
                for i in range(NB):
                    w = lin * lin * d['gain'][i] ** 2 * 10 ** ((tl[i] + self.tilt_db(i)) / 10.0) / BP[i]
                    for (ri, ti, c) in d['wash'][i]:
                        e = c * w
                        for c2 in range(2):
                            ec = e * ch[c2] ** 2
                            store[fam, c2, i, ri] += ec if ti < 0 else 0.0
                            if ti >= 0:
                                j = swell_index(ri, ti)
                                store[fam, c2, i, ri] += ec; store[fam, c2, i, j] -= 2 * ec; store[fam, c2, i, j + 1] += ec
                    if EDGES[i + 1] > 180:
                        w = lin * lin * d['cgain'][i] ** 2 * 10 ** ((cl[i] + self.tilt_db(i)) / 10.0) / BP[i]
                        for ci in range(len(CLICK)):
                            for c2 in range(2): store[fam, c2, i, len(RATES) + ci] += d['click'][i][ci] * w * ch[c2] ** 2
                # partials
                a0 = k * CTRL; m = n - a0
                if m > 0 and d['part']:
                    tt = np.arange(m) / SR; sig = np.zeros(m)
                    semis = ((kp.tune - 0.5) * 10.0 - 0.7 * kp.age) if TUNED[voice] else 0.0
                    rm = self.rate_mult(fam)
                    for (f0, amp, rate) in d['part']:
                        ph = rs.u() * 2 * math.pi; vary = rs.g() * 1.5
                        cents = rs.g() * 12.0 * kp.age if TUNED[voice] else 0.0
                        f = f0 * 2.0 ** (semis / 12.0 + cents / 1200.0)
                        bi = max(0, min(NB - 1, int(np.searchsorted(EDGES, f0)) - 1))
                        g = lin * amp * 10 ** ((tl[bi] + vary) / 20.0)
                        lam = rate * rm * math.log(10.0) / 20.0
                        mm = min(m, int((100.0 / (rate * rm) + 0.3) * SR))
                        env = np.exp(-lam * tt[:mm]); kr = max(1, int(min(0.006, 1.0 / f) * SR))
                        kr = min(kr, mm); env[:kr] *= 0.5 - 0.5 * np.cos(np.pi * np.arange(kr) / kr)
                        sig[:mm] += g * env * np.sin(2 * np.pi * f * tt[:mm] + ph)
                    st = np.stack([sig * ch[0], sig * ch[1]], 1); part[a0:] += st
                    if voice == HATO: hat_open.append((a0, st))
            # this tick's band powers, then the stores decay
            tot = store.sum(axis=(0, 3))                                     # channel, band
            if sw[k] > 0:
                drift = 0.995 * drift + rd.g() * 0.12                        # the hand is never quite even: about 1.2 dB
                lv = 20 * math.log10(sw[k]) + drift; inside = max(swd['lx'][0], min(swd['lx'][-1], lv))
                pan = PAN[BR_SWEEP] * kp.width; lin = kp.level * kp.mix[MIX[BR_SWEEP]]
                for i in range(NB):
                    if EDGES[i + 1] <= 130: continue
                    dbb = float(np.interp(inside, swd['lx'], [row[i] for row in swd['bands']])) + (lv - inside) + self.tilt_db(i)
                    pw = lin * lin * 10 ** (dbb / 10.0) / BP[i]
                    tot[0, i] += pw * 0.5 * (1 - pan); tot[1, i] += pw * 0.5 * (1 + pan)
            gain2[k] = np.maximum(tot, 0.0)
            for f in range(3): store[f] *= dec[f]
        # the noise through the bands, with the gains run linearly between ticks
        gain = np.sqrt(gain2); ticks = np.arange(nt + 1) * CTRL; ns = np.arange(n)
        for c2 in range(2):
            nz = noise(self.seed * 2 + 11 + c2, n); col = np.zeros(n)
            for dl, g in TAPS: col[dl:] += g * nz[:n - dl]
            for i in range(NB):
                if not gain[:, c2, i].any(): continue
                out[:, c2] += sosfilt(SOS[i], col) * np.interp(ns, ticks, gain[:, c2, i])
        return out + part


def play_notes(notes, total, kp=None, seed=1):
    """notes from rider_ref.play: (t, voice, vel, dur, beat)"""
    k = Kit(kp, seed)
    for t, voice, vel, dur, beat in notes:
        if t < 0: t = 0.0
        if voice == BR_SWEEP: k.sweep(t, dur, vel)
        else: k.stroke(t, voice, vel)
    return k.render(total)
