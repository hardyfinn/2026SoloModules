#!/usr/bin/env python3
"""Analyses the renders from ./render <dir> against cubit.md section 12. Prints PASS / FAIL per check."""
import hashlib, os, sys
import numpy as np

D = sys.argv[1] if len(sys.argv) > 1 else "."
FS = 48000
fails = 0


def ld(n):
    return np.fromfile(os.path.join(D, n + ".f32"), np.float32).astype(np.float64)


def report(name, ok, detail):
    global fails
    fails += 0 if ok else 1
    print(f"{'PASS' if ok else 'FAIL'}  {name}: {detail}")


def freq(x, fs=FS):
    """Frequency from rising mean-crossings, interpolated, over the second half."""
    x = x[len(x) // 2:] - np.mean(x[len(x) // 2:])
    i = np.flatnonzero((x[:-1] < 0) & (x[1:] >= 0))
    t = i + (-x[i]) / (x[i + 1] - x[i])
    return fs * (len(t) - 1) / (t[-1] - t[0])


def spectrum(x, skip=4800):
    x = x[skip:]
    x = x - x.mean()
    w = np.blackman(len(x))
    X = np.abs(np.fft.rfft(x * w)) / (w.sum() / 2)
    f = np.fft.rfftfreq(len(x), 1 / FS)
    return f, X


def db(a, ref):
    return 20 * np.log10(max(a, 1e-30) / ref)


# 1 pitch (exact period via crossings is biased by the staircase; use the spectral peak with interpolation)
def peak_freq(x, near):
    f, X = spectrum(x)
    i = np.argmin(np.abs(f - near))
    i = i - 20 + np.argmax(X[i - 20:i + 21])
    a, b, c = np.log(X[i - 1:i + 2] + 1e-30)
    return f[i] + 0.5 * (a - c) / (a - 2 * b + c) * (f[1] - f[0])

f0 = peak_freq(ld("c1_out_0v"), 261.63)
f1 = peak_freq(ld("c1_out_1v"), 523.25)
fs_ = peak_freq(ld("c1_sub_0v"), 130.8)
fc = freq(ld("c1_clk_m2v"))
report("1 pitch", abs(f0 - 261.6256) < 0.05 and abs(f1 / f0 - 2) < 1e-4 and abs(fs_ * 2 - f0) < 0.05
       and abs(fc - 64 * 261.6256 / 4) < 0.5,
       f"0 V {f0:.3f} Hz, +1 V {f1:.3f} Hz, SUB {fs_:.3f} Hz, CLK OUT at -2 V {fc:.2f} Hz (want {64 * 261.6256 / 4:.2f})")

# 2 levels on 3-0-0 at 20 Hz: mid-step values and overshoot
rom = np.fromfile(os.path.join(D, "c10_factory.bin"), np.uint8).astype(float)
wave = rom[3 * 64:4 * 64]


def levels(suffix, fs):
    out, uni = ld("c2_out" + suffix), ld("c2_uni" + suffix)
    spp = fs / (20 * 64)                      # samples per step
    n = np.arange(len(out))
    best = None
    for lat in np.arange(0, 40, 0.25):        # find the latency against the ideal staircase
        st = np.floor((n - lat) / spp).astype(int) % 64
        e = np.mean(np.abs((wave[st] - 127.5) / 127.5 * 7 - out)[2000:])
        if best is None or e < best[0]:
            best = (e, lat)
    lat = best[1]
    mid, over = [], 0.0
    for k in range(int(len(out) / spp) - 2):
        if k * spp < 2000:
            continue
        c = int(round(lat + (k + 0.5) * spp))
        mid.append((out[c] - (wave[k % 64] - 127.5) / 127.5 * 7, uni[c] - wave[k % 64] / 48))
        a, b = int(lat + (k - 0.5) * spp), int(lat + (k + 0.5) * spp)      # window around edge k
        lo, hi = sorted((wave[(k - 1) % 64], wave[k % 64]))
        seg = uni[a:b] * 48
        h = max(hi - lo, 1)
        over = max(over, (seg.max() - hi) / h, (lo - seg.min()) / h)
    return np.abs(np.array(mid)).max(axis=0), over, lat


m48, o48, lat = levels("", FS)
m96, o96, _ = levels("_96k", 96000)
report("2 levels", max(m48.max(), m96.max()) < 1e-3 and o96 < 0.02,
       f"mid-step error OUT {m48[0] * 1e3:.3f} mV, UNI {m48[1] * 1e3:.3f} mV; overshoot {o96 * 100:.2f} % at 96 kHz "
       f"(kernel alone), {o48 * 100:.2f} % at 48 kHz (the 2x decimator's ringing, see PR)")

# 3 the grit: 0-0-0 at 110 Hz, component near 7040 Hz at -36 dB
f, X = spectrum(ld("c34_out_110"))
fund = X[np.argmin(np.abs(f - 110)) - 3:np.argmin(np.abs(f - 110)) + 4].max()
band = (f > 7040 - 110 - 5) & (f < 7040 + 110 + 5)
img = X[band].max()
report("3 grit", abs(db(img, fund) + 36) < 1.5, f"strongest component 6930..7150 Hz at {db(img, fund):.2f} dB re fundamental")


# 4 nothing else: inharmonic below -90 dB, aliases below -60 dB
def harm_check(hz):
    f, X = spectrum(ld(f"c34_out_{hz}"))
    df = f[1] - f[0]
    fund = X[np.argmin(np.abs(f - hz)) - 3:np.argmin(np.abs(f - hz)) + 4].max()
    # true components: k*hz for k*hz < fs/2 (kept images are real harmonics, k up to any)
    # aliases: reflections of k*hz above fs/2 that land at frequencies that are not multiples of hz
    worst_alias, worst_other = -400, -400
    mask = np.ones(len(f), bool)
    halfw = 8 * df + 2
    harmonics = np.arange(hz, FS / 2, hz)
    for h in harmonics:
        mask &= np.abs(f - h) > halfw
    mask &= f > 20
    aliasf = []
    for k in range(1, 400):
        a = (k * hz) % FS
        a = FS - a if a > FS / 2 else a
        if k * hz > FS / 2 and np.min(np.abs(harmonics - a)) > halfw:
            aliasf.append(a)
    am = np.zeros(len(f), bool)
    for a in aliasf:
        am |= np.abs(f - a) <= halfw
    if (mask & am).any():
        worst_alias = db(X[mask & am].max(), fund)
    rest = mask & ~am
    if rest.any():
        worst_other = db(X[rest].max(), fund)
    return worst_alias, worst_other

res4 = {hz: harm_check(hz) for hz in (1000, 5000, 13000)}
report("4 nothing else", all(a < -60 and o < -90 for a, o in res4.values()),
       ", ".join(f"{hz} Hz: aliases {a:.1f} dB, other {o:.1f} dB" for hz, (a, o) in res4.items()))

# 5 sync: exact 100 Hz periodicity
f, X = spectrum(ld("c5_out"))
fund = X[(f > 95) & (f < 105)].max()
m = np.ones(len(f), bool)
for h in np.arange(100, FS / 2, 100):
    m &= np.abs(f - h) > 8
m &= f > 20
report("5 sync", db(X[m].max(), fund) < -60, f"strongest non-harmonic of 100 Hz at {db(X[m].max(), fund):.1f} dB")

# 6 external clock
u = ld("c6_uni_pulses")
per = 100
vals = np.array([u[per * (k + 1) + 50] for k in range(74)])   # value 50 samples after pulse k (settled)
idx = [np.argmin(np.abs(rom[:64] / 48 - v)) for v in vals]
seq_ok = all(abs(vals[k] - rom[(k + 1) % 64] / 48) < 1e-3 for k in range(73))
start = u[40]
returns = abs(vals[63] - rom[0] / 48) < 1e-3 and abs(start - rom[0] / 48) < 1e-3
fe = peak_freq(ld("c6_out_6k"), 93.75)
report("6 external clock", seq_ok and returns and abs(fe - 93.75) < 0.02,
       f"each pulse moves one byte: {seq_ok}, 64 pulses return to start: {returns}, 6 kHz train gives {fe:.3f} Hz")

# 7 taps: count gates in one bar (64 steps, first 2 s)
counts = []
for j in range(1, 5):
    t = ld(f"c7_tap{j}")[:2 * FS + 200] > 5
    t = t[30:]   # skip the renderer start-up
    counts.append(int(np.sum(t[1:] & ~t[:-1]) + (1 if t[0] else 0)))
report("7 taps", counts == [4, 2, 4, 0], f"gates per bar on TAP 1..4: {counts} (want [4, 2, 4, 0])")

# 8 tuning: UNI steps of 1/12 V
u = ld("c8_uni")
spp = FS / 32.0
levels = np.array([u[int(40 + (k + 0.5) * spp)] for k in range(128)])
steps = np.diff(levels)
nz = steps[np.abs(steps) > 1e-3]
err = np.abs(np.abs(nz) - 1 / 12).min(initial=1) if len(nz) else 1
semis = np.round(nz * 12)
dev = np.abs(nz - semis / 12).max()
report("8 tuning", dev < 1e-4 and len(nz) > 0, f"{len(nz)} note changes, worst deviation from n/12 V {dev * 1e3:.4f} mV; "
       f"steps in semitones: {sorted(set(semis.astype(int).tolist()))}")

# 9 splice / res / depth (latency from check 2 applies: same pitch, same renderer)
spp = FS / (20 * 64)
w0, w1 = ld("c9_wave0"), ld("c9_wave1")
u = ld("c9_splice_uni")
got = np.array([u[int(round(lat + (64 * 4 + s + 0.5) * spp))] * 48 for s in range(64)])
exp = np.concatenate([w0[:32], w1[32:]])
spl_ok = np.abs(got - exp).max() < 0.05
u = ld("c9_res8_uni")
got = np.array([u[int(round(lat + (64 * 4 + s + 0.5) * spp))] * 48 for s in range(64)])
held = len(set(np.round(got[::8]).tolist())) <= 8 and all(np.abs(np.diff(got.reshape(8, 8), axis=1)).max(axis=1) < 0.05)
o = ld("c9_depth1_out")[4000:]
lv = sorted(set(np.round(o[np.abs(np.diff(o, prepend=o[0])) < 1e-6], 3).tolist()))
dep_ok = len(lv) == 2 and abs(lv[0] + lv[1]) < 1e-3
report("9 splice / res / depth", spl_ok and held and dep_ok,
       f"splice halves match waves 0 and 1: {spl_ok}; RES 8 holds 8 values per cycle: {held}; DEPTH 1 levels {lv}")

# 10 ROM
md5 = hashlib.md5(open(os.path.join(D, "c10_factory.bin"), "rb").read()).hexdigest()
rt = open(os.path.join(D, "c10_export.bin"), "rb").read() == open(os.path.join(D, "c10_factory.bin"), "rb").read()
report("10 ROM", md5 == "a460ca4191a8d4fcec08f827b4cd56db" and rt, f"embedded md5 {md5}, export file identical: {rt} "
       "(WAV load and re-load printed by ./render)")

# 12 drift: pitch within +-30 cents over 60 s at DRIFT 1
x = ld("c12_drift1")
xs = x - np.convolve(x, np.ones(480) / 480, "same")
win = 4800
cents = []
for s in range(2 * FS, len(x) - win, win):
    seg = x[s:s + win] - x[s:s + win].mean()
    i = np.flatnonzero((seg[:-1] < 0) & (seg[1:] >= 0))
    if len(i) > 3:
        t = i + (-seg[i]) / (seg[i + 1] - seg[i])
        fr = FS * (len(t) - 1) / (t[-1] - t[0])
        cents.append(1200 * np.log2(fr / 100))
cents = np.array(cents)
report("12 drift", np.abs(cents).max() < 30, f"DRIFT 1 pitch over 60 s: {cents.min():+.1f} .. {cents.max():+.1f} cents "
       "(DRIFT 0 null printed by ./render)")

print(f"\n{fails} failed")
sys.exit(1 if fails else 0)
