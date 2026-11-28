// Electricity prototype: one circuit, simulated at 16x.
//   source (DC + mains AC + feedback) -> ballast Rs + choke Ls -> gap node (stray Cp)
//   across the gap: the arc (Mayr conductance, breakdown, ionisation memory, stretch/climb, turbulence)
//                   a series L-C tank (the tuned circuit: zap ring / Duddell singing arc)
//                   corona micro-discharges
//   sound = d/dt of arc power (thermoacoustic) + field pickup (currents) + transformer hum body
// usage: arc <scene> <out.f32>      (mono float32 @48k written as stereo pairs)
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#define SR 48000
#define OS 16
#define NT (OS*24)
static unsigned long long rs=88172645463325252ULL;
static inline double frand(void){ rs^=rs<<13; rs^=rs>>7; rs^=rs<<17; return (double)(rs>>11)*(1.0/9007199254740992.0); }
static inline double nrand(void){ return (frand()+frand()+frand()+frand()-2.0)*1.7320508; }

typedef struct {
  double Edc, Eac, fm;      // source
  double cur;               // current = 1/Rs
  double choke;             // Ls/Rs seconds
  double gap;               // electrode spacing (1 = strikes at 1 V)
  double ft, Zt, Qt;        // tank
  double tau;               // arc thermal time constant (s)
  double P0;                // arc power per unit length
  double turb;              // turbulence
  double climb;             // Jacob's ladder climb rate (gap units / s)
  double mem;               // ionisation memory time (s)
  double jit;               // strike jitter
  double corona;            // corona rate
  double fb;                // acoustic feedback into source
  double fbdel;             // feedback delay (s)
  double hum;               // transformer hum level
  double sat;               // core saturation
  double arcl, fieldl;      // output mix
  double fp;                // stray resonance Hz
  double iron;              // tank inductor saturation (pitch falls as a ring decays)
  int trig;                 // force a breakdown this sample (triggered gap)
  double goff;              // quench: conductance below which the arc goes out
  int key;                  // key the tank: swap in a discharged capacitor (starts a note)
  double restr;             // how much a hot gap lowers the restrike voltage
} P;

static double seg(double t,double t0,double t1,double a,double b){ if(t<=t0) return a; if(t>=t1) return b; return a+(b-a)*(t-t0)/(t1-t0); }
static double pulse(double t,double t0,double len){ return (t>=t0 && t<t0+len)?1.0:0.0; }
#include "scenes.h"

static double fir[NT];
static void mkfir(void){ double fc=19000.0/(SR*OS), s=0; for(int i=0;i<NT;i++){ double x=i-(NT-1)/2.0; double w=0.42-0.5*cos(2*M_PI*i/(NT-1))+0.08*cos(4*M_PI*i/(NT-1)); double h=(x==0)?2*fc:sin(2*M_PI*fc*x)/(M_PI*x); fir[i]=h*w; s+=fir[i]; } for(int i=0;i<NT;i++) fir[i]/=s; }
typedef struct { double b[NT]; int w; } Dec;
static inline void dpush(Dec*d,double x){ d->b[d->w]=x; d->w=(d->w+1)%NT; }
static inline double dout(Dec*d){ double s=0; int k=d->w; for(int i=0;i<NT;i++){ s+=fir[i]*d->b[k]; if(++k==NT) k=0; } return s; }
typedef struct { double y1,y2,a1,a2,b0; } Res;
static void rset(Res*r,double f,double q){ double w=2*M_PI*f/SR, R=exp(-w/(2*q)); r->a1=2*R*cos(w); r->a2=-R*R; r->b0=(1-R*R)*0.5; }
static inline double rproc(Res*r,double x){ double y=r->b0*x+r->a1*r->y1+r->a2*r->y2; r->y2=r->y1; r->y1=y; return y; }

int main(int argc,char**argv){
  int sc=atoi(argv[1]); FILE*fo=fopen(argv[2],"wb"); mkfir();
  double dur=scene_dur(sc); long N=(long)(dur*SR); double dt=1.0/(SR*OS);
  static Dec dP,dIs,dIt,dCor; double is=0,vg=0,it=0,vct=0,g=1e-7,h=0,c=0,jz=0; int on=0;
  double tn=0,tslow=0,ph=0,phflux=0; double pprev=0,fprev=0,hp1=0,hp2=0,lp=0;
  static double fbbuf[SR]; int fbw=0; double fbv=0,fblp=0;
  Res hr[6]; double hf[6]={100,200,300,417,733,1180}, hq[6]={30,25,20,18,14,10}, hg[6]={1.0,0.8,0.5,0.5,0.35,0.3};
  for(int k=0;k<6;k++){ memset(&hr[k],0,sizeof(Res)); }
  P p; float *out=malloc(sizeof(float)*2*N); double dbg_on=0;
  for(long n=0;n<N;n++){
    double t=(double)n/SR; scene(sc,t,&p);
    double Rs=1.0/p.cur, Ls=Rs*p.choke;
    double Lt=p.Zt/(2*M_PI*p.ft), Ct=1.0/(2*M_PI*p.ft*p.Zt), Rt=p.Zt/p.Qt;
    double Lpar=Lt*Ls/(Lt+Ls); double Cp=1.0/((2*M_PI*p.fp)*(2*M_PI*p.fp)*Lpar); double gp=0.15*sqrt(Cp/Lpar);
    double corsum=0;
    if(p.key){ vct=0; it=0; }
    for(int o=0;o<OS;o++){
      ph+=p.fm*dt; if(ph>1) ph-=1;
      double sn=sin(2*M_PI*ph);
      // saturating transformer: flux lags by 90deg, magnetising current spikes pull the peaks down
      double fl=-cos(2*M_PI*ph); double imag=p.sat*pow(fl,9);
      double e=p.Edc + p.Eac*(sn - 0.25*imag) + fbv;
      // turbulence
      tn+= (nrand()-tn)*(2*M_PI*900*dt); { double a=2*M_PI*4*dt; tslow+= -tslow*a + sqrt(2*a)*nrand(); }
      double len=p.gap + c; double lt=len*exp(p.turb*(4.0*tslow+16.0*tn)); if(lt<0.004) lt=0.004;
      double Pw=p.P0*lt;
      double Vb=lt*(1.0+p.jit*jz)*(1.0-p.restr*h); if(Vb<0.01) Vb=0.01;
      // corona: micro-discharges, more and smaller on the negative half
      if(p.corona>0){ double av=fabs(vg), Vc=0.55*lt; double x=(av-Vc)/(0.08*lt+1e-9); if(x>-6){ if(x>3) x=3; double rate=p.corona*(vg<0?9000.0:2500.0)*exp(x); if(frand()<rate*dt){ double q=(vg<0?0.04:0.16)*(0.4+frand())*av; vg-= (vg<0?-1:1)*q; corsum+=q*(vg<0?1:-1.6);} } }
      // circuit
      is=(is+dt/Ls*(e-vg))/(1.0+dt*Rs/Ls);
      it+=dt/Lt*(1.0+p.iron*it*it)*(vg-vct-Rt*it); vct+=dt/Ct*it;
      vg=(vg+dt/Cp*(is-it))/(1.0+dt*(g+gp)/Cp);
      double ia=g*vg, pw=vg*ia;
      // arc
      if(!on){ if(fabs(vg)>Vb || (p.trig&&o==0)){ on=1; g=0.4+0.4*frand(); } }
      else { // Mayr arc with Schwarz exponents: thin arcs cool faster and need less power
        double sg=sqrt(g); double tg=p.tau*sg; if(tg<2*dt) tg=2*dt; double ex=dt/tg*(pw/(Pw*sg)-1.0); if(ex>0.5) ex=0.5; if(ex<-0.5) ex=-0.5; g*=exp(ex); if(g>400) g=400; if(g<p.goff){ on=0; g=1e-7; jz=nrand(); } }
      // ionisation memory and climb
      if(on) h+=(1-h)*dt/0.002; else h-=h*dt/p.mem;
      if(h>0.08) c+=p.climb*h*dt; else c-=c*dt/0.004;
      dpush(&dP,pw); dpush(&dIs,is); dpush(&dIt,it); dbg_on+=on;
    }
    double pw=dout(&dP), isd=dout(&dIs), itd=dout(&dIt);
    // thermoacoustic: pressure ~ d(power)/dt, plus a little of the power itself (the thump)
    double dp=(pw-pprev)*SR/700.0; pprev=pw; hp1+=(pw-hp1)*(2*M_PI*25.0/SR); double thump=pw-hp1;
    double arc=dp+0.1*thump + corsum*6.0;
    // field pickup: di/dt of the loop currents
    double fcur=isd+0.6*itd; double fld=(fcur-fprev)*SR/2000.0; fprev=fcur; hp2+=(fcur-hp2)*(2*M_PI*30.0/SR); fld+=0.15*(fcur-hp2);
    // transformer body: magnetostriction (flux^2 -> 2f) + winding force (i^2), rattling laminations
    phflux=ph; double fl=-cos(2*M_PI*phflux); double sfl=tanh((1.0+2.5*p.sat)*fl);
    double drive=(p.Eac>0?sfl*sfl:0) + 0.4*isd*isd; double rat=drive>0.75?(drive-0.75)*3.0*(0.5+frand()):0;
    if(n==0) for(int k=0;k<6;k++) rset(&hr[k],hf[k]*p.fm/50.0,hq[k]);
    double hm=0; for(int k=0;k<6;k++) hm+=hg[k]*rproc(&hr[k],drive+rat);
    double y=p.arcl*arc+p.fieldl*fld+p.hum*hm*0.6;
    if(getenv("SOLO")){ int so=atoi(getenv("SOLO")); y= so==1?p.arcl*arc: so==2?p.fieldl*fld: so==3?p.hum*hm*0.6: so==4?vg: so==5?isd: itd; }
    // acoustic feedback (the arc hears itself): delayed, band-limited, into the source
    fbbuf[fbw]=y; int d=(int)(p.fbdel*SR); if(d<1)d=1; int r=(fbw-d+SR)%SR; fbw=(fbw+1)%SR;
    fblp+=(fbbuf[r]-fblp)*0.35; fbv=p.fb*tanh(fblp*0.5);
    lp=y; out[2*n]=out[2*n+1]=(float)lp;
  }
  fwrite(out,sizeof(float),2*N,fo); fclose(fo);
  fprintf(stderr,"scene %d: arc on %.1f%% of the time\n",sc,100.0*dbg_on/((double)N*OS));
  return 0;
}
