"""Compare a human-hands render with the machine render of the same plan, window by window."""
import numpy as np, wave, sys
def load(p):
    w=wave.open(p); x=np.frombuffer(w.readframes(w.getnframes()),'<i2').reshape(-1,w.getnchannels())/32768; return x.mean(1), w.getframerate()
def compare(hp, mp, spb, a, b, win=0.25):
    h, sr = load(hp); m, _ = load(mp)
    lags=[]; bad=0; silent=0; n=0
    for t in np.arange(a, b, win):
        i0=int(t*spb); L=int(win*spb)
        seg=m[i0:i0+L]
        if np.sqrt((seg**2).mean()) < 0.01: silent+=1; continue
        n+=1; best=(-1,0)
        for lag in range(-1800,1801,4):
            j0=i0+lag
            if j0<0 or j0+L>len(h): continue
            u=h[j0:j0+L]; c=np.dot(seg,u)/np.sqrt(np.dot(seg,seg)*np.dot(u,u)+1e-12)
            if c>best[0]: best=(c,lag)
        if best[0]<0.5: bad+=1
        else: lags.append(best[1]/sr*1000)
    lags=np.array(lags)
    return n, bad, np.median(np.abs(lags)), np.mean(np.abs(lags)), np.percentile(np.abs(lags),90)
if __name__=='__main__':
    spb=791328/32
    n,bad,med,mean,p90=compare(sys.argv[1],sys.argv[2],spb,float(sys.argv[3]),float(sys.argv[4]))
    print('windows %d, unlike machine %d (%.0f%%), timing |lag| median %.1f ms mean %.1f ms 90%% %.1f ms'%(n,bad,100*bad/n,med,mean,p90))
