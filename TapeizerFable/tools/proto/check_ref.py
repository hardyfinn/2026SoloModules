import sys; sys.path.insert(0,'/mnt/project-files/tape-vcv/')
import numpy as np
from tape_ref import *
fs=int(sys.argv[1]) if len(sys.argv)>1 else 48000
t=np.arange(fs*2)/fs
def lvl(y,f):
    w=np.hanning(len(y)); return 2*abs(np.sum(y*w*np.exp(-2j*np.pi*f*np.arange(len(y))/fs)))/np.sum(w)
F=(40,100,1000,5000,10000,14000)
def resp(name,L,**kw):
    r=[]
    for f in F:
        x=db(L)*np.sin(2*np.pi*f*t)[:,None]; y=deck(x,fs,name,quiet=True,age=0.0,**kw)[fs//2:,0]; r.append(float(round(20*np.log10(lvl(y,f)/db(L)),2)))
    return r
print('fs',fs)
for name in MACHINES:
    print(name, {L:resp(name,L) for L in (-20,0,6)})
print('4TR strength .5',{L:resp('4TR',L,strength=0.5) for L in (-20,6)}, 'strength 0', resp('4TR',6,strength=0.0))
for name,tp in (('4TR','I'),('4TR','II'),('4TR','IV'),('8TR 15','II')):
    mol=(TAPES_REEL if MACHINES[name]['reel'] else TAPES_CASS)[tp]['mol']
    for dl in (-10,0):
        x=db(mol+dl)*np.sin(2*np.pi*315*t)[:,None]; y=deck(x,fs,name,tape=tp,quiet=True,age=0)[fs//2:,0]
        print(name,tp,'HD3 at MOL%+d: %.2f%%'%(dl,100*lvl(y,945)/lvl(y,315)))
x=db(0)*np.sin(2*np.pi*315*t)[:,None]
for b in (-1,0,1): 
    y=deck(x*db(-30),fs,'4TR',quiet=True,age=0,bias=b)[fs//2:,0]; print('bias',b,'gain at -30 dB 315 Hz: %.2f dB'%(20*np.log10(lvl(y,315)/db(-30))), '10k -20:',resp('4TR',-20,bias=b)[4])
for az in (0,0.5,1): print('azimuth',az,resp('4TR',-20,azimuth=az)[3:], 'age1',resp('4TR',-20,azimuth=0) if 0 else '')
for ag in (0.25,1.0): 
    r=[]
    for f in F:
        xx=db(-20)*np.sin(2*np.pi*f*t)[:,None]; y=deck(xx,fs,'4TR',quiet=True,age=ag,unit_seed=1)[fs//2:,0]; r.append(float(round(20*np.log10(lvl(y,f)/db(-20)),2)))
    print('age',ag,r)
print('unit seed1',make_unit(1,3)); print('unit 12345',make_unit(12345,2))
for k in ('X2','B','C'):
    xs=np.stack([0.1*np.sin(2*np.pi*1000*t)*(1+np.sign(np.sin(2*np.pi*2*t)))/2+0.003*np.sin(2*np.pi*6000*t)],1)
    e=nr_encode(xs,k,fs); d=nr_decode(e,k,fs); print(k,'roundtrip dB',round(20*np.log10(np.std(d-xs)/np.std(xs)),1))
    row={}
    for L in (-60,-40,-20,0):
        row[L]=[float(round(20*np.log10(np.std(nr_encode(db(L)*np.sin(2*np.pi*f*t)[:,None],k,fs)[fs:])/np.std(db(L)*np.sin(2*np.pi*f*t))),2)) for f in (200,1000,5000)]
    print(k,'encode gain dB at 200/1k/5k',row)
# latency
imp=np.zeros((fs,1)); imp[100]=1e-3; y=deck(imp,fs,'4TR',quiet=True,age=0)[:,0]; print('latency samples',np.argmax(abs(y))-100, latency(fs))
