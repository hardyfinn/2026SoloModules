// Prototype for the "metal against metal" module (working name JANGLE).
// Bodies = sets of ringing modes plus one slow motion (sway / swing / free slide).
// Contacts = Hertz-type collisions between bodies (or a body and its frame).
// Everything heard comes from the collisions. Offline sketch, not module code.
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#define FS 96000.0
#define T (1.0/FS)
#define MAXB 128
#define MAXD 84
#define MAXC 400
typedef struct { int nd; double a1[MAXD],a2[MAXD],b[MAXD],c[MAXD],o[MAXD],q[MAXD],qp[MAXD],f[MAXD];
  double sb;   /* slow dof: displacement per unit acceleration (for shake), 0 = not shaken */
  double pl,pr,u,up,F; } Body;
typedef struct { int a,b,a2; double wa,wa2,gap,K,mu; long hits; int on; } Contact;
static Body B[MAXB]; static Contact C[MAXC]; static int nb=0,nc=0;
static unsigned long long rs=88172645463325252ULL;
static double rnd(){ rs^=rs<<13; rs^=rs>>7; rs^=rs<<17; return (rs>>11)*(1.0/9007199254740992.0); }
static double bi(){ return 2*rnd()-1; }
static int newBody(double pan){ Body*x=&B[nb]; memset(x,0,sizeof(Body)); x->pl=sqrt(0.5*(1-pan)); x->pr=sqrt(0.5*(1+pan)); return nb++; }
// f Hz (0 = free mass, then t60 is the time for its speed to fall by 60 dB), c = shape at the contact point
static void addDof(int bi_, double f, double t60, double c, double mass, double o){
  Body*x=&B[bi_]; int j=x->nd++; double al=6.908/t60, w=2*M_PI*f;
  if(f<=0){ double r=exp(-2*al*T); x->a1[j]=1+r; x->a2[j]=r; x->b[j]=T*T/mass; }
  else { x->a1[j]=2*exp(-al*T)*cos(w*T); x->a2[j]=exp(-2*al*T); x->b[j]=exp(-al*T)*T*sin(w*T)/w/mass; }
  x->c[j]=c; x->o[j]=o; x->f[j]=f; }
static int addContact(int a,int b,double gap,double K,double mu){ Contact*k=&C[nc]; k->a=a;k->b=b;k->a2=-1;k->wa=1;k->wa2=0;k->gap=gap;k->K=K;k->mu=mu;k->hits=0;k->on=0; return nc++; }
static void kick(int b,int dof,double v){ B[b].qp[dof]-=v*T; }

// ---- cantilever rod with a weight on top: exact frequency equation
static double gfun(double be,double mu){ return 1.0/cosh(be)+cos(be)+mu*be*(cos(be)*tanh(be)-sin(be)); }
static int rodModes(double mu,double f1bare,double fmax,double*fr,double*phi){
  int n=0; double b0=0.3,g0=gfun(b0,mu);
  for(double be=0.305; n<MAXD-2; be+=0.005){ double g=gfun(be,mu);
    if((g0<0)!=(g<0)){ double lo=be-0.005,hi=be; for(int i=0;i<50;i++){ double m=0.5*(lo+hi); if((gfun(lo,mu)<0)!=(gfun(m,mu)<0)) hi=m; else lo=m; }
      double r=0.5*(lo+hi); double f=f1bare*(r/1.875104)*(r/1.875104); if(f>fmax) break;
      fr[n]=f; phi[n]=2*(sin(r)-cos(r)*tanh(r))/(tanh(r)+sin(r)/cosh(r)); n++; }
    g0=g; }
  return n; }
// rod: f1bare = sway rate of the bare rod, m = rod mass, mu = top weight / rod mass
static int makeRod(double pan,double f1bare,double m,double mu,double t60low,double bright){
  static double fr[MAXD],phi[MAXD]; int n=rodModes(mu,f1bare,13000,fr,phi); int b=newBody(pan);
  for(int k=0;k<n;k++){ double f=fr[k]; double t60=(k==0)? t60low*2 : t60low/(1+f/bright);
    double mm=m*(1+mu*phi[k]*phi[k]); double o=(f<30)?0:fmin(1.0,pow(f/2000.0,1.3));
    addDof(b,f,t60,phi[k],mm,o); }
  return b; }
// hanging bar or tube: swing (pendulum) + free-free bending modes
static int makeBar(double pan,double f1,double m,double fswing,double t60,double tswing,const double*cs,int nm,int shaken){
  static const double R[8]={1,2.756,5.404,8.933,13.344,18.64,24.81,31.87};
  int b=newBody(pan); addDof(b,fswing,tswing,1.0,m,0); if(shaken) B[b].sb=B[b].b[0]*m;
  for(int k=0;k<nm;k++){ double f=f1*R[k]*(1+0.002*bi()); if(f>16000) break;
    addDof(b,f,t60/(1+f/4000.0),cs[k],m*0.5,1.0); }
  return b; }

static long S(double t){ return (long)(t*FS); }
static double fa=0; // frame acceleration this sample
static float *out; static long nout;
static void run(long n0,long n1){
  for(long n=n0;n<n1;n++){
    for(int i=0;i<nb;i++){ Body*x=&B[i]; double u=0,up=0; for(int j=0;j<x->nd;j++){ u+=x->c[j]*x->q[j]; up+=x->c[j]*x->qp[j]; } x->u=u; x->up=up; x->F=0; }
    for(int i=0;i<nc;i++){ Contact*k=&C[i]; double ua=k->wa*B[k->a].u, va=k->wa*(B[k->a].u-B[k->a].up);
      if(k->a2>=0){ ua+=k->wa2*B[k->a2].u; va+=k->wa2*(B[k->a2].u-B[k->a2].up); }
      double d=ua-B[k->b].u-k->gap;
      if(d>0){ double dv=(va-(B[k->b].u-B[k->b].up))*FS; double F=k->K*d*sqrt(d)*(1+k->mu*dv); if(F<0)F=0;
        B[k->a].F-=k->wa*F; if(k->a2>=0) B[k->a2].F-=k->wa2*F; B[k->b].F+=F; if(!k->on){k->on=1;k->hits++;} } else k->on=0; }
    double L=0,R=0;
    for(int i=0;i<nb;i++){ Body*x=&B[i]; double s=0,F=x->F;
      for(int j=0;j<x->nd;j++){ double qn=x->a1[j]*x->q[j]-x->a2[j]*x->qp[j]+x->b[j]*x->c[j]*F; if(j==0&&x->sb!=0) qn-=x->sb*fa;
        s+=x->o[j]*(qn-x->q[j]); x->qp[j]=x->q[j]; x->q[j]=qn; }
      s*=FS; L+=x->pl*s; R+=x->pr*s; }
    out[2*n]=(float)L; out[2*n+1]=(float)R; }
}

static long totalHits();
static void runD(long n0,long n1){ long st=S(2.0); for(long n=n0;n<n1;){ long e=((n/st)+1)*st; if(e>n1)e=n1; long h0=totalHits(); run(n,e); if(e%st==0) fprintf(stderr,"%ld ",totalHits()-h0); n=e; } }
static long totalHits(){ long h=0; for(int i=0;i<nc;i++) h+=C[i].hits; return h; }

int main(int argc,char**argv){
  if(argc<3){ fprintf(stderr,"scene outfile\n"); return 1; }
  const char*sc=argv[1]; double dur=20;
  // ---------------- standing rods
  if(!strncmp(sc,"rods",4)){
    int heavy=!strcmp(sc,"rods-heavy"), hard=!strcmp(sc,"rods-hard");
    int N=heavy?9:16; dur=heavy?40:(hard?40:26);
    double f1=heavy?1.9:1.16, mu=heavy?1.2:0.0, m=heavy?0.45:0.07, gap=heavy?0.012:0.017;
    for(int i=0;i<N;i++){ makeRod(-0.8+1.6*i/(N-1.0), f1*(1+0.07*bi()), m, mu, heavy?34:34, heavy?900:3000); }
    for(int i=0;i+1<N;i++) addContact(i,i+1,gap*(1+0.2*bi()),heavy?6e6:1e7,0.08);
    out=calloc(2*S(dur),sizeof(float)); long n=0;
    double v=heavy?0.45:(hard?1.3:0.22), dt=hard?0.05:0.11;
    for(int i=0;i<N;i++){ long t=S(0.3+i*dt); run(n,t); n=t; kick(i,0,v*(0.8+0.4*rnd())/B[i].c[0]); }
    if(hard){ runD(n,S(13.0)); n=S(13.0); for(int i=N-1;i>=0;i--){ long t=S(13.0+(N-1-i)*0.09); run(n,t); n=t; kick(i,0,-0.5*(0.8+0.4*rnd())/B[i].c[0]); } }
    runD(n,S(dur));
  }
  // ---------------- bar chimes (mark tree), swept up then down
  else if(!strcmp(sc,"barchimes")){
    int N=30; dur=14; static const double cs[8]={2,-2,2,-2,2,-2,2,-2};
    for(int i=0;i<N;i++){ double x=i/(N-1.0); double f=880*pow(6.2,x); double m=0.11*pow(0.2,x);
      makeBar(-0.9+1.8*x,f,m,2.0*(1+0.04*bi()),5.5,5.0,cs,5,0); }
    for(int i=0;i+1<N;i++) addContact(i,i+1,0.004*(1+0.25*bi()),1.2e7,0.3);
    out=calloc(2*S(dur),sizeof(float)); long n=0;
    for(int i=0;i<N;i++){ long t=S(0.4+i*0.028); run(n,t); n=t; kick(i,0,0.55*(0.8+0.4*rnd())); for(int j=1;j<B[i].nd;j++) kick(i,j,0.004*bi()); }
    for(int i=N-1;i>=0;i--){ long t=S(6.5+(N-1-i)*0.045); run(n,t); n=t; kick(i,0,-0.3*(0.8+0.4*rnd())); for(int j=1;j<B[i].nd;j++) kick(i,j,0.003*bi()); }
    run(n,S(dur));
  }
  // ---------------- wind chimes: six tubes round a wooden clapper, gusty wind
  else if(!strcmp(sc,"windchimes")){
    dur=32; static const double cs[5]={1.0,0.3,0.8,0.25,0.5}; static const double fs[6]={392,440,523.25,587.33,659.26,783.99};
    for(int i=0;i<6;i++) makeBar(-0.7+1.4*i/5.0,fs[i],0.16*392/fs[i],1.15*(1+0.05*bi()),9,14,cs,5,0);
    int cx=newBody(0), cy=newBody(0); addDof(cx,0.8,6,1,0.09,0); addDof(cy,0.83,6,1,0.09,0);
    for(int i=0;i<6;i++){ double th=2*M_PI*i/6.0+0.2; int k=addContact(cx,i,0.024,6e5,6.0); C[k].a2=cy; C[k].wa=cos(th); C[k].wa2=sin(th); }
    out=calloc(2*S(dur),sizeof(float)); double wx=0,wy=0,g=0;
    for(long n=0;n<S(dur);n+=96){ double t=n/FS; g=0.5+0.5*sin(2*M_PI*t/11.0-1.2); g=g*g*(0.35+0.5*(t>14));
      wx+=0.004*(bi()*g*14-wx); wy+=0.004*(bi()*g*14-wy);
      B[cx].qp[0]-=wx*T*T*96*30; B[cy].qp[0]-=wy*T*T*96*30; run(n,n+96); }
  }
  // ---------------- tambourine: ten pairs of jingles loose in their slots, hand shaking 16ths at 122 BPM
  else if(!strcmp(sc,"tambourine")||!strcmp(sc,"sleigh")){
    int sl=!strcmp(sc,"sleigh"); dur=sl?10:12; int NP=sl?14:10;
    int fr=newBody(0); if(!sl){ addDof(fr,420,0.05,1,0.25,0.5); addDof(fr,910,0.035,-0.8,0.25,0.5); } else addDof(fr,300,0.03,1,0.4,0.0);
    static const double ZR[9]={1,1.52,2.21,2.9,3.7,4.55,5.5,6.6,7.8}, SR[6]={1,1.83,2.41,3.12,4.2,5.4};
    int first[32];
    for(int p=0;p<NP;p++){ double pan=-0.6+1.2*p/(NP-1.0);
      if(!sl){ int z[2]; for(int h=0;h<2;h++){ int b=newBody(pan); z[h]=b; double base=(1700+900*rnd()); addDof(b,0,0.9,1,0.0035,0); B[b].sb=T*T;
            for(int k=0;k<9;k++){ double f=base*ZR[k]*(1+0.04*bi()); if(f>17000) break; addDof(b,f,0.16/(1+f/9000.0),(rnd()<0.5?-1:1)*(0.6+0.8*rnd()),0.0035,1.0); } }
          double h0=0.0015*(0.7+0.6*rnd()); addContact(fr,z[0],h0,1.5e6,3.0); addContact(z[0],z[1],h0,2e7,0.6); addContact(z[1],fr,h0,1.5e6,3.0); first[p]=z[0]; }
      else { int sh=newBody(pan); double base=2100*pow(2.3,rnd()); for(int k=0;k<6;k++){ double f=base*SR[k]*(1+0.03*bi()); if(f>17000) break; addDof(sh,f,0.7/(1+f/7000.0),(k&1?-1:1)*(0.6+0.8*rnd()),0.012,1.0); }
          int pe=newBody(pan); addDof(pe,0,1.5,1,0.0012,0); B[pe].sb=T*T; double h0=0.004*(0.7+0.6*rnd()); addContact(sh,pe,h0,1.2e7,1.0); addContact(pe,sh,h0,1.2e7,1.0); }
    }
    out=calloc(2*S(dur),sizeof(float)); double bpm=122, six=60/bpm/4;
    for(long n=0;n<S(dur);n++){ double t=n/FS; double a;
      if(!sl){ double ph=t/six; int st=(int)ph; double acc=(st%4==0)?1.0:((st%4==2)?0.7:0.5); if(t>8) acc*= (st%16<12)?1:0.0; if(t<0.3||t>11.2) acc=0;
        double fr2=ph-st; a=acc*((st&1)?-1:1)*((fr2<0.22)?150*sin(M_PI*fr2/0.22):-150*0.22/0.78*0.0); a+= -acc*((st&1)?-1:1)*((fr2>=0.22)?150*0.22*2/M_PI/0.78:0)*0; }
      else { double ph=t/(six*4); double f=ph-floor(ph); double e=(t>0.2&&t<8.6)?1:0; a=e*60*(exp(-f*9)*sin(2*M_PI*f*3.0)); }
      fa=a; run(n,n+1); }
  }
  // ---------------- unreal: 26 hanging bars tuned to a chord, shaken gently for ever
  else if(!strcmp(sc,"chord")){
    dur=36; int N=26; static const double cs[8]={2,-2,2,-2,2,-2,2,-2}; static const int ch[7]={0,3,7,10,14,17,22}; // D minor 11
    for(int i=0;i<N;i++){ int oct=i/7, d=ch[i%7]; double f=146.83*pow(2,oct+d/12.0)*(1+0.0015*bi()); int j=(i*11)%N;
      makeBar(-0.9+1.8*j/(N-1.0),f,0.12*pow(300/f,0.5),1.6*(1+0.12*bi()),7,8,cs,4,1); }
    for(int i=0;i<N;i++) addContact(i,(i+1)%N,0.006*(1+0.3*bi()),4e6,0.5);
    out=calloc(2*S(dur),sizeof(float)); double w=0,w2=0;
    for(long n=0;n<S(dur);n++){ double t=n/FS; double g=fmin(1,t/6.0)*(t>28?fmax(0,1-(t-28)/2.0):1)*(0.6+0.4*sin(2*M_PI*t/13));
      w+=0.0004*(bi()*900-w); w2+=0.0004*(w-w2); fa=g*w2; // body-dependent phase comes from the swing detune
      run(n,n+1); }
  }
  else { fprintf(stderr,"unknown scene\n"); return 1; }
  long dofs=0; for(int i=0;i<nb;i++) dofs+=B[i].nd;
  fprintf(stderr,"%s: bodies %d, modes %ld, contacts %d, collisions %ld\n",sc,nb,dofs,nc,totalHits());
  FILE*f=fopen(argv[2],"wb"); fwrite(out,sizeof(float),2*S(dur),f); fclose(f); return 0;
}
