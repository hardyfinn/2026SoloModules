import sys, glob, os, numpy as np, soundfile as sf
from scipy import signal
def analyse(p):
    x, sr = sf.read(p, always_2d=True); x = x.mean(1)
    if len(x) < 64: return
    pk = np.max(np.abs(x))+1e-12; x = x/pk
    env = np.abs(signal.hilbert(x)); 
    i0 = int(np.argmax(env > 0.1)); ip = int(np.argmax(env))
    # smoothed env decay: time from peak to -20 dB and -40 dB
    b,a = signal.butter(2, 400/(sr/2)); es = signal.filtfilt(b,a,env)
    def tdrop(db):
        th = es[ip:].max()*10**(-db/20); idx = np.where(es[ip:] < th)[0]
        return 1000*idx[0]/sr if len(idx) else float('nan')
    seg = x[max(0,i0-16): i0+int(0.03*sr)]
    n = 1<<14; S = np.abs(np.fft.rfft(seg*np.hanning(len(seg)), n))**2; f = np.fft.rfftfreq(n, 1/sr)
    # smooth 1/6 oct, find top peaks
    cent = (S*f).sum()/S.sum()
    fb = np.geomspace(150, min(16000, sr/2*0.95), 60); sm = np.array([S[(f>c/1.12)&(f<c*1.12)].mean() for c in fb])
    smdb = 10*np.log10(sm/sm.max()+1e-12)
    pks,_ = signal.find_peaks(smdb, prominence=3)
    pks = sorted(pks, key=lambda i:-smdb[i])[:4]
    # -3dB bandwidth of top peak -> Q
    print(f"{os.path.basename(os.path.dirname(p))}/{os.path.basename(p):18s} sr={sr} len={1000*len(x)/sr:6.0f}ms att={1000*(ip-i0)/sr:5.2f}ms t20={tdrop(20):6.1f} t40={tdrop(40):6.1f} cent={cent:6.0f} peaks=" + " ".join(f"{fb[i]:.0f}({smdb[i]:.0f})" for i in pks))
for d in sys.argv[1:]:
    for p in sorted(glob.glob(d+"/*.wav")): 
        try: analyse(p)
        except Exception as e: print(p, e)
