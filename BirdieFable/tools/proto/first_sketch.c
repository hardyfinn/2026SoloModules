// Birdsong voice sketch: two-sided syrinx (normal-form labial oscillator) -> trachea -> throat (OEC) -> beak
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#define SR 48000.0
#define OS 16
#define MAXS 4096
typedef struct { double t0,dur,b0,b1; int curve; double a; double att,rel; double buzzHz,buzzDepth,buzzAmp; double v2b0,v2b1,v2mix; double noise; } Syl;
typedef struct { double size, trach, r, oecTrack, oecQ, oecMix, beak, breath; } Tract;
static Syl S[MAXS]; static int nS=0; static Tract T; static double total=0;
static uint32_t rs=12345; static double rnd(){ rs=rs*1664525u+1013904223u; return (rs>>8)/16777216.0; }
static double rr(double a,double b){return a+(b-a)*rnd();}
static Syl* add(double t0,double dur,double st0,double st1,int curve,double a){
  Syl*s=&S[nS++]; memset(s,0,sizeof*s); s->t0=t0; s->dur=dur; s->curve=curve; s->a=a; s->att=0.15; s->rel=0.3;
  // st = semitones of tension relative to beta=1 (approx 4 kHz at size 1). f ~ sqrt(beta)
  s->b0=pow(2.0,st0/6.0); s->b1=pow(2.0,st1/6.0); if(t0+dur+0.3>total) total=t0+dur+0.3; return s; }
static double shape(int c,double x){ switch(c){case 1: return sin(M_PI*x); case 2: return 1-(1-x)*(1-x); case 3: return x*x; case 4: return 0.5-0.5*cos(2*M_PI*x)>0? (x<0.5? 2*x: 2-2*x):0; default: return x;} }
typedef struct {double x,y;} Osc;
static inline void deriv(double x,double y,double a,double b,double G,double*dx,double*dy){ *dx=y; *dy=-a*G*G-b*G*G*x-G*G*x*x*x-G*x*x*y+G*G*x*x-G*x*y; }
static inline void step(Osc*o,double a,double b,double G,double dt){ double x=o->x,y=o->y,k1x,k1y,k2x,k2y,k3x,k3y,k4x,k4y;
  deriv(x,y,a,b,G,&k1x,&k1y); deriv(x+.5*dt*k1x,y+.5*dt*k1y,a,b,G,&k2x,&k2y); deriv(x+.5*dt*k2x,y+.5*dt*k2y,a,b,G,&k3x,&k3y); deriv(x+dt*k3x,y+dt*k3y,a,b,G,&k4x,&k4y);
  o->x=x+dt/6*(k1x+2*k2x+2*k3x+k4x); o->y=y+dt/6*(k1y+2*k2y+2*k3y+k4y); if(!isfinite(o->x)||fabs(o->x)>50){o->x=0;o->y=0;} }
static void render(const char*fn){
  int N=(int)(total*SR); float*out=calloc(N,sizeof(float));
  double G=24000.0/T.size; double dt=1.0/(SR*OS);
  Osc o1={0.001,0},o2={0.001,0};
  // trachea delay
  double dl[4096]={0}; int wp=0; double D=2.0*T.trach*T.size/343.0*SR; // round trip in samples (trach in m)
  double lp1=0,lp2=0,bp=0,low=0,nz=0,hp=0,hpx=0,bl=0;
  for(int n=0;n<N;n++){
    double t=n/SR; double a1=-0.08,b1=1,a2=-0.08,b2=1,mix2=0,nzAmt=0,env=0;
    for(int i=0;i<nS;i++){ Syl*s=&S[i]; if(t>=s->t0&&t<s->t0+s->dur){ double x=(t-s->t0)/s->dur;
        double e=1; if(x<s->att) e=0.5-0.5*cos(M_PI*x/s->att); else if(x>1-s->rel) e=0.5-0.5*cos(M_PI*(1-x)/s->rel);
        double p=shape(s->curve,x); double bz=s->buzzHz>0? sin(2*M_PI*s->buzzHz*(t-s->t0)):0;
        double lb=log(s->b0)+(log(s->b1)-log(s->b0))*p; b1=exp(lb)*(1+s->buzzDepth*bz);
        env=e*(1-s->buzzAmp*(0.5+0.5*bz)); a1=-0.08+(s->a+0.08)*env; 
        if(s->v2mix>0){ double l2=log(s->v2b0)+(log(s->v2b1)-log(s->v2b0))*p; b2=exp(l2); a2=a1; mix2=s->v2mix; }
        nzAmt=s->noise; break; } }
    nz+=0.25*((rnd()*2-1)-nz); // lowpassed noise on pressure
    a1+=nzAmt*nz*env; a2+=nzAmt*nz*env;
    double acc=0;
    for(int k=0;k<OS;k++){ step(&o1,a1,b1,G,dt); double s=o1.y/G; if(mix2>0){ step(&o2,a2,b2,G,dt); s=s*(1-0.5*mix2)+mix2*o2.y/G; } acc+=s; }
    double src=acc/OS; lp1+=0.6*(src-lp1); src=lp1; // gentle decimation smoothing
    src+=T.breath*env*(rnd()*2-1)*0.05;
    // trachea: a(t)=s(t)-r*a(t-D)
    double rd=wp-D; while(rd<0)rd+=4096; int i0=(int)rd; double fr=rd-i0; double del=dl[i0&4095]*(1-fr)+dl[(i0+1)&4095]*fr;
    double a=src-T.r*del; dl[wp]=a; wp=(wp+1)&4095; double tr=a;
    // throat (OEC) resonator tracking estimated f0
    double f0=3820.0/T.size*sqrt(fmax(b1+1.5*fmax(a1,0),0.02)); double fc=fmin(fmax(T.oecTrack*f0,150),12000);
    double g=2*sin(M_PI*fc/SR), q=1.0/T.oecQ; low+=g*bp; double high=tr-low-q*bp; bp+=g*high;
    double v=tr*(1-T.oecMix)+bp*q*2.0*T.oecMix;
    // beak: lowpass by gape + radiation highpass
    double bc=fmin(T.beak/T.size,20000); double k=1-exp(-2*M_PI*bc/SR); bl+=k*(v-bl); lp2+=k*(bl-lp2);
    hp=0.985*(hp+lp2-hpx); hpx=lp2; out[n]=(float)hp;
  }
  double mx=1e-9; for(int n=0;n<N;n++) if(fabs(out[n])>mx)mx=fabs(out[n]);
  FILE*f=fopen(fn,"wb"); int ds=N*2; int32_t i32; int16_t i16;
  fwrite("RIFF",1,4,f); i32=36+ds; fwrite(&i32,4,1,f); fwrite("WAVEfmt ",1,8,f); i32=16; fwrite(&i32,4,1,f); i16=1; fwrite(&i16,2,1,f); fwrite(&i16,2,1,f);
  i32=(int)SR; fwrite(&i32,4,1,f); i32=(int)SR*2; fwrite(&i32,4,1,f); i16=2; fwrite(&i16,2,1,f); i16=16; fwrite(&i16,2,1,f); fwrite("data",1,4,f); fwrite(&ds,4,1,f);
  for(int n=0;n<N;n++){ i16=(int16_t)(out[n]/mx*0.8*32767); fwrite(&i16,2,1,f);} fclose(f); free(out);
  fprintf(stderr,"%s %.1fs peak %.3f\n",fn,total,mx);
}
static void tract(double size,double trach,double r,double track,double q,double mix,double beak,double breath){ T=(Tract){size,trach,r,track,q,mix,beak,breath}; }
// seeded phrase generator: the module's "stream of notes" idea
static void phrase(double t,uint32_t seed,int notes,double rate,double centre,double range,double vary){
  rs=seed*2654435761u+1; int ntypes=1+(int)(rnd()*3.99); Syl types[4]; int nSave=nS;
  for(int k=0;k<ntypes;k++){ Syl*s=add(0,0.1,centre+rr(-range,range),centre+rr(-range,range),(int)(rnd()*3.99),rr(0.12,0.35));
    s->dur=rr(0.35,0.9); s->att=rr(0.08,0.3); s->rel=rr(0.15,0.45);
    if(rnd()<0.25){s->buzzHz=rr(60,300); s->buzzDepth=rr(0.05,0.3); s->buzzAmp=rr(0,0.6);} 
    if(rnd()<0.2){ s->v2b0=s->b0*rr(1.1,1.6); s->v2b1=s->b1*rr(0.7,1.5); s->v2mix=0.5; }
    types[k]=*s; }
  nS=nSave; int pat=(int)(rnd()*3.99); double drift=rr(-1,1)*vary, acc=1+rr(-0.06,0.06)*vary; double per=1.0/rate; double tt=t;
  for(int i=0;i<notes;i++){ int k= pat==0?0: pat==1? i%ntypes: pat==2? (i%3==2?1%ntypes:0) : (i*ntypes/notes);
    Syl*s=&S[nS++]; *s=types[k]; s->t0=tt; s->dur=types[k].dur*per; double sh=pow(2.0,drift*i/6.0); s->b0*=sh; s->b1*=sh; if(s->v2mix>0){s->v2b0*=sh;s->v2b1*=sh;}
    tt+=per; per*=acc; if(tt+0.5>total) total=tt+0.5; }
}
int main(int argc,char**argv){
  const char*dir=argc>1?argv[1]:"."; char fn[512];
  #define OUT(name) snprintf(fn,512,"%s/%s.wav",dir,name); render(fn); nS=0; total=0; rs=777;
  Syl*s; double t;
  // 01 whistles: clear slurred notes
  tract(1.0,0.025,0.6,1.0,5,0.7,9000,0.3); t=0.2;
  for(int i=0;i<4;i++){ add(t,0.28,2,-9,2,0.2); t+=0.36; } t+=0.3;
  for(int i=0;i<4;i++){ add(t,0.22,-10,3,3,0.2); t+=0.3; } t+=0.3;
  for(int i=0;i<3;i++){ add(t,0.4,-4,4,1,0.18); t+=0.5; }
  OUT("01-whistles")
  // 02 trills at three speeds
  tract(0.8,0.02,0.6,1.0,5,0.7,10000,0.3); t=0.2;
  for(int i=0;i<12;i++){ add(t,0.06,3,-4,0,0.25); t+=0.085; } t+=0.4;
  for(int i=0;i<24;i++){ add(t,0.028,2,-5,0,0.3); t+=0.04; } t+=0.4;
  for(int i=0;i<40;i++){ s=add(t,0.016,-2+i*0.1,-7+i*0.1,0,0.3); s->att=0.3; t+=0.024; }
  OUT("02-trills")
  // 03 chirps and chevrons (sparrow-like)
  tract(0.9,0.02,0.6,1.0,4,0.6,10000,0.4); t=0.2;
  for(int i=0;i<10;i++){ s=add(t,rr(0.07,0.12),rr(-6,-2),rr(1,5),1,rr(0.2,0.35)); t+=rr(0.16,0.4); if(i%3==2){ add(t,0.05,4,-6,0,0.3); t+=0.12; } }
  OUT("03-chirps")
  // 04 buzzes: fast tension and pressure modulation
  tract(1.0,0.025,0.6,1.0,3,0.5,9000,0.5); t=0.2;
  s=add(t,0.5,-2,-3,0,0.25); s->buzzHz=90; s->buzzDepth=0.25; s->buzzAmp=0.7; t+=0.7;
  s=add(t,0.5,1,-2,0,0.25); s->buzzHz=180; s->buzzDepth=0.3; s->buzzAmp=0.4; t+=0.7;
  s=add(t,0.6,-5,2,0,0.25); s->buzzHz=300; s->buzzDepth=0.4; s->buzzAmp=0.2; t+=0.8;
  for(int i=0;i<3;i++){ add(t,0.09,-1,3,0,0.25); t+=0.13; } s=add(t,0.7,0,-1,0,0.25); s->buzzHz=140; s->buzzDepth=0.2; s->buzzAmp=0.8;
  OUT("04-buzzes")
  // 05 two voices: both sides of the syrinx at once
  tract(1.1,0.03,0.6,1.0,3,0.4,9000,0.3); t=0.2;
  s=add(t,0.6,-6,-2,2,0.2); s->v2b0=pow(2,2/6.0); s->v2b1=pow(2,-5/6.0); s->v2mix=0.5; t+=0.8;
  s=add(t,0.5,-3,-3,0,0.2); s->v2b0=pow(2,-2.3/6.0); s->v2b1=pow(2,-1.2/6.0); s->v2mix=0.5; t+=0.7;
  s=add(t,0.7,-8,0,1,0.2); s->v2b0=pow(2,4/6.0); s->v2b1=pow(2,-1/6.0); s->v2mix=0.5; t+=0.9;
  for(int i=0;i<5;i++){ s=add(t,0.12,-4+i,-6+i,0,0.22); s->v2b0=pow(2,(3-i)/6.0); s->v2b1=pow(2,(5-i)/6.0); s->v2mix=0.5; t+=0.17; }
  OUT("05-two-voices")
  // 06 coo: big bird, low tension, closed beak
  tract(8.0,0.02,0.5,1.0,6,0.9,8000,0.2); t=0.3;
  for(int r=0;r<2;r++){ s=add(t,0.5,-1,1,1,0.12); s->att=0.3; t+=0.7; s=add(t,0.9,1,-1,2,0.14); s->att=0.2; s->rel=0.5; t+=1.1; s=add(t,0.35,0,-1,0,0.1); t+=0.9; }
  OUT("06-coo")
  // 07 caw and harsh calls: large bird, labia driven hard near the rich low-tension edge, noisy pressure
  tract(3.2,0.04,0.7,2.2,2,0.5,14000,1.0); t=0.3;
  for(int i=0;i<3;i++){ s=add(t,0.38,-22,-25,1,0.42); s->noise=0.25; s->att=0.08; s->rel=0.35; t+=0.6; } t+=0.4;
  for(int i=0;i<2;i++){ s=add(t,0.5,-17,-23,0,0.5); s->noise=0.4; s->buzzHz=55; s->buzzDepth=0.1; s->buzzAmp=0.3; t+=0.75; }
  OUT("07-caw")
  // 08 quack and honk: pulse-like source through a long resonant trachea
  tract(4.0,0.06,0.85,3.0,3,0.5,16000,0.5); t=0.3;
  for(int i=0;i<5;i++){ s=add(t,0.16-0.01*i,-27,-29,0,0.4); s->noise=0.1; s->att=0.1; t+=0.24; } t+=0.5;
  s=add(t,0.25,-22,-18,0,0.4); t+=0.27; s=add(t,0.45,-17,-20,2,0.45); t+=0.8;
  s=add(t,0.25,-22,-18,0,0.4); t+=0.27; s=add(t,0.45,-17,-20,2,0.45);
  OUT("08-quack-honk")
  // 09 song phrases from the phrase generator, eight seeds
  tract(1.0,0.025,0.6,1.0,4,0.6,10000,0.3); t=0.2;
  for(int k=0;k<8;k++){ int notes=5+(k*5)%9; double rate=4+(k*7)%17; phrase(t,100+k,notes,rate,-3+(k%3)*2,6,1.0); t=total+0.2; }
  OUT("09-generated-phrases")
  // 10 same generator, same seeds, bird ten times larger and slower: synthetic territory
  tract(8.0,0.025,0.6,1.0,4,0.6,10000,0.3); t=0.2;
  for(int k=0;k<5;k++){ int notes=5+(k*5)%9; double rate=(4+(k*7)%17)/3.0; phrase(t,100+k,notes,rate,-3+(k%3)*2,6,1.0); t=total+0.3; }
  OUT("10-stretched-large")
  // 11 held tone: syllable frozen and sustained, tension swept slowly into the rich edge and back
  tract(6.0,0.04,0.75,1.5,3,0.4,16000,0.2); t=0.2;
  s=add(t,6.0,-6,-26,1,0.35); s->att=0.02; s->rel=0.05; t+=6.3;
  s=add(t,5.0,-20,-24,1,0.4); s->buzzHz=7; s->buzzDepth=0.5; s->att=0.02; s->rel=0.1; s->v2b0=pow(2,-20.2/6.0)*1.0; s->v2b1=pow(2,-13/6.0); s->v2mix=0.5;
  OUT("11-held-drone")
  // 12 bass bird: 16th-note line at 124 bpm, huge bird, each note a syllable
  tract(48.0,0.004,0.7,2.0,3,0.5,200000,0.0); t=0.2; { double st=60.0/124/4; int pat[16]={0,0,7,0,-2,0,12,0,0,3,0,0,7,0,-5,10};
  for(int bar=0;bar<4;bar++) for(int i=0;i<16;i++){ if((i*7+bar)%5==4) {t+=st; continue;} double p=-12+pat[i]; int slide=(i%4==2);
    s=add(t,st*(slide?1.0:0.6),p+(slide?5:0.5),p-(bar>=2?3:0),2,0.3+0.15*(i%4==0)); s->att=0.05; s->rel=0.4; t+=st; } }
  OUT("12-bass-bird")
  return 0;
}
