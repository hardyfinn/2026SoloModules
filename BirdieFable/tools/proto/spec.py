import sys, numpy as np, soundfile as sf, matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt, glob, os
from scipy.signal import spectrogram
d=sys.argv[1]; fs=sorted(glob.glob(d+"/*.wav")); low=sys.argv[3].split(",") if len(sys.argv)>3 else []
fig,ax=plt.subplots(len(fs),1,figsize=(16,2.4*len(fs)))
for a,f in zip(np.atleast_1d(ax),fs):
    x,sr=sf.read(f); lo=any(k in f for k in low); nper=2048 if lo else 512
    fr,t,S=spectrogram(x,sr,nperseg=nper,noverlap=nper*3//4)
    a.pcolormesh(t,fr,10*np.log10(S+1e-12),vmin=-80,vmax=-15,cmap="magma"); a.set_ylim(0,3000 if lo else 12000); a.set_title(os.path.basename(f),fontsize=9,loc="left")
    e=S.sum(0); act=(e>e.max()*1e-3); dom=fr[S.argmax(0)][act]
    print("%-22s active %3.0f%%  dominant %5.0f..%5.0f Hz (median %5.0f)"%(os.path.basename(f),100*act.mean(),np.percentile(dom,5),np.percentile(dom,95),np.median(dom)))
plt.tight_layout(); plt.savefig(sys.argv[2],dpi=55)
