# Generates the control-voltage test signal for fitting Silverbox to a real unit with CV, GATE,
# FILTER, ACCENT and SLIDE input sockets. 48 kHz, 24-bit, 6 channels, written as volts / FS_VOLTS.
#   ch1 pitch CV   ch2 gate   ch3 filter CV   ch4 accent   ch5 slide   ch6 marker (loop back to an input)
# Every voltage is between 0 and +5 V.
import numpy as np, json, wave, sys
SR = 48000
FS_VOLTS = 10.0          # assumed output volts at digital full scale; the loopback take checks it
GATE_V = 5.0
P, G, F, A, S, M = range(6)
segs = []                # (channel, start_s, end_s, volts)
secs = []
T = 1.0                  # running time, seconds

def hold(ch, t0, t1, v): segs.append((ch, t0, t1, v))
def section(name, start, **meta): secs.append(dict(name=name, start_s=round(start, 6), **meta))
def note(t0, dur, pitch, accent=False, slide=False, filt=None):
    hold(P, t0 - 0.004, t0 + dur + 0.004, pitch)     # pitch settles 4 ms before the gate
    hold(G, t0, t0 + dur, GATE_V); hold(M, t0, t0 + dur, GATE_V)
    if accent: hold(A, t0 - 0.004, t0 + dur, GATE_V)
    if slide: hold(S, t0 - 0.004, t0 + dur, GATE_V)

# A. marker: three 10 ms pulses on the marker channel only
section("marker", T, pulses=3, spacing_s=0.25)
for i in range(3): hold(M, T + 0.25 * i, T + 0.25 * i + 0.01, GATE_V)
T += 1.5

# B. pitch: 1.5 s notes from 1 V to 4 V in 0.5 V steps
pv = [1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0]
section("pitch", T, volts=pv, note_s=1.5, step_s=1.8)
for i, v in enumerate(pv): note(T + 1.8 * i, 1.5, v)
T += 1.8 * len(pv) + 0.5

# C. filter CV staircase: low note retriggered every 3 s, 2.7 s gate, filter CV 0 to 5 V
fv = [round(0.5 * i, 2) for i in range(11)]
section("filter_steps", T, volts=fv, pitch_v=1.0, note_s=2.7, step_s=3.0)
for i, v in enumerate(fv):
    hold(F, T + 3.0 * i - 0.1, T + 3.0 * i + 2.9, v); note(T + 3.0 * i, 2.7, 1.0)
T += 3.0 * len(fv) + 0.5

# D. envelope: four 60 ms notes then four 500 ms notes, one per second
section("env_short", T, count=4, note_s=0.06, step_s=1.0, pitch_v=1.0)
for i in range(4): note(T + i, 0.06, 1.0)
T += 4
section("env_long", T, count=4, note_s=0.5, step_s=1.0, pitch_v=1.0)
for i in range(4): note(T + i, 0.5, 1.0)
T += 4.5

# E. accent: plain, accented, fast accented (build-up), alternating
def run(name, n, step, acc):
    global T
    section(name, T, count=n, step_s=step, note_s=step / 2, accents=[bool(acc(i)) for i in range(n)], pitch_v=1.5)
    for i in range(n): note(T + step * i, step / 2, 1.5, accent=acc(i))
    T += step * n + 1.0
run("accent_none", 4, 0.25, lambda i: False)
run("accent_all", 4, 0.25, lambda i: True)
run("accent_fast", 8, 0.125, lambda i: True)
run("accent_alternate", 8, 0.125, lambda i: i % 2 == 0)

# F. slide: one held gate, pitch stepping every second, with and without slide
sv = [1.0, 2.0, 1.0, 1.25, 3.0, 1.0]
for name, sl in (("slide_on", True), ("slide_off", False)):
    section(name, T, volts=sv, step_s=1.0)
    hold(G, T, T + len(sv), GATE_V); hold(M, T, T + len(sv), GATE_V)
    if sl: hold(S, T + 0.5, T + len(sv), GATE_V)
    for i, v in enumerate(sv): hold(P, T + i, T + i + 1, v)
    T += len(sv) + 1.0

# G. pattern: a 16-step line at 125 bpm, four bars plain, then four bars with filter CV rising 0 to 5 V
step = 60 / 125 / 4
semis = [0, 0, 12, 0, 3, 0, 15, 12, 0, 7, 0, 10, 12, 0, 3, 5]
acc   = [1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0]
sld   = [0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0]   # slide into the next step
rest  = [0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0]
def pattern(name, bars, ramp):
    global T
    section(name, T, bars=bars, step_s=round(step, 6), semitones=semis, accent=acc, slide=sld, rest=rest,
            base_v=1.5, filter_ramp_v=[0, 5] if ramp else None)
    n = bars * 16
    for k in range(n):
        i = k % 16
        if rest[i]: continue
        t0 = T + k * step
        tied = sld[i] and not rest[(i + 1) % 16]
        prev_tied = sld[(i - 1) % 16] and not rest[(i - 1) % 16] and k > 0
        dur = step if tied else step * 0.5
        hold(P, t0 - 0.004, t0 + step, 1.5 + semis[i] / 12)
        hold(G, t0, t0 + dur, GATE_V); hold(M, t0, t0 + dur, GATE_V)
        if acc[i]: hold(A, t0 - 0.004, t0 + dur, GATE_V)
        if prev_tied: hold(S, t0 - 0.004, t0 + dur, GATE_V)
    if ramp: segs.append((F, T, T + n * step, "ramp"))
    T += n * step + 1.0
pattern("pattern_plain", 4, False)
pattern("pattern_filter_ramp", 4, True)

section("marker_end", T, pulses=3, spacing_s=0.25)
for i in range(3): hold(M, T + 0.25 * i, T + 0.25 * i + 0.01, GATE_V)
T += 2.0

N = int(T * SR)
x = np.zeros((N, 6))
pitch_set = np.zeros(N, bool)
for ch, t0, t1, v in segs:
    a, b = max(0, int(round(t0 * SR))), min(N, int(round(t1 * SR)))
    if v == "ramp": x[a:b, ch] = np.linspace(0, 5, b - a)
    else: x[a:b, ch] = v
    if ch == P: pitch_set[a:b] = True
# pitch holds its last value between notes, so the oscillator never jumps to 0 V
last = 1.0
idx = np.where(pitch_set, np.arange(N), 0); np.maximum.accumulate(idx, out=idx)
x[:, P] = np.where(idx > 0, x[idx, P], last)
assert x.min() >= 0 and x.max() <= 5.0

def write(path, data):
    q = np.clip(np.round(data / FS_VOLTS * (2**23 - 1)), -2**23, 2**23 - 1).astype(np.int32)
    b = q.astype('<i4').tobytes()
    raw = np.frombuffer(b, np.uint8).reshape(-1, 4)[:, :3].tobytes()
    with wave.open(path, 'wb') as w:
        w.setnchannels(data.shape[1]); w.setsampwidth(3); w.setframerate(SR); w.writeframes(raw)
write("silverbox-cv-test.wav", x)
# short check file: marker, three notes, filter CV at 0 then 5 V
chk = int(secs[1]["start_s"] * SR) + int(1.8 * 3 * SR)
write("silverbox-cv-check.wav", x[:chk])
json.dump(dict(sample_rate=SR, fs_volts=FS_VOLTS, channels=["pitch", "gate", "filter", "accent", "slide", "marker"],
               length_s=round(T, 3), sections=secs), open("cv-test-map.json", "w"), indent=1)
print(round(T, 2), "s", [s["name"] for s in secs])
