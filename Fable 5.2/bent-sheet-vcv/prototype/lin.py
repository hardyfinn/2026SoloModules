import numpy as np, sys
NF,M,G=6,40,48
kap2=[0,0,1,2.757,5.404,8.933]; gam=[0,0.35,1,1,1,1]
xg=(np.arange(G)+0.5)/G; xi=2*xg-1
phi=np.sqrt(2/G)*np.sin(np.outer(np.arange(1,M+1),np.pi*xg))
def modes(n,bend,shape,grip=1,taper=0,F1=110,asp=5,WB=1,spot=0):
    a=(np.pi/(4.73*asp))**2; mm=np.arange(1,M+1)
    r=a*mm*mm+0.5*np.sqrt(a)*mm if n==1 else kap2[n]+a*mm*mm
    f=F1*r; w=2*np.pi*f; w1=2*np.pi*F1
    K=bend*(np.cos(shape*np.pi/2)+1.6*np.sin(shape*np.pi/2)*(xi-spot))
    wd=np.exp(-1.1*taper*(xg-0.5))
    pot=w1*w1*kap2[n]**2*(wd**-4-1)+(w1*WB*gam[n]*K)**2
    A=np.diag(w*w)+phi@np.diag(pot)@phi.T
    ev,V=np.linalg.eigh(A); fr=np.sqrt(np.maximum(ev,0))/2/np.pi
    e0=np.maximum(0,1-xg/0.22); e1=np.maximum(0,1-(1-xg)/0.16); d=grip*grip*900*(e0**2+e1**2)
    U=phi.T@V  # grid shapes
    al_mat=(V**2*(6.908/(20/(1+f/500)))[:,None]).sum(0)
    al=al_mat+0.5*(U**2*d[:,None]).sum(0)
    cen=(U**2*xg[:,None]).sum(0); wid=np.sqrt((U**2*(xg[:,None]-cen)**2).sum(0))
    return fr,6.908/al,cen,wid
if __name__=="__main__":
    for shape in (0,1):
        for bend in (0,0.5,1,2,4,8):
            fr,t60,cen,wid=modes(2,bend,shape)
            print(f"shape {shape} bend {bend:4}: "+"  ".join(f"{fr[i]:6.1f}Hz T60 {t60[i]:4.1f}s x{cen[i]:.2f}±{wid[i]:.2f}" for i in range(4)))
