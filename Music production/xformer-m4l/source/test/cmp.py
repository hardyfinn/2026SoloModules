# Compare a firmware trace with a port trace.
# Gate segments must match within 1 tick (ms-rate ratchets can land one tick apart).
# Pitch (as a MIDI note) is compared at the end of each segment unless glide is on.
import sys, math
def segs(path):
    out, g, st, cv = [], 0, 0, 0.0
    for line in open(path):
        t, gg, c = line.split(); t, gg, c = int(t), int(gg), float(c)
        if gg != g:
            if g == 1: out.append((st, t, cv))
            st = t
        g, cv = gg, c
    return out
fw, js, pitch = segs(sys.argv[1]), segs(sys.argv[2]), sys.argv[3] == '1'
midi = lambda v: 60 + math.floor(v * 12 + 0.01)
bad = 0
# the simulator closes a note that is still sounding when the run ends; ignore that last edge
end = max([t[1] for t in fw + js] or [0])
if len(fw) == len(js) + 1 and fw[-1][1] >= end - 1: fw = fw[:-1]
if len(js) == len(fw) + 1 and js[-1][1] >= end - 1: js = js[:-1]
for i in range(max(len(fw), len(js))):
    if i >= len(fw) or i >= len(js): bad += 1; break
    a, b = fw[i], js[i]
    if abs(a[0]-b[0]) > 1 or abs(a[1]-b[1]) > 1 or (pitch and midi(a[2]) != midi(b[2])):
        print('  seg', i, a, b); bad += 1
        if bad > 3: break
sys.exit(1 if bad else 0)
