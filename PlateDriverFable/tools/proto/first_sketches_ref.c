// Singing metal prototype: a thin metal plate that is never struck. A pickup feeds a driver on the same
// plate (like an EBow held on a gong). The plate's own stretching nonlinearity decides what the loop does:
// one pure mode at low drive, then multiphonics (combination resonances), then the full roaring wash.
// Plate: N modes. Nonlinearity: C "stretch channels", S_c = sum_j a_cj q_j + 1/2 sum_j l_cj q_j^2,
// potential 1/2 sum_c S_c^2 (never negative, so it cannot create energy). a = curvature (quadratic, gong/shell),
// l = flat-plate stretching (cubic). Offline sketch, not module code.
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#define FS 96000.0
#define T (1.0/FS)
#define N 128
#define C 6
#define BLK 32
static unsigned long long rs=0x2545F4914F6CDD1DULL;
static double rnd(){ rs^=rs<<13; rs^=rs>>7; rs^=rs<<17; return (rs>>11)*(1.0/9007199254740992.0); }
static double bi(){ return 2*rnd()-1; }
static double f0[N],kw[N],ph[N],q[N],qp[N],a1[N],a2[N],b[N],w[N],al[N],ow[N];
static double A[C][N],Lm[C][N],Sg[C][N],sw[N],cEta=0.6,dW[N],pW[N],oL[N],oR[N];
// controls
static double cDrive=0,cFc=200,cFocus=0,cDelay=0,cCurve=0.3,cTune=1,cDamp=0,cPosD=0.31,cPosP=0.31,cBloom=1,cGain=40;
static double strikeAt=-1,strikeV=0;
static void shapes(){ for(int j=0;j<N;j++){ dW[j]=cos(kw[j]*cPosD+ph[j]); pW[j]=cos(kw[j]*cPosP+ph[j]); } }
static void coefs(){ for(int j=0;j<N;j++){ double f=f0[j]*cTune; w[j]=2*M_PI*f; double t60=9.0/(1+f/700.0); al[j]=6.908/t60*(1+cDamp*(8+f/40.0));
    a1[j]=2*exp(-al[j]*T)*cos(w[j]*T); a2[j]=exp(-2*al[j]*T); b[j]=exp(-al[j]*T)*T*sin(w[j]*T)/w[j]; ow[j]=1.0/(1+f/1500.0); sw[j]=sqrt(w[j]); } }
typedef void (*Ctl)(double t);
static float*out; static double dl[4096]; static int dp=0; static long guards=0;
static double svl=0,svb=0;
static void render(double dur,Ctl ctl,const char*name,double f1){
  for(int j=0;j<N;j++){ f0[j]=f1*(1+0.55*(j+ (j?0.45*bi():0))); kw[j]=3.1*sqrt(f0[j]/f1); ph[j]=2*M_PI*rnd(); q[j]=qp[j]=0;
    oL[j]=cos(kw[j]*0.13+ph[j]); oR[j]=cos(kw[j]*0.88+ph[j]); }
  for(int c=0;c<C;c++) for(int j=0;j<N;j++){ A[c][j]=(rnd()<0.5?-1:1)/sqrt((double)C*N); Lm[c][j]=(0.3+rnd())/sqrt((double)C)*0.8; Sg[c][j]=(rnd()<0.5?-1:1)/sqrt((double)C); }
  long n=(long)(dur*FS); out=calloc(2*n,sizeof(float)); memset(dl,0,sizeof dl); svl=svb=0;
  FILE*dg=fopen("diag.txt","a"); fprintf(dg,"# %s\n",name);
  double Eacc[N]; memset(Eacc,0,sizeof Eacc); long dn=0;
  for(long i=0;i<n;i++){
    double t=i*T;
    if(i%BLK==0){ ctl(t); coefs(); shapes(); }
    // pickup
    double y=0; for(int j=0;j<N;j++) y+=pW[j]*(q[j]-qp[j]); y*=FS;
    // focus filter (state variable band-pass, unity at fc)
    double g=2*sin(M_PI*fmin(cFc,20000)/FS), Q=1.0+8*cFocus; svl+=g*svb; double hp=y-svl-svb/Q; svb+=g*hp; double bp=svb/Q;
    double yf=(1-cFocus)*y+cFocus*bp*2.0;
    dl[dp]=yf; int di=(dp-(int)(cDelay*FS)+4096*4)&4095; double yd=dl[di]; dp=(dp+1)&4095;
    double F=cDrive*tanh(cGain*yd/(cDrive+1e-9))+1e-4*bi();
    if(strikeAt>=0 && t>=strikeAt){ for(int j=0;j<N;j++) qp[j]-=strikeV*dW[j]*T*exp(-f0[j]/900.0); strikeAt=-1; }
    // stretch channels
    double S[C],Gq[C]; for(int c=0;c<C;c++){ double s=0,s2=0,gq=0; for(int j=0;j<N;j++){ double wq=w[j]*q[j]; s+=A[c][j]*wq; s2+=Lm[c][j]*wq*q[j]; gq+=Sg[c][j]*sw[j]*q[j]; } Gq[c]=cEta*cEta*gq; S[c]=cCurve*s+0.5*s2+0.5*Gq[c]*gq; }
    double L=0,R=0,E=0;
    for(int j=0;j<N;j++){ double fn=0; for(int c=0;c<C;c++) fn+=S[c]*(cCurve*A[c][j]*w[j]+Lm[c][j]*w[j]*q[j]+Gq[c]*Sg[c][j]*sw[j]);
      double qn=a1[j]*q[j]-a2[j]*qp[j]+b[j]*(dW[j]*F-cBloom*fn);
      double v=(qn-q[j])*FS; double e=0.5*v*v+0.5*w[j]*w[j]*qn*qn; E+=e; Eacc[j]+=e; L+=oL[j]*ow[j]*v; R+=oR[j]*ow[j]*v; qp[j]=q[j]; q[j]=qn; }
    if(!(E<1e9)){ guards++; for(int j=0;j<N;j++){ q[j]*=0.5; qp[j]*=0.5; } }
    out[2*i]=(float)L; out[2*i+1]=(float)R;
    if(++dn==(long)(0.5*FS)){ double s=0,s2=0,mx=0; int mj=0; for(int j=0;j<N;j++){ s+=Eacc[j]; s2+=Eacc[j]*Eacc[j]; if(Eacc[j]>mx){mx=Eacc[j];mj=j;} }
      fprintf(dg,"t %5.1f drive %7.1f E %9.3g PR %5.1f top %6.1f Hz (%4.1f%%) A_top %.3f\n",t,cDrive,s/dn,s*s/(s2+1e-30),f0[mj]*cTune,100*mx/(s+1e-30),sqrt(2*mx/dn)/w[mj]);
      memset(Eacc,0,sizeof Eacc); dn=0; }
  }
  fclose(dg); char fn[256]; sprintf(fn,"raw-%s.f32",name); FILE*f=fopen(fn,"wb"); fwrite(out,sizeof(float),2*n,f); fclose(f); free(out);
  fprintf(stderr,"%s done, guard events %ld\n",name,guards); guards=0;
}
static double lerp(double a,double b,double x){ x=fmax(0,fmin(1,x)); return a+(b-a)*x; }
static double xlerp(double a,double b,double x){ x=fmax(0,fmin(1,x)); return a*pow(b/a,x); }
// --- scenes
static void sweep(double t){ cFocus=1.0; cFc=262; cCurve=0.3; cDrive= t<30? xlerp(5,20000,t/30) : xlerp(20000,5,(t-30)/15); }

static double slew(double*st,double target,double rate){ *st+=(target-*st)*rate; return *st; }
static void base(){ cFocus=0.5; cFc=262; cCurve=0.6; cEta=1.0; cTune=1; cDamp=0; cDelay=0; cGain=40; cPosD=cPosP=0.31; }
// 1 one note, drive slowly up and back down
static void sc_swell(double t){ base(); cDrive= t<20? xlerp(250,60000,t/20) : (t<24?60000: xlerp(60000,150,(t-24)/14)); }
// 2 melody: the whole plate follows V/OCT, drive swells on each note to the edge of splitting
static double stT=1,stD=0;
static void sc_melody(double t){ base(); static const int nt[14]={0,3,7,5,3,10,7,12,10,7,3,5,0,0}; double L=1.7; int k=(int)(t/L); if(k>13)k=13; double x=t/L-k;
  slew(&stT,pow(2,nt[k]/12.0),0.02); cTune=stT; cFc=262*cTune; cFocus=0.8;
  double acc=(k==5||k==7||k==12)?1.0:0.0; double pk=acc? 9000:2600; double d=600+(pk-600)*sin(M_PI*fmin(1,x*1.15)); if(t>23.4) d=0; cDrive=slew(&stD,d,0.01); }
// 3 overblow: fixed plate, the focus pitch glides up through its modes
static void sc_overblow(double t){ base(); cFocus=1.0; cFc=xlerp(90,2400,t/30); cDrive=xlerp(1500,7000,t/30); cGain=60; if(t>30){cDrive=0;} }
// 4 infinite gong: struck once, then the loop catches the ring, holds it and lets it grow into a roar, then lets go
static int st4=0;
static void sc_infinite(double t){ base(); cFocus=0.0; cFc=200; cGain=25;
  if(!st4&&t>=1.0){ st4=1; strikeAt=t; strikeV=420; }
  cDrive= t<5?0 : t<9? xlerp(300,3000,(t-5)/4) : t<20? xlerp(3000,45000,(t-9)/11) : t<25? 45000 : 0; }
// 5 breathing drone: big plate, drive rocks across the edge between pure and roar, the driver wanders
static void sc_drone(double t){ base(); cFocus=0.35; cFc=110; cGain=40; cPosD=cPosP=0.31+0.12*sin(2*M_PI*t/37);
  double l=0.5-0.5*cos(2*M_PI*t/11.0); cDrive=xlerp(900,38000,l*l)*fmin(1,t/3)*(t>41?fmax(0,1-(t-41)/1.5):1); }
// 6 moving the driver across the plate at steady drive: the chord it sings keeps changing
static void sc_position(double t){ base(); cFocus=0.15; cFc=330; cGain=30; cDrive=2600*(t>27?0:1); cPosD=cPosP=0.12+0.7*t/28.0; }
// 7 pulses: gated drive, 125 BPM, the plate grabbed between notes, grab loosening over four bars
static int lastStep=-1;
static void sc_pulse(double t){ base(); static const int nt[16]={0,0,12,0,3,0,10,0,0,7,0,12,0,3,5,0}; static const int ac[16]={1,0,0,0,0,0,1,0,0,0,1,0,0,0,1,0};
  double st=60.0/125/4; int k=(int)(t/st); double x=t/st-k; int s=k%16; cTune=pow(2,nt[s]/12.0); cFc=196*cTune; cFocus=0.7; cGain=500;
  double open=fmin(1,t/(st*48)); int on=x<0.55 && t<st*64;
  cDrive= on? (ac[s]? xlerp(9000,30000,open):xlerp(2500,6000,open)) : 0; cDamp= on?0: xlerp(8,0.5,open);
  if(k!=lastStep && t<st*64){ lastStep=k; strikeAt=t; strikeV=ac[s]?260:120; } }
static double pF=1,pC=0.3,pE=0.6,pMax=20000;
static void sw2(double t){ cFocus=pF; cFc=262; cCurve=pC; cEta=pE; cDrive= t<30? xlerp(5,pMax,t/30) : xlerp(pMax,5,(t-30)/15); }
int main(int argc,char**argv){
  if(argc>5){ pF=atof(argv[2]); pC=atof(argv[3]); pE=atof(argv[4]); pMax=atof(argv[5]); remove("diag.txt"); render(48,sw2,"test",65.4); return 0; }
  remove("diag.txt");
  const char*a=argv[1];
  if(!strcmp(a,"swell")) render(40,sc_swell,"swell",65.4);
  if(!strcmp(a,"melody")) render(27,sc_melody,"melody",65.4);
  if(!strcmp(a,"overblow")) render(34,sc_overblow,"overblow",49.0);
  if(!strcmp(a,"infinite")) render(36,sc_infinite,"infinite",55.0);
  if(!strcmp(a,"drone")) render(46,sc_drone,"drone",27.5);
  if(!strcmp(a,"position")) render(32,sc_position,"position",82.4);
  if(!strcmp(a,"pulse")) render(10,sc_pulse,"pulse",49.0);
  return 0; }
