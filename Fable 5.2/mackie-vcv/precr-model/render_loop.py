import precr, numpy as np, sys, wave, os
from scipy.io import wavfile
from scipy.signal import resample_poly
from scipy.optimize import brentq
from multiprocessing import Pool
OS=4
sr,x=wavfile.read("in.wav"); x=x.astype(float)/32768*10.0   # full scale = 10 V at IN
t=np.arange(len(x)*OS)/(sr*OS)
def g1k(r):
    f,g=precr.ac("out",gain_r=r); return np.interp(1000,f,g)
def job(a):
    name,r,ch=a; xo=resample_poly(x[:,ch],OS,1)
    y=precr.tran(t,xo,tstep=1/(sr*OS),gain_r=r); return name,ch,resample_poly(y,1,OS)
def wr(path,y):
    pcm=np.round(np.clip(y,-1,1)*(2**23-1)).astype('<i4'); b=np.frombuffer(pcm.tobytes(),dtype=np.uint8).reshape(-1,4)[:,:3].tobytes()
    with wave.open(path,"wb") as o: o.setnchannels(y.shape[1]); o.setsampwidth(3); o.setframerate(sr); o.writeframes(b)
if __name__=="__main__":
    g0=g1k(10e3); sets=[("1_clean",10e3)]
    for i,d in enumerate((8,12,16)):
        r=brentq(lambda r:g1k(r)-g0-d,0.1,10e3); sets.append((f"{i+2}_gain_plus{d}dB",r))
    for n,r in sets: print(n,"Rpot %.0f ohm, gain %.2f dB"%(r,g1k(r)))
    with Pool(8) as p: res=p.map(job,[(n,r,ch) for n,r in sets for ch in (0,1)])
    out={}
    for n,ch,y in res: out.setdefault(n,[None,None])[ch]=y
    os.makedirs("out",exist_ok=True); ref=None
    for n,_ in sets:
        y=np.stack(out[n],1); rms=np.sqrt(np.mean((y-y.mean(0))**2)); print(n,"peak V",np.abs(y).max(),"rms V",rms,"dc",y.mean(0))
        wr(f"out/{n}_true_level.wav",y/12.5)
        if ref is None: ref=rms
        wr(f"out/{n}_level_matched.wav",(y-y.mean(0))*ref/rms/12.5)
