"""SPRECHER reference engine: rule-based speech from typed words. Two engines share one front end.
  FORMANT: a phoneme synthesiser in the manner of the 1970s phoneme boxes: sawtooth-like voice source and noise
           through a cascade of formant resonators, with RC-smoothed moves between phoneme targets.
  CHIP:    the same parameter tracks re-coded the way the late-70s speech chips did it: 8 kHz, 25 ms frames,
           coarse parameter tables, strictly voiced OR unvoiced excitation, integer pitch period, stepped output.
Nothing here is recorded or copied from any chip ROM. All tables below were written for this module."""
import numpy as np, os, re
from numba import njit
from wb_common import *

CTRL = 0.005          # control period, s: targets are read every 5 ms
# ---------------------------------------------------------------- phoneme table
# Each phoneme is a list of segments. A segment is:
#  (ms, F1, F2, F3, AV, AH, AF, FC, FB, NZ, flags)
#   ms = duration at RATE 1;  F1..F3 formant targets Hz (0 = keep the neighbouring vowel's value: see "borrow");
#   AV voicing amplitude, AH aspiration (noise through the formants), AF frication (noise through its own filter);
#   FC, FB frication filter centre and bandwidth Hz;  NZ nasal amount;
#   flags: 'v' vowel (may carry stress and be held when singing), 'q' quick (amplitudes jump, no smoothing: stops),
#          'b' borrow formants from the next vowel (used by HH and by stop aspiration)
def V(ms, f1, f2, f3, av=1.0): return [(ms, f1, f2, f3, av, 0, 0, 0, 0, 0, 'v')]
def D2(ms, a, b):               # diphthong: 45 % on the first target, 55 % gliding to the second
    return [(ms * 0.45, a[0], a[1], a[2], 1.0, 0, 0, 0, 0, 0, 'v'), (ms * 0.55, b[0], b[1], b[2], 1.0, 0, 0, 0, 0, 0, 'v')]
def FR(ms, af, fc, fb, av=0.0, f=(300, 1600, 2600), ah=0.0): return [(ms, f[0], f[1], f[2], av, ah, af, fc, fb, 0, '')]
def STOP(close, locus, burst_ms, baf, bfc, bfb, asp_ms, voiced):
    segs = [(close, 200, locus, 2600, 0.22 if voiced else 0.0, 0, 0, 0, 0, 0, 'q'),
            (burst_ms, 200, locus, 2600, 0.22 if voiced else 0.0, 0, baf, bfc, bfb, 0, 'q')]
    if asp_ms > 0: segs.append((asp_ms, 0, 0, 0, 0.0, 0.45, 0.0, 0, 0, 0, 'b'))
    return segs
_IY=(270,2290,3010); _IH=(390,1990,2550); _UH=(440,1020,2240); _A=(750,1300,2500); _AO=(570,840,2410); _EH=(530,1840,2480); _OO=(400,800,2400)
PH = {
 # vowels
 'IY': V(140,*_IY), 'IH': V(90,*_IH), 'EH': V(110,*_EH), 'AE': V(150,660,1720,2410), 'AA': V(150,730,1090,2440),
 'A': V(130,*_A), 'AH': V(90,640,1190,2390), 'AX': V(60,500,1500,2500,0.8), 'AO': V(140,*_AO), 'UH': V(90,*_UH),
 'UW': V(140,300,870,2240), 'ER': V(130,490,1350,1690), 'EE': V(150,390,2100,2700), 'OO': V(150,*_OO),
 'UE': V(140,270,1750,2150), 'UY': V(80,350,1500,2100), 'OE': V(140,390,1500,2200), 'OX': V(90,500,1450,2300),
 # diphthongs
 'AY': D2(190,_A,_IH), 'AW': D2(190,_A,_UH), 'OY': D2(190,_AO,_IH), 'EY': D2(170,_EH,_IY), 'OW': D2(170,_OO,_UH),
 # glides and liquids
 'W': [(60,290,610,2150,0.8,0,0,0,0,0,'')], 'Y': [(60,260,2070,3020,0.8,0,0,0,0,0,'')],
 'R': [(70,310,1060,1380,0.8,0,0,0,0,0,'')], 'L': [(60,310,1050,2880,0.8,0,0,0,0,0,'')],
 'RR': [(25,450,1400,2500,0.25,0,0,0,0,0,'q'), (15,450,1400,2500,0.8,0,0,0,0,0,'q')],       # tap
 'RX': [(55,450,1200,2400,0.6,0,0.12,1500,800,0,'')],                                        # German uvular r
 # nasals
 'M': [(70,250,900,2200,0.7,0,0,0,0,1,'')], 'N': [(65,250,1700,2600,0.7,0,0,0,0,1,'')], 'NG': [(70,250,2300,2750,0.7,0,0,0,0,1,'')],
 # fricatives
 'F': FR(100,0.25,5500,5000), 'TH': FR(90,0.20,6000,5000), 'S': FR(110,0.80,6500,1800), 'SH': FR(110,0.70,2900,1200),
 'CX': FR(100,0.50,3800,1200), 'KX': FR(100,0.50,1500,900,ah=0.2),
 'HH': [(60,0,0,0,0.0,0.5,0,0,0,0,'b')],
 'V': FR(60,0.12,5500,5000,av=0.6), 'DH': FR(50,0.08,6000,5000,av=0.6), 'Z': FR(80,0.45,6500,1800,av=0.5), 'ZH': FR(80,0.40,2900,1200,av=0.5),
 # stops
 'P': STOP(60,800,8,0.40,1200,2500,35,False), 'T': STOP(55,1800,10,0.70,4800,2500,40,False), 'K': STOP(60,2000,14,0.70,2200,900,45,False),
 'B': STOP(50,800,6,0.20,1000,2500,0,True), 'D': STOP(45,1800,8,0.30,4500,2500,0,True), 'G': STOP(50,2000,10,0.35,2200,900,0,True),
 # pause
 '_': [(80,0,0,0,0,0,0,0,0,0,'b')],
}
# affricates are built from a stop closure and a fricative
PH['CH'] = PH['T'][:1] + FR(70,0.70,2900,1200)
PH['JH'] = PH['D'][:1] + FR(50,0.40,2900,1200,av=0.5)
PH['TS'] = PH['T'][:1] + FR(80,0.80,6500,1800)
PH['PF'] = PH['P'][:1] + FR(70,0.25,5500,5000)
VOWELS = {k for k, v in PH.items() if v[0][10] == 'v'}

# ---------------------------------------------------------------- dictionary and letter-to-sound
LANGS = ['en', 'de', 'fr', 'es', 'it', 'ja', 'ru']
def load_words(path=None):
    path = path or os.path.join(os.path.dirname(os.path.abspath(__file__)), 'words.txt')
    d = {l: {} for l in LANGS}
    for line in open(path, encoding='utf-8'):
        line = line.split('#')[0].strip()
        if not line: continue
        p = line.split()
        for ph in p[2:]:
            assert ph.lstrip("'") in PH, (line, ph)
        d[p[0]][p[1]] = p[2:]
    return d

# Letter-to-sound fallback. Ordered rules: (pattern, phonemes). The longest match at each position wins;
# '^' = word start, '$' = word end. These are deliberately small. Unknown English words will sound approximate.
LTS = {
 'de': [('sch','SH'),('tsch','CH'),('chs','K S'),('ch','CX'),('ck','K'),('ph','F'),('qu','K V'),('ng','NG'),('pf','PF'),('tz','TS'),
        ('ei','AY'),('ai','AY'),('eu','OY'),('äu','OY'),('au','AW'),('ie','IY'),('ee','EE'),('aa','A'),('oo','OO'),('ah','A'),('eh','EE'),
        ('ih','IY'),('oh','OO'),('uh','UW'),('äh','EH'),('öh','OE'),('üh','UE'),('er$','AX'),('en$','AX N'),('e$','AX'),('^st','SH T'),('^sp','SH P'),
        ('ss','S'),('ß','S'),('a','A'),('ä','EH'),('b','B'),('c','K'),('d','D'),('e','EH'),('f','F'),('g','G'),('h','HH'),('i','IH'),('j','Y'),
        ('k','K'),('l','L'),('m','M'),('n','N'),('o','AO'),('ö','OX'),('p','P'),('r','RX'),('s','Z'),('t','T'),('u','UH'),('ü','UY'),
        ('v','F'),('w','V'),('x','K S'),('y','UY'),('z','TS')],
 'en': [('tion','SH AX N'),('sion','ZH AX N'),('ough','AO'),('igh','AY'),('tch','CH'),('ch','CH'),('sh','SH'),('th','TH'),('ph','F'),('wh','W'),
        ('ck','K'),('ng','NG'),('qu','K W'),('ee','IY'),('ea','IY'),('oo','UW'),('ou','AW'),('ow','OW'),('oi','OY'),('oy','OY'),('ai','EY'),
        ('ay','EY'),('au','AO'),('aw','AO'),('er','ER'),('ir','ER'),('ur','ER'),('ar','AA R'),('or','AO R'),('y$','IY'),('e$',''),('ss','S'),('ll','L'),
        ('a','AE'),('b','B'),('c','K'),('d','D'),('e','EH'),('f','F'),('g','G'),('h','HH'),('i','IH'),('j','JH'),('k','K'),('l','L'),('m','M'),
        ('n','N'),('o','AA'),('p','P'),('r','R'),('s','S'),('t','T'),('u','AH'),('v','V'),('w','W'),('x','K S'),('y','Y'),('z','Z')],
 'es': [('ch','CH'),('ll','Y'),('rr','RR RR'),('qu','K'),('gu','G'),('ñ','N Y'),('j','KX'),('h',''),('z','S'),('c','K'),('v','B'),
        ('a','A'),('b','B'),('d','D'),('e','EH'),('f','F'),('g','G'),('i','IY'),('k','K'),('l','L'),('m','M'),('n','N'),('o','AO'),('p','P'),
        ('r','RR'),('s','S'),('t','T'),('u','UW'),('w','W'),('x','K S'),('y','Y')],
 'it': [('gli','L Y'),('gn','N Y'),('sch','S K'),('sc','SH'),('ch','K'),('gh','G'),('ci','CH'),('ce','CH EH'),('gi','JH'),('ge','JH EH'),('qu','K W'),
        ('zz','TS'),('z','TS'),('h',''),('a','A'),('b','B'),('c','K'),('d','D'),('e','EH'),('f','F'),('g','G'),('i','IY'),('l','L'),('m','M'),
        ('n','N'),('o','AO'),('p','P'),('r','RR'),('s','S'),('t','T'),('u','UW'),('v','V')],
 'ja': [('sh','SH'),('ch','CH'),('ts','TS'),('j','JH'),('f','F'),('a','A'),('i','IY'),('u','UW'),('e','EH'),('o','AO'),('k','K'),('g','G'),
        ('s','S'),('z','Z'),('t','T'),('d','D'),('n','N'),('h','HH'),('b','B'),('p','P'),('m','M'),('y','Y'),('r','RR'),('w','W')],
}
LTS['fr'] = LTS['es']; LTS['ru'] = LTS['es']     # no real rules for these two: numbers come from the dictionary

def lts(word, lang):
    rules = LTS[lang]; w = word; i = 0; out = []
    while i < len(w):
        best = None
        for pat, ph in rules:
            p = pat; start = p.startswith('^'); end = p.endswith('$'); core = p.strip('^$')
            if start and i != 0: continue
            if not w.startswith(core, i): continue
            if end and i + len(core) != len(w): continue
            if best is None or len(core) > len(best[0]): best = (core, ph)
        if best is None: i += 1; continue
        out += best[1].split(); i += len(best[0])
    # stress: the first vowel
    for j, p in enumerate(out):
        if p in VOWELS: out[j] = "'" + p; break
    return out

def parse(text, lang, words):
    """Text field to a list of words; each word is (label, phoneme list).
    Tokens: [PH PH ...] = phonemes typed directly; digits = number words 0..20 in the chosen language
    (longer numbers are spoken digit by digit); other tokens are looked up in the chosen language, then English,
    then German, and finally sent through letter-to-sound."""
    out = []
    for tok in re.findall(r'\[[^\]]*\]|[^\s,.;:!?]+', text):
        if tok.startswith('['):
            out.append((tok, [p for p in tok[1:-1].upper().split() if p.lstrip("'") in PH])); continue
        t = tok.lower()
        if t.isdigit():
            if t in words[lang]: out.append((t, words[lang][t]))
            else:
                for ch in t: out.append((ch, words[lang][ch]))
            continue
        for l in (lang, 'en', 'de'):
            if t in words[l]: out.append((t, words[l][t])); break
        else:
            out.append((t, lts(t, lang if lang in LTS else 'en')))
    return out

# ---------------------------------------------------------------- targets
def word_targets(phs, rate=1.0):
    """Phoneme list to control frames. Returns an array [frames, 12]:
    F1 F2 F3 AV AH AF FC FB NZ quick hold stress   (hold = 1 on the frames of the vowel that is held when singing)"""
    segs = []
    for p in phs:
        st = p.startswith("'"); name = p.lstrip("'")
        for (ms, f1, f2, f3, av, ah, af, fc, fb, nz, fl) in PH[name]:
            if st and 'v' in fl: ms *= 1.2
            segs.append([ms / rate, f1, f2, f3, av, ah, af, fc, fb, nz, fl, st and 'v' in fl, name])
    # borrow: a segment flagged 'b' takes its formants from the next segment that has them, else the previous one
    for i, s in enumerate(segs):
        if 'b' in s[10] or s[1] == 0:
            src = None
            for j in range(i + 1, len(segs)):
                if 'b' not in segs[j][10] and 'q' not in segs[j][10]: src = segs[j]; break
            if src is None:
                for j in range(i - 1, -1, -1):
                    if 'b' not in segs[j][10]: src = segs[j]; break
            if src is None: src = [0, 500, 1500, 2500]
            s[1], s[2], s[3] = src[1], src[2], src[3]
    # the held vowel: first stressed vowel, else first vowel. For a diphthong, hold its first segment.
    hold = None
    for i, s in enumerate(segs):
        if s[11]: hold = i; break
    if hold is None:
        for i, s in enumerate(segs):
            if 'v' in s[10]: hold = i; break
    rows = []
    for i, s in enumerate(segs):
        n = max(1, int(round(s[0] / 1000.0 / CTRL)))
        for k in range(n):
            rows.append([s[1], s[2], s[3], s[4], s[5], s[6], s[7] or 3000, s[8] or 2000, s[9],
                         1.0 if 'q' in s[10] else 0.0, 1.0 if i == hold else 0.0, 1.0 if s[11] else 0.0])
    rows.append([rows[-1][0], rows[-1][1], rows[-1][2], 0, 0, 0, 3000, 2000, 0, 0, 0, 0])   # closing frame
    return np.array(rows)

def schedule(words, gates, fs, rate=1.0, hold=True):
    """words: list of phoneme lists. gates: list of (start s, length s), one per word.
    Returns the frame array for the whole performance at the control rate, with silence between words.
    If hold is on and the gate is still high when the held vowel's frames end, its last frame repeats until the gate falls."""
    total = max(g[0] + g[1] for g in gates) + 2.0
    nfr = int(total / CTRL); T = np.zeros((nfr, 12)); T[:, 0] = 500; T[:, 1] = 1500; T[:, 2] = 2500; T[:, 6] = 3000; T[:, 7] = 2000
    for w, (t0, ln) in zip(words, gates):
        fr = word_targets(w, rate); i = int(t0 / CTRL); gate_end = int((t0 + ln) / CTRL); k = 0
        while k < len(fr) and i < nfr:
            T[i] = fr[k]
            last_hold = fr[k, 10] > 0 and (k + 1 >= len(fr) or fr[k + 1, 10] == 0)
            if hold and last_hold and i < gate_end: i += 1; continue
            i += 1; k += 1
        if i < nfr: T[i:, :3] = fr[-1, :3]
    return T

# ---------------------------------------------------------------- synthesis
@njit(cache=True)
def reson(f, bw, fs):
    """Klatt resonator coefficients A, B, C: y = A x + B y1 + C y2. Unity gain at 0 Hz."""
    f = min(f, 0.45 * fs)
    C = -np.exp(-2.0 * np.pi * bw / fs); B = 2.0 * np.exp(-np.pi * bw / fs) * np.cos(2.0 * np.pi * f / fs)
    return 1.0 - B - C, B, C

@njit(cache=True)
def formant_engine(T, f0, size, whisper, age, U, seed32, fs):
    """T = frames at 5 ms. f0[n] Hz at the engine rate. size = vocal tract scale (1 = adult male; formants are divided by it).
    whisper 0..1 replaces voicing with aspiration. Returns (audio in units, F1 track Hz)."""
    N = len(f0); out = np.zeros(N); f1o = np.zeros(N)
    hop = int(round(CTRL * fs))
    cur = np.zeros(9); cur[0] = 500; cur[1] = 1500; cur[2] = 2500; cur[6] = 3000; cur[7] = 2000
    kf = 1.0 - np.exp(-1.0 / (0.022 * fs)); ka = 1.0 - np.exp(-1.0 / (0.007 * fs)); kq = 1.0 - np.exp(-1.0 / (0.0015 * fs))
    y1 = np.zeros(6); y2 = np.zeros(6); cf = np.zeros((6, 3))
    fy1 = 0.0; fy2 = 0.0; fA = 0.0; fB = 0.0; fC = 0.0
    ph = 0.0; rs = np.int64(seed32); d = np.zeros(2); lp = 0.0; hpz = 0.0; nlp = 0.0; sh = 0.0
    BW = np.array([70.0, 100.0, 160.0, 250.0, 300.0])
    clk = 1.0 + age * 0.02 * U[0]                       # the one clock sets pitch AND formants, as on the phoneme boxes
    hiss = 0.0 if age <= 0.6 else (age - 0.6) / 0.4 * 1.5e-4
    for n in range(N):
        fi = min(n // hop, T.shape[0] - 1); t = T[fi]
        rs, r = xs32(rs)
        dr = drift_tick(d, r, fs); ck = clk * (1.0 + age * 0.006 * dr)
        for j in range(3): cur[j] += kf * (t[j] - cur[j])
        kk = kq if t[9] > 0.5 else ka
        for j in range(3, 6): cur[j] += kk * (t[j] - cur[j])
        cur[6] = t[6]; cur[7] = t[7]
        cur[8] += ka * (t[8] - cur[8])
        if n % 16 == 0:
            sc = ck / size
            for j in range(3):
                bw = BW[j] * (1.0 + 1.0 * cur[8] if j == 0 else 1.0)       # nasals widen F1
                cf[j, 0], cf[j, 1], cf[j, 2] = reson(cur[j] * sc, bw, fs)
            cf[3, 0], cf[3, 1], cf[3, 2] = reson(3500.0 * sc, BW[3], fs)
            cf[4, 0], cf[4, 1], cf[4, 2] = reson(4500.0 * sc, BW[4], fs)
            cf[5, 0], cf[5, 1], cf[5, 2] = reson(270.0 * sc, 100.0, fs)     # nasal resonance, used in parallel
            fA, fB, fC = reson(cur[6] * sc, cur[7], fs)
        # voice source: band-limited sawtooth, softened by a 4 kHz one-pole
        dt = min(f0[n] * ck / fs, 0.45)
        ph += dt
        if ph >= 1.0: ph -= 1.0
        saw = 2.0 * ph - 1.0 - blep(ph, dt)
        lp += (1.0 - np.exp(-2.0 * np.pi * 4000.0 / fs)) * (saw - lp)
        av = cur[3] * (1.0 - whisper); ah = cur[4] + cur[3] * whisper * 1.5
        nlp += 0.5 * (r - nlp)
        x = 0.5 * av * lp + 0.25 * ah * nlp
        y = x
        for j in range(5):
            yy = cf[j, 0] * y + cf[j, 1] * y1[j] + cf[j, 2] * y2[j]; y2[j] = y1[j]; y1[j] = yy; y = yy
        nn = cf[5, 0] * x + cf[5, 1] * y1[5] + cf[5, 2] * y2[5]; y2[5] = y1[5]; y1[5] = nn
        y = y * (1.0 - 0.4 * cur[8]) + 0.8 * cur[8] * nn
        # frication: its own filter, added after the formants; differenced to brighten
        ff = fA * r + fB * fy1 + fC * fy2; fy2 = fy1; fy1 = ff
        y += 0.45 * cur[5] * ff
        hpz += (1.0 - np.exp(-2.0 * np.pi * 60.0 / fs)) * (y - hpz); y -= hpz
        sh += (1.0 - np.exp(-2.0 * np.pi * 1500.0 / fs)) * (y - sh); y += 1.0 * (y - sh)     # +6 dB shelf above 1.5 kHz
        rs, r2 = xs32(rs)
        out[n] = np.tanh(1.1 * y) + hiss * r2
        f1o[n] = cur[0] / size
    return out, f1o

# CHIP engine tables (written for this module)
CH_F1 = np.geomspace(200.0, 1000.0, 32); CH_F2 = np.geomspace(600.0, 2800.0, 32); CH_F3 = np.geomspace(1500.0, 3500.0, 16)
CH_EN = np.concatenate((np.zeros(1), 10.0 ** (np.arange(-28, 2, 2) / 20.0)))        # 16 energy steps, 2 dB apart
CH_FR = 0.025; CH_SR = 8000.0

@njit(cache=True)
def _q(tab, x):
    b = 0; e = 1e30
    for i in range(len(tab)):
        dd = abs(tab[i] - x)
        if dd < e: e = dd; b = i
    return tab[b]

@njit(cache=True)
def chip_engine(T, f0, size, whisper, age, U, seed32, fs):
    """Same inputs as formant_engine. Internally 8 kHz; output is a zero-order hold at the engine rate
    followed by one two-pole 4.5 kHz low-pass, so the images above 4 kHz are deliberately audible."""
    N = len(f0); out = np.zeros(N); f1o = np.zeros(N)
    clk = CH_SR * (1.0 + age * 0.03 * U[0])
    y1 = np.zeros(4); y2 = np.zeros(4); cf = np.zeros((4, 3))
    a = np.zeros(6); b = np.zeros(6)                 # frame start / end: F1 F2 F3 energy period voiced
    cur = np.zeros(6); a[0] = 500; a[1] = 1500; a[2] = 2500; a[4] = 80
    for j in range(6): b[j] = a[j]
    acc = 0.0; smp = 0.0; per = 0.0; fcount = 0; fN = int(CH_FR * CH_SR); sub = 0
    rs = np.int64(seed32); lf = np.int64(0x1ACE)
    g = np.tan(np.pi * min(4500.0, 0.45 * fs) / fs); k = 1.2
    l1 = 0.0; l2 = 0.0; tchip = 0.0; d = np.zeros(2)
    fric = False
    for n in range(N):
        rs, r = xs32(rs); dr = drift_tick(d, r, fs)
        acc += clk * (1.0 + age * 0.01 * dr) / fs
        while acc >= 1.0:                             # one chip sample
            acc -= 1.0
            if fcount == 0:                           # new frame: read and quantise the targets
                tt = tchip
                fi = min(int(tt / CTRL), T.shape[0] - 1); t = T[fi]
                for j in range(6): a[j] = b[j]
                av = t[3] * (1.0 - whisper); nz = max(t[4], t[5]) + t[3] * whisper * 0.6
                voiced = av > 0.3
                fric = (not voiced) and nz > 0.05
                if fric and t[5] > t[4]:              # a fricative: two wide resonances shaped from its noise filter
                    fcn = min(t[6] / size, 3600.0)
                    b[0] = _q(CH_F1, 0.25 * fcn); b[1] = _q(CH_F2, 0.55 * fcn); b[2] = _q(CH_F3, fcn)
                else:
                    b[0] = _q(CH_F1, t[0] / size); b[1] = _q(CH_F2, t[1] / size); b[2] = _q(CH_F3, min(t[2] / size, 3500.0))
                e = av if voiced else 0.5 * nz
                b[3] = _q(CH_EN, e)
                pidx = min(max(round(CH_SR / max(f0[n], 20.0)), 15.0), 159.0)      # integer pitch period, 6-bit range
                b[4] = pidx; b[5] = 1.0 if voiced else 0.0
                if b[5] != a[5]:                      # no interpolation across a voiced/unvoiced change
                    for j in range(5): a[j] = b[j]
                    a[5] = b[5]
            sub = (fcount * 8) // fN                  # 8 interpolation steps per frame
            w = (sub + 1) / 8.0
            if fcount % (fN // 8) == 0:
                for j in range(5): cur[j] = a[j] + w * (b[j] - a[j])
                cur[5] = b[5]
                wide = fric and cur[5] < 0.5
                cf[0, 0], cf[0, 1], cf[0, 2] = reson(cur[0], 600.0 if wide else 80.0, CH_SR)
                cf[1, 0], cf[1, 1], cf[1, 2] = reson(cur[1], 800.0 if wide else 100.0, CH_SR)
                cf[2, 0], cf[2, 1], cf[2, 2] = reson(cur[2], 500.0 if wide else 140.0, CH_SR)
                cf[3, 0], cf[3, 1], cf[3, 2] = reson(3300.0, 200.0, CH_SR)
            # excitation: one impulse per period, or a one-bit noise sequence
            if cur[5] > 0.5:
                per += 1.0
                if per >= cur[4]: per = 0.0; x = cur[3] * 0.35 * np.sqrt(cur[4])
                else: x = 0.0
            else:
                lf = ((lf >> 1) ^ (-(lf & 1) & 0xB400)) & 0xFFFF
                x = cur[3] * 0.7 * (1.0 if (lf & 1) else -1.0)
            y = x
            for j in range(4):
                if j == 3 and cur[5] < 0.5: continue
                yy = cf[j, 0] * y + cf[j, 1] * y1[j] + cf[j, 2] * y2[j]; y2[j] = y1[j]; y1[j] = yy; y = yy
            y = min(max(y * 1.2, -1.0), 1.0)
            smp = np.round(y * 127.0) / 127.0          # 8-bit converter
            fcount += 1
            if fcount >= fN: fcount = 0
            tchip += 1.0 / CH_SR
        v3 = smp - l2; v1 = a1c(g, k) * l1 + g * a1c(g, k) * v3; v2 = l2 + g * a1c(g, k) * l1 + g * g * a1c(g, k) * v3
        l1 = 2.0 * v1 - l1; l2 = 2.0 * v2 - l2
        out[n] = v2; f1o[n] = cur[0]
    return out, f1o

@njit(cache=True)
def a1c(g, k): return 1.0 / (1.0 + g * (g + k))

def pitch_track(T, base_hz, inflect, fs):
    """Spoken pitch when nothing is patched to V/OCT: monotone at base_hz, plus (INFLECTION) a rise of up to
    3 semitones on stressed vowels and a fall of up to 4 semitones over each word. Smoothed with 40 ms."""
    hop = int(round(CTRL * fs)); n = T.shape[0]; semi = np.zeros(n)
    k = 0
    while k < n:
        if T[k, 3] + T[k, 4] + T[k, 5] > 0:
            j = k
            while j < n and (T[j, 3] + T[j, 4] + T[j, 5] > 0 or (j + 12 < n and np.any(T[j:j + 12, 3:6] > 0))): j += 1
            L = max(1, j - k)
            for i in range(k, j): semi[i] = inflect * (3.0 * T[i, 11] - 4.0 * (i - k) / L + 1.0)
            k = j
        else: k += 1
    f = base_hz * 2.0 ** (np.repeat(semi, hop) / 12.0)
    from scipy.signal import lfilter
    a = 1.0 - np.exp(-1.0 / (0.04 * fs)); return lfilter([a], [1, a - 1.0], f, zi=[f[0] * (1 - a)])[0]

def say(text, lang='en', engine='formant', base_hz=110.0, inflect=0.3, rate=1.0, size=1.0, whisper=0.0, age=0.0,
        gap=0.12, gate_len=None, f0=None, fs=SR, words=None, seed=7):
    """Speak the text, one word after another. gate_len = None: each word plays at its natural length."""
    words = words or load_words()
    wl = [p for _, p in parse(text, lang, words)]
    gates = []; t = 0.05
    for w in wl:
        ln = (len(word_targets(w, rate)) * CTRL) if gate_len is None else gate_len
        gates.append((t, 0.01 if gate_len is None else gate_len)); t += max(ln, len(word_targets(w, rate)) * CTRL) + gap
    T = schedule(wl, gates, fs, rate, hold=gate_len is not None)
    N = int(t * fs) + int(0.2 * fs); T = T[:int(N / (CTRL * fs)) + 1]
    f = pitch_track(T, base_hz, inflect, fs)[:N] if f0 is None else np.resize(f0, N)
    if len(f) < N: f = np.concatenate((f, np.full(N - len(f), f[-1])))
    eng = formant_engine if engine == 'formant' else chip_engine
    return eng(T, f, size, whisper, age, unit(seed, 4), 4242, float(fs))
