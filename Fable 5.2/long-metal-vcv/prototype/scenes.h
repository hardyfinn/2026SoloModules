// Scenes for the Long Metal prototype.
static void defaults(void){
  p.N=256; p.M=4; p.dlen=2; p.a0=-0.9; p.ties=0; p.rhoL=-0.95; p.rhoR=-0.95; p.g=0.9995;
  p.lowcut=0; p.lowf=300; p.hidamp=0.002; p.hif=5000; p.warp=0; p.warpEnv=0; p.loose=0; p.age=0; p.pick=20; p.sat=1.0; p.grip=0; p.snap=0; p.gstuck=0.9; p.chatter=0;
  roughLevel=0; flatLevel=0; jointLevel=0; windLevel=0; nax=0; nw=0; njoint=0;
}
static double scene_setup(int sc){
  defaults();
  switch(sc){
  case 0: return 4;                                   // test: one far strike on a plain cable
  case 9: p.ties=0.12; p.dlen=10; p.M=3; p.a0=-0.7; p.rhoL=p.rhoR=0; return 3;  // test: supported rail, one strike
  case 5: // RAIL with break-loose onset: train rolls onto the line at 3 s
    p.N=320; p.M=3; p.dlen=9; p.a0=-0.72; p.ties=0.65; p.rhoL=0.3; p.rhoR=0.3; p.g=0.9955;
    p.lowcut=0.035; p.lowf=500; p.hidamp=0.004; p.hif=7000; p.age=0.35; p.pick=40;
    p.grip=1e-4; p.snap=18; p.gstuck=0.9; p.chatter=0.28;
    train_init(8); trainPos=347; trainSpeed=-9.0; roughLevel=1; flatLevel=1; jointLevel=1; wheelHz=5.5;
    njoint=3; joints[0]=215; joints[1]=150; joints[2]=85;
    return 46;
  case 6: // HARD HITS: supports + WARP + LOOSE, the liked half of sketch 2
    p.N=256; p.M=4; p.dlen=6; p.a0=-0.9; p.pick=12; p.g=0.9992; p.hidamp=0.0015; p.ties=0.14; p.age=0.3; p.warp=0.25; p.warpEnv=0.3;
    return 36;
  case 7: // LIVE LINE 2: more of the self-singing line
    p.N=200; p.M=3; p.dlen=3; p.a0=-0.6; p.ties=0.25; p.rhoL=-0.9; p.rhoR=0.9; p.g=1.0; p.sat=0.6;
    p.hidamp=0.01; p.hif=7000; p.lowcut=0.01; p.age=0.4; p.pick=60; return 44;
  case 1: // RAIL SHIMMER: train approaches from far, arrives, passes
    p.N=320; p.M=3; p.dlen=9; p.a0=-0.72; p.ties=0.65; p.rhoL=0.3; p.rhoR=0.3; p.g=0.9955;
    p.lowcut=0.035; p.lowf=500; p.hidamp=0.004; p.hif=7000; p.age=0.35; p.pick=40;
    train_init(8); trainPos=262; trainSpeed=-7.4; roughLevel=1; flatLevel=1; jointLevel=1; wheelHz=5.5;
    njoint=3; joints[0]=215; joints[1]=150; joints[2]=85;
    return 44;
  case 2: // ZAPS: struck cable, echoes that never repeat
    p.N=256; p.M=4; p.dlen=2; p.a0=-0.9; p.pick=12; p.g=0.9993; p.hidamp=0.0015; return 30;
  case 3: // WIND: cables singing
    p.N=48; p.M=3; p.dlen=8; p.a0=-0.8; p.rhoL=-0.985; p.rhoR=-0.985; p.g=0.9992; p.hidamp=0.003; p.hif=4000;
    p.ties=0.4; p.age=0.5; p.pick=15; wind_init(10); windLock=2; return 36;
  case 4: // LIVE LINE: gain above unity, the line sings by itself
    p.N=200; p.M=3; p.dlen=7; p.a0=-0.75; p.ties=0.16; p.rhoL=-0.9; p.rhoR=0.9; p.g=1.0; p.sat=0.6;
    p.hidamp=0.01; p.hif=6000; p.lowcut=0.01; p.age=0.4; p.pick=60; return 32;
  }
  return 1;
}
static void scene_ctl(int sc,double t){
  switch(sc){
  case 5: case 1: {
    double d=fabs(trainPos-p.pick);
    wheelHz=5.5; roughBright=0.35;
    if(t>(sc==5?42:40)) { roughLevel=flatLevel=jointLevel=lerp(1,0,(t-(sc==5?42:40))/4); }
  } break;
  case 7: {
    if(t<10){ p.g=lerp(1.003,1.010,sm(t/6)); p.a0=lerp(-0.6,-0.9,sm(t/10)); }
    else if(t<21){ p.warp=lerp(0,0.3,sm((t-10)/5)); p.ties=0.22+0.18*sin(2*M_PI*(t-10)/5.5); p.a0=-0.9+0.1*sin(2*M_PI*(t-10)/7.0); p.g=1.010; }
    else if(t<32){ p.dlen=14; p.loose=0.15; p.g=1.015; p.ties=0.2; p.a0=-0.72+0.15*sin(2*M_PI*(t-21)/2.2)*sm((t-21)/3); p.warp=0.25; }
    else if(t<40){ double ph=fmod((t-32)*(2.0+0.5*(t-32)/8),1.0); p.g=(ph<0.45)?1.03:0.985; p.dlen=7; p.a0=-0.8+0.1*sin(2*M_PI*(t-32)/3.1); p.loose=0.1; }
    else { p.g=lerp(1.0,0.99,(t-40)/2); }
  } break;
  case 3: {
    double gust=0.5+0.5*sin(2*M_PI*t/17.0-1.2)*sin(2*M_PI*t/5.3)+0.25*sin(2*M_PI*t/2.9);
    double u=lerp(0.25,1.0,sm(t/26.0))*(0.75+0.35*gust);
    if(t>31) u*=lerp(1,0.2,(t-31)/5);
    windHz=150+700*u; windLevel=0.12*u*u+0.004; windTurb=0.18;
    p.warpEnv=0.0;
  } break;
  case 4: {
    double x=t/32.0;
    p.g=lerp(1.002,1.012,sm(t/12.0)); if(t>24) p.g=lerp(1.012,0.996,(t-24)/5);
    p.warp=lerp(0,0.3,sm((t-8)/12.0)); p.loose=lerp(0,0.15,sm((t-14)/8.0));
    p.a0=-0.75+0.12*sin(2*M_PI*t/9.0)*sm((t-6)/6.0);
    (void)x;
  } break;
  }
}
static void zap(long t,double at,double pos,double amp,double ms){ if(t==(long)(at*SR)) strike(pos,amp,ms); }
static void scene_audio(int sc,long t){
  switch(sc){
  case 0: zap(t,0.05,230,1,0.1); strike_step(); break;
  case 9: zap(t,0.05,230,1,0.1); strike_step(); break;
  case 5: case 1: train_step(); break;
  case 2: {
    double ts=t/SR;
    // a: far strike, plain cable, let the echoes run
    zap(t,0.2,240,1,0.08);
    // b: nearer and nearer
    if(t==(long)(5.0*SR)){ p.a0=-0.9; }
    zap(t,5.0,200,1,0.08); zap(t,5.8,140,1,0.08); zap(t,6.6,80,1,0.08); zap(t,7.4,30,1,0.08);
    // c: longer line (discrete echoes)
    if(t==(long)(9.5*SR)){ p.dlen=40; p.g=0.9985; }
    zap(t,9.6,240,1.2,0.08);
    // d: supports on: the zap rings on the support tones
    if(t==(long)(15.5*SR)){ p.dlen=6; p.ties=0.14; p.g=0.9992; p.age=0.3; }
    zap(t,15.6,240,1.2,0.08); zap(t,17.6,120,1.2,0.08);
    // e: hit hard with WARP: dispersion depends on the signal
    if(t==(long)(20.5*SR)){ p.dlen=3; p.ties=0.0; p.warp=0.25; p.warpEnv=0.3; p.g=0.9993; p.age=0.2; }
    zap(t,20.6,240,3,0.1); zap(t,22.4,240,8,0.1); zap(t,24.4,180,20,0.12);
    zap(t,26.0,60,20,0.12); zap(t,26.5,200,20,0.12); zap(t,27.0,120,20,0.12);
    (void)ts; strike_step();
  } break;
  case 6: {
    // A: supported line, hits from three distances, harder each time
    zap(t,0.2,240,8,0.1); zap(t,2.2,120,14,0.1); zap(t,4.0,240,24,0.12); zap(t,5.6,40,24,0.12);
    // B: plain stiff cable, signal-dependent dispersion, a rhythm of hard hits
    if(t==(long)(8.0*SR)){ p.dlen=3; p.ties=0.0; p.age=0.2; }
    zap(t,8.1,240,20,0.12); zap(t,9.6,180,20,0.12); zap(t,10.6,60,20,0.12); zap(t,11.1,200,20,0.12); zap(t,11.6,120,20,0.12);
    zap(t,12.6,240,35,0.15); zap(t,13.1,30,35,0.15); zap(t,13.35,150,35,0.15); zap(t,14.1,220,50,0.15);
    // C: strong, rattling supports
    if(t==(long)(16.5*SR)){ p.dlen=5; p.ties=0.3; p.loose=0.5; p.warp=0.4; p.age=0.45; p.g=0.9994; }
    zap(t,16.6,240,20,0.12); zap(t,18.6,100,40,0.15); zap(t,20.4,200,60,0.15); zap(t,21.4,60,60,0.15); zap(t,22.0,160,60,0.15);
    // D: long line, break-loose threshold on: soft hit does nothing, hard hits crack and avalanche
    if(t==(long)(25.0*SR)){ p.dlen=18; p.ties=0.12; p.loose=0.2; p.warp=0.3; p.age=0.35; p.g=0.9988; p.grip=2e-3; p.snap=12; p.gstuck=0.9;
      for(int i=0;i<NMAX;i++){stuck[i]=1; gl[i]=-1; rel[i]=0; es[i]=0;} }
    zap(t,25.2,240,0.3,0.12); zap(t,26.4,240,30,0.15); zap(t,29.2,140,50,0.15); zap(t,31.6,220,60,0.15); zap(t,32.0,60,60,0.15);
    strike_step();
  } break;
  case 7: { inject(100,(t<SR*0.3?0.002:2e-5)*nrand());
    zap(t,12.0,150,4,0.15); zap(t,15.0,40,6,0.15); zap(t,17.5,120,8,0.15); zap(t,24.0,150,6,0.15); zap(t,27.5,80,8,0.15); strike_step(); } break;
  case 3: wind_step(); break;
  case 4: { inject(100,(t<SR*0.3?0.002:2e-5)*nrand()); } break;
  }
}
