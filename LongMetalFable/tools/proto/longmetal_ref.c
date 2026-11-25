// Long Metal reference engine (Fable, 2026-11-25). Plain C, no dependencies.
//   gcc -O2 -o lm longmetal_ref.c -lm
//   ./lm rail|zaps|hard|live|live2 out.f32        raw float32 stereo, 48 kHz
//   ./lm fuzz <seed> out.f32                      random patches, must print guard 0
//   ./lm lossless                                 energy conservation check
// A dispersive, periodically supported, bidirectional waveguide. The core (normalised allpasses +
// orthogonal scattering junctions) is lossless for ANY time-varying coefficient, so the pushed
// controls (WARP, SWELL, LOOSE) cannot add energy. Loss, gain and saturation are explicit.
// L out = right-going wave at the listener (sound arriving from the left), R = left-going.
// This file is the definition of the sound. The engine is defined at a nominal 48 kHz;
// PITCH re-clocks it (see BUILD-INSTRUCTIONS.md section 3.9).
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define SR 48000.0
#define NMAX 320        // cells
#define MMAX 4          // allpasses per cell per direction
#define DMAX 64         // cell delay buffer (power of two); d is 1..40
#define NAX 8           // axles

typedef struct {
  int N, M;               // LENGTH (16..320), GAUGE (3 or 4)
  int d;                  // SPACING: 1..40. NOTE the buffer indexing makes the true cell delay d+1 samples; keep it so
  double a0;              // STIFF: allpass coefficient, 0 .. -0.95
  double ties;            // SUPPORTS: junction angle, rad
  double rhoL, rhoR;      // END A, END B
  double g;               // LIVE: per-cell gain (<1 loss, >1 active)
  double lowcut, lowf;    // GROUND: per-cell low loss amount; corner 500 Hz
  double hidamp, hif;     // BRIGHT: per-cell high loss amount; corner 6 kHz
  double warp;            // WARP: signal-dependent dispersion
  double swell;           // SWELL: energy-dependent dispersion
  double loose;           // LOOSE: amplitude-dependent junction angle
  double grip, snap, gstuck; // GRIP: break-loose threshold (energy); release burst; stuck gain
  double age;             // AGE 0..1
  double pick;            // LISTEN: listener cell (float)
  double sat;             // ceiling of the active line (0.6)
} P;

static P p;
static double dR[NMAX][DMAX], dL[NMAX][DMAX];
static int wp;
static double sR[NMAX][MMAX], sL[NMAX][MMAX];
static double llR[NMAX], llL[NMAX], lhR[NMAX], lhL[NMAX];
static double rOut[NMAX], lOut[NMAX], rIn[NMAX], lIn[NMAX], inj[NMAX];
static double envc[NMAX], es[NMAX], gl[NMAX], bz[NMAX];
static int stuck[NMAX];
static double rndD[NMAX], rndA[NMAX], rndT[NMAX], rndG[NMAX];
static long guard = 0, nsnap = 0;

static unsigned long long rs = 88172645463325252ULL;
static inline double urand(void){ rs ^= rs<<13; rs ^= rs>>7; rs ^= rs<<17; return (rs>>11)*(1.0/9007199254740992.0); }
static inline double nrand(void){ return urand()+urand()+urand()+urand()-2.0; }

static void line_init(void){
  memset(dR,0,sizeof dR); memset(dL,0,sizeof dL); memset(sR,0,sizeof sR); memset(sL,0,sizeof sL);
  memset(llR,0,sizeof llR); memset(llL,0,sizeof llL); memset(lhR,0,sizeof lhR); memset(lhL,0,sizeof lhL);
  memset(rOut,0,sizeof rOut); memset(lOut,0,sizeof lOut); memset(envc,0,sizeof envc); memset(inj,0,sizeof inj);
  for(int i=0;i<NMAX;i++){ rndD[i]=nrand(); rndA[i]=nrand(); rndT[i]=nrand(); rndG[i]=nrand(); }   // this order
  wp=0; for(int i=0;i<NMAX;i++){ stuck[i]=1; gl[i]=-1; bz[i]=0; es[i]=0; }
}
static void grip_reset(void){ for(int i=0;i<NMAX;i++){ stuck[i]=1; gl[i]=-1; es[i]=0; } }

static inline void inject(double pos, double v){          // off the line = silent
  if(pos<0||pos>p.N-1) return;
  int i=(int)pos; double f=pos-i; if(i>=p.N-1){i=p.N-2;f=1;}
  inj[i]+=v*(1-f); inj[i+1]+=v*f;
}
static inline double softsat(double x, double lim){ return lim*tanh(x/lim); }

static void line_step(double *oL, double *oR){
  int N=p.N, M=p.M;
  double kl=1-exp(-2*M_PI*p.lowf/SR), kh=1-exp(-2*M_PI*p.hif/SR);
  for(int i=0;i<N;i++){
    int d=(int)(p.d + p.age*1.5*rndD[i] + 0.5); if(d<1)d=1; if(d>DMAX-1)d=DMAX-1;
    int rp=(wp-d)&(DMAX-1);
    double xr=dR[i][rp], xl=dL[i][rp];
    double a=p.a0*(1+0.08*p.age*rndA[i]);
    double e=xr*xr+xl*xl; envc[i]+=0.002*(e-envc[i]);
    double ar=a, al=a;
    if(p.warp!=0||p.swell!=0){
      double be=p.swell*envc[i]/(1+envc[i]);              // energy stiffening, bounded
      ar=a+be+p.warp*xr/(1+fabs(xr)); al=a+be+p.warp*xl/(1+fabs(xl));
    }
    if(ar>0.5)ar=0.5; if(ar<-0.985)ar=-0.985; if(al>0.5)al=0.5; if(al<-0.985)al=-0.985;
    double br=sqrt(1-ar*ar), bl=sqrt(1-al*al);
    for(int m=0;m<M;m++){                                 // normalised allpass: an orthogonal rotation of (x, state)
      double y=ar*xr+br*sR[i][m]; sR[i][m]=br*xr-ar*sR[i][m]; xr=y;
      y=al*xl+bl*sL[i][m]; sL[i][m]=bl*xl-al*sL[i][m]; xl=y;
    }
    llR[i]+=kl*(xr-llR[i]); xr-=p.lowcut*llR[i];
    llL[i]+=kl*(xl-llL[i]); xl-=p.lowcut*llL[i];
    lhR[i]+=kh*(xr-lhR[i]); xr+=p.hidamp*(lhR[i]-xr);
    lhL[i]+=kh*(xl-lhL[i]); xl+=p.hidamp*(lhL[i]-xl);
    double g=p.g*(1-0.004*p.age*fabs(rndG[i]));
    if(p.grip>0){
      if(gl[i]<0) gl[i]=p.gstuck;
      double thr=p.grip*(1+0.6*p.age*rndT[i]); if(thr<0.1*p.grip) thr=0.1*p.grip;
      es[i]+=2e-5*(e-es[i]);                              // 1 s envelope decides re-gripping
      if(stuck[i] && envc[i]>thr){ stuck[i]=0; es[i]=thr; bz[i]=p.snap*sqrt(p.grip); nsnap++; }
      else if(!stuck[i] && es[i]<thr*0.005) stuck[i]=1;
      gl[i]+=0.004*((stuck[i]?p.gstuck:1.0)-gl[i]);
      g*=gl[i];
      if(bz[i]>1e-9){ inj[i]+=bz[i]*nrand(); bz[i]*=0.996; }
    }
    xr*=g; xl*=g;
    if(p.g>1.0){ xr=softsat(xr,p.sat); xl=softsat(xl,p.sat); }
    rOut[i]=xr; lOut[i]=xl;
  }
  for(int i=0;i<N-1;i++){                                 // supports: orthogonal scattering
    double th=p.ties*(1+0.5*p.age*rndT[i]);
    if(p.loose!=0){ double amp=fabs(rOut[i])+fabs(lOut[i+1]); th+=p.loose*amp/(1+amp); }
    double c=cos(th), s=sin(th);
    rIn[i+1]= c*rOut[i]+s*lOut[i+1];
    lIn[i]  =-s*rOut[i]+c*lOut[i+1];
  }
  rIn[0]=p.rhoL*lOut[0]; lIn[N-1]=p.rhoR*rOut[N-1];
  wp=(wp+1)&(DMAX-1);
  for(int i=0;i<N;i++){
    double r=rIn[i]+0.5*inj[i], l=lIn[i]+0.5*inj[i]; inj[i]=0;
    if(!(fabs(r)<1e6)){r=0;guard++;} if(!(fabs(l)<1e6)){l=0;guard++;}
    dR[i][wp]=r; dL[i][wp]=l;
  }
  int k=(int)p.pick; double f=p.pick-k; if(k<0){k=0;f=0;} if(k>=N-1){k=N-2;f=1;}
  *oL=rOut[k]*(1-f)+rOut[k+1]*f; *oR=lOut[k]*(1-f)+lOut[k+1]*f;
}
static double total_energy(void){                          // exact when lowcut = hidamp = 0
  double E=0; for(int i=0;i<p.N;i++){
    int d=(int)(p.d + p.age*1.5*rndD[i] + 0.5); if(d<1)d=1; if(d>DMAX-1)d=DMAX-1;
    for(int k=0;k<=d;k++){ int q=(wp-k)&(DMAX-1);   // a sample written now is read d+1 steps later: the cell delay is d+1
      E+=dR[i][q]*dR[i][q]+dL[i][q]*dL[i][q]; }
    for(int m=0;m<p.M;m++) E+=sR[i][m]*sR[i][m]+sL[i][m]*sL[i][m]; }
  return E;
}

// ---------------- train: 8 axles in bogie pairs ----------------
typedef struct { double off; double lp, hp, amp, ampT; double flat, wph; double lastpos; } Axle;
static Axle ax[NAX]; static int nax=0;
static double trainPos=1e9, trainSpeed=0;   // lead axle (cells), cells/s; trailing axles sit on the side the train came from
static double trainSide=1;                  // +1: axles trail toward higher cells
static double roughLevel=0, flatLevel=0, jointLevel=0, roughBright=0.35, wheelHz=5.5, roughFloor=0.15, roughDens=120;
static int joints[3]; static int njoint=0;
static void train_init(int n){
  nax=n; double o=0;
  for(int k=0;k<n;k++){ ax[k].off=o; o+=(k%2==0)?3.0:9.0+(k%4==1?0:5.0); ax[k].lp=ax[k].hp=0; ax[k].amp=1; ax[k].ampT=1;
    ax[k].flat=(k%3==0)?1.0:(k%5==0?0.5:0.0); ax[k].wph=urand(); ax[k].lastpos=1e9; }
}
static void train_step(void){
  static int cnt=0; cnt++;
  for(int k=0;k<nax;k++){
    Axle*a=&ax[k];
    double pos=trainPos+trainSide*a->off;
    if((cnt&1023)==0) a->ampT=0.5+urand();                 // roughness is patchy
    a->amp+=0.0005*(a->ampT-a->amp);
    double n=roughFloor*nrand(); if(urand()<roughDens/SR){ double u=urand(); n+=(urand()<0.5?-1:1)*12.0/(0.15+u*u*3); }  // grainy
    a->lp+=roughBright*(n-a->lp); a->hp+=0.01*(a->lp-a->hp);
    double v=roughLevel*a->amp*(a->lp-a->hp)*0.5;
    a->wph+=wheelHz/SR*(1+0.01*k);
    if(a->wph>=1){ a->wph-=1; v+=flatLevel*a->flat*(0.6+0.8*urand())*6.0; }          // wheel flat
    for(int j=0;j<njoint;j++){ double jp=joints[j];
      if((a->lastpos-jp)*(pos-jp)<0 && fabs(a->lastpos)<1e8) v+=jointLevel*14.0*(0.7+0.6*urand()); }  // joint clack
    a->lastpos=pos;
    inject(pos,v);
  }
  trainPos+=trainSpeed/SR;
}

// ---------------- strike ----------------
static double stPos=0, stAmp=0; static int stN=0, stLen=1;
static void strike(double pos,double amp,double ms){ stPos=pos; stAmp=amp; stLen=(int)(ms*0.001*SR); if(stLen<1)stLen=1; stN=stLen; }
static void strike_step(void){ if(stN>0){ double ph=(double)(stLen-stN+0.5)/stLen; inject(stPos, stAmp*0.5*(1-cos(2*M_PI*ph))/stLen*8.0); stN--; } }

static double lerp(double a,double b,double x){ if(x<0)x=0; if(x>1)x=1; return a+(b-a)*x; }
static double sm(double x){ if(x<0)x=0; if(x>1)x=1; return x*x*(3-2*x); }

#include "scenes.h"

int main(int argc,char**argv){
  if(argc<2){ fprintf(stderr,"usage: lm scene out.f32 | lm fuzz seed out.f32 | lm lossless\n"); return 1; }
  int sc=scene_id(argv[1]); if(sc<0){ fprintf(stderr,"unknown scene\n"); return 1; }
  const char*outp=argc>2?argv[2]:"/dev/null";
  if(sc==SC_FUZZ){ if(argc<4) return 1; rs^=(unsigned long long)atoll(argv[2])*0x9E3779B97F4A7C15ULL; outp=argv[3]; }
  FILE*f=fopen(outp,"wb");
  line_init();
  double dur=scene_setup(sc);
  long n=(long)(dur*SR); float buf[2]; double pk=0, E0=0, Emin=1e300, Emax=0;
  for(long t=0;t<n;t++){
    if((t&15)==0) scene_ctl(sc,t/SR);
    scene_audio(sc,t);
    double l,r; line_step(&l,&r);
    if(sc==SC_LOSSLESS && t>=4800 && (t%480)==0){ double E=total_energy(); if(t==4800)E0=E; if(E<Emin)Emin=E; if(E>Emax)Emax=E; }
    buf[0]=(float)l; buf[1]=(float)r; if(fabs(l)>pk)pk=fabs(l); if(fabs(r)>pk)pk=fabs(r);
    fwrite(buf,sizeof(float),2,f);
  }
  fclose(f);
  fprintf(stderr,"%s: %.1fs peak %.3g guard %ld snaps %ld\n",argv[1],dur,pk,guard,nsnap);
  if(sc==SC_LOSSLESS) fprintf(stderr,"energy: start %.6g min %.6g max %.6g (max/min-1 = %.2e)\n",E0,Emin,Emax,Emax/Emin-1);
  return 0;
}
