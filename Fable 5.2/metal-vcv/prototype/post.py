import numpy as np, sys, soundfile as sf
from scipy.signal import resample_poly, stft
import matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
names=sys.argv[2:]; outdir=sys.argv[1]
fig,axs=plt.subplots(len(names),1,figsize=(11,2.3*len(names)))
for ax,(i,nm) in zip(np.atleast_1d(axs),enumerate(names)):
    x=np.fromfile(f'raw-{nm}.f32',dtype=np.float32).reshape(-1,2).astype(np.float64)
    assert np.all(np.isfinite(x)), nm
    y=resample_poly(x,1,2,axis=0); pk=np.abs(y).max()
    # gentle level: normalise to -3 dBFS peak, soft-limit nothing
    y=y/pk*0.707
    m=y.mean(1); n=len(m); sr=48000
    f,t,Z=stft(m,sr,nperseg=4096,noverlap=3072); P=np.abs(Z)**2
    cen=(P*f[:,None]).sum(0)/(P.sum(0)+1e-20); rms=np.sqrt(P.sum(0))
    seg=np.linspace(0,len(t)-1,7).astype(int)
    print(f'{nm:12s} {n/sr:5.1f}s peak {pk:9.3g} centroid Hz at 7 points:',' '.join(f'{cen[s]:6.0f}' for s in seg),'| level dB:',' '.join(f'{20*np.log10(rms[s]/rms.max()+1e-9):4.0f}' for s in seg))
    fn=f'{outdir}/{i+1:02d}-{nm}.wav'; sf.write(fn,y,sr,subtype='PCM_24')
    ax.pcolormesh(t,f,10*np.log10(P+1e-12),vmin=10*np.log10(P.max())-80,shading='auto',cmap='magma'); ax.set_yscale('symlog',linthresh=200); ax.set_ylim(40,20000); ax.set_title(nm,fontsize=9,loc='left'); ax.set_ylabel('Hz')
plt.tight_layout(); plt.savefig(f'{outdir}/spectrograms.png',dpi=70)
