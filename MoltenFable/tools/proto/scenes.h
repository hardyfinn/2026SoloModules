// 1 HEAT ALONE: one held note at a fixed, modest drive. Only HEAT moves: cold, red-hot, molten, then a quench.
static void sc_heat(double t){ base(); cFocus=0.5; cFc=196; cDrive=2500*fmin(1,t/2)*(t>41?fmax(0,1-(t-41)/2):1);
  cHeat= t<5?0 : t<17? lerp(0,1.0,(t-5)/12) : t<27? lerp(1.0,1.8,(t-17)/10) : t<32?1.8 : 0;
  cQuench= t>32? 0.3*seg(t,32,32.3) : 0; cRate= t>32?0.4:3; }
// 2 CAST: the same body melted, stirred by the loop and quenched four times. Each time it sets as a different bell.
static int k2=-1;
static void sc_cast(double t){ base(); cFocus=0.2; cFc=147; cGain=40; double L=9.5; int k=(int)(t/L); if(k>3)k=3; double x=t-k*L;
  cHeat= x<3.2?1.85:0; cRate= x<3.2?6:0.5; cQuench= x>3.2&&x<6? 0.5:0; cUc=0; cLatent=0.3;
  cDrive= x<3.2? 5000 : x<6? xlerp(5000,900,(x-3.2)/2.8) : 900; if(t>37) cDrive=0;
  static double sx[4]={0.2,0.55,0.33,0.71}; cPosD=cPosP=sx[k];
  if(x>6.3&&k!=k2){ k2=k; strikeAt=t; strikeV=420; strikeX=0.23; } }
// 3 FLAME: steady drive, a narrow flame walks along the body and melts it a patch at a time.
static void sc_flame(double t){ base(); cFocus=0.25; cFc=220; cGain=35; cDrive=3200*fmin(1,t/2)*(t>38?fmax(0,1-(t-38)/2):1);
  cWf=0.16; cCond=0.6; cAir=0.9; cXf=0.05+0.9*(0.5-0.5*cos(2*M_PI*t/26)); cHeat=2.0*seg(t,2,6); cRate=9; cLatent=0.25; cPing=0.2;
  if(t>34){ cHeat=0; cQuench=0.4; } }
// 4 PUSHED: hard drive with the amp up, HEAT rocking across the melting point faster and faster, quench at the end.
static void sc_pushed(double t){ base(); cFocus=0.4; cFc=110; cGain=80; cBright=1; cPick=1; cAmp=0.6*seg(t,2,12); cOut=0.6*seg(t,10,20);
  cDrive=xlerp(3000,40000,seg(t,0,10)); double phs=0.12*t+0.03*t*t; cHeat=1.0+0.8*sin(2*M_PI*phs); cRate=5+t*0.5; cLatent=0.25; cLoose=3; cLiqNL=40; cPing=1;
  if(t>26){ cHeat=0; cQuench=0.5; cDrive=xlerp(40000,500,seg(t,26,30)); } if(t>33) cDrive=0; }
int main(int argc,char**argv){ const char*a=argv[1]; if(getenv("RM")) RM=atof(getenv("RM"));
  if(!strcmp(a,"heat")) render(45,sc_heat,"heat",65.4);
  if(!strcmp(a,"cast")) render(40,sc_cast,"cast",55.0);
  if(!strcmp(a,"flame")) render(42,sc_flame,"flame",65.4);
  if(!strcmp(a,"pushed")) render(34,sc_pushed,"pushed",49.0);
  return 0; }
