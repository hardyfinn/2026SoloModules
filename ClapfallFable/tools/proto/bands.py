import sys, numpy as np, soundfile as sf
FB = [200,315,500,630,800,1000,1250,1600,2000,2500,3150,4000,5000,6300,8000,10000,12500]
def bands(x, sr, win=0.04):
    env = np.abs(x); i0 = int(np.argmax(env > 0.1*env.max())); seg = x[max(0,i0-32): i0+int(win*sr)]
    n = 1<<15; S = np.abs(np.fft.rfft(seg, n))**2; f = np.fft.rfftfreq(n, 1/sr)
    b = np.array([S[(f>c/1.122)&(f<c*1.122)].sum() for c in FB]); return 10*np.log10(b/b.max()+1e-12)
def avg(paths):
    out = []
    for p in paths:
        x, sr = sf.read(p, always_2d=True); out.append(bands(x.mean(1), sr))
    return np.mean(out, 0)
if __name__ == '__main__':
    print('band   ' + ' '.join(f'{c:5d}' for c in FB))
    for label, paths in [a.split('=') for a in sys.argv[1:]]:
        import glob; ps = sorted(sum([glob.glob(p) for p in paths.split(',')], []))
        print(f'{label:6s} ' + ' '.join(f'{v:5.0f}' for v in avg(ps)))
