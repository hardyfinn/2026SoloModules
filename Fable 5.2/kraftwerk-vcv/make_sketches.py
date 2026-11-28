# Sound sketches for the Kraftwerk-style module proposal. Fable, 2026-11-23.
# Rough prototypes of the DSP ideas, not the final models.
import numpy as np, os
from scipy.signal import lfilter, butter, sosfilt
from scipy.io import wavfile
SR=48000; OUT=os.path.join(os.path.dirname(os.path.abspath(__file__)),'sketches')
rng=np.random.default_rng(7)
def save(name,x):
    x=np.asarray(x,float); x=x/max(1e-9,np.max(np.abs(x)))*0.8
    wavfile.write(os.path.join(OUT,name),SR,(x*32767).astype(np.int16)); return x
def reson(x,f,q):                       # two-pole resonator, f may be array
    f=np.broadcast_to(f,x.shape); y=np.zeros_like(x); y1=y2=0.0
    for n in range(len(x)):
        r=np.exp(-np.pi*f[n]/q/SR); a1=2*r*np.cos(2*np.pi*f[n]/SR); a2=-r*r
        y0=(1-r)*x[n]+a1*y1+a2*y2; y[n]=y0; y2=y1; y1=y0
    return y
def bp(x,lo,hi,o=2): return sosfilt(butter(o,[lo,hi],'bp',fs=SR,output='sos'),x)
def hp(x,f,o=2): return sosfilt(butter(o,f,'hp',fs=SR,output='sos'),x)
def lp(x,f,o=2): return sosfilt(butter(o,f,'lp',fs=SR,output='sos'),x)
def env(n,t): return np.exp(-np.arange(n)/(t*SR))

# ---- BLECH: preset-rhythm-box voices (struck damped resonators + gated noise) ----
def pulse(n,w=0.001):
    p=np.zeros(n); p[:int(w*SR)]=1; return p
def tdrum(f,decay,n,bend=0.0,vel=1.0):   # bridged-T style: pulse rings a damped sine
    t=np.arange(n)/SR; ff=f*(1+bend*np.exp(-t/0.012))
    ph=2*np.pi*np.cumsum(ff)/SR
    return vel*np.sin(ph)*np.exp(-t/decay)+0.3*vel*lp(pulse(n),2000)
def noise(n): return rng.standard_normal(n)
def snare(n,vel=1.0):
    return vel*(0.6*tdrum(190,0.06,n)+0.9*bp(noise(n),1500,7000)*env(n,0.07))
def hat(n,dec=0.03,vel=1.0):             # metallic: 6 square oscillators + noise, high-passed
    t=np.arange(n)/SR; m=sum(np.sign(np.sin(2*np.pi*f*t)) for f in (2050,3010,4120,5350,6800,8230))
    return 0.3*vel*hp(0.4*m/6+noise(n),6500,4)*env(n,dec)
def clave(n): return tdrum(2400,0.018,n)
def zap(n,f0=6000,f1=90,t=0.03,q=18,vel=1.0):   # ZAP: resonant filter pinged, cutoff swept down
    tt=np.arange(n)/SR; f=f1+(f0-f1)*np.exp(-tt/t)
    # filter at the edge of self-oscillation: a swept sine whose ring time is set by resonance
    y=np.sin(2*np.pi*np.cumsum(f)/SR)*np.exp(-tt/(0.004*q))+0.15*hp(noise(n),3000)*env(n,0.004)
    return vel*y
def place(buf,x,t):
    i=int(t*SR); x=x[:len(buf)-i]; buf[i:i+len(x)]+=x
bpm=120; s16=60/bpm/4
def pattern(bars,rows):
    buf=np.zeros(int(bars*16*s16*SR)+SR)
    for fn,pat in rows:
        for b in range(bars):
            for i,c in enumerate(pat):
                if c!='.': place(buf,fn(1.0 if c=='X' else 0.55),(b*16+i)*s16)
    return buf
N=int(0.5*SR)
rhythm_box=pattern(4,[
 (lambda v:tdrum(62,0.12,N,1.5,v),'X.....X.X.......'),
 (lambda v:snare(N,v),            '....X.......X...'),
 (lambda v:hat(N,0.025,v),        'x.x.x.x.x.x.x.X.'),
 (lambda v:tdrum(330,0.05,N,0,v), '.......x..x.....'),
 (lambda v:clave(N)*v,            '...x.....x....x.')])
save('01-blech-rhythm-box-voices.wav',rhythm_box)
electro=pattern(4,[
 (lambda v:tdrum(55,0.16,N,2.5,v),            'X.....X...X.....'),
 (lambda v:zap(N,7000,180,0.02,10,v)+0.5*snare(N,v),'....X.......X...'),
 (lambda v:hat(N,0.02,v),                     'xxx.xxx.xxx.xxxx'),
 (lambda v:zap(N,3000,400,0.05,25,v)*0.7,     '..x....x.x....x.')])
save('02-zap-electro-kit.wav',electro)
zs=np.zeros(int(6*SR))
for i,(f0,f1,t,q) in enumerate([(8000,60,0.01,6),(8000,60,0.04,12),(5000,200,0.08,25),(2000,900,0.15,40),(300,3000,0.06,20),(12000,40,0.2,30)]):
    for k in range(4): place(zs,zap(N,f0,f1,t,q),i+k*0.22)
save('03-zap-sweep-of-settings.wav',zs)

# ---- TAKT: 16-step quantised sequence, transposed by a slow "keyboard" ----
def saw(f,n,ph0=0):
    ph=(ph0+np.cumsum(np.broadcast_to(f,(n,)))/SR)%1; return 2*ph-1
def ladder(x,fc,res):                    # crude 4-pole with feedback
    y=np.zeros_like(x); s=np.zeros(4); fc=np.broadcast_to(fc,x.shape)
    for n in range(len(x)):
        g=1-np.exp(-2*np.pi*fc[n]/SR); u=np.tanh(x[n]-res*s[3])
        for k in range(4): s[k]+=g*(u-s[k]); u=s[k]
        y[n]=s[3]
    return y
steps=[0,12,0,7,0,12,3,10, 0,12,0,7,5,12,3,15]; gates='X.XXX.XXX.XXXX.X'
trans=[0,0,3,-2]; ns=int(s16*SR); seq=[]
for bar in range(8):
    for i,st in enumerate(steps):
        f=55*2**((st+trans[bar//2])/12); n=ns
        o=saw(f,n)+saw(f*1.004,n)
        e=env(n,0.09) if gates[i]=='X' else np.zeros(n)
        seq.append(ladder(o*e,200+2600*e,2.2)*1.0)
seq=np.concatenate(seq)
save('04-takt-sequence-transposed.wav',seq)

# ---- ORCHESTER: optical-disc choir (looped scanned waveform, wobble, band limit, hiss) ----
def choir_note(f,dur):
    n=int(dur*SR); t=np.arange(n)/SR
    wob=1+0.004*np.sin(2*np.pi*0.9*t+rng.uniform(0,6))+0.0015*np.sin(2*np.pi*7.3*t)
    src=saw(f*wob,n,rng.uniform())+0.15*noise(n)
    y=sum(a*reson(src,fm,qq) for fm,qq,a in ((600,6,1.0),(1040,9,0.6),(2700,10,0.35),(3400,10,0.2)))
    am=1+0.25*np.sin(2*np.pi*f*wob.mean()/16*t)   # disc-rate flutter in level
    a=np.minimum(1,t/0.03)*np.minimum(1,(dur-t)/0.05)
    return lp(hp(y*am,150),4500,4)*a
ch=np.zeros(int(13*SR))
for b,chord in enumerate([(220,261.6,329.6),(196,246.9,293.7),(174.6,220,261.6),(164.8,207.7,246.9)]):
    for f in chord: place(ch,choir_note(f,3.1),b*3)
ch=ch+0.003*np.max(np.abs(ch))*hp(noise(len(ch)),2000)
save('05-orchester-optical-choir.wav',ch)

# ---- SPRECHER: formant speech from a phoneme list ----
PH={ # F1,F2,F3, voiced amp, noise amp, noise centre, ms
 'a':(730,1090,2440,1,0,0,150),'i':(270,2290,3010,1,0,0,130),'u':(300,870,2240,1,0,0,130),
 'e':(530,1840,2480,1,0,0,130),'o':(570,840,2410,1,0,0,140),'@':(500,1500,2500,1,0,0,80),
 'n':(250,1400,2500,.5,0,0,80),'m':(250,1000,2200,.5,0,0,80),'w':(300,700,2200,.7,0,0,60),
 'r':(420,1300,1600,.8,0,0,70),'l':(360,1300,2700,.7,0,0,60),'v':(300,1100,2300,.5,.2,3000,60),
 's':(300,1800,2600,0,.7,6500,110),'f':(300,1400,2500,0,.4,4500,90),'T':(300,1600,2600,0,.4,5000,80),
 't':(300,1800,2600,0,.9,4500,30),'k':(300,1500,2500,0,.9,2200,35),'ts':(300,1800,2600,0,.9,6000,90),
 'x':(400,1400,2500,0,.5,2500,90),'_':(400,1400,2500,0,0,0,60),'d':(250,1700,2600,.4,.2,3500,30)}
def speak(words,pitch=110,rate=1.0,flat=True,gap=0.18):
    F=[];V=[];Nn=[];Nc=[]
    for w in words:
        for p in w+['_']:
            f1,f2,f3,v,na,nc,ms=PH[p]; n=int(ms/1000*SR/rate)
            F+= [[f1,f2,f3]]*n; V+=[v]*n; Nn+=[na]*n; Nc+=[nc or 3000]*n
        n=int(gap*SR); F+=[F[-1]]*n; V+=[0]*n; Nn+=[0]*n; Nc+=[3000]*n
    F=np.array(F,float); k=int(0.025*SR); ker=np.hanning(k); ker/=ker.sum()
    sm=lambda a:np.convolve(np.pad(np.asarray(a,float),(k//2,k//2),'edge'),ker,'valid')[:len(V)]
    F=np.stack([sm(F[:,i]) for i in range(3)],1); V=sm(V); Nn=sm(Nn); n=len(V)
    f0=pitch*np.ones(n) if flat else pitch*(1+0.08*np.sin(np.arange(n)/SR*2.5))
    ph=np.cumsum(f0)/SR; glot=np.diff(np.floor(ph),prepend=0)          # impulse train
    glot=lfilter([1],[1,-0.97],glot)*V*20
    y=sum(a*reson(glot,F[:,i],q) for i,(q,a) in enumerate(((8,1),(12,.7),(14,.4))))
    nz=noise(n)*Nn; y+=0.5*(reson(nz,np.array(Nc,float),3)+0.3*nz)
    return y
de=[['a','i','n','ts'],['ts','v','a','i'],['d','r','a','i'],['f','i','r']]
en=[['w','a','n'],['t','u'],['T','r','i'],['f','o','r']]
sp=np.concatenate([speak(de,100),speak(en,100),speak(de,150,1.3),speak(en,75,0.8)])
save('06-sprecher-counting-formant-voice.wav',sp)
# 8 kHz, coarse "speech chip" mode: decimate, quantise, hold
def chip(x,rate=8000,bits=6):
    x=lp(x,rate*0.45,6); k=SR//rate; y=np.repeat(x[::k],k)[:len(x)]
    y/=np.max(np.abs(y)); q=2**(bits-1); return np.round(y*q)/q
save('07-sprecher-speech-chip-mode.wav',chip(np.concatenate([speak(de,95),speak(en,95)])))

# ---- VOKODER: 20-band channel vocoder with voiced/unvoiced switch ----
def vocode(mod,car,bands=20,lo=120,hi=7500,rel=0.012):
    edges=np.geomspace(lo,hi,bands+1); out=np.zeros(len(mod)); a=np.exp(-1/(rel*SR))
    for i in range(bands):
        m=np.abs(bp(mod,edges[i],edges[i+1])); e=lfilter([1-a],[1,-a],m)
        out+=bp(car,edges[i],edges[i+1])*e
    hfe=lfilter([1-a],[1,-a],np.abs(hp(mod,4000))); lfe=lfilter([1-a],[1,-a],np.abs(lp(mod,1500)))
    uv=(hfe>1.2*lfe)*hfe                                # sibilance passes as noise
    return out+0.5*hp(noise(len(mod)),4000)*uv
m=np.concatenate([speak(de,110,0.8,gap=0.3),speak(en,110,0.8,gap=0.3)])
n=len(m); half=n//2
car=np.concatenate([sum(saw(f,half) for f in (110,164.8,220,277.2)),sum(saw(f,n-half) for f in (98,146.8,196,246.9))])
save('08-vokoder-chord-carrier.wav',vocode(m,car))
save('09-vokoder-mono-melody-carrier.wav',vocode(m,np.concatenate([saw(55*2**(s/12),n//8+1) for s in (0,0,3,5,7,7,5,3)])[:n]))

# ---- RECHNER: toy beeps ----
def beep(f,d,boing=0):
    n=int(d*SR); t=np.arange(n)/SR; ff=f*(1+boing*np.exp(-t/0.05)*np.sin(2*np.pi*14*t))
    return np.sign(np.sin(2*np.pi*np.cumsum(ff)/SR))*np.minimum(1,(d-t)/0.004)
tb=np.zeros(int(8.5*SR)); mel=[76,76,79,76,83,81,79,76, 72,72,76,72,79,77,76,72]
for r in range(2):
    for i,mn in enumerate(mel): place(tb,beep(440*2**((mn-69)/12),0.09),(r*16+i)*0.25)
for i in range(4): place(tb,beep(300,0.3,0.5)*0.8,i*2+0.125)
save('10-rechner-toy-beeps.wav',lp(hp(tb,300),5000))

# ---- gallery: everything together ----
L=len(seq); g=np.zeros(L)
def fit(x):
    x=np.tile(x,int(np.ceil(L/len(x))))[:L]; return x/np.max(np.abs(x))
g+=0.9*fit(electro[:int(4*16*s16*SR)])+0.6*fit(seq)
voc=vocode(m,np.resize(sum(saw(f,SR*4) for f in (110,164.8,220)),len(m)))
place(g,0.6*voc/np.max(np.abs(voc)),4*16*s16)
save('00-gallery-all-together.wav',g)
print('ok')
