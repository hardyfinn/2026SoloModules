static double pS,pB,pG,pBow,pV,pDip,pSnap,pHold,pNL=1;
static int st0=0;
static void test(double t){ base(); cShape=pS; cBendHand=pB; cGrip=pG; cHold=0.9; cDip=pDip; cSnap=pSnap; cNL=pNL;
  if(pBow>0){ cBowP=pBow*fmin(1,t/0.5); cBowV=pV; if(t>6) cBowP=0; }
  else if(!st0&&t>=0.5){ st0=1; strikeAt=t; strikeV=pV; } }
// T-flex: arch, struck, hand swings the bend through zero at 1.5 Hz; hold loose; snap param
static int kf=-1;
static void tflex(double t){ base(); cShape=0; cGrip=pG; cHold=pHold; cSnap=pSnap; cBendHand=pB*sin(2*M_PI*1.5*t); int k=(int)(t/2); if(pV>0&&k!=kf){ kf=k; strikeAt=t; strikeV=pV; } }
// T-dip: struck arch (bend pB), dipped 0 -> pDip -> 0 over 2..8 s
static void tdip(double t){ base(); cShape=pS; cGrip=pG; cHold=0.9; cBendHand=pB; cDip=pDip*sin(M_PI*seg(t,2,8)); if(!st0&&t>=0.5){ st0=1; strikeAt=t; strikeV=pV; } }

static double slw(double*st,double target,double tc){ *st+=(target-*st)*(1-exp(-(BLK*T)/tc)); return *st; }
// ---- 1 SAW: tapered blade in an S-bend, gentle bow riding the S-point; melody played by moving the S-point and the bend
static double s1sp=-0.229,s1b=0,s1p=0,s1v=0.15;
static void sc_saw(double t){ base(); cTaper=1; cShape=1; cGrip=1; cHold=0.9;
  static const double sp[7]={-0.350,-0.229,-0.046,0.075,0.196,0.379,0.500}, bd[7]={3.407,4.132,5.483,6.591,7.897,10.283,12.204};
  static const int nt[13]={1,4,3,2,1,0,1,2,4,5,4,3,1}; static const double du[13]={2.0,2.0,1.5,1.5,2.0,1.5,1.0,1.5,2.0,2.5,1.5,1.5,9.0};
  double a=0.8; int k=0; while(k<12 && t>a+du[k]){ a+=du[k]; k++; } double x=t-a; int n=nt[k];
  double vib=1+0.010*sin(2*M_PI*5.3*t)*seg(x,0.35,1.0);
  slw(&s1sp,sp[n],0.16); slw(&s1b,bd[n],0.16); cSpot=s1sp; cBendHand=s1b*vib; cBowX=0.5+0.5*cSpot;
  double P=0.22,V=0.14+0.08*sin(M_PI*fmin(1,x/du[k]));
  if(k==12){ P=lerp(0.22,1.0,seg(x,1.5,4.0))-0.78*seg(x,5.0,5.8); V=lerp(0.2,1.1,seg(x,1.5,4.5))-0.9*seg(x,5.0,5.8); if(x>7.0) P=0; }
  if(t<0.8) P=0; cBowP=slw(&s1p,P,0.05); cBowV=slw(&s1v,V,0.08); }
// ---- 2 FLEX: flexatone glide, then wobble-board whoops through flat, then shaken into thunder
static int s2k=-1; static double s2b=1,s2n=0;
static void sc_flex(double t){ base(); cShape=0; cGrip=0.12; cHard=0.6;
  if(t<10){ cHold=0.9; double g=1.2+2.4*(0.5-0.5*cos(2*M_PI*t/4.2))*(1+0.5*seg(t,4,9)); slw(&s2b,g*(1+0.04*sin(2*M_PI*6.5*t)),0.03); cBendHand=s2b;
    int k=(int)(t*11.5); if(k!=s2k && t>0.3 && t<9.2){ s2k=k; strikeAt=t; strikeV=(k&1)?16:11; cStrikeX=(k&1)?0.37:0.58; } }
  else if(t<22){ double x=t-10; cHold=0.2; cSnap=0.55*seg(x,0,2); cGrip=0.2; double r=lerp(2.0,3.2,seg(x,2,11)); static double ph=0; ph+=2*M_PI*r*BLK*T; cBendHand=3.0*sin(ph)*seg(x,0,0.5); }
  else { double x=t-22; cHold=0.38; cSnap=0.9; cGrip=0.04; s2n+=(bi()-s2n)*0.01; static double ph=0; ph+=2*M_PI*(3.0+3.0*seg(x,0,7)+0.8*sin(2*M_PI*x/2.3))*BLK*T;
    double amp=lerp(3.5,8,seg(x,0,7)); double on=(x<9.5?1:0); cBendHand=amp*sin(ph)*on; cShake=on*3e4*seg(x,1,7)*s2n; cKick=1+1.2*seg(x,0,7); cNL=1+seg(x,2,8); } }
// ---- 3 DIP: a hanging sheet struck and dunked, then a bowed saw note lowered into the water
static int s3k=0; static double s3d=0,s3p=0;
static void sc_dip(double t){ base(); cDepth=1.3; double d=0;
  if(t<15){ cShape=0; cGrip=0; cHold=0.9; cBendHand=1.0; cHard=0.45;
    static const double st[2]={0.5,7.5}; if(s3k<2 && t>=st[s3k]){ strikeAt=t; strikeV= s3k?50:40; cStrikeX= s3k?0.62:0.37; s3k++; }
    if(t<7.5) d=(0.35+0.2*seg(t,1,6))*(0.5-0.5*cos(2*M_PI*(t-1.0)/1.6))*seg(t,1.0,1.1)*(t<5.8?1:0);
    else d=0.9*seg(t,8.2,11.0)-0.9*seg(t,11.5,14.0); }
  else { double x=t-15; cShape=1; cTaper=1; cGrip=1; cHold=0.9; cSpot=0.075; cBendHand=6.591*fmin(1,x/0.3+0.3); cBowX=0.5+0.5*cSpot;
    double P= x<12.5?0.24:0; cBowP=slw(&s3p,P,0.05); cBowV=0.2;
    cDepth=0.8; d=0.60*(0.5-0.5*cos(2*M_PI*seg(x,2.5,8.5)))+0.68*seg(x,9.5,12)-0.68*seg(x,13.5,15.5); }
  cDip=slw(&s3d,d,0.03); }
// ---- 4 BREAK-UP: heavy bow on a loose, aged sheet whose bend is swept through flat, then thrown about
static double s4p=0,s4v=0.3;
static void sc_break(double t){ base(); cTaper=0.5; cGrip=0.22; cAge=0.6*seg(t,6,20); cNL=1+0.8*seg(t,10,24);
  cShape=0.5+0.5*sin(2*M_PI*t/13.0); cSpot=0.35*sin(2*M_PI*t/7.3); cBowX=0.5+0.5*cSpot*cShape+0.12*sin(2*M_PI*t/5.1);
  cHold=lerp(0.8,0.3,seg(t,19,24)); cSnap=0.7*seg(t,19,23);
  static double ph=0; ph+=2*M_PI*(lerp(0.35,0.9,seg(t,8,19))+1.6*seg(t,20,25))*BLK*T;
  double sw=lerp(0.4,1.0,seg(t,4,14)); cBendHand=3.5*(1-sw)+5.5*sw*sin(ph);
  double P=lerp(0.35,1.0,seg(t,0.5,8))*(1+0.6*seg(t,12,22)), V=lerp(0.3,2.6,seg(t,2,22)); if(t>27) P=0;
  cBowP=slw(&s4p,P,0.05); cBowV=slw(&s4v,V,0.05); cKick=0.12+0.7*seg(t,20,24); }
// ---- 5 BOWED, ALIVE: the bowed voice pushed: ladder hops, bend arcs, surges into roar, dragged through water
static double s5p=0,s5v=0.3,s5d=0;
static void sc_alive(double t){ base(); cTaper=0.7; cShape=1; cGrip=0.6; cHold=0.85; cAge=0.3; cNL=1.3; cDepth=1.1;
  cSpot= t<4 ? lerp(-0.3,0.3,seg(t,0.3,3.8)) : 0.1+0.3*sin(2*M_PI*(t-4)/9.0);
  double b=5; if(t>4) b=5.5+3.5*sin(2*M_PI*(t-4)*0.4)*seg(t,4,6); cBendHand=b*(1+0.012*sin(2*M_PI*5.1*t));
  cBowX=0.5+0.5*cSpot+0.13*sin(2*M_PI*t/3.7)*seg(t,1,4);
  double P=0.45+0.15*sin(2*M_PI*t/1.3), V=0.5;
  if(t>4){ double sg=pow(0.5-0.5*cos(2*M_PI*(t-4)/2.0),3); P=0.5+0.5*sg; V=0.5+0.9*sg; }
  if(t>14){ P=0.7+0.2*sin(2*M_PI*t/1.9); V=0.9+0.25*sin(2*M_PI*t/2.7); }
  if(t>24){ cShape=1-0.55*seg(t,24,28); P=lerp(0.8,1.2,seg(t,24,28)); V=lerp(0.9,1.3,seg(t,24,29)); cGrip=lerp(0.6,0.08,seg(t,24,27)); }
  if(t>30.5) P=0;
  double d= t<14?0 : t<24? 0.5*(0.5-0.5*cos(2*M_PI*(t-14)/3.3)) : 0.55*seg(t,31.5,35.5);
  cDip=slw(&s5d,d,0.04); cBowP=slw(&s5p,P,0.05); cBowV=slw(&s5v,V,0.06); cKick=0.3; }
// ---- fuzz: every control random over its full (past-physical) range, new values every 0.5 s
static unsigned long long fzs=1; static double fr_(){ fzs^=fzs<<13; fzs^=fzs>>7; fzs^=fzs<<17; return (fzs>>11)*(1.0/9007199254740992.0); }
static int fzk=-1; static double fv[24];
static void sc_fuzz(double t){ int k=(int)(t/0.5); if(k!=fzk){ fzk=k; for(int i=0;i<24;i++) fv[i]=fr_(); if(fv[20]<0.4){ strikeAt=t; strikeV=80*fv[21]; } }
  base(); cTune=pow(2,6*fv[0]-3); cTaper=fv[1]; cGrip=fv[2]; cBendHand=24*(2*fv[3]-1)*fabs(2*fv[3]-1); cShape=fv[4]; cSpot=0.8*(2*fv[5]-1); cHold=fv[6]; cSnap=1.5*fv[7]; cCrack=3*fv[8]*fv[8];
  cDip=fv[9]<0.3?0:fv[9]; cDepth=fv[10]<0.25? -1 : 4*fv[10]*fv[10]; cBowP=fv[11]<0.2?0:1.6*fv[11]; cBowV=0.05*pow(60,fv[12]); cBowX=0.1+0.8*fv[13]; cHard=fv[14]; cNL=3*fv[15]; cAge=fv[16]; cPick=fv[17]; cOut=fv[18];
  cIn=fv[19]<0.3? 3000*bi():0; cKick=1; }
int main(int argc,char**argv){
  const char*a=argv[1];
  if(!strcmp(a,"test")){ pS=atof(argv[2]); pB=atof(argv[3]); pG=atof(argv[4]); pBow=atof(argv[5]); pV=atof(argv[6]); pDip=argc>7?atof(argv[7]):0; pSnap=argc>8?atof(argv[8]):0; pNL=argc>9?atof(argv[9]):1; render(10,test,argc>10?argv[10]:"test",110); }
  if(!strcmp(a,"tflex")){ pB=atof(argv[2]); pG=atof(argv[3]); pHold=atof(argv[4]); pSnap=atof(argv[5]); pV=atof(argv[6]); render(10,tflex,"test",110); }
  if(!strcmp(a,"tdip")){ pS=atof(argv[2]); pB=atof(argv[3]); pG=atof(argv[4]); pDip=atof(argv[5]); pV=atof(argv[6]); render(10,tdip,"test",110); }
  if(!strcmp(a,"saw")) render(37,sc_saw,"saw",180);
  if(!strcmp(a,"flex")) render(36,sc_flex,"flex",130);
  if(!strcmp(a,"dip")) render(33,sc_dip,"dip",98);
  if(!strcmp(a,"break")) render(31,sc_break,"break",82);
  if(!strcmp(a,"alive")) render(37,sc_alive,"alive",110);
  if(!strcmp(a,"fuzz")){ fzs=atoll(argv[2])*2654435761ULL+1; render(30,sc_fuzz,argv[3],130.81); }
  return 0; }
