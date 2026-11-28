import numpy as np, sys, soundfile as sf
from scipy.signal import resample_poly, stft
import matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
outdir=sys.argv[1]; names=sys.argv[2:]
fig,axs=plt.subplots(len(names),1,figsize=(14,3.2*len(names)))
for ax,(i,nm) in zip(np.atleast_1d(axs),enumerate(names)):
    x=np.fromfile(f'raw-{nm}.f32',dtype=np.float32).reshape(-1,2).astype(np.float64)
    assert np.all(np.isfinite(x)), nm
    y=resample_poly(x,1,2,axis=0); sr=48000
    # level riding: slow 2:1 compression on a 150 ms RMS so quiet tails and loud hits sit in one listenable range
    m=np.sqrt((y**2).sum(1)); k=int(0.15*sr); env=np.sqrt(np.convolve(m**2,np.ones(k)/k,'same'))+1e-9
    ref=np.percentile(env,95); g=(np.maximum(env,ref*0.02)/ref)**-0.5; y=y*g[:,None]
    y=np.tanh(y/np.percentile(np.abs(y),99.9)*1.0)*0.92
    n=int(0.02*sr); y[:n]*=np.linspace(0,1,n)[:,None]; y[-n:]*=np.linspace(1,0,n)[:,None]
    mm=y.mean(1); f,t,Z=stft(mm,sr,nperseg=4096,noverlap=3584); P=np.abs(Z)**2
    sf.write(f'{outdir}/{i+1:02d}-{nm}.wav',y,sr,subtype='PCM_24')
    ax.pcolormesh(t,f,10*np.log10(P+1e-12),vmin=10*np.log10(P.max())-75,shading='auto',cmap='magma'); ax.set_yscale('symlog',linthresh=100); ax.set_ylim(25,14000); ax.set_title(nm,fontsize=9,loc='left'); ax.set_ylabel('Hz')
    rms=np.sqrt((y**2).mean()); print(nm,'rms',round(float(rms),3))
plt.tight_layout(); plt.savefig(f'{outdir}/spectrograms.png',dpi=80)
