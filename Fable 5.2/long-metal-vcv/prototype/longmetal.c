// Long Metal prototype: a dispersive, periodically supported, bidirectional waveguide.
// Lossless core (normalised allpasses + orthogonal scattering junctions), explicit losses.
// Sources: strike, rolling train (moving axles), wind (wake oscillators), noise/audio.
// Output: raw float32 stereo at 48 kHz. L = waves arriving from the left, R = from the right.
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define SR 48000.0
#define NMAX 400
#define MMAX 6
#define DMAX 128
#define NAX 16
#define NWIND 16

typedef struct {
  // structure
  int N, M;
  double dlen;            // cell delay (samples, integer part used; per-cell jitter from age)
  double a0;              // allpass coefficient (0..-0.97): STIFF
  double ties;            // junction angle (rad): SUPPORTS
  double rhoL, rhoR;      // end reflections
  double g;               // per-cell gain (<1 loss, >1 active)
  double lowcut, lowf;    // per-cell low loss amount, corner (Hz)
  double hidamp, hif;     // per-cell high loss amount, corner
  double warp;            // signal-dependent dispersion
  double warpEnv;         // energy-dependent dispersion (pitch bend)
  double loose;           // amplitude-dependent junction angle
  double age;             // disorder 0..1
  double pick;            // pickup cell (float)
  double sat;             // saturation level for active line
  double grip, snap, gstuck, chatter; // break-loose threshold (energy), release burst, stuck gain
} P;

static P p;
static double dR[NMAX][DMAX], dL[NMAX][DMAX];
static int wp;
static double sR[NMAX][MMAX], sL[NMAX][MMAX];
static double llR[NMAX], llL[NMAX], lhR[NMAX], lhL[NMAX];
static double rOut[NMAX], lOut[NMAX], rIn[NMAX], lIn[NMAX], inj[NMAX];
static double envc[NMAX];
static double rndD[NMAX], rndA[NMAX], rndT[NMAX], rndG[NMAX];
static long guard = 0;
static int stuck[NMAX]; static double gl[NMAX], bz[NMAX]; static long nsnap=0; static double es[NMAX], bzd[NMAX]; static int rel[NMAX]; static long tm[NMAX];

static unsigned long long rs = 88172645463325252ULL;
static inline double urand(void){ rs ^= rs<<13; rs ^= rs>>7; rs ^= rs<<17; return (rs>>11)*(1.0/9007199254740992.0); }
static inline double nrand(void){ return urand()+urand()+urand()+urand()-2.0; }

static void line_init(void){
  memset(dR,0,sizeof dR); memset(dL,0,sizeof dL); memset(sR,0,sizeof sR); memset(sL,0,sizeof sL);
  memset(llR,0,sizeof llR); memset(llL,0,sizeof llL); memset(lhR,0,sizeof lhR); memset(lhL,0,sizeof lhL);
  memset(rOut,0,sizeof rOut); memset(lOut,0,sizeof lOut); memset(envc,0,sizeof envc); memset(inj,0,sizeof inj);
  for(int i=0;i<NMAX;i++){ rndD[i]=nrand(); rndA[i]=nrand(); rndT[i]=nrand(); rndG[i]=nrand(); }
  wp=0; for(int i=0;i<NMAX;i++){stuck[i]=1; gl[i]=-1; bz[i]=0;}
}

static inline void inject(double pos, double v){
  if(pos<0||pos>p.N-1) return;
  int i=(int)pos; double f=pos-i; if(i>=p.N-1){i=p.N-2;f=1;}
  inj[i]+=v*(1-f); inj[i+1]+=v*f;
}

static inline double softsat(double x, double lim){ return lim*tanh(x/lim); }

static void line_step(double *oL, double *oR){
  int N=p.N, M=p.M;
  double kl=1-exp(-2*M_PI*p.lowf/SR), kh=1-exp(-2*M_PI*p.hif/SR);
  for(int i=0;i<N;i++){
    int d=(int)(p.dlen + p.age*1.5*rndD[i] + 0.5); if(d<1)d=1; if(d>DMAX-1)d=DMAX-1;
    int rp=(wp-d)&(DMAX-1);
    double xr=dR[i][rp], xl=dL[i][rp];
    double a=p.a0*(1+0.08*p.age*rndA[i]);
    double e=xr*xr+xl*xl; envc[i]+=0.002*(e-envc[i]);
    double ar=a, al=a;
    if(p.warp!=0||p.warpEnv!=0){
      double be=p.warpEnv*envc[i]/(1+envc[i]);       // energy stiffening, bounded
      ar=a+be+p.warp*xr/(1+fabs(xr)); al=a+be+p.warp*xl/(1+fabs(xl));
    }
    if(ar>0.5)ar=0.5; if(ar<-0.985)ar=-0.985; if(al>0.5)al=0.5; if(al<-0.985)al=-0.985;
    double br=sqrt(1-ar*ar), bl=sqrt(1-al*al);
    for(int m=0;m<M;m++){
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
      double thr=p.grip*(1+0.6*p.age*rndT[i]);
      es[i]+=2e-5*(e-es[i]);
      if(stuck[i] && envc[i]>thr){ stuck[i]=0; rel[i]++; tm[i]=0; es[i]=thr;
        bz[i]=p.snap*sqrt(p.grip)*(rel[i]==2?1.4:1.0); bzd[i]=(rel[i]==2?0.9985:0.996); nsnap++; }
      else if(!stuck[i]){ tm[i]++;
        if(rel[i]==1 && p.chatter>0 && tm[i]>(long)(p.chatter*SR)) stuck[i]=1;      // one recoil: grip again, then let go for good
        else if(es[i]<thr*0.005) { stuck[i]=1; rel[i]=0; } }
      gl[i]+=0.004*((stuck[i]?p.gstuck:1.0)-gl[i]);
      g*=gl[i];
      if(bz[i]>1e-9){ inj[i]+=bz[i]*nrand(); bz[i]*=bzd[i]; }
    }
    xr*=g; xl*=g;
    if(p.g>1.0){ xr=softsat(xr,p.sat); xl=softsat(xl,p.sat); }
    rOut[i]=xr; lOut[i]=xl;
  }
  // junctions
  for(int i=0;i<N-1;i++){
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
  int k=(int)p.pick; double f=p.pick-k; if(k>=N-1){k=N-2;f=1;}
  *oL=rOut[k]*(1-f)+rOut[k+1]*f; *oR=lOut[k]*(1-f)+lOut[k+1]*f;
}
static inline double tapsum(int i){ return rOut[i]+lOut[i]; }

// ---------------- train ----------------
typedef struct { double off; double lp, hp, amp, ampT; double flat, wph; double lastpos; } Axle;
static Axle ax[NAX]; static int nax=0;
static double trainPos=1e9, trainSpeed=0;   // lead axle position (cells), cells/s (signed, negative = moving to lower index)
static double roughLevel=1, flatLevel=1, jointLevel=1, roughBright=0.35, wheelHz=7;
static int joints[8]; static int njoint=0; static double roughFloor=0.15, roughDens=120;
static void train_init(int n){
  nax=n; double o=0;
  for(int k=0;k<n;k++){ ax[k].off=o; o+=(k%2==0)?3.0:9.0+ (k%4==1?0:5.0); ax[k].lp=ax[k].hp=0; ax[k].amp=1; ax[k].ampT=1;
    ax[k].flat=(k%3==0)?1.0:(k%5==0?0.5:0.0); ax[k].wph=urand(); ax[k].lastpos=1e9; }
}
static void train_step(void){
  static int cnt=0; cnt++;
  for(int k=0;k<nax;k++){
    Axle*a=&ax[k];
    double pos=trainPos+(trainSpeed<0?a->off:-a->off);
    if((cnt&1023)==0) a->ampT=0.5+urand();           // roughness is patchy
    a->amp+=0.0005*(a->ampT-a->amp);
    double n=roughFloor*nrand(); if(urand()<roughDens/SR){ double u=urand(); n+=(urand()<0.5?-1:1)*12.0/(0.15+u*u*3); }
    a->lp+=roughBright*(n-a->lp); a->hp+=0.01*(a->lp-a->hp);
    double v=roughLevel*a->amp*(a->lp-a->hp)*0.5;
    a->wph+=wheelHz/SR*(1+0.01*k);
    if(a->wph>=1){ a->wph-=1; v+=flatLevel*a->flat*(0.6+0.8*urand())*6.0; }
    for(int j=0;j<njoint;j++){ double jp=joints[j];
      if((a->lastpos-jp)*(pos-jp)<0 && fabs(a->lastpos)<1e8) v+=jointLevel*14.0*(0.7+0.6*urand()); }
    a->lastpos=pos;
    inject(pos,v);
  }
  trainPos+=trainSpeed/SR;
}

// ---------------- wind ----------------
typedef struct { double q,v; int cell; double turb, turbT, sens; } Wake;
static Wake wk[NWIND]; static int nw=0; static double windHz=0, windLevel=0, windLock=0, windTurb=0.15;
static void wind_init(int n){ nw=n; for(int k=0;k<n;k++){ wk[k].q=0.01*nrand(); wk[k].v=0; wk[k].cell=(int)((k+0.5+0.3*nrand())*p.N/n); if(wk[k].cell<0)wk[k].cell=0; if(wk[k].cell>p.N-1)wk[k].cell=p.N-1; wk[k].turb=wk[k].turbT=0; wk[k].sens=(k%3==0?2.0:(k%3==1?1.0:1.5))*(0.9+0.2*urand()); } }
static void wind_step(void){
  static int cnt=0; cnt++;
  for(int k=0;k<nw;k++){ Wake*w=&wk[k];
    if((cnt&4095)==0) w->turbT=nrand();
    w->turb+=0.00008*(w->turbT-w->turb);
    double f=windHz*w->sens*(1+windTurb*w->turb); if(f<1)f=1; if(f>5000)f=5000;
    double Om=2*M_PI*f/SR;
    double y=tapsum(w->cell);
    double acc=-Om*Om*w->q + 0.3*Om*(1-w->q*w->q)*w->v + windLock*Om*Om*tanh(2*y)*0.5 + 1e-4*Om*Om*nrand();
    w->v+=acc; w->q+=w->v;
    if(fabs(w->q)>8){w->q=0;w->v=0;guard++;}
    inject(w->cell, windLevel*w->q*0.02*(0.6+0.4*w->turb));
  }
}

// ---------------- strike ----------------
static double stPos=0, stAmp=0; static int stN=0, stLen=1;
static void strike(double pos,double amp,double ms){ stPos=pos; stAmp=amp; stLen=(int)(ms*0.001*SR); if(stLen<1)stLen=1; stN=stLen; }
static void strike_step(void){ if(stN>0){ double ph=(double)(stLen-stN+0.5)/stLen; inject(stPos, stAmp*0.5*(1-cos(2*M_PI*ph))/stLen*8.0); stN--; } }

static double lerp(double a,double b,double x){ if(x<0)x=0; if(x>1)x=1; return a+(b-a)*x; }
static double sm(double x){ if(x<0)x=0; if(x>1)x=1; return x*x*(3-2*x); }

#include "scenes.h"

int main(int argc,char**argv){
  if(argc<3){ fprintf(stderr,"usage: longmetal scene out.f32\n"); return 1; }
  int sc=atoi(argv[1]); FILE*f=fopen(argv[2],"wb");
  line_init();
  double dur=scene_setup(sc);
  long n=(long)(dur*SR); float buf[2]; double pk=0;
  for(long t=0;t<n;t++){
    if((t&15)==0) scene_ctl(sc,t/SR);
    if(getenv("DBG") && t%(long)(SR)==0){ int fr=0; for(int i=0;i<p.N;i++) fr+=!stuck[i]; fprintf(stderr,"t=%ld env@pick %.3g free %d\n",t/(long)SR,envc[(int)p.pick],fr); }
    scene_audio(sc,t);
    double l,r; line_step(&l,&r);
    buf[0]=(float)l; buf[1]=(float)r; if(fabs(l)>pk)pk=fabs(l); if(fabs(r)>pk)pk=fabs(r);
    fwrite(buf,sizeof(float),2,f);
  }
  fclose(f); fprintf(stderr,"scene %d: %.1fs peak %.3g guard %ld snaps %ld\n",sc,dur,pk,guard,nsnap);
  return 0;
}
