// Molten, second prototype. The Plate Driver body (192 modes, stretch channels, pickup -> driver loop) with a
// temperature map on top. Heat sinks and dulls each mode by the zones it lives in, loosens the metal so it
// splits and roars at lower drive, and melts it: a molten zone swaps plate stiffness for surface tension, so
// the same modes slide into a dense low cluster. Freezing spreads them out again and casts a new tuning.
// gcc -O3 -ffast-math -o molten molten_ref.c -lm ; ./molten heat|cast|flame|pushed  -> raw-<scene>.f32 (stereo float, 96 kHz), log-<scene>.txt
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#define FS 96000.0
#define T (1.0/FS)
#define N 192
#define C 6
#define K 16
#define BLK 32
static double RM=5;
static unsigned long long rs=0x2545F4914F6CDD1DULL;
static double rnd(){ rs^=rs<<13; rs^=rs>>7; rs^=rs<<17; return (rs>>11)*(1.0/9007199254740992.0); }
static double bi(){ return 2*rnd()-1; }
static double f0[N],kw[N],ph[N],q[N],qp[N],a1[N],a2[N],b[N],w[N],al[N],ow[N];
static double A[C][N],Lm[C][N],Sg[C][N],sw[N],cEta=1.0,dW[N],pW[N],oL[N],oR[N];
static double zw[N][K],cast[N],castT[N],F1;
// plate driver controls
static double cDrive=0,cFc=200,cFocus=0,cCurve=0.6,cTune=1,cDamp=0,cPosD=0.31,cPosP=0.31,cBloom=1,cGain=40;
static double strikeAt=-1,strikeV=0,strikeX=0.31;
static double cPick=0,cAmp=0,cNu=4e-5,cBright=0,cOut=0;
// molten controls
static double cHeat=0,cRate=3,cXf=0.3,cWf=2,cQuench=0,cLatent=0.5,cUc=0,cCond=4,cSag=0.6,cTens=1,cLoose=2.5,cLiqNL=6,cCast=1,cPing=0.5,cAir=0;
// thermal state
static double hh[K],TT[K],phi[K],phip[K],Tp[K],Mliq=0,Tsol=0,Tliq=1,bloomEff=1,curveEff=0.6; static int liq[K],nuc[K];
static long casts=0; static double Mprev=0;
static double Sprev[C],Sbar[C],lam=1; // lam: looseness, the stretching sees the motion lam times larger static long casts=0; static double Mprev=0;
static void shapes(){ for(int j=0;j<N;j++){ dW[j]=cos(kw[j]*cPosD+ph[j]); pW[j]=cos(kw[j]*cPosP+ph[j]); } }
static void thermal(){
  double dt=BLK*T, lap[K];
  for(int k=0;k<K;k++){ double tl=TT[k>0?k-1:k], tr=TT[k<K-1?k+1:k]; lap[k]=tl-2*TT[k]+tr; }
  for(int k=0;k<K;k++){ double x=(k+0.5)/K, g=exp(-pow((x-cXf)/cWf,2));
    double dh= cHeat>TT[k]? cRate*g*(cHeat-TT[k]) : 0.6*cRate*(cHeat-TT[k]);
    dh+=-cAir*TT[k]+cCond*lap[k]+cQuench*2.5*(0.5+1.5*fabs(2*x-1))*(0-TT[k]); hh[k]+=dh*dt; if(hh[k]<0)hh[k]=0; }
  for(int k=0;k<K;k++){
    if(hh[k]>=1+cLatent) liq[k]=1;
    if(liq[k]&&hh[k]<1+cLatent){ int seed=(hh[k]-cLatent)<1-cUc;
      int nb=(k>0&&!liq[k-1]&&phi[k-1]<0.999)||(k<K-1&&!liq[k+1]&&phi[k+1]<0.999);
      if(seed){ liq[k]=0; nuc[k]=-1; }
      else if(nb){ if(nuc[k]<0) nuc[k]=(int)((0.004+0.03*fmax(0,1-3*(1-(hh[k]-cLatent))))/dt); else if(--nuc[k]<=0){ liq[k]=0; nuc[k]=-1; } }
    } else nuc[k]=-1;
    Tp[k]=TT[k]; phip[k]=phi[k];
    if(liq[k]){ TT[k]=hh[k]-cLatent; phi[k]=1; } else if(hh[k]<1){ TT[k]=hh[k]; phi[k]=0; } else { TT[k]=1; phi[k]=(hh[k]-1)/cLatent; }
    // a ping as each zone freezes
    double p=cPing*(0.6*fabs(TT[k]-Tp[k])*(1-phi[k])+2.0*fmax(0,phip[k]-phi[k]));
    if(rnd()<p){ double x=(k+rnd())/K, amp=(60+250*rnd()*rnd())*(rnd()<0.5?-1:1);
      for(int j=0;j<N;j++){ double f=w[j]/(2*M_PI); qp[j]-=amp*cos(kw[j]*x+ph[j])*T*f*f/(f*f+1500.0*1500.0)*exp(-f/6000.0); } }
  }
  double ms=0; Mliq=0; Tsol=0; Tliq=0;
  for(int k=0;k<K;k++){ Mliq+=phi[k]/K; Tsol+=TT[k]*(1-phi[k]); ms+=1-phi[k]; Tliq+=TT[k]*phi[k]; }
  Tsol= ms>1e-6? Tsol/ms : 1; Tliq= Mliq>1e-6? Tliq/(Mliq*K) : 1;
  // casting: the shape the liquid holds as it sets becomes the new tuning of the solid
  if(Mprev>=0.5 && Mliq<0.5){ double r=0; for(int j=0;j<N;j++) r+=w[j]*w[j]*q[j]*q[j]; r=sqrt(r/N)+1e-9;
    for(int j=0;j<N;j++) castT[j]=1+0.14*cCast*tanh(1.5*w[j]*q[j]/r)*(0.4+0.6*rnd()); casts++; }
  Mprev=Mliq;
  bloomEff=cBloom; lam=sqrt(exp(cLoose*fmin(Tsol,1.0)*(1-Mliq))*(1+cLiqNL*Mliq)); curveEff=cCurve*(1+0.5*Mliq);
}
static void coefs(){
  double sig=fmax(0.2,1-0.5*(Tliq-1)), visc=exp(-2.2*(Tliq-1));
  for(int j=0;j<N;j++){ double Tb=0,pb=0; for(int k=0;k<K;k++){ Tb+=zw[j][k]*fmin(TT[k],1.0); pb+=zw[j][k]*phi[k]; }
    cast[j]+=(castT[j]-cast[j])*0.02;
    double fs=f0[j]*cTune*cast[j], fl=F1*cTune*cTens*pow(f0[j]/F1,0.75)*sqrt(sig);
    double f=sqrt((1-pb)*(1-cSag*Tb*Tb)*fs*fs+pb*fl*fl); if(f<4)f=4; int mute=f>0.25*FS; if(mute)f=0.25*FS;
    double als=6.908/(9.0/(1+fs/700.0))*(1+2*Tb*Tb*Tb), all=6.908/4.0*sqrt(fl/(F1*cTune*cTens))*visc;
    w[j]=2*M_PI*f; al[j]=((1-pb)*als+pb*all)*(1+8*pb*(1-pb))*(1+cDamp*(8+f/40.0));
    a1[j]=2*exp(-al[j]*T)*cos(w[j]*T); a2[j]=exp(-2*al[j]*T); b[j]=exp(-al[j]*T)*T*sin(w[j]*T)/w[j];
    ow[j]=mute?0:1.0/(1+f/(1500.0*(1+4*cBright))); sw[j]=sqrt(w[j]); if(mute){ a1[j]=a2[j]=b[j]=0; } } }
typedef void (*Ctl)(double t);
static float*out; static long guards=0,lim=0; static double svl=0,svb=0;
static void render(double dur,Ctl ctl,const char*name,double f1){
  F1=f1;
  for(int j=0;j<N;j++){ f0[j]=f1*(1+0.6*(j+(j?0.45*bi():0))); kw[j]=3.1*sqrt(f0[j]/f1); ph[j]=2*M_PI*rnd(); q[j]=qp[j]=0; cast[j]=castT[j]=1;
    oL[j]=cos(kw[j]*0.13+ph[j]); oR[j]=cos(kw[j]*0.88+ph[j]);
    double tot=0; for(int k=0;k<K;k++){ double s=0; for(int i=0;i<8;i++){ double x=(k+(i+0.5)/8)/K, c=cos(kw[j]*x+ph[j]); s+=c*c; } zw[j][k]=s/8+0.1; tot+=zw[j][k]; }
    for(int k=0;k<K;k++) zw[j][k]/=tot; }
  for(int c=0;c<C;c++) for(int j=0;j<N;j++){ A[c][j]=(rnd()<0.5?-1:1)/sqrt((double)C*N); Lm[c][j]=(0.3+rnd())/sqrt((double)C)*0.8; Sg[c][j]=(rnd()<0.5?-1:1)/sqrt((double)C); }
  long n=(long)(dur*FS); out=calloc(2*n,sizeof(float)); svl=svb=0; memset(Sprev,0,sizeof Sprev); memset(Sbar,0,sizeof Sbar);
  ctl(0); for(int k=0;k<K;k++){ hh[k]= cHeat<1? cHeat : cHeat+cLatent; liq[k]=cHeat>1; nuc[k]=-1; TT[k]=Tp[k]=cHeat; phi[k]=phip[k]=cHeat>1; } Mprev=cHeat>1; coefs();
  char fn[256]; sprintf(fn,"log-%s.txt",name); FILE*dg=fopen(fn,"w");
  double Eacc[N]; memset(Eacc,0,sizeof Eacc); long dn=0;
  for(long i=0;i<n;i++){
    double t=i*T;
    if(i%BLK==0){ ctl(t); thermal(); coefs(); shapes(); }
    double y=0,up=0,uL=0,uR=0; for(int j=0;j<N;j++){ y+=pW[j]*(q[j]-qp[j]); up+=pW[j]*q[j]; uL+=oL[j]*q[j]; uR+=oR[j]*q[j]; } y*=FS;
    { double g=1+0.85*cPick*tanh(up*0.6); y/=g*g; }
    double g=2*sin(M_PI*fmin(cFc,20000)/FS), Q=1.0+8*cFocus; svl+=g*svb; double hp=y-svl-svb/Q; svb+=g*hp; double bp=svb/Q;
    double yf=(1-cFocus)*y+cFocus*bp*2.0;
    double x=cGain*yf/fmax(cDrive,1.0);
    { double gg=1+29*cAmp*cAmp, bb=0.45*cAmp; x=tanh(gg*x+bb)-tanh(bb); }
    double F=cDrive*x+1e-4*bi();
    if(strikeAt>=0 && t>=strikeAt){ for(int j=0;j<N;j++) qp[j]-=strikeV*cos(kw[j]*strikeX+ph[j])*T*exp(-f0[j]/900.0); { int k=(int)(strikeX*K); if(k>=K)k=K-1; if(strikeV>250 && liq[k] && hh[k]<1+cLatent) liq[k]=0; } strikeAt=-1; } // a hard strike seeds freezing in supercooled liquid
    // stretching, in the loosened coordinate qs = lam*q; the force is divided by lam again below
    double S[C],Gq[C],ktot=0,wmax=1; for(int j=0;j<N;j++) if(w[j]>wmax) wmax=w[j];
    double smax=fmax((1.5*FS)*(1.5*FS)-wmax*wmax,0.1*wmax*wmax)/(3.5*wmax), kroom=2.0*fmax((1.9*FS)*(1.9*FS)-wmax*wmax,0.05*FS*FS);
    for(int c=0;c<C;c++){ double s=0,s2=0,gq=0,saa=0,sab=0,gs2=0,lwm=0;
      for(int j=0;j<N;j++){ double qs=lam*q[j], P=A[c][j]*w[j], Lw=Lm[c][j]*w[j], G=Sg[c][j]*sw[j]; s+=P*qs; s2+=Lw*qs*qs; gq+=G*qs;
        double a=curveEff*P+Lw*qs; saa+=a*a; sab+=a*G; gs2+=G*G; if(Lw>lwm) lwm=Lw; }
      Gq[c]=cEta*cEta*gq; double g2=saa+2*Gq[c]*sab+Gq[c]*Gq[c]*gs2;
      S[c]=curveEff*s+0.5*s2+0.5*Gq[c]*gq; double sd=(S[c]-Sprev[c])*FS; Sprev[c]=S[c]; S[c]=S[c]+cNu*sd;
      { double sb=smax/fmax(bloomEff,0.05); S[c]=sb*tanh(S[c]/sb); }
      ktot+=bloomEff*(g2+fabs(S[c])*(lwm+cEta*cEta*gs2)); }
    double kscale= ktot>kroom? kroom/ktot : 1.0; if(kscale<1) lim++;
    double L=0,R=0,E=0,Ek=0;
    for(int j=0;j<N;j++){ double fn=0; for(int c=0;c<C;c++) fn+=S[c]*(curveEff*A[c][j]*w[j]+Lm[c][j]*w[j]*lam*q[j]+Gq[c]*Sg[c][j]*sw[j]);
      fn*=kscale/lam;
      double qn=a1[j]*q[j]-a2[j]*qp[j]+b[j]*(dW[j]*F-bloomEff*fn);
      double v=(qn-q[j])*FS; double e=0.5*v*v+0.5*w[j]*w[j]*qn*qn; E+=e; Ek+=0.5*v*v; Eacc[j]+=e; L+=oL[j]*ow[j]*v; R+=oR[j]*ow[j]*v; qp[j]=q[j]; q[j]=qn; }
    // a bent-in (buckled) rest shape holds energy without moving; with the drive off, let it relax
    if(cDrive<=0 && Ek<1e-3*E){ for(int j=0;j<N;j++){ double m=0.5*(q[j]+qp[j]); q[j]-=2e-4*m; qp[j]-=2e-4*m; } }
    if(!(E<1e9)){ guards++; for(int j=0;j<N;j++){ if(E==E){ q[j]*=0.5; qp[j]*=0.5; } else q[j]=qp[j]=0; } }
    { double gl=1+0.85*cPick*tanh(uL*0.6), gr=1+0.85*cPick*tanh(uR*0.6); L/=gl*gl; R/=gr*gr; }
    { double gg=1+14*cOut*cOut, r=2500; L=r*tanh(gg*L/r)/sqrt(gg); R=r*tanh(gg*R/r)/sqrt(gg); }
    if(!(L==L)||!(R==R)){ L=R=0; } out[2*i]=(float)L; out[2*i+1]=(float)R;
    if(++dn==(long)(0.5*FS)){ double s=0,s2=0,mx=0; int mj=0; for(int j=0;j<N;j++){ s+=Eacc[j]; s2+=Eacc[j]*Eacc[j]; if(Eacc[j]>mx){mx=Eacc[j];mj=j;} }
      fprintf(dg,"t %5.1f heat %.2f M %.2f Ts %.2f drive %7.0f E %9.3g PR %5.1f top %6.1f Hz (%4.1f%%) g %ld\n",t,cHeat,Mliq,Tsol,cDrive,s/dn,s*s/(s2+1e-30),w[mj]/(2*M_PI),100*mx/(s+1e-30),guards);
      memset(Eacc,0,sizeof Eacc); dn=0; }
  }
  fclose(dg); sprintf(fn,"raw-%s.f32",name); FILE*f=fopen(fn,"wb"); fwrite(out,sizeof(float),2*n,f); fclose(f); free(out);
  fprintf(stderr,"%s done, guard events %ld, casts %ld, limited %.3f%%\n",name,guards,casts,100.0*lim/((double)n)); guards=0;
}
static double lerp(double a,double b,double x){ x=fmax(0,fmin(1,x)); return a+(b-a)*x; }
static double xlerp(double a,double b,double x){ x=fmax(0,fmin(1,x)); return a*pow(b/a,x); }
static double seg(double t,double a,double b){ return fmax(0,fmin(1,(t-a)/(b-a))); }
static void base(){ cPick=cAmp=cBright=cOut=0; cNu=4e-5; cBloom=1; cFocus=0.5; cFc=262; cCurve=0.6; cEta=1.0; cTune=1; cDamp=0; cGain=40; cPosD=cPosP=0.31;
  cHeat=0; cRate=3; cXf=0.3; cWf=2; cQuench=0; cLatent=0.5; cUc=0; cCond=4; cSag=0.6; cTens=1; cLoose=5; cLiqNL=400; cAir=0; cCast=1; cPing=0.5; }
#include "scenes.h"
