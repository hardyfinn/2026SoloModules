# Renders the held-back validation mix from the test file through the SPICE model at five settings.
import precr, numpy as np, json, wave
from scipy.signal import resample_poly
from multiprocessing import Pool
SR=48000; OS=4
m=json.load(open("../test-signal-map.json")); s=[x for x in m["sections"] if x["name"]=="validation_mix_-1"][0]
w=wave.open("../mackie-test-signal.wav"); raw=np.frombuffer(w.readframes(w.getnframes()),dtype=np.uint8).reshape(-1,3)
x=(raw[:,0].astype(np.int32)|(raw[:,1].astype(np.int32)<<8)|(raw[:,2].astype(np.int8).astype(np.int32)<<16))/2**23
x=x[s["start"]:s["start"]+s["length"]]*10.0   # assume 0 dBFS = 10 V at the NT jack
xo=resample_poly(x,OS,1); t=np.arange(len(xo))/(SR*OS)
SET=[("1 clean: GAIN min, LEVEL 5",dict(gain_r=10e3)),
     ("2 GAIN about 11 o'clock",dict(gain_r=1e3)),
     ("3 GAIN about 2 o'clock",dict(gain_r=100)),
     ("4 GAIN max",dict(gain_r=0)),
     ("5 GAIN min, LEVEL 10 (output stage clipping)",dict(gain_r=10e3,level=1.0))]
def go(a):
    name,kw=a; y=precr.tran(t,xo,tstep=1/(SR*OS),**kw); y=resample_poly(y,1,OS); return name,y
if __name__=="__main__":
    with Pool(5) as p: res=p.map(go,SET)
    out=[]
    for name,y in res:
        y=y-np.mean(y); print(name,"peak V %.2f rms V %.2f"%(np.max(np.abs(y)),np.sqrt(np.mean(y**2))))
        np.save("preview_"+name[0]+".npy",y)
        y=y/np.sqrt(np.mean(y**2))*10**(-18/20); out+= [y,np.zeros(SR//2)]
    y=np.concatenate(out); pk=np.max(np.abs(y)); 
    if pk>0.98: y=y*0.98/pk
    pcm=np.round(y*(2**23-1)).astype('<i4'); b=np.frombuffer(pcm.tobytes(),dtype=np.uint8).reshape(-1,4)[:,:3].tobytes()
    with wave.open("../precr-model-preview.wav","wb") as o: o.setnchannels(1); o.setsampwidth(3); o.setframerate(SR); o.writeframes(b)
    print("done",len(y)/SR,"s peak",pk)
