import numpy as np, soundfile as sf, os
from clapfall_proto import *
tabs = build_tables(); os.makedirs('sk', exist_ok=True)
def k_density(hz): return np.log(hz/1.5)/np.log(400)
def k_decay(s):    return np.log(s/0.012)/np.log(500)
def k_freq(hz):    return np.log(hz/150)/np.log(10000/150)
def seq(name, hits, dur, **kw):
    y = burst(tabs, dur=dur, trigs=hits, **kw); sf.write(f'sk/{name}.wav', y, SR); return y
beat = 60/118/4*4  # quarter at 118 bpm
back = [beat*(1+2*i) for i in range(8)]            # backbeats, 8 of them
D = beat*17
# 1 drum machine clap: 808 particles, regular 100/s, short burst, room reverb
y808 = seq('01-machine-808', back, D, surface='808', density=k_density(95), decay=k_decay(0.09), loose=0.0, freq=k_freq(1100), spread=0.0, env_mode=2, filt_mode=2, reverb=0.12, size=0.2)
seq('02-machine-909', back, D, surface='909', density=k_density(85), decay=k_decay(0.13), loose=0.0, freq=k_freq(1400), spread=0.0, env_mode=2, filt_mode=2, reverb=0.2, size=0.3)
# 3 one person, 4 small group, 5 big group on the backbeat
seq('03-one-person', back, D, surface='CLAP', density=k_density(20), decay=k_decay(0.012), loose=0.5, freq=k_freq(1600), spread=0.05, env_mode=1, filt_mode=2, reverb=0.25, size=0.2)
seq('04-five-people', back, D, surface='GROUP', density=k_density(170), decay=k_decay(0.06), loose=1.0, freq=k_freq(1500), spread=0.22, env_mode=2, filt_mode=2, reverb=0.3, size=0.3)
seq('05-gospel-room', back, D, surface='GROUP', density=k_density(420), decay=k_decay(0.11), loose=1.0, freq=k_freq(1400), spread=0.3, env_mode=2, filt_mode=2, reverb=0.5, size=0.55)
# 6 applause: free running, swell by moving DECAY (done as segments)
segs = []
for dk, dn in [(0.15, 0.35), (0.4, 0.6), (0.8, 0.8), (1.0, 0.95), (0.7, 0.8), (0.35, 0.55), (0.12, 0.3)]:
    segs.append(burst(tabs, 'GROUP', 2.0, density=dn, decay=dk, loose=1.0, freq=k_freq(1700), spread=0.4, env_mode=2, filt_mode=2, free=True, trigs=(), reverb=0.35, size=0.7, seed=int(dk*100)))
sf.write('sk/06-applause.wav', np.concatenate(segs), SR)
# 7 snaps on the offbeat, 8 sticks/rim clatter, 9 rain of drops, 10 coins
seq('07-snaps', back, D, surface='SNAP', density=k_density(60), decay=k_decay(0.03), loose=0.9, freq=k_freq(3000), spread=0.12, env_mode=1, filt_mode=2, reverb=0.3, size=0.3)
seq('08-sticks-roll', [beat*i for i in range(0, 16, 2)], D, surface='STICKS', density=k_density(28), decay=k_decay(0.5), loose=0.15, freq=k_freq(2500), spread=0.15, env_mode=0, filt_mode=2, reverb=0.2, size=0.3)
seq('09-drops', [0.0, 4.0], 8.0, surface='DROP', density=k_density(40), decay=k_decay(3.0), loose=1.0, freq=0.5, spread=0.6, env_mode=2, filt_mode=1, reverb=0.4, size=0.6)
seq('10-coins', [0.0, 2.0, 4.0], 6.0, surface='COINS', density=k_density(70), decay=k_decay(0.5), loose=1.0, freq=0.55, spread=0.35, env_mode=2, filt_mode=1, reverb=0.25, size=0.4)
seq('11-ping-pong', [0.0, 3.0], 6.0, surface='PING', density=k_density(9), decay=k_decay(2.5), loose=0.3, freq=0.5, spread=0.1, env_mode=0, filt_mode=1, reverb=0.3, size=0.3)
# envelope print for the 808 clap, first hit
h = y808[int(back[0]*SR):int((back[0]+0.4)*SR), 0]
e = [np.sqrt(np.mean(h[i:i+96]**2)) for i in range(0, int(0.08*SR), 96)]
print('ours 808 env 2ms:', ' '.join(f'{v*100:3.0f}' for v in e))
x, sr = sf.read('../ref/Dirt-Samples/cp/HANDCLP0.wav', always_2d=True); x = x.mean(1); i0 = np.argmax(np.abs(x) > 0.05)
e = [np.sqrt(np.mean(x[i0+i:i0+i+88]**2)) for i in range(0, int(0.08*sr), 88)]
print('real cp0 env 2ms:', ' '.join(f'{v*100:3.0f}' for v in e))
e = [np.sqrt(np.mean(x[i0+i:i0+i+882]**2)) for i in range(0, len(x)-i0-882, 882)]; print('real cp0 20ms:', ' '.join(f'{v*100:3.0f}' for v in e))
e = [np.sqrt(np.mean(h[i:i+960]**2)) for i in range(0, len(h)-960, 960)]; print('ours 20ms:', ' '.join(f'{v*100:3.0f}' for v in e))
for f in sorted(os.listdir('sk')):
    y,_ = sf.read('sk/'+f); print(f, 'peak %.2f rms %.3f' % (np.abs(y).max(), np.sqrt((y**2).mean())))
