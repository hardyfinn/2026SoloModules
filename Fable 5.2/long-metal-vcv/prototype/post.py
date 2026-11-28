import numpy as np, sys, soundfile as sf
from scipy.signal import stft
import matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
outdir=sys.argv[1]; items=[a.split(':') for a in sys.argv[2:]]   # raw:name:ratio
fig,axs=plt.subplots(len(items),1,figsize=(12,2.8*len(items)))
for ax,(i,(raw,nm,ratio)) in zip(np.atleast_1d(axs),enumerate(items)):
    y=np.fromfile(raw,dtype=np.float32).reshape(-1,2).astype(np.float64); sr=48000
    assert np.all(np.isfinite(y)), nm
    # level riding so the quiet far field and the loud arrival sit in one listenable range
    m=np.sqrt((y**2).sum(1)); k=int(0.25*sr); env=np.sqrt(np.convolve(m**2,np.ones(k)/k,'same'))+1e-12
    ref=np.percentile(env,97); g=(np.maximum(env,ref*0.003)/ref)**(-(1-1/float(ratio))); y=y*g[:,None]
    y=np.tanh(y/np.percentile(np.abs(y),99.95))*0.9
    n=int(0.03*sr); y[:n]*=np.linspace(0,1,n)[:,None]; y[-n:]*=np.linspace(1,0,n)[:,None]
    sf.write(f'{outdir}/{i+1:02d}-{nm}.wav',y,sr,subtype='PCM_16')
    mm=y.mean(1); f,t,Z=stft(mm,sr,nperseg=2048,noverlap=1536); P=np.abs(Z)**2
    ax.pcolormesh(t,f,10*np.log10(P+1e-12),vmin=10*np.log10(P.max())-70,shading='auto',cmap='magma'); ax.set_ylim(0,10000); ax.set_title(f'{i+1:02d}-{nm}',fontsize=9,loc='left'); ax.set_ylabel('Hz')
    print(nm, 'rms by 4s (dB):',' '.join('%.0f'%(10*np.log10((mm[j:j+4*sr]**2).mean()+1e-20)) for j in range(0,len(mm)-sr,4*sr)))
plt.tight_layout(); plt.savefig(f'{outdir}/spectrograms.png',dpi=75)
