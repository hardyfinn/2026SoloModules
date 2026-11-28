import numpy as np,sys
y=np.fromfile(sys.argv[1],dtype=np.float32).reshape(-1,2).mean(1).astype(float); sr=48000
st=60/124/4; sps=int(st*sr)
for bar in (int(a) for a in sys.argv[2:]):
    row=[]
    for s in range(16):
        seg=y[(bar*16+s)*sps:(bar*16+s+1)*sps]; e=np.abs(seg); thr=0.02
        dur=(np.nonzero(e>thr)[0][-1]-np.nonzero(e>thr)[0][0])/sr*1000 if (e>thr).any() else 0
        on=np.nonzero(e>thr)[0][0]/sr*1000 if (e>thr).any() else -1
        row.append('%4.2f/%3.0f/%2.0f'%(np.sqrt((seg**2).mean()),dur,on))
    print('bar',bar,' '.join(row))
