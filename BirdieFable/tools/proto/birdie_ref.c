#include "birdie_ref.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
static inline uint32_t xs32(uint32_t* s){ uint32_t x=*s; x^=x<<13; x^=x>>17; x^=x<<5; *s=x; return x; }
static inline double u01(uint32_t* s){ return (xs32(s)>>8)*(1.0/16777216.0); }       // [0,1)
static inline double bip(uint32_t* s){ return 2.0*u01(s)-1.0; }                        // [-1,1)
static inline double clampd(double v,double lo,double hi){ return v<lo?lo:(v>hi?hi:v); }

void bd_init(Birdie* b, double sr){
  memset(b,0,sizeof *b); b->sr=sr;
  b->os = sr<=50000.0 ? 16 : (sr<=100000.0 ? 8 : 4);
  b->dt = 1.0/(sr*b->os);
  b->o1.x=b->o2.x=0.001; b->nrng=0x1234567u; b->dSm=8.0;
  // windowed-sinc low-pass, cutoff 0.45 of the base rate, Blackman window, unity DC gain
  b->ntaps=b->os*BD_FIRQ; double sum=0, fc=0.45/b->os;
  for(int i=0;i<b->ntaps;i++){ double m=i-(b->ntaps-1)/2.0; double s=(m==0)?2*fc:sin(2*M_PI*fc*m)/(M_PI*m);
    double w=0.42-0.5*cos(2*M_PI*i/(b->ntaps-1))+0.08*cos(4*M_PI*i/(b->ntaps-1)); b->fir[i]=s*w; sum+=b->fir[i]; }
  for(int i=0;i<b->ntaps;i++) b->fir[i]/=sum;
  // rich edge of the singing range (saddle-node line): alpha = 2x^3 - x^2, beta = 2x - 3x^2, x >= 0.5
  for(int i=0;i<=BD_EDGE_N;i++){ double a=(double)i/BD_EDGE_N, x=0.5+0.6*sqrt(a);
    for(int k=0;k<40;k++){ double F=2*x*x*x-x*x-a, D=6*x*x-2*x; if(D<1e-9) break; x-=F/D; if(x<0.5) x=0.5; }
    b->edge[i]=2*x-3*x*x; }
}
static inline double bd_edge(const Birdie* b,double a){ if(a<=0) return 0.25; if(a>=1.0) return b->edge[BD_EDGE_N];
  double f=a*BD_EDGE_N; int i=(int)f; return b->edge[i]+(f-i)*(b->edge[i+1]-b->edge[i]); }

static inline void bd_deriv(double x,double y,double a,double be,double G,double* dx,double* dy){
  *dx=y; *dy=-a*G*G - be*G*G*x - G*G*x*x*x - G*x*x*y + G*G*x*x - G*x*y;
}
static inline void bd_rk4(BdOsc* o,double a,double be,double G,double dt){
  double x=o->x,y=o->y,k1x,k1y,k2x,k2y,k3x,k3y,k4x,k4y;
  bd_deriv(x,y,a,be,G,&k1x,&k1y);
  bd_deriv(x+.5*dt*k1x,y+.5*dt*k1y,a,be,G,&k2x,&k2y);
  bd_deriv(x+.5*dt*k2x,y+.5*dt*k2y,a,be,G,&k3x,&k3y);
  bd_deriv(x+dt*k3x,y+dt*k3y,a,be,G,&k4x,&k4y);
  o->x=x+dt/6*(k1x+2*k2x+2*k3x+k4x); o->y=y+dt/6*(k1y+2*k2y+2*k3y+k4y);
  if(!(fabs(o->x)<20.0) || !(fabs(o->y)<60.0*G)){ o->x=0.001; o->y=0; }   // blow-up guard (also catches NaN)
}

static double bd_shape(int sh,double x){
  switch(sh){
    case SH_FALL:   return 1.0-(1.0-x)*(1.0-x);
    case SH_SWOOP:  return x*x;
    case SH_ARCH:   return sin(M_PI*x);
    case SH_DOUBLE: return fabs(sin(2.0*M_PI*x));
    default:        return x;
  }
}

static void bd_plain(BdType* t){ memset(t,0,sizeof *t); t->lenF=t->pressF=t->attF=t->relF=1.0; t->shape=-1; }

static void bd_start(Birdie* b,const BdParams* p,const BdType* t,double drift,double swell,double maxDur){
  b->cur=*t; b->curDrift=drift; b->curSwell=swell;
  double d=p->length*t->lenF*p->time; if(maxDur>0 && d>maxDur) d=maxDur; if(d<0.004) d=0.004;
  b->ndur=d; b->nt=0; b->noteOn=1; b->buzzPhase=0;
  // pluck: a silent syrinx takes many cycles to start, so nudge it at note-on
  double G=BD_GAMMA0/p->size;
  if(b->o1.x*b->o1.x+(b->o1.y/G)*(b->o1.y/G)<0.01) b->o1.x+=0.1;
  if(b->o2.x*b->o2.x+(b->o2.y/G)*(b->o2.y/G)<0.01) b->o2.x+=0.1;
}

void bd_note(Birdie* b,const BdParams* p){ BdType t; bd_plain(&t); b->phOn=0; bd_start(b,p,&t,0.0,1.0,0.0); }

void bd_phrase(Birdie* b,const BdParams* p,uint32_t seed){
  uint32_t r = seed*2654435761u + 0x9E3779B9u; if(!r) r=1; for(int i=0;i<4;i++) xs32(&r);
  double v=p->vary;
  bd_plain(&b->types[0]);                       // A is always the note on the panel
  for(int k=1;k<4;k++){ BdType* t=&b->types[k]; bd_plain(t);   // draw order is fixed: 16 draws per type
    t->dT     = bip(&r)*7.0*v;
    t->dSlide = bip(&r)*11.0*v;
    { double u=u01(&r), w=u01(&r); if(u<0.6*v) t->shape=(int)(w*SH_COUNT); }
    t->lenF   = pow(2.0,bip(&r)*v);
    t->pressF = 1.0+bip(&r)*0.3*v;
    { double u=u01(&r), a=u01(&r), c=u01(&r), d=u01(&r);
      if(u<0.3*v){ t->buzzOn=1; t->buzzRate=60.0*pow(5.0,a); t->buzz=0.05+0.25*c; t->chop=0.6*d; } }
    { double u=u01(&r), a=bip(&r), c=bip(&r);
      if(u<0.25*v){ t->v2On=1; t->v2mix=0.5; t->v2int=a*7.0; t->v2slide=c; } }
    t->attF   = pow(2.0,bip(&r)*v);
    t->relF   = pow(2.0,bip(&r)*v);
    u01(&r); u01(&r);                            // two spare draws, reserved
  }
  b->shufRng = r^0xA5A5A5A5u; if(!b->shufRng) b->shufRng=1;
  b->seed=seed; b->phOn=1; b->phIdx=0; b->phN=p->notes<1?1:(p->notes>32?32:p->notes);
  b->phNext=0.0; b->phDrift=p->drift; b->phAccel=p->accel; b->phSwell=p->swell; b->phPattern=p->pattern;
  b->phPeriod = p->clockPeriod>0 ? p->clockPeriod : p->time/p->rate;
}

static void bd_phrase_tick(Birdie* b,const BdParams* p){
  if(!b->phOn) return;
  if(b->phNext<=0.0){
    if(b->phIdx>=b->phN){ if(!b->noteOn){ b->phOn=0; b->outEnd=1; } return; }
    int i=b->phIdx, N=b->phN, k=0; double f = N>1 ? (double)i/(N-1) : 0.0;
    switch(b->phPattern){
      case PT_PAIR:    k=i&1; break;
      case PT_MOTIF:   k=(i%3==2)?1:0; break;
      case PT_TAIL:  { int tail=N/4; if(tail<1) tail=1; k=(i>=N-tail && N>1)?1:0; } break;
      case PT_RUN:   { int K=N<4?N:4; k=i*K/N; } break;
      case PT_CALL:    k=i&3; break;
      case PT_SHUFFLE: k=(int)(u01(&b->shufRng)*4.0); break;
      default: k=0;
    }
    double per=b->phPeriod*pow(2.0,-b->phAccel*f);
    double sw=1.0; if(b->phSwell>0) sw=1.0-0.7*b->phSwell*(1.0-f); else if(b->phSwell<0) sw=1.0+0.7*b->phSwell*f;
    bd_start(b,p,&b->types[k],b->phDrift*f,sw,0.92*per);
    b->phIdx++; b->phNext+=per;
  }
  b->phNext-=1.0/b->sr;
}

double bd_process(Birdie* b,const BdParams* p){
  const double sr=b->sr, T=1.0/sr;
  b->outEnd=0;
  bd_phrase_tick(b,p);
  const BdType* t=&b->cur;
  // ---- gesture
  double env=0.0, pos=0.5;
  if(b->noteOn){
    double x=b->nt/b->ndur;
    if(x>=1.0){ b->noteOn=0; }
    else{
      double a=clampd(p->attack*t->attF,0.02,0.6), r=clampd(p->release*t->relF,0.02,0.6);
      if(a+r>1.0){ double s=1.0/(a+r); a*=s; r*=s; }
      env=1.0; if(x<a) env=0.5-0.5*cos(M_PI*x/a); else if(x>1.0-r) env=0.5-0.5*cos(M_PI*(1.0-x)/r);
      int sh = t->shape>=0 ? t->shape : p->shape;
      pos=bd_shape(sh,x);
      b->nt+=T;
    }
  }
  b->holdSm += (1.0-exp(-T/0.010))*((p->hold?1.0:0.0)-b->holdSm);
  if(b->holdSm>env) env=b->holdSm;
  b->envSm += (1.0-exp(-T/0.0015))*(env-b->envSm);      // 1.5 ms slew: no clicks on retrigger
  env=b->envSm;
  int    bzOn  = b->noteOn && t->buzzOn;
  double bRate = bzOn? t->buzzRate : p->buzzRate, bDep = bzOn? t->buzz : p->buzz, bChop = bzOn? t->chop : p->chop;
  b->buzzPhase += bRate/p->time*T; if(b->buzzPhase>=1.0) b->buzzPhase-=1.0;
  double bz=sin(2.0*M_PI*b->buzzPhase);
  int    v2On  = b->noteOn && t->v2On;
  double m2 = v2On? t->v2mix : p->v2mix, i2 = v2On? t->v2int : p->v2int, s2 = v2On? t->v2slide : p->v2slide;
  double slide = b->noteOn ? p->slide + t->dSlide : 0.0;
  double centre = p->tension + (b->noteOn ? t->dT + b->curDrift : 0.0);
  double st1 = centre + slide*(pos-0.5);
  double st2 = centre + i2 + s2*slide*(pos-0.5);
  double envP = env*(1.0-bChop*(0.5+0.5*bz));
  double press = p->pressure*(b->noteOn ? t->pressF*b->curSwell : 1.0);
  double alpha = BD_ALPHA_IDLE + (press-BD_ALPHA_IDLE)*envP;
  // ---- noise (rates fixed in Hz, so the same at any sample rate)
  double w1=xs32(&b->nrng)*(2.0/4294967296.0)-1.0, w2=xs32(&b->nrng)*(2.0/4294967296.0)-1.0, w3=xs32(&b->nrng)*(2.0/4294967296.0)-1.0;
  b->nzFast += (1.0-exp(-2.0*M_PI*3000.0*T))*(w1-b->nzFast);
  b->nzSlow += (1.0-exp(-2.0*M_PI*250.0*T))*(w2*4.0-b->nzSlow);
  alpha += p->rough*0.6*b->nzFast*envP;
  double jit = 1.0 + p->rough*0.35*clampd(b->nzSlow,-1.5,1.5);
  // tension is measured upward from the rich edge, which moves with pressure
  double ed  = bd_edge(b,alpha);
  double d1  = pow(2.0,st1/BD_ST_PER_OCT)*(1.0+bDep*bz)*jit;
  double d2  = pow(2.0,st2/BD_ST_PER_OCT)*(1.0+bDep*bz)*jit;
  double be1 = ed+d1, be2 = ed+d2;
  // ---- two sides of the syrinx, oversampled
  double G=BD_GAMMA0/p->size, acc=0.0; int os=b->os, nt=b->ntaps;
  for(int k=0;k<os;k++){
    bd_rk4(&b->o1,alpha,be1,G,b->dt); double s=b->o1.y/G;
    if(m2>0.001){ bd_rk4(&b->o2,alpha,be2,G,b->dt); s=s*(1.0-0.5*m2)+m2*b->o2.y/G; }
    b->hist[b->hpos]=s; b->hpos=(b->hpos+1)%nt;
  }
  for(int i=0;i<nt;i++) acc+=b->fir[i]*b->hist[(b->hpos+i)%nt];
  double src=acc + p->breath*0.06*envP*w3;
  // ---- trachea: one-end-open tube, a[n] = s[n] - r * a[n - D]
  double Dt = clampd(2.0*p->trachea*p->size/343.0*sr,1.0,BD_DELAY-4.0);
  b->dSm += (1.0-exp(-T/0.02))*(Dt-b->dSm);
  double rd=b->wp-b->dSm; while(rd<0) rd+=BD_DELAY; int i0=(int)rd; double fr=rd-i0;
  double del=b->dl[i0%BD_DELAY]*(1.0-fr)+b->dl[(i0+1)%BD_DELAY]*fr;
  double tr=src-p->tube*del; b->dl[b->wp]=tr; b->wp=(b->wp+1)%BD_DELAY;
  tr*=(1.0-0.45*p->tube);                                        // level trim
  // ---- throat: band-pass that follows the pitch, or a fixed formant
  double fEst=BD_F_COEF*G*pow(fmax(d1,1e-4),0.39);
  double fc = p->follow ? fEst*pow(2.0,p->tune*3.0-1.0) : 300.0*pow(20.0,p->tune)/sqrt(p->size);
  fc=clampd(fc,20.0,0.45*sr);
  double Q=2.0+6.0*p->throat, kq=1.0/Q, g=tan(M_PI*fc/sr);
  double a1=1.0/(1.0+g*(g+kq)), a2=g*a1, a3=g*a2, v3=tr-b->ic2, v1=a1*b->ic1+a2*v3, v2=b->ic2+a2*b->ic1+a3*v3;
  b->ic1=2.0*v1-b->ic1; b->ic2=2.0*v2-b->ic2;
  double th = tr*(1.0-p->throat) + kq*v1*p->throat*(1.0+p->throat);
  // ---- beak: two one-pole low-passes, then DC blocker
  double kb=1.0-exp(-2.0*M_PI*clampd(p->beak/sqrt(p->size),20.0,0.45*sr)*T);
  b->lpA+=kb*(th-b->lpA); b->lpB+=kb*(b->lpA-b->lpB);
  double R=exp(-2.0*M_PI*12.0*T); double y=b->lpB-b->dcX+R*b->dcY; b->dcX=b->lpB; b->dcY=y;
  // ---- output
  double out = y*sqrt(fmax(envP,0.0))*4.0;
  out = 8.0*tanh(out/8.0);
  b->outPress=10.0*clampd(envP,0.0,1.0); b->outPitch=log2(fEst/261.6256); b->outGate=b->noteOn;
  return out;
}
