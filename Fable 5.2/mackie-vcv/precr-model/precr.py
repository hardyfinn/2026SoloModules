# SPICE model of the Feedback Modules PRE CR (schematic 6/14/2016, build manual 1.3) driven from python via ngspice.
import subprocess, numpy as np, os, tempfile
MODELS = r"""
.model BC550C NPN(IS=7.049f BF=493 NF=1.005 VAF=62.8 IKF=0.08 ISE=1.3p NE=2.2 BR=7 RB=10 RC=0.9 CJE=11.5p CJC=5.4p TF=0.41n TR=50n)
.model BC560C PNP(IS=1.02f BF=500 VAF=50 IKF=0.08 ISE=5p NE=1.6 BR=8 RB=10 RC=1 CJE=12p CJC=10p TF=0.5n TR=60n)
.model D4148 D(IS=2.52n RS=0.568 N=1.752 CJO=4p M=0.4 TT=20n BV=100)
.model D5819 D(IS=31.7u RS=0.051 N=1.373 CJO=110p M=0.35 BV=40)
.model DCL D(IS=1e-14 N=1)
* RC4560-like macro model: GBW 10 MHz, slew 4 V/us, output swings to about 1 V from each rail
.subckt OPA p n o vp vm
Rin p n 5meg
B1 0 x I=64u*tanh(v(p,n)*1m/64u)
R1 x 0 100meg
C1 x 0 15.9p
Vhp vp xp 1.6
Vhm xm vm 1.6
D1 x xp DCL
D2 xm x DCL
Eo oi 0 x 0 1
Ro oi o 50
.ends
"""
def netlist(gain_r=10e3, hi=0.5, low=0.5, level=0.5, src="Vin in 0 dc 0 ac 1"):
    """gain_r: resistance of POT1 in circuit (10k = min gain, 0 = max). hi/low/level: wiper 0..1 (1 = clockwise)."""
    e=1e-3; hi=min(max(hi,e),1-e); low=min(max(low,e),1-e); level=min(max(level,e),1-e)
    return f"""PRE CR
{MODELS}
Vp12 p12 0 12
Vm12 m12 0 -12
D5 p12 pa D5819
R23 pa vp 10
C16 vp 0 100u
D6 vm ma D5819
R24 ma m12 10
C17 vm 0 100u
{src}
C1 in n1 22u
R1 n1 b1 4.7k
R2 b1 0 3.9k
C2 b1 0 1n
D1 b1 vp D4148
D2 vm b1 D4148
Q1 c1 b1 e1 BC560C
R6 vp e1 2.49k
Q3 e1 c1 n3 BC550C
R7 c1 n3 510
R9 n3 vm 820
R12 n3 0 3.9k
C3 0 x3 22u
R3 x3 b2 4.7k
R4 b2 0 3.9k
C4 b2 0 1n
D3 b2 vp D4148
D4 vm b2 D4148
Q2 c2 b2 e2 BC560C
R5 vp e2 2.49k
Q4 e2 c2 n4 BC550C
R8 c2 n4 510
R10 n4 vm 820
R13 n4 pre 3.9k
R11 e1 g1 22
C5 g1 g2 470u
Rpot1 g2 e2 {gain_r+0.5}
XU1A n3 n4 pre vp vm OPA
C6 pre eqin 22u
C7 eqin h3 6.8n
Rp2a h3 hw {50e3*(1-hi)}
Rp2b hw h1 {50e3*hi}
C8 h1 eqout 6.8n
R14 hw hx 3.6k
Rdc1 hw 0 1e10
Rdc2 eqin 0 1e10
Rdc3 sum 0 1e10
C11 hx sum 10n
R15 eqin l3 6.8k
Rp3a l3 lw {50e3*(1-low)}
Rp3b lw l1 {50e3*low}
C9 l3 lw 33n
C10 lw l1 33n
R16 l1 eqout 6.8k
R17 lw sum 10k
C12 sum inv 22u
R18 inv eqout 1meg
C13 inv eqout 100p
XU2A 0 inv eqout vp vm OPA
R19 eqout la 5.6k
Rp4a la lwp {25e3*level}
Rp4b lwp l3p {25e3*(1-level)}
C14 lwp 0 470u
R20 l3p fbn 430
R21 fbn outo 7.5k
C15 fbn outo 100p
XU2B la fbn outo vp vm OPA
R22 outo out 100
Rload out 0 100k
"""
def run(net, cmds, outs):
    d=tempfile.mkdtemp(); f=os.path.join(d,"c.cir"); o=os.path.join(d,"o.txt")
    open(f,"w").write(net+".control\nset wr_singlescale\nset wr_vecnames\nset numdgt=9\n"+cmds+f"\nwrdata {o} {outs}\n.endc\n.end\n")
    r=subprocess.run(["ngspice","-b",f],capture_output=True,text=True)
    if not os.path.exists(o): raise RuntimeError(r.stdout[-3000:]+r.stderr[-2000:])
    return np.loadtxt(o,skiprows=1)
def ac(node="out",**kw):
    a=run(netlist(**kw),"ac dec 40 5 40k",f"vdb({node}) vp({node})"); return a[:,0],a[:,1]
def tran(t, v, node="out", tstep=None, **kw):
    """drive with samples v at times t (PWL), return output resampled on t"""
    d=tempfile.mkdtemp(); pf=os.path.join(d,"in.txt"); np.savetxt(pf,np.c_[t,v],fmt="%.9e")
    src=f'Ain %vd([in 0]) fs\n.model fs filesource (file="{pf}" amploffset=[0] amplscale=[1] timeoffset=0 timescale=1 timerelative=false amplstep=false)'
    ts=tstep or (t[1]-t[0])/4
    a=run(netlist(src=src,**kw),f"tran {ts} {t[-1]} 0 {ts}",f"v({node})")
    return np.interp(t,a[:,0],a[:,1])
if __name__=="__main__":
    print("DC op check / gain at 1 kHz vs POT1 resistance (LEVEL 5, EQ centre):")
    for r in (10e3,3e3,1e3,300,100,30,0):
        f,g=ac(gain_r=r); print(f"  Rpot={r:7.0f}  {np.interp(1000,f,g):6.2f} dB   pre: {np.interp(1000,*ac('pre',gain_r=r)):6.2f} dB")
    print("LEVEL law at 1 kHz (gain min):")
    for l in np.linspace(0,1,11):
        f,g=ac(level=l); print(f"  level {l*10:4.1f}: {np.interp(1000,f,g)-np.interp(1000,*ac('eqout')):7.2f} dB")
    fr=[20,40,80,160,320,1000,3000,6000,12000,20000]
    print("EQ (eqout re pre), freqs",fr)
    for name,kw in [("flat",{}),("low max",dict(low=1)),("low min",dict(low=0)),("hi max",dict(hi=1)),("hi min",dict(hi=0)),("low .75",dict(low=.75)),("hi .75",dict(hi=.75))]:
        f,g=ac("eqout",**kw); f2,g2=ac("pre",**kw)
        print(f"  {name:8s}"," ".join(f"{np.interp(x,f,g)-np.interp(x,f2,g2):6.1f}" for x in fr))
