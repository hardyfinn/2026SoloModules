"""Reference renders from tape_ref.py (the engine the module is built from). python3 make_reference.py <outdir>"""
import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np, soundfile as sf
from tape_ref import *
from songs import house, hiphop, techno, mix
FS = 48000; OUT = sys.argv[1] if len(sys.argv) > 1 else 'reference'; os.makedirs(OUT, exist_ok=True)
rms = lambda x: np.sqrt(np.mean(x ** 2))
def chain(x, strength=0.5, mt='4TR', mt_tape='II', mt_rec=0.0, nr='X2', bounce=1, comp=None, ms='DECK', ms_tape='II', ms_rec=0.0, ms_nr='B', age=0.25, wear=0.0, seed=1, trim=0.0):
    """Stereo in (peak 1.0 = 5 V), the module's default routing: DESK -> multitrack (x bounce) -> BUS -> master."""
    y = x * 1.41
    if trim: y = desk(y, FS, trim)
    for b in range(bounce):
        if mt: y = deck(y, FS, mt, mt_tape, mt_rec, nr, nr, age=age, wear=wear, unit_seed=seed, noise_seed=seed * 10 + b, strength=strength)
    if comp: y = bus_comp(y, FS, *comp)
    if ms == 'DAT': y = dat(y, FS)
    elif ms: y = deck(y, FS, ms, ms_tape, ms_rec, ms_nr, ms_nr, age=age, wear=wear, unit_seed=seed + 100, noise_seed=seed * 10 + 9, strength=strength)
    return y / 1.41
def bars(x, bpm, a, b): L = 4 * 60 / bpm; return x[int(a * L * FS):int(b * L * FS)]
def cat(parts, gap=0.35):
    out = []
    for p in parts:
        p = p.copy(); n = int(0.01 * FS); p[:n] *= np.linspace(0, 1, n)[:, None]; p[-n:] *= np.linspace(1, 0, n)[:, None]; out += [p, np.zeros((int(gap * FS), 2))]
    return np.concatenate(out[:-1])
def save(name, y, ref):
    y = y * rms(ref) / rms(y) * 0.89 / np.abs(ref).max(); pk = np.abs(y).max()
    if pk > 0.99: y *= 0.99 / pk
    sf.write(f'{OUT}/{name}.wav', y, FS, subtype='PCM_16')
    X = (np.abs(np.fft.rfft(y * np.hanning(len(y))[:, None], axis=0)) ** 2).sum(1); f = np.fft.rfftfreq(len(y), 1 / FS)
    bd = lambda a, b: 10 * np.log10(X[(f >= a) & (f < b)].sum() + 1e-20)
    print(name, 'hf %.1f lf %.1f crest %.1f' % (bd(8000, 16000) - bd(200, 2000), bd(30, 120) - bd(200, 2000), 20 * np.log10(np.abs(y).max() / rms(y))))
H, P, T = house(), hiphop(), techno(); dH, dP, dT = [m / np.abs(m).max() for m in (mix(H), mix(P), mix(T))]
m = lambda x, r: x * rms(r) / rms(x)
save('01-strength-0-25-50-75-100-house', cat([m(bars(chain(dH, s), 120, 0, 4), bars(dH, 120, 0, 4)) for s in (0, 0.25, 0.5, 0.75, 1.0)]), dH)
save('02-house-dry', dH, dH)
save('03-house-default', chain(dH), dH)
save('04-house-two-decks-1985', chain(dH, 0.6, mt='DECK', mt_tape='I', mt_rec=3, nr=None, ms='DECK', ms_tape='I', ms_rec=3, ms_nr=None), dH)
save('05-house-in-the-red', chain(dH, 0.6, mt_rec=9, trim=8, ms_rec=6), dH)
save('06-hiphop-dry', dP, dP)
save('07-hiphop-default', chain(dP), dP)
save('08-hiphop-third-bounce', chain(dP, 0.5, mt='4TR HI', bounce=3, mt_rec=3), dP)
save('09-techno-dry', dT, dT)
save('10-techno-studio-8-to-quarter-inch', chain(dT, 0.6, mt='8TR 7.5', mt_rec=6, ms='1/4 INCH', ms_rec=5, ms_nr=None), dT)
save('11-techno-260-to-dat', chain(dT, 0.5, mt='4TR C', nr='C', mt_rec=4, ms='DAT'), dT)
save('12-house-neglected', chain(dH, 0.8, age=1.0, wear=0.5), dH)
