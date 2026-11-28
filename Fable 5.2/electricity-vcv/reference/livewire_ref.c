// Live Wire reference engine. Plain C, no dependencies.
//   gcc -O2 -o livewire livewire_ref.c -lm
//   ./livewire <scene> <out.f32>           scenes: arc sing kit kit2 sub        (stereo float32, 48 kHz)
//   ./livewire fuzz <seed> <out.f32>       every knob random, steps every 0.5 s
// One circuit, simulated at OS x the host rate:
//   source (DC + mains AC + surge + audio in + feedback) -> series R (ballast, loose connection) + choke -> gap node (stray C)
//   across the gap: the arc (Mayr conductance with Schwarz exponents, breakdown, ionisation memory, wind, climb),
//                   a series L-C tank with a saturating inductor, corona micro-discharges
//   sound: ARC = d/dt of arc power, FIELD = d/dt of loop currents, HUM = transformer body
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "mapping.h"
#define MAXOS 16
#define MAXNT (MAXOS*24)

typedef struct {            // panel state, everything 0..1 unless noted
  double volts, mains, freq, current, choke;
  double gap, heat, cool, quench, memory, restrike, jitter;
  double wind, climb;
  double tune, ring, load, iron;
  double corona, hum, fb, dist, age;
  double arc, field, out, draw, in;
  int track;                // COOL follows TUNE
  double voct, voctMains;   // octaves
  int trig, surge;          // one-sample pulses
  int key, keyPatched;      // tank key gate
  int touch;                // touch gate
} K;

typedef struct { double b[MAXNT]; int w; } Dec;
typedef struct { double y1,y2,a1,a2,b0; } Res;
typedef struct {
  double sr; int os, nt; double fir[MAXNT];
  unsigned long long rs;
  K s;                      // smoothed knobs
  int first;
  double is,vg,it,vct,g,h,c,jz,path; int on;
  double tf,ts,ts2,ts3,ph;
  double gapEff, surgeEnv; int keyPrev;
  long lag;                 // pending triggered breakdown, in engine samples (-1 none)
  double looseR; long looseN;
  Dec dP,dIs,dIt;
  double pprev,fprev,hp1,hp2,dcL,dcR;
  Res hr[2][6]; double humf;
  double *fbbuf; int fbn,fbw; double fbv,fblp;
  double pan;
  long strikes; long guard;               // count of state clamps (must stay 0 outside the fuzz)
  double dbgOn;
} LW;

static inline double frand(LW*e){ e->rs^=e->rs<<13; e->rs^=e->rs>>7; e->rs^=e->rs<<17; return (double)(e->rs>>11)*(1.0/9007199254740992.0); }
static inline double nrand(LW*e){ return (frand(e)+frand(e)+frand(e)+frand(e)-2.0)*1.7320508; }
static void rset(Res*r,double f,double q,double sr){ if(f>0.45*sr) f=0.45*sr; double w=2*M_PI*f/sr, R=exp(-w/(2*q)); r->a1=2*R*cos(w); r->a2=-R*R; r->b0=(1-R*R)*0.5; }
static inline double rproc(Res*r,double x){ double y=r->b0*x+r->a1*r->y1+r->a2*r->y2; r->y2=r->y1; r->y1=y; return y; }
static inline void dpush(Dec*d,double x,int nt){ d->b[d->w]=x; if(++d->w==nt) d->w=0; }
static inline double dout(Dec*d,const double*fir,int nt){ double s=0; int k=d->w; for(int i=0;i<nt;i++){ s+=fir[i]*d->b[k]; if(++k==nt) k=0; } return s; }

static void lw_init(LW*e,double sr,unsigned long long seed){
  memset(e,0,sizeof(LW)); e->sr=sr; e->os= sr<=50000?16:(sr<=100000?8:4); e->nt=e->os*24;
  double fc=(0.396*sr<19000?0.396*sr:19000)/(sr*e->os), s=0;   // cutoff 19 kHz (0.396 x host rate below 48 k)
  for(int i=0;i<e->nt;i++){ double x=i-(e->nt-1)/2.0; double w=0.42-0.5*cos(2*M_PI*i/(e->nt-1))+0.08*cos(4*M_PI*i/(e->nt-1)); double h=(x==0)?2*fc:sin(2*M_PI*fc*x)/(M_PI*x); e->fir[i]=h*w; s+=e->fir[i]; }
  for(int i=0;i<e->nt;i++) e->fir[i]/=s;
  e->rs=seed*2654435761ULL+88172645463325252ULL; for(int i=0;i<8;i++) frand(e);
  e->g=1e-7; e->first=1; e->lag=-1; e->path=1; e->pan=0.5;
  e->fbn=(int)(0.021*sr)+4; e->fbbuf=calloc(e->fbn,sizeof(double));
}

static const double HF[6]={2,4,6,417,733,1180};       // first three are multiples of the mains frequency, the rest are case modes in Hz
static const double HQ[6]={30,25,20,18,14,10};
static const double HGL[6]={1.0,0.8,0.5,0.62,0.26,0.36}, HGR[6]={1.0,0.72,0.6,0.40,0.44,0.24};

// one host sample. in: audio in, 1.0 = 5 V. outputs are before the x5 V scaling.
static void lw_process(LW*e,const K*k,double in,double*L,double*R,double*oArc,double*oField){
  double sr=e->sr; int OS=e->os; double dt=1.0/(sr*OS);
  // ---- knob smoothing, 5 ms one-pole on the 0..1 values
  { double a=e->first?1.0:1.0-exp(-1.0/(0.005*sr)); double *d=(double*)&e->s, *q=(double*)k; int n=28; for(int i=0;i<n;i++) d[i]+=(q[i]-d[i])*a; }
  if(e->first){ e->gapEff=M_GAP(k->gap); e->first=0; }
  const K*s=&e->s; double age=s->age;
  // ---- slow random processes (unit variance), once per host sample
  { double a=2*M_PI*900/sr; if(a>1)a=1; e->tf+=-e->tf*a+sqrt(a*(2-a))*nrand(e);
    a=2*M_PI*4/sr;   e->ts +=-e->ts *a+sqrt(2*a)*nrand(e);
    a=2*M_PI*0.7/sr; e->ts2+=-e->ts2*a+sqrt(2*a)*nrand(e);
    a=2*M_PI*0.15/sr;e->ts3+=-e->ts3*a+sqrt(2*a)*nrand(e); }
  // ---- mapping
  double E=M_VOLTS(s->volts)*(1.0+0.05*age*e->ts2);
  if(k->surge) e->surgeEnv=1.0; e->surgeEnv*=exp(-1.0/(0.06*sr));
  E*=1.0+2.0*e->surgeEnv;
  double Edc=E*(1.0-s->mains), Eac=E*s->mains;
  double fm=M_FREQ(s->freq)*pow(2.0,k->voctMains)*(1.0+0.004*age*e->ts3); fm=lw_clamp(fm,5.0,2000.0);
  double cur=M_CUR(s->current)*(1.0+e->surgeEnv), Rs=1.0/cur, Ls=Rs*M_CHOKE(s->choke);
  double ft=lw_clamp(M_TUNE(s->tune)*pow(2.0,k->voct)*(1.0+0.006*age*e->ts3),16.0,0.4*sr); if(ft>18000) ft=18000;
  double Zt=M_LOAD(s->load), Qt=M_RING(s->ring)*(1.0-0.5*age);
  int keyOn=k->keyPatched?k->key:1; if(!keyOn) Qt=1.2; if(Qt<0.6) Qt=0.6;
  if(k->keyPatched && k->key && !e->keyPrev){ e->vct=0; e->it=0; }   // a key swaps in a discharged capacitor
  e->keyPrev=k->key;
  double Lt=Zt/(2*M_PI*ft), Ct=1.0/(2*M_PI*ft*Zt), Rt=Zt/Qt;
  double iron=M_IRON(s->iron); double ironMax=0.5/(2*M_PI*ft*dt); ironMax=ironMax*ironMax; if(ironMax>16) ironMax=16; // saturation raises the tank pitch at most 4x, and never above 0.08 x engine rate
  double fp=17000.0*(1.0-0.45*age);
  double Lpar=Lt*Ls/(Lt+Ls), Cp=1.0/((2*M_PI*fp)*(2*M_PI*fp)*Lpar), gp=0.15*sqrt(Cp/Lpar);
  double tau=M_COOL(s->cool)*(k->track?261.6256/ft:1.0); if(tau<4e-6) tau=4e-6; if(tau>0.02) tau=0.02;
  double P0=M_HEAT(s->heat), goff=M_QUENCH(s->quench), mem=M_MEM(s->memory), restr=M_RESTR(s->restrike);
  double jit=lw_clamp(M_JIT(s->jitter)+0.3*age,0,0.9), wind=M_WIND(s->wind)+0.01*age, climb=M_CLIMB(s->climb);
  double corona=M_CORONA(s->corona), fb=M_FB(s->fb);
  // ---- touch and draw: a touch closes the gap fast, release pulls the electrodes apart at a steady rate
  { double gk=M_GAP(s->gap); if(k->touch) e->gapEff+=(0.012-e->gapEff)*(1.0-exp(-1.0/(0.015*sr)));
    else { e->gapEff+=M_GAP(1.0)/(M_DRAW(s->draw)*sr); } if(e->gapEff>gk) e->gapEff=gk; }
  double len=(e->gapEff+e->c)*e->path; double lt=len*exp(wind*(4.0*e->ts+0.97*e->tf)); if(lt<0.004) lt=0.004;
  double Pw=P0*lt, VbCold=lt*(1.0+jit*e->jz); if(VbCold<0.01) VbCold=0.01;
  // ---- loose connection (AGE): random series-resistance dropouts
  if(e->looseN>0){ if(--e->looseN==0) e->looseR=0; }
  else if(age>0 && frand(e)<12.0*age*age/sr){ e->looseN=(long)((0.0002-0.002*log(frand(e)+1e-12))*sr); e->looseR=Rs*(8.0+30.0*frand(e)); }
  double Rtot=Rs+e->looseR;
  // ---- triggered gap: statistical lag and misfires scale with JITTER
  if(k->trig && !e->on){ double r=fabs(e->vg)/VbCold; if(r>1) r=1;
    if(frand(e)<1.0-1.2*jit*(1.0-r)) e->lag=(long)(jit*(-log(frand(e)+1e-12))*0.008*(1.5-r)/dt);
    else e->vg*=0.85; }                                              // fizzle: a little charge lost, no spark
  double corL=0,corR=0;
  if(e->lag>0 && frand(e)<900.0*jit/sr){ double q=0.12*e->vg, pn=frand(e); e->vg-=q; corL+=3.0*q*sqrt(1-pn); corR+=3.0*q*sqrt(pn); }   // leader ticks while a triggered strike is pending
  double sat=lw_clamp(0.1+0.45*Eac,0,1)+0.4*age; if(sat>1.4) sat=1.4;
  for(int o=0;o<OS;o++){
    e->ph+=fm*dt; if(e->ph>=1) e->ph-=1;
    double a=2*M_PI*e->ph, sn=sin(a), fl=-cos(a); double f3=fl*fl*fl, f9=f3*f3*f3;
    double ev=Edc + Eac*(sn - 0.25*sat*f9 + age*(0.12*sin(3*a)+0.07*sin(5*a+1.0))) + e->fbv + M_IN(s->in)*in;
    double Vb=VbCold*(1.0-restr*e->h); if(Vb<0.01) Vb=0.01;
    // corona: micro-discharges, more and smaller on the negative half
    if(corona>0){ double av=fabs(e->vg), x=(av-0.55*lt)/(0.08*lt); if(x>-6){ if(x>3) x=3; int neg=e->vg<0; double rate=corona*(neg?9000.0:2500.0)*exp(x);
        if(frand(e)<rate*dt){ double q=(neg?0.04:0.16)*(0.4+frand(e))*av; e->vg-=(neg?-1:1)*q; double cs=q*((e->vg<0)?1:-1.6), pn=frand(e); corL+=cs*sqrt(1-pn); corR+=cs*sqrt(pn); } } }
    // circuit (semi-implicit, order matters)
    e->is=(e->is+dt/Ls*(ev-e->vg))/(1.0+dt*Rtot/Ls);
    { double f=1.0+iron*e->it*e->it; if(f>ironMax) f=ironMax; e->it+=dt/Lt*f*(e->vg-e->vct-Rt*e->it); } e->vct+=dt/Ct*e->it;
    e->vg=(e->vg+dt/Cp*(e->is-e->it))/(1.0+dt*(e->g+gp)/Cp);
    double pw=e->vg*e->vg*e->g;
    // arc
    if(!e->on){ int fire=fabs(e->vg)>Vb; if(e->lag>=0){ if(e->lag==0) fire=1; e->lag--; }
      if(fire){ e->strikes++; e->on=1; e->g=0.4+0.4*frand(e); e->lag=-1; } }
    else { double sg=sqrt(e->g), tg=tau*sg; if(tg<2*dt) tg=2*dt; double ex=dt/tg*(pw/(Pw*sg)-1.0); if(ex>0.5) ex=0.5; if(ex<-0.5) ex=-0.5;
      e->g*=exp(ex); if(e->g>400) e->g=400;
      if(e->g<goff){ e->on=0; e->g=1e-7; e->jz=nrand(e); e->path=exp(0.6*jit*nrand(e)); } }
    // ionisation memory and climb
    if(e->on) e->h+=(1-e->h)*dt/0.002; else e->h-=e->h*dt/mem;
    if(e->h>0.08) e->c+=climb*e->h*dt; else e->c-=e->c*dt/0.004;
    if(e->c>12) e->c=12;
    // guards
    if(!(fabs(e->is)<200)){ e->is=e->is>0?200:-200; if(e->is!=e->is) e->is=0; e->guard++; }
    if(!(fabs(e->it)<200)){ e->it=e->it>0?200:-200; if(e->it!=e->it) e->it=0; e->guard++; }
    if(!(fabs(e->vg)<200)){ e->vg=e->vg>0?200:-200; if(e->vg!=e->vg) e->vg=0; e->guard++; }
    if(!(fabs(e->vct)<200)){ e->vct=e->vct>0?200:-200; if(e->vct!=e->vct) e->vct=0; e->guard++; }
    dpush(&e->dP,pw,e->nt); dpush(&e->dIs,e->is,e->nt); dpush(&e->dIt,e->it,e->nt); e->dbgOn+=e->on;
  }
  double pw=dout(&e->dP,e->fir,e->nt), isd=dout(&e->dIs,e->fir,e->nt), itd=dout(&e->dIt,e->fir,e->nt);
  // ARC: pressure ~ d(power)/dt, plus a little of the power itself (the thump)
  double dp=(pw-e->pprev)*sr/700.0; e->pprev=pw; e->hp1+=(pw-e->hp1)*(1.0-exp(-2*M_PI*25.0/sr)); double arc=dp+0.1*(pw-e->hp1);
  // FIELD: di/dt of the loop currents plus a little of the current
  double fc=isd+0.6*itd; double fld=(fc-e->fprev)*sr/2000.0; e->fprev=fc; e->hp2+=(fc-e->hp2)*(1.0-exp(-2*M_PI*30.0/sr)); fld+=0.15*(fc-e->hp2);
  // HUM: magnetostriction (flux^2) and winding force (i^2) into a rattling case
  if(fm!=e->humf){ e->humf=fm; for(int c=0;c<2;c++) for(int j=0;j<6;j++) rset(&e->hr[c][j], j<3?HF[j]*fm:HF[j], HQ[j], sr); }
  double hL=0,hR=0;
  { double fl=-cos(2*M_PI*e->ph), amp=Eac/0.8; if(amp>1) amp=1; double sfl=tanh((1.0+2.5*sat)*fl*amp);
    double drive=sfl*sfl+0.4*isd*isd; if(drive>50) drive=50; double th=0.75-0.35*age; double rat=drive>th?(drive-th)*3.0*(0.5+frand(e)):0;
    for(int j=0;j<6;j++){ hL+=HGL[j]*rproc(&e->hr[0][j],drive+rat); hR+=HGR[j]*rproc(&e->hr[1][j],drive+rat); } }
  double hum=M_HUM(s->hum)*0.6;
  // stereo: the arc wanders with the slow wind and leans as it climbs
  { double target=0.5+0.22*tanh(e->ts2)+0.10*tanh(e->ts); e->pan+=(target-e->pan)*(1.0-exp(-1.0/(0.02*sr))); }
  double gl=sqrt(2.0*(1.0-e->pan)), gr=sqrt(2.0*e->pan);
  double al=M_ARC(s->arc), flv=M_FIELD(s->field);
  double yl=al*(arc*gl+6.0*corL*1.4142)+flv*fld+hum*hL, yr=al*(arc*gr+6.0*corR*1.4142)+flv*fld+hum*hR;
  // acoustic feedback: the arc hears itself. Delayed, low-passed, soft-limited, into the source
  { double y=0.5*(yl+yr); e->fbbuf[e->fbw]=y; int d=(int)(M_DIST(s->dist)*sr); if(d<1)d=1; if(d>e->fbn-1)d=e->fbn-1; int r=e->fbw-d; if(r<0) r+=e->fbn; if(++e->fbw==e->fbn) e->fbw=0;
    e->fblp+=(e->fbbuf[r]-e->fblp)*0.35; e->fbv=fb*tanh(e->fblp*0.5); }
  // output: gain, soft clip, 20 Hz high-pass
  double og=M_OUT(s->out)*0.35; yl=1.6*tanh(og*yl/1.6); yr=1.6*tanh(og*yr/1.6);
  { double a=1.0-exp(-2*M_PI*20.0/sr); e->dcL+=(yl-e->dcL)*a; e->dcR+=(yr-e->dcR)*a; }
  *L=yl-e->dcL; *R=yr-e->dcR; *oArc=1.6*tanh(0.35*arc/1.6); *oField=1.6*tanh(0.35*fld/1.6);
}

#include "scenes.h"

int main(int argc,char**argv){
  if(argc<3){ fprintf(stderr,"usage: livewire <scene> <out.f32> | fuzz <seed> <out.f32>\n"); return 1; }
  double sr=getenv("SR")?atof(getenv("SR")):48000; int fuzz=!strcmp(argv[1],"fuzz"); const char*outp=fuzz?argv[3]:argv[2];
  int sc=fuzz?-1:scene_id(argv[1]); if(!fuzz&&sc<0){ fprintf(stderr,"unknown scene\n"); return 1; }
  double dur=fuzz?30:scene_dur(sc); long N=(long)(dur*sr); float*out=malloc(sizeof(float)*2*N);
  LW*e=malloc(sizeof(LW)*3); for(int i=0;i<3;i++) lw_init(&e[i],sr,fuzz?atoi(argv[2])*7+i:1+i);
  K k[3]; int ninst=1; unsigned long long fr=fuzz?atoi(argv[2])*977+5:1; double pk=0,ss=0; long bad=0;
  K fz; memset(&fz,0,sizeof fz);
  for(long n=0;n<N;n++){
    double t=n/sr, L=0,R=0;
    if(fuzz){ ninst=1; fz.trig=fz.surge=0;
      if(n%(long)(0.5*sr)==0){ double*d=(double*)&fz; for(int i=0;i<28;i++){ fr^=fr<<13; fr^=fr>>7; fr^=fr<<17; d[i]=(fr>>11)*(1.0/9007199254740992.0); }
        fr^=fr<<13; fr^=fr>>7; fr^=fr<<17; fz.track=fr&1; fz.keyPatched=(fr>>1)&1; fz.key=(fr>>2)&1; fz.touch=((fr>>3)&7)==0; fz.voct=((fr>>8)&255)/255.0*8-4; fz.voctMains=((fr>>16)&255)/255.0*6-2; }
      if(n%(long)(0.11*sr)==0) fz.trig=1; if(n%(long)(1.7*sr)==0) fz.surge=1;
      k[0]=fz; }
    else scene(sc,t,n,sr,k,&ninst);
    double in=fuzz?((n/(long)(0.25*sr))%3==0?sin(n*0.05)*2.0:0):0;
    for(int i=0;i<ninst;i++){ double l,r,a,f; long g0=e[i].guard; lw_process(&e[i],&k[i],in,&l,&r,&a,&f); if(getenv("SOLO")&&atoi(getenv("SOLO"))!=i) continue; L+=l; R+=r;
      if(e[i].guard>g0 && g0==0){ K*q=&k[i]; fprintf(stderr,"first guard at t=%.3f: E %.2f mains %.2f fm %.0f cur %.3f choke %.5f gap %.2f heat %.4f cool %.6f track %d ft %.0f Q %.1f Z %.2f iron %.2f fb %.2f age %.2f in %.2f\n",t,M_VOLTS(q->volts),q->mains,M_FREQ(q->freq),M_CUR(q->current),M_CHOKE(q->choke),M_GAP(q->gap),M_HEAT(q->heat),M_COOL(q->cool),q->track,M_TUNE(q->tune)*pow(2,q->voct),M_RING(q->ring),M_LOAD(q->load),M_IRON(q->iron),M_FB(q->fb),q->age,q->in); } }
    if(!isfinite(L)||!isfinite(R)){ bad++; L=R=0; }
    out[2*n]=(float)L; out[2*n+1]=(float)R; if(fabs(L)>pk) pk=fabs(L); ss+=L*L;
  }
  FILE*fo=fopen(outp,"wb"); fwrite(out,sizeof(float),2*N,fo); fclose(fo);
  fprintf(stderr,"%s: peak %.2f rms %.3f  arc on %.0f%%  guard %ld  nonfinite %ld\n",argv[1],pk,sqrt(ss/N),100.0*e[0].dbgOn/((double)N*e[0].os),e[0].guard+e[1].guard+e[2].guard,bad);
  if(getenv("DBG")) for(int i=0;i<ninst;i++) fprintf(stderr,"  inst %d strikes %ld on %.1f%%\n",i,e[i].strikes,100.0*e[i].dbgOn/((double)N*e[i].os));
  return bad?2:0;
}
