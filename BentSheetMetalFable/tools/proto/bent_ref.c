// Bent sheet prototype (offline sketch, not module code).
// One strip of spring steel. Sound field: NF transverse families x M lengthwise sine modes (exact modal
// resonators). Everything geometric is done in real space on G grid points along the length:
//   K_g      curvature at point g = bend * (c0 + s0*xi_g) + dents           (C-bend ... S-bend)
//   S_{n,g}  = wB*gam_n*K_g*u_{n,g} + 1/2*eta*Q_g,  Q_g = sum_n l_n u_{n,g}^2   (membrane stretch)
//   V        = 1/2 sum S^2  -> bend stiffening (pitch up, K^2), trapping where K crosses zero (saw),
//              quadratic softening (K*u*Q) and cubic hardening (Q^2): one energy, cannot create energy.
//   bend     is itself a slow mass on a spring held by the "hand", with an optional double well (SNAP);
//              its acceleration kicks the sound modes (wobble board), and the sound pushes back on it.
//   grip     damping localised at the two ends: trapped modes never reach it, so they ring for ages.
//   water    added mass (wavenumber dependent) + loss on the wet part of the grid.
//   bow      friction at one grid point, solved implicitly per sample.
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#define FS 96000.0
#define T (1.0/FS)
#define NF 6
#define M 40
#define G 48
#define N (NF*M)
#define BLK 32
static unsigned long long rs=0x2545F4914F6CDD1DULL;
static double rnd(){ rs^=rs<<13; rs^=rs>>7; rs^=rs<<17; return (rs>>11)*(1.0/9007199254740992.0); }
static double bi(){ return 2*rnd()-1; }
static const double kap2[NF]={0,0,1,2.757,5.404,8.933};
static const double gam[NF]={0,0.35,1,1,1,1};
static const double eb[NF]={0,0,1,0.45,0.25,0.15};
static const double ln_[NF]={0.3,0.6,1.0,2.0,3.2,4.5};
static double phi[M][G],xg[G],xi[G],aL[G],aR[G];
static double mu2[N],f0[N],det[N],q[N],qp[N],w[N],a1[N],a2[N],b[N],al[N],Gw[N],mu[N];
static double u[NF][G],upv[NF][G],fg[NF][G],Kg[G],dg[G],wet[G],dent[G],tap[NF][G];
// controls
static double cTune=1,cAspect=5,cBendHand=0,cShape=0,cGrip=0.5,cSnap=0,cHold=0.5,cDip=0,cTaper=0,cAge=0,cNL=1;
static double cBowP=0,cBowV=1,cBowX=0.5,cStrikeX=0.37,cHard=0.5,cWB=1.0,cBack=1,cKick=1,cShake=0,cDepth=1,cSpot=0,cCrack=1,cPick=0,cOut=0,cIn=0;
static double strikeAt=-1,strikeV=0;
static double bend=0,bendv=0,benda=0,bowD=0;
static double F1=110;
static void coefs(){
  double a=pow(M_PI/(4.730*cAspect),2);
  for(int n=0;n<NF;n++) for(int m=0;m<M;m++){ int j=n*M+m; double mm=m+1;
    double r = n==1 ? a*mm*mm+0.5*sqrt(a)*mm : kap2[n]+a*mm*mm;
    double f=F1*cTune*r*(1+det[j]*(0.003+0.02*cAge)); if(f>0.42*FS) f=0.42*FS; f0[j]=f; w[j]=2*M_PI*f;
    double t60=20.0/(1+f/500.0)/(1+3*cAge); al[j]=6.908/t60;
    a1[j]=2*exp(-al[j]*T)*cos(w[j]*T); a2[j]=exp(-2*al[j]*T); b[j]=exp(-al[j]*T)*T*sin(w[j]*T)/w[j];
    double kt=sqrt(r)+0.05; double be=1.2*fabs(cDepth)/kt; Gw[j]=sqrt(be/(1+be)); if(cDepth<0){ double x=w[j]*T, lim=0.6*(1+cos(x))/(x*sin(x)+1e-9); if(Gw[j]*Gw[j]>lim) Gw[j]=sqrt(lim); } }
  double c0=cos(cShape*M_PI/2), s0=1.6*sin(cShape*M_PI/2);
  double w1=2*M_PI*F1*cTune;
  for(int g=0;g<G;g++){ double x=xg[g];
    Kg[g]=bend*(c0+s0*(xi[g]-cSpot))+cAge*1.2*dent[g]; { double km=0.18*FS/(F1*cTune*cWB); if(Kg[g]>km)Kg[g]=km; if(Kg[g]<-km)Kg[g]=-km; }
    double e0=fmax(0,1-x/0.22), e1=fmax(0,1-(1-x)/0.16); dg[g]=cGrip*cGrip*900*(e0*e0+e1*e1);
    double z=(cDip-x)/0.05; z=z<0?0:(z>1?1:z); wet[g]=z*z*(3-2*z); dg[g]+=wet[g]*3;
    double wd=exp(-1.1*cTaper*(x-0.5)); // relative width, narrower toward x=1 when taper>0
    for(int n=0;n<NF;n++){ tap[n][g]=w1*w1*kap2[n]*kap2[n]*(1/(wd*wd*wd*wd)-1); if(tap[n][g]>0.5*FS*FS) tap[n][g]=0.5*FS*FS; } }
}
typedef void (*Ctl)(double t);
static char dname[64]="diag.txt"; static float*out; static long guards=0,limits=0,clamps=0; static double handS=0;
static double frc(double d,double*dd){ // friction coefficient vs relative speed (bow faster than sheet: d>0)
  double ve=0.02, vs=0.25, mus=0.9, mud=0.25; double th=tanh(d/ve), ex=exp(-fabs(d)/vs);
  double m=mud+(mus-mud)*ex; *dd=(1-th*th)/ve*m + th*(-(mus-mud)*ex/vs)*(d>=0?1:-1); return th*m; }
static void render(double dur,Ctl ctl,const char*name,double f1){
  F1=f1; rs=0x2545F4914F6CDD1DULL; sprintf(dname,"diag-%s.txt",name); remove(dname);
  for(int g=0;g<G;g++){ xg[g]=(g+0.5)/G; xi[g]=2*xg[g]-1; dent[g]=0; }
  for(int g=0;g<G;g++){ double r=bi(); aL[g]=cos(M_PI*xg[g]/2)*(0.6+0.4*r)*0.5; r=bi(); aR[g]=sin(M_PI*xg[g]/2)*(0.6+0.4*r)*0.5; }
  for(int k=0;k<5;k++){ double c=rnd(),s=0.03+0.05*rnd(),a=bi(); for(int g=0;g<G;g++) dent[g]+=a*exp(-pow((xg[g]-c)/s,2)); }
  for(int m=0;m<M;m++) for(int g=0;g<G;g++) phi[m][g]=sqrt(2.0/G)*sin((m+1)*M_PI*xg[g]);
  for(int j=0;j<N;j++){ det[j]=bi(); mu2[j]=bi(); q[j]=qp[j]=0; int n=j/M,m=j%M; mu[j]= n==0 ? ((m%2==0)?1.0/(m+1):0.15*bi()/(m+1)) : 0.25*bi()/(m+1); }
  memset(u,0,sizeof u); memset(upv,0,sizeof upv); bend=bendv=benda=0; bowD=0; handS=0;
  long n_=(long)(dur*FS); out=calloc(2*n_,sizeof(float));
  FILE*dgf=fopen(dname,"a"); fprintf(dgf,"# %s\n",name);
  double Eacc[N]; memset(Eacc,0,sizeof Eacc); long dn=0; double hpL=0,hpR=0,bmin=1e9,bmax=-1e9; long snaps=0; int side=0;
  double Ybow=0; int gb=G/2, gs=G/3, gL=(int)(0.29*G), gR=(int)(0.63*G);
  for(long i=0;i<n_;i++){
    double t=i*T;
    if(i%BLK==0){ ctl(t); coefs(); gb=(int)(cBowX*G); if(gb<1)gb=1; if(gb>G-2)gb=G-2; gs=(int)(cStrikeX*G);
      Ybow=0; for(int n=0;n<NF;n++) for(int m=0;m<M;m++) Ybow+=eb[n]*eb[n]*b[n*M+m]*phi[m][gb]*phi[m][gb]; Ybow*=FS; }
    double w1=2*M_PI*F1*cTune, wB=w1*cWB, eta=6e5*cNL;
    // forward transform
    for(int n=0;n<NF;n++){ double*qq=q+n*M; for(int g=0;g<G;g++){ double s=0; for(int m=0;m<M;m++) s+=phi[m][g]*qq[m]; upv[n][g]=u[n][g]; u[n][g]=s; } }
    // stretch and forces on the grid
    double dVdb=0, kb=0, c0=cos(cShape*M_PI/2), s0=1.6*sin(cShape*M_PI/2);
    for(int g=0;g<G;g++){ double Q=0; for(int n=0;n<NF;n++) Q+=ln_[n]*u[n][g]*u[n][g];
      double S[NF],Ss=0; for(int n=0;n<NF;n++){ S[n]=wB*gam[n]*Kg[g]*u[n][g]+0.5*eta*Q; Ss+=S[n]; }
      double kloc=wB*wB*Kg[g]*Kg[g]+eta*4.5*fabs(Ss)+eta*eta*4.5*Q*NF, sc=kloc*T*T>1.0? 1.0/(kloc*T*T):1.0; if(sc<1) limits++;
      double sh=c0+s0*(xi[g]-cSpot);
      for(int n=0;n<NF;n++){ double v=(u[n][g]-upv[n][g])*FS;
        fg[n][g]=-tap[n][g]*u[n][g]-sc*(wB*gam[n]*Kg[g]*S[n]+eta*ln_[n]*u[n][g]*Ss)-dg[g]*v;
        dVdb+=sc*S[n]*wB*gam[n]*u[n][g]*sh; kb+=wB*wB*gam[n]*gam[n]*u[n][g]*u[n][g]*sh*sh; } }
    if(strikeAt>=0 && t>=strikeAt){ for(int n=0;n<NF;n++) for(int m=0;m<M;m++){ int j=n*M+m; qp[j]-=strikeV*phi[m][gs]*T*exp(-f0[j]/(400+6000*cHard*cHard))*(n==1?0.5:1); } strikeAt=-1; }
    if(cIn!=0){ for(int n=0;n<NF;n++) fg[n][gs]+=cIn*(n<2?0.3:1); }
    // back transform (+ inertial kick from the bend's own acceleration)
    double fm[N];
    for(int n=0;n<NF;n++) for(int m=0;m<M;m++){ double s=0; for(int g=0;g<G;g++) s+=phi[m][g]*fg[n][g]; int j=n*M+m; fm[j]=s-cKick*0.05*mu[j]*benda; }
    // water: added mass as a projection of the total force on the wet part
    if(cDip>0.001){ double pg[NF][G];
      for(int n=0;n<NF;n++) for(int g=0;g<G;g++){ double s=0; for(int m=0;m<M;m++){ int j=n*M+m; double ft=fm[j]-w[j]*w[j]*q[j]-2*al[j]*(q[j]-qp[j])*FS; s+=phi[m][g]*Gw[j]*ft; } pg[n][g]=s*wet[g]; }
      for(int n=0;n<NF;n++) for(int m=0;m<M;m++){ double s=0; for(int g=0;g<G;g++) s+=phi[m][g]*pg[n][g]; int j=n*M+m; fm[j]-=(cDepth<0?-1:1)*Gw[j]*s; } }
    // modal update
    double qn[N],E=0,vfree=0;
    for(int j=0;j<N;j++){ qn[j]=a1[j]*q[j]-a2[j]*qp[j]+b[j]*fm[j]; }
    if(cBowP>0){ for(int n=0;n<NF;n++) for(int m=0;m<M;m++){ int j=n*M+m; vfree+=eb[n]*phi[m][gb]*(qn[j]-q[j]); } vfree*=FS;
      double YP=Ybow*cBowP*cBowP*2000, rhs=cBowV-vfree, d=bowD, dd; // d + YP*fr(d) = rhs
      for(int it=0;it<6;it++){ double fr=frc(d,&dd); double gq=d+YP*fr-rhs, gp=1+YP*dd; if(fabs(gp)<0.2) gp=gp<0?-0.2:0.2; double st=gq/gp; if(st>0.5)st=0.5; if(st<-0.5)st=-0.5; d-=st; }
      bowD=d; double Fb=cBowP*cBowP*2000*frc(d,&dd);
      for(int n=0;n<NF;n++) for(int m=0;m<M;m++){ int j=n*M+m; qn[j]+=eb[n]*b[j]*phi[m][gb]*Fb; } }
    double L=0,R=0;
    for(int j=0;j<N;j++){ double v=(qn[j]-q[j])*FS; double e=0.5*v*v+0.5*w[j]*w[j]*qn[j]*qn[j]; E+=e; Eacc[j]+=e; qp[j]=q[j]; q[j]=qn[j]; }
    if(E>5e6 && E<1e12){ double k=sqrt(5e6/E); clamps++; for(int j=0;j<N;j++){ q[j]*=k; qp[j]*=k; } }
    if(!(E<1e12)){ if(guards<3) fprintf(stderr,"guard t=%.3f tune %.2f bend %.1f hand %.1f shape %.2f snap %.2f dip %.2f depth %.2f bowP %.2f V %.2f NL %.2f age %.2f grip %.2f in %.0f E %g\n",t,cTune,bend,cBendHand,cShape,cSnap,cDip,cDepth,cBowP,cBowV,cNL,cAge,cGrip,cIn,E); guards++; for(int j=0;j<N;j++){ q[j]*=0.5; qp[j]*=0.5; } }
    for(int n=0;n<NF;n++){ double o= n==0?0.5:1, sl=0, sr=0; for(int g=0;g<G;g++){ double v=u[n][g]-upv[n][g]; sl+=aL[g]*v; sr+=aR[g]*v; } L+=o*sl*FS; R+=o*((n&1)?-sr:sr)*FS; }
    { double uLs=0,uRs=0; for(int n=2;n<NF;n++) for(int g=0;g<G;g++){ uLs+=aL[g]*u[n][g]; uRs+=aR[g]*u[n][g]; }
      double gl=1+0.85*cPick*tanh(uLs*2500), gr=1+0.85*cPick*tanh(uRs*2500); L/=gl*gl; R/=gr*gr;
      double g=1+14*cOut*cOut, r=1.5; L=r*tanh(g*L/r)/sqrt(g); R=r*tanh(g*R/r)/sqrt(g); }
    hpL+=(L-hpL)*(2*M_PI*60*T); hpR+=(R-hpR)*(2*M_PI*60*T); out[2*i]=(float)(L-hpL); out[2*i+1]=(float)(R-hpR);
    // the bend: a slow mass held by the hand, optional double well
    { double we=2*M_PI*7.0*sqrt(cTune), wh=2*M_PI*(1.5+60*cHold*cHold), z=0.12+0.6*cHold;
      handS+=(cBendHand-handS)*(1-exp(-T/0.002));
      double acc=-wh*wh*(bend-handS)-we*we*cSnap*(-2.2*bend+bend*bend*bend/2.25)-2*z*sqrt(wh*wh+we*we)*bendv-cBack*dVdb+cShake;
      double kt_=wh*wh+cBack*kb+we*we*cSnap*3*bend*bend/2.25, ct_=2*z*sqrt(wh*wh+we*we);
      double b0_=bend, dv=T*acc/(1+T*ct_+T*T*kt_); benda=dv*FS; if(benda>3e5)benda=3e5; if(benda<-3e5)benda=-3e5; bendv+=dv; bend+=bendv*T;
      if(bend>30){bend=30; if(bendv>0)bendv=0;} if(bend<-30){bend=-30; if(bendv<0)bendv=0;}
      if((b0_<0)!=(bend<0)){ double A=0.08*cCrack*fabs(bendv); for(int j=0;j<N;j++) qp[j]-=A*mu2[j]*T*exp(-f0[j]/(700+3000*cHard))*(bendv>0?1:-1); }
 if(bend<bmin)bmin=bend; if(bend>bmax)bmax=bend;
      int sd=bend>0.3?1:(bend<-0.3?-1:side); if(sd!=side&&side!=0) snaps++; side=sd; }
    if(++dn==(long)(0.5*FS)){ double s=0,s2=0,mx=0,fam[NF]; int mj=0; memset(fam,0,sizeof fam); for(int j=0;j<N;j++){ s+=Eacc[j]; s2+=Eacc[j]*Eacc[j]; fam[j/M]+=Eacc[j]; if(Eacc[j]>mx){mx=Eacc[j];mj=j;} }
      fprintf(dgf,"t %5.1f bend %6.2f..%6.2f snaps %ld E %9.3g PR %5.1f top f%d m%d %6.1f Hz (%4.1f%%) fam %2.0f %2.0f %2.0f %2.0f %2.0f %2.0f Atop %.2g\n",t,bmin,bmax,snaps,s/dn,s*s/(s2+1e-30),mj/M,mj%M+1,f0[mj],100*mx/(s+1e-30),
        100*fam[0]/(s+1e-30),100*fam[1]/(s+1e-30),100*fam[2]/(s+1e-30),100*fam[3]/(s+1e-30),100*fam[4]/(s+1e-30),100*fam[5]/(s+1e-30),sqrt(2*mx/dn)/w[mj]);
      memset(Eacc,0,sizeof Eacc); dn=0; bmin=1e9; bmax=-1e9; snaps=0; }
  }
  fclose(dgf); char fn[256]; sprintf(fn,"raw-%s.f32",name); FILE*f=fopen(fn,"wb"); fwrite(out,sizeof(float),2*n_,f); fclose(f); free(out);
  fprintf(stderr,"%s done, guard events %ld, stiffness limits %ld, energy clamps %ld\n",name,guards,limits,clamps); guards=limits=clamps=0;
}
static double lerp(double a,double b,double x){ x=fmax(0,fmin(1,x)); return a+(b-a)*x; }
static double xlerp(double a,double b,double x){ x=fmax(0,fmin(1,x)); return a*pow(b/a,x); }
static double seg(double t,double a,double b){ return fmax(0,fmin(1,(t-a)/(b-a))); }
static void base(){ cTune=1; cAspect=5; cBendHand=0; cShape=0; cGrip=0.5; cSnap=0; cHold=0.5; cDip=0; cTaper=0; cAge=0; cNL=1; cBowP=0; cBowV=1; cBowX=0.5; cStrikeX=0.37; cHard=0.5; cWB=1; cBack=1; cKick=1; cShake=0; cDepth=1; cSpot=0; cCrack=1; cPick=0; cOut=0; cIn=0; }
#include "scenes.h"  // scenes and main()
