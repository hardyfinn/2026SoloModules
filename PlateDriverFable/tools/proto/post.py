import numpy as np, sys, soundfile as sf
from scipy.signal import resample_poly, stft
import matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
outdir=sys.argv[1]; names=sys.argv[2:]
fig,axs=plt.subplots(len(names),1,figsize=(11,2.3*len(names)))
for ax,(i,nm) in zip(np.atleast_1d(axs),enumerate(names)):
    x=np.fromfile(f'raw-{nm}.f32',dtype=np.float32).reshape(-1,2).astype(np.float64)
    assert np.all(np.isfinite(x)), nm
    y=resample_poly(x,1,2,axis=0); sr=48000
    # level riding: slow 2:1 compression on a 150 ms RMS so the pure tone and the roar sit in one listenable range
    m=np.sqrt((y**2).sum(1)); k=int(0.15*sr); env=np.sqrt(np.convolve(m**2,np.ones(k)/k,'same'))+1e-9
    ref=np.percentile(env,95); g=(np.maximum(env,ref*0.01)/ref)**-0.5; y=y*g[:,None]
    y=np.tanh(y/np.abs(y).max()*1.2)/np.tanh(1.2)*0.8
    mm=y.mean(1); f,t,Z=stft(mm,sr,nperseg=8192,noverlap=6144); P=np.abs(Z)**2
    seg=np.linspace(2,len(t)-3,8).astype(int); desc=[]
    for s_ in seg:
        p=P[:,s_]; pk=f[np.argmax(p)]; n_strong=int((p>p.max()*0.01).sum()); flat=np.exp(np.mean(np.log(p[10:1700]+1e-20)))/np.mean(p[10:1700]+1e-20)
        desc.append(f'{t[s_]:.0f}s:{pk:.0f}Hz/{n_strong}b/{flat:.3f}')
    print(f'{nm:9s}',' '.join(desc))
    sf.write(f'{outdir}/{i+1:02d}-{nm}.wav',y,sr,subtype='PCM_24')
    ax.pcolormesh(t,f,10*np.log10(P+1e-12),vmin=10*np.log10(P.max())-75,shading='auto',cmap='magma'); ax.set_yscale('symlog',linthresh=200); ax.set_ylim(30,12000); ax.set_title(nm,fontsize=9,loc='left'); ax.set_ylabel('Hz')
plt.tight_layout(); plt.savefig(f'{outdir}/spectrograms.png',dpi=70)
