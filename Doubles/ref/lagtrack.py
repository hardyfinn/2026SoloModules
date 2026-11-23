import numpy as np, wave, sys
def load(p):
    w=wave.open(p); x=np.frombuffer(w.readframes(w.getnframes()),'<i2').reshape(-1,w.getnchannels())/32768; return x.mean(1)
y=load(sys.argv[1]); src=load('/mnt/project-files/beat-juggling/source/baby-cry-8bar.wav')
N=len(src); spb=N/32; a,b,st=float(sys.argv[2]),float(sys.argv[3]),float(sys.argv[4])
for t in np.arange(a,b,st):
    i0=int(t*spb); n=int(st*spb*0.9); seg=y[i0:i0+n]; best=(0,0)
    for lag in range(-1800,1801,3):
        ref=src[(np.arange(n)+i0+lag)%N]; c=np.dot(seg,ref)/np.sqrt(np.dot(seg,seg)*np.dot(ref,ref)+1e-12)
        if c>best[0]: best=(c,lag)
    print('%6.2f corr %.3f lag %+6.2f ms rms %.3f'%(t, best[0], best[1]/44.1, np.sqrt((seg**2).mean())))
