import numpy as np, sys, soundfile as sf
from scipy.signal import stft
import matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
outdir=sys.argv[1]; items=[a.split(':') for a in sys.argv[2:]]   # raw:name:ratio
fig,axs=plt.subplots(len(items),1,figsize=(13,2.8*len(items)))
for ax,(i,(raw,nm,ratio)) in zip(np.atleast_1d(axs),enumerate(items)):
    y=np.fromfile(raw,dtype=np.float32).reshape(-1,2).astype(np.float64); sr=48000
    assert np.all(np.isfinite(y)), nm
    # gentle level riding so quiet hum and loud arcs sit in one listenable range, then a soft clip
    m=np.sqrt((y**2).sum(1)); k=int(0.25*sr); env=np.sqrt(np.convolve(m**2,np.ones(k)/k,'same'))+1e-12
    ref=np.percentile(env,97); g=(np.maximum(env,ref*0.01)/ref)**(-(1-1/float(ratio))); y=y*g[:,None]
    y=np.tanh(y/np.percentile(np.abs(y),99.9))*0.9
    n=int(0.02*sr); y[:n]*=np.linspace(0,1,n)[:,None]; y[-n:]*=np.linspace(1,0,n)[:,None]
    sf.write(f'{outdir}/{nm}.wav',y,sr,subtype='PCM_16')
    mm=y.mean(1); f,t,Z=stft(mm,sr,nperseg=2048,noverlap=1536); Pw=np.abs(Z)**2
    ax.pcolormesh(t,f,10*np.log10(Pw+1e-12),vmin=10*np.log10(Pw.max())-70,shading='auto',cmap='magma'); ax.set_ylim(0,12000); ax.set_title(nm,fontsize=9,loc='left'); ax.set_ylabel('Hz')
    S=Pw.sum(0)+1e-20; cen=(f[:,None]*Pw).sum(0)/S
    print(nm,'\n  rms/2s dB:',' '.join('%.0f'%(10*np.log10((mm[j:j+2*sr]**2).mean()+1e-20)) for j in range(0,len(mm)-sr,2*sr)),
          '\n  centroid/2s Hz:',' '.join('%.0f'%cen[(t>=j)&(t<j+2)].mean() for j in range(0,int(t[-1]),2)),
          '\n  crest dB: %.1f'%(20*np.log10(np.abs(mm).max()/np.sqrt((mm**2).mean()))))
    if 'sing' in nm or 'kit' in nm:
        f2,t2,Z2=stft(mm,sr,nperseg=8192,noverlap=7168); fig2=plt.figure(figsize=(14,4)); a2=fig2.gca(); P2=np.abs(Z2)**2
        a2.pcolormesh(t2,f2,10*np.log10(P2+1e-12),vmin=10*np.log10(P2.max())-60,shading='auto',cmap='magma'); a2.set_ylim(0,1500); fig2.savefig(f'{outdir}/zoom-{nm}.png',dpi=70); plt.close(fig2)
plt.tight_layout(); plt.savefig(f'{outdir}/spectrograms.png',dpi=70)
