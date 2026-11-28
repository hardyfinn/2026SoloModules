import sys, os, subprocess, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bulgaria_proto import *
OUT = '/mnt/project-files/bulgaria-vcv/sketches/'
def seq(items, t=0.0, gatefrac=0.92):
    notes = []
    for semi, dur, *c in items:
        if semi is not None: notes.append((t, dur*gatefrac, semi, bool(c and c[0])))
        t += dur
    return notes, t
def run(notes, total, P):
    p, g, pu, _, _ = harmonise(notes, total, P); return render(p, g, pu, P['root'])
def cat(parts, gap=0.5):
    z = np.zeros((2, int(gap*SR))); o = []
    for p in parts: o += [p, z]
    return np.concatenate(o, axis=1)
def save(name, x):
    write(OUT+name+'.wav', reverb(x))
    subprocess.run(['ffmpeg','-y','-loglevel','error','-i',OUT+name+'.wav','-b:a','192k',OUT+name+'.mp3'], check=True)
    os.remove(OUT+name+'.wav')

# a narrow Shope-style tune, written as a sequencer would send it (semitones, 12-TET)
tune = [(3,.45),(5,.45),(7,.9),(5,.3),(3,.3),(5,1.5),(None,.35),
        (7,.45),(8,.3),(7,.3),(5,.6),(3,.45),(2,.45),(3,1.9,1),(None,.6),
        (5,.3),(7,.3),(5,.3),(3,.6),(2,.9),(0,.6),(2,2.2,1),(None,.6)]
notes, T = seq(tune)

# 1. the idea: plain melody, then three parts
P = dict(BASE)
save('01-plain-then-three-parts', cat([run(notes, T, dry_params(P)), run(notes, T, P)]))

# 2. rub control: cents then beat rate, same rising line; then a width sweep
rise = [(s, 1.1) for s in (-9, -5, 0, 3, 7, 12, 15)]
n2, T2 = seq(rise)
Pc = dict(BASE, mode='AEOLIAN', voices=2, loose=0.1, root=220.0)
def pair(P):  # lead + a second below on every note
    Q = dict(P); p, g, pu, _, _ = harmonise(n2, T2, dict(Q, voices=2)); return render(p, g, pu, Q['root'])
# force rub on every degree: use chance=1 rule=False keeps rule path; simplest is to lift the line above the drone
n2b = [(t, d, s+12, c) for (t, d, s, c) in n2]
def pairb(P):
    p, g, pu, _, _ = harmonise(n2b, T2, dict(P, voices=2)); return render(p, g, pu, P['root']/2)
sweepn = [(0, 9.0, 19, False)]
def sweep():
    P = dict(Pc, voices=2); n = int(9.5*CR)
    p, g, pu, _, _ = harmonise(sweepn, 9.5, P)
    w = np.interp(np.arange(n)/CR, [0, 0.5, 9], [250, 250, 25])
    p[1] = p[0] - w
    return render(p, g, pu, P['root'])
save('02-rub-control-cents-beat-sweep', cat([pairb(dict(Pc, rubmode='cents', width=170)), pairb(dict(Pc, rubmode='hz', beat=32)), sweep()]))

# 3. ornaments and cadences: slides, shake, whoop, more looseness
P3 = dict(BASE, slide=0.6, shake=0.8, whoop=1.0, loose=0.8)
a = run(notes, T+0.3, P3)
b = run(notes, T+0.3, dict(P3, cadence='resolve', whoop=0.0))
save('03-ornaments-and-cadences', cat([a, b]))

# 4. eight-voice cluster on a 126 bpm sixteenth pattern, beat-rate rub
s16 = 60/126/4
pat = [0,None,12,0, 3,None,0,10, None,0,12,1, 0,None,7,5]
items = []
for bar in range(4):
    for i, s in enumerate(pat):
        last = (i == 15 and bar % 2 == 1)
        items.append((s, s16) if not last else (s, s16, 1))
n4, T4 = seq(items, gatefrac=0.7)
n4 = [(t, (d if (i % 5) else d*2.2), s, c) for i, (t, d, s, c) in enumerate(n4)]
P4 = dict(BASE, mode='PHRYGIAN', voices=8, rubmode='hz', beat=34, root=220.0, loose=0.25, slide=0.3, whoop=1.0)
def kick(T):
    N = int(T*SR); y = np.zeros(N); L = int(.28*SR); t = np.arange(L)/SR
    k = np.sin(2*np.pi*(48*t + 14*(1-np.exp(-t/0.03))))*np.exp(-t/0.09)
    for b in range(int(T/(s16*4))+1):
        i = int(b*s16*4*SR)
        if i+L < N: y[i:i+L] += k
    return np.vstack([y, y])*2.2
d = run(n4, T4, dict(dry_params(P4))); w = run(n4, T4, P4)
d = d/np.max(np.abs(d)); w = w/np.max(np.abs(w))
save('04-eight-voice-cluster-126bpm', np.concatenate([d*0.8+kick(T4)[:, :d.shape[1]]*0.4, w+kick(T4)[:, :w.shape[1]]*0.4], axis=1))
print('ok')
