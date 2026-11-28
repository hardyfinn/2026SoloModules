# Generates the Mackie CR-1604 test signal (48 kHz, 24-bit, mono) and a section map.
import numpy as np, json, wave
SR=48000; rng=np.random.default_rng(1604)
db=lambda d:10**(d/20)
out=[]; secs=[]; pos=0
def add(name,x,gap=0.3,**meta):
    global pos
    x=np.asarray(x,float)
    secs.append(dict(name=name,start=pos,length=len(x),**meta))
    out.append(x); pos+=len(x)
    g=np.zeros(int(gap*SR)); out.append(g); pos+=len(g)
def fade(x,ms=10):
    n=int(ms*SR/1000); w=np.ones(len(x)); r=0.5-0.5*np.cos(np.pi*np.arange(n)/n)
    w[:n]=r; w[-n:]=r[::-1]; return x*w
t_=lambda d:np.arange(int(d*SR))/SR
sine=lambda f,d:np.sin(2*np.pi*f*t_(d))
def click():
    x=np.zeros(int(0.5*SR)); x[:48]=db(-6)*np.sin(2*np.pi*4000*np.arange(48)/SR)*np.hanning(48); return x
def ess(d,f0=20,f1=20000):
    t=t_(d); K=d/np.log(f1/f0); return fade(np.sin(2*np.pi*f0*K*(np.exp(t/K)-1)),20)

out.append(np.zeros(SR)); pos+=SR
add("marker_start",np.concatenate([click()]*3),gap=0.5)
add("ref_1k_-20",fade(sine(1000,5))*db(-20),gap=1)
for L in (-40,-20,-6,-1):
    add(f"sweep_{L}",ess(8)*db(L),gap=1.0,level=L,f0=20,f1=20000,dur=8)
levels=list(range(-50,-1,2))+[-1]
for f in (50,1000,5000):
    add(f"steps_{f}",np.concatenate([fade(sine(f,0.4))*db(L) for L in levels]),freq=f,levels=levels,step=0.4)
tri=2/np.pi*np.arcsin(np.sin(2*np.pi*30*t_(3)))
add("triangle_30",fade(tri,20)*db(-1))
t=t_(10); add("ramp_220",fade(np.sin(2*np.pi*220*t)*db(-60+59*t/10),5),from_db=-60,to_db=-1)
il=list(range(-30,-1,3))+[-1]
add("imd_60_7000",np.concatenate([fade(0.8*sine(60,0.5)+0.2*sine(7000,0.5))*db(L) for L in il]),levels=il,step=0.5)
add("imd_19k_20k",np.concatenate([fade(0.5*sine(19000,0.5)+0.5*sine(20000,0.5))*db(L) for L in il]),levels=il,step=0.5)
for ms in (5,20,100):
    hot=sine(1000,ms/1000)*db(-1); quiet=fade(sine(3000,0.3),2)*db(-40)
    add(f"recovery_{ms}ms",np.concatenate([hot,quiet]),hot_ms=ms)
add("square_5",fade(np.sign(sine(5,2)),1)*db(-6))
# pink noise
w=rng.standard_normal(5*SR); F=np.fft.rfft(w); k=np.arange(len(F)); k[0]=1; p=np.fft.irfft(F/np.sqrt(k),len(w))
p=p/np.sqrt(np.mean(p**2))*db(-20); add("pink_-20rms",fade(np.clip(p,-0.9,0.9),20),gap=1)

# ---- drum machine style sounds ----
def onepole_lp(x,fc):
    a=np.exp(-2*np.pi*fc/SR); y=np.zeros_like(x); s=0
    for i,v in enumerate(x): s=(1-a)*v+a*s; y[i]=s
    return y
def hp(x,fc): return x-onepole_lp(x,fc)
def kick808(d=0.9):
    t=t_(d); f=48+110*np.exp(-t/0.012); ph=2*np.pi*np.cumsum(f)/SR
    return np.sin(ph)*np.exp(-t/0.28)
def kick909(d=0.45):
    t=t_(d); f=52+260*np.exp(-t/0.03); ph=2*np.pi*np.cumsum(f)/SR
    body=np.tanh(1.8*np.sin(ph))*np.exp(-t/0.11)
    ck=hp(rng.standard_normal(len(t)),3000)*np.exp(-t/0.003)*0.4
    return body+ck
def snare(d=0.3):
    t=t_(d); tone=(np.sin(2*np.pi*185*t)+0.6*np.sin(2*np.pi*330*t))*np.exp(-t/0.05)
    nz=hp(rng.standard_normal(len(t)),1500)*np.exp(-t/0.09); return 0.6*tone+0.5*nz
def hat(d=0.08,dec=0.02):
    t=t_(d); fs=[205.3,304.4,369.6,522.7,540,800]
    m=sum(np.sign(np.sin(2*np.pi*f*t*1.0)) for f in fs)/6
    return hp(hp(m,7000),7000)*np.exp(-t/dec)
def clap(d=0.3):
    t=t_(d); nz=hp(onepole_lp(rng.standard_normal(len(t)),4000),800)
    env=sum(np.where(t>=o,np.exp(-(t-o)/0.006),0) for o in (0,0.011,0.022))*0.6+np.where(t>=0.03,np.exp(-(t-0.03)/0.07),0)
    return nz*env
def norm(x): return x/np.max(np.abs(x))
hits=dict(kick808=norm(kick808()),kick909=norm(kick909()),snare=norm(snare()),hat_closed=norm(hat()),hat_open=norm(hat(0.4,0.12)),clap=norm(clap()))
for n,h in hits.items():
    for L in (-18,-12,-6,-1):
        add(f"hit_{n}_{L}",h*db(L),gap=0.15,level=L)
BPM=126; S=60/BPM/4
def pattern(bars,rows,swing=0.0):
    n=int(bars*16*S*SR)+SR; y=np.zeros(n)
    for name,(pat,g) in rows.items():
        h=hits[name]
        for i in range(bars*16):
            if pat[i%len(pat)]=='x':
                o=int((i*S+(swing*S if i%2 else 0))*SR); y[o:o+len(h)]+=g*h[:n-o]
    return y[:int(bars*16*S*SR)+int(0.4*SR)]
pa=dict(kick909=("x...x...x...x...",1.0),hat_closed=("..x...x...x...x.",0.35),hat_open=("..............x.",0.3),clap=("....x.......x...",0.55),snare=("............x..x",0.0))
pa.pop("snare")
A=norm(pattern(2,pa))
for L in (-12,-6,-1): add(f"drums_A_{L}",A*db(L),level=L)
def acid(bars,notes,cut=600,res=0.85,seed=0):
    n=int(bars*16*S*SR); y=np.zeros(n+SR)
    lp=bp=0.0; ph=0.0; r=np.random.default_rng(seed)
    for i in range(bars*16):
        nt=notes[i%len(notes)]
        if nt is None: continue
        f=55*2**(nt/12); acc=(i%4==2); L=int(S*SR*0.9); o=int(i*S*SR)
        for k in range(L):
            ph=(ph+f/SR)%1; saw=2*ph-1
            e=np.exp(-k/(0.08*SR)); fc=cut*(1+ (5 if acc else 2.5)*e)
            g=2*np.sin(np.pi*min(fc,10000)/SR); q=1-res
            lp+=g*bp; hpv=saw-lp-2*q*bp; bp+=g*hpv
            y[o+k]+=lp*np.exp(-k/(0.25*SR))*(1.3 if acc else 1)
    return y[:n+int(0.3*SR)]
notes=[0,0,12,None,0,3,None,0,10,0,None,12,0,7,None,3]
B=norm(acid(2,notes))
for L in (-12,-6,-1): add(f"acid_{L}",B*db(L),level=L)
k8=norm(pattern(2,dict(kick808=("x.....x...x.....",1.0),hat_closed=("x.x.x.x.x.x.x.x.",0.25),snare=("....x.......x...",0.6))))
for L in (-6,-1): add(f"drums_808_{L}",k8*db(L),level=L)
# held-back validation: different pattern, drums + bass + stab, not used for fitting
pv=dict(kick909=("x...x...x...x..x",1.0),hat_closed=("x.xxx.x.x.xxx.x.",0.3),hat_open=("..x...x...x...x.",0.28),clap=("....x.......x...",0.5),snare=(".......x......x.",0.3))
dv=pattern(4,pv,swing=0.12); bv=acid(4,[0,None,0,7,None,0,12,0,None,5,0,None,0,3,10,None],cut=450,res=0.9)
n=min(len(dv),len(bv)); tt=np.arange(n)/SR
stab=sum(np.sign(np.sin(2*np.pi*f*tt)) for f in (220*2**(3/12),220*2**(7/12),220*2**(10/12)))
env=np.zeros(n)
for i in range(4*16):
    if i%16 in (3,10): o=int(i*S*SR); L=min(int(0.2*SR),n-o); env[o:o+L]=np.exp(-np.arange(L)/(0.06*SR))
V=norm(dv[:n]+0.6*norm(bv[:n])+0.12*onepole_lp(stab*env,2500))
for L in (-9,-1): add(f"validation_mix_{L}",V*db(L),level=L,held_back=True,gap=0.5)
add("marker_end",np.concatenate([click()]*3),gap=1)
y=np.concatenate(out); assert np.max(np.abs(y))<=1.0, np.max(np.abs(y))
pcm=np.round(y*(2**23-1)).astype('<i4')
b=pcm.astype('<i4').tobytes(); b=np.frombuffer(b,dtype=np.uint8).reshape(-1,4)[:,:3].tobytes()
with wave.open("mackie-test-signal.wav","wb") as w:
    w.setnchannels(1); w.setsampwidth(3); w.setframerate(SR); w.writeframes(b)
json.dump(dict(sr=SR,samples=len(y),sections=secs),open("test-signal-map.json","w"),indent=1)
print(len(y)/SR, np.max(np.abs(y)))
for s in secs:
    print(f"{s['start']/SR:7.1f}  {s['length']/SR:5.1f}  {s['name']}")
