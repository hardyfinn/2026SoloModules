# Writes a synthetic drum loop (no samples used) for the offline test:
# 8 beats at 170 BPM, 48 kHz 24-bit stereo, BPM in the file name.
import math, random, struct, sys, os
out = sys.argv[1]; os.makedirs(out, exist_ok=True)
rate, bpm, beats = 48000, 170, 8
n = int(rate * beats * 60 / bpm)
L = [0.0] * n
random.seed(3)
def kick(t0):
    for i in range(int(.25 * rate)):
        if t0 + i >= n: break
        t = i / rate; f = 50 + 120 * math.exp(-t * 30)
        L[t0 + i] += .9 * math.sin(2 * math.pi * f * t) * math.exp(-t * 9)
def snare(t0):
    for i in range(int(.2 * rate)):
        if t0 + i >= n: break
        t = i / rate
        L[t0 + i] += (.5 * (random.random() * 2 - 1) + .3 * math.sin(2 * math.pi * 190 * t)) * math.exp(-t * 18)
def hat(t0):
    for i in range(int(.05 * rate)):
        if t0 + i >= n: break
        L[t0 + i] += .2 * (random.random() * 2 - 1) * math.exp(-i / rate * 60)
step = n // 32
pattern_k = [0, 10, 16, 26]; pattern_s = [8, 24, 30]
for s in range(32):
    if s in pattern_k: kick(s * step)
    if s in pattern_s: snare(s * step)
    if s % 2 == 0: hat(s * step)
data = bytearray()
for i in range(n):
    v = max(-1, min(1, L[i] * .7))
    x = int(v * 8388607)
    b = struct.pack('<i', x)[:3]
    data += b + b
fmt = struct.pack('<HHIIHH', 1, 2, rate, rate * 6, 6, 24)
wav = b'RIFF' + struct.pack('<I', 36 + len(data)) + b'WAVEfmt ' + struct.pack('<I', 16) + fmt + b'data' + struct.pack('<I', len(data)) + data
open(os.path.join(out, 'test break 170bpm.wav'), 'wb').write(wav)
# a short one-shot too
open(os.path.join(out, 'zz one shot.wav'), 'wb').write(wav[:44] + data[:6 * 12000])
