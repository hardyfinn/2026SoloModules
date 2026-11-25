// Scenes for the Long Metal reference. rail, hard, live and live2 are the sketches Hardy approved.
enum { SC_RAIL, SC_ZAPS, SC_HARD, SC_LIVE, SC_LIVE2, SC_FUZZ, SC_LOSSLESS };
static int scene_id(const char*s){ const char*n[]={"rail","zaps","hard","live","live2","fuzz","lossless"}; for(int i=0;i<7;i++) if(!strcmp(s,n[i])) return i; return -1; }
static void defaults(void){
  p.N=256; p.M=4; p.d=2; p.a0=-0.9; p.ties=0; p.rhoL=-0.95; p.rhoR=-0.95; p.g=0.9995;
  p.lowcut=0; p.lowf=500; p.hidamp=0.002; p.hif=5000; p.warp=0; p.swell=0; p.loose=0; p.age=0; p.pick=20; p.sat=0.6;
  p.grip=0; p.snap=12; p.gstuck=0.9;
  roughLevel=0; flatLevel=0; jointLevel=0; nax=0; njoint=0;
}
static double fz_next=0;
static void fuzz_draw(void){
  p.N=16+(int)(urand()*305); p.M=urand()<0.5?3:4; p.d=1+(int)(urand()*40); p.a0=-0.95*urand(); p.ties=1.3*urand()*urand();
  p.rhoL=0.98*(2*urand()-1); p.rhoR=0.98*(2*urand()-1); p.g=urand()<0.5?1-0.02*pow(urand(),2.5):1+0.03*pow(urand(),1.5);
  p.lowcut=0.07*urand(); p.hidamp=0.012*urand()*urand(); p.hif=6000; p.warp=0.6*urand(); p.swell=urand(); p.loose=0.8*urand();
  p.grip=urand()<0.5?0:2e-5*pow(1000,urand()); p.age=urand(); p.pick=2+urand()*(p.N-5);
  roughLevel=4*urand()*urand(); flatLevel=2*roughLevel*urand(); jointLevel=flatLevel; wheelHz=2*pow(8,urand()); roughDens=120*(0.3+1.43*urand());
  roughBright=0.15+0.4*urand(); trainPos=urand()*p.N; trainSpeed=(urand()-0.5)*200; trainSide=urand()<0.5?1:-1;
  njoint=3; joints[0]=(int)(0.27*p.N); joints[1]=(int)(0.47*p.N); joints[2]=(int)(0.67*p.N);
  strike(urand()*p.N,100*urand(),0.06+0.6*urand());
}
static double scene_setup(int sc){
  defaults();
  switch(sc){
  case SC_RAIL: // RAIL SHIMMER: train approaches from far, arrives, passes (sketch 01)
    p.N=320; p.M=3; p.d=9; p.a0=-0.72; p.ties=0.65; p.rhoL=0.3; p.rhoR=0.3; p.g=0.9955;
    p.lowcut=0.035; p.hidamp=0.004; p.hif=7000; p.age=0.35; p.pick=40;
    train_init(8); trainPos=262; trainSpeed=-7.4; trainSide=1; roughLevel=1; flatLevel=1; jointLevel=1; wheelHz=5.5;
    njoint=3; joints[0]=215; joints[1]=150; joints[2]=85;
    return 44;
  case SC_ZAPS: // clean struck cable (sketch 02; Hardy found the clean first half boring: this is the quiet end only)
    p.N=256; p.M=4; p.d=2; p.a0=-0.9; p.pick=12; p.g=0.9993; p.hidamp=0.0015; return 30;
  case SC_HARD: // HARD HITS (sketch 06, "excellent")
    p.N=256; p.M=4; p.d=6; p.a0=-0.9; p.pick=12; p.g=0.9992; p.hidamp=0.0015; p.ties=0.14; p.age=0.3; p.warp=0.25; p.swell=0.3;
    return 36;
  case SC_LIVE: // LIVE LINE (sketch 04)
    p.N=200; p.M=3; p.d=7; p.a0=-0.75; p.ties=0.16; p.rhoL=-0.9; p.rhoR=0.9; p.g=1.0;
    p.hidamp=0.01; p.hif=6000; p.lowcut=0.01; p.age=0.4; p.pick=60; return 32;
  case SC_LIVE2: // LIVE LINE 2 (sketch 07)
    p.N=200; p.M=3; p.d=3; p.a0=-0.6; p.ties=0.25; p.rhoL=-0.9; p.rhoR=0.9; p.g=1.0;
    p.hidamp=0.01; p.hif=7000; p.lowcut=0.01; p.age=0.4; p.pick=60; return 44;
  case SC_FUZZ: train_init(8); fuzz_draw(); return 30;
  case SC_LOSSLESS: // no loss, pushed controls on and moving: energy must stay constant
    p.N=200; p.M=4; p.d=5; p.a0=-0.8; p.ties=0.3; p.rhoL=-1; p.rhoR=1; p.g=1.0; p.hidamp=0; p.lowcut=0; p.age=0;
    p.warp=0.5; p.swell=0.8; p.loose=0.6; p.pick=50; return 10;
  }
  return 1;
}
static void scene_ctl(int sc,double t){
  switch(sc){
  case SC_RAIL: if(t>40) { roughLevel=flatLevel=jointLevel=lerp(1,0,(t-40)/4); } break;
  case SC_LIVE:
    p.g=lerp(1.002,1.012,sm(t/12.0)); if(t>24) p.g=lerp(1.012,0.996,(t-24)/5);
    p.warp=lerp(0,0.3,sm((t-8)/12.0)); p.loose=lerp(0,0.15,sm((t-14)/8.0));
    p.a0=-0.75+0.12*sin(2*M_PI*t/9.0)*sm((t-6)/6.0);
    break;
  case SC_LIVE2:
    if(t<10){ p.g=lerp(1.003,1.010,sm(t/6)); p.a0=lerp(-0.6,-0.9,sm(t/10)); }
    else if(t<21){ p.warp=lerp(0,0.3,sm((t-10)/5)); p.ties=0.22+0.18*sin(2*M_PI*(t-10)/5.5); p.a0=-0.9+0.1*sin(2*M_PI*(t-10)/7.0); p.g=1.010; }
    else if(t<32){ p.d=14; p.loose=0.15; p.g=1.015; p.ties=0.2; p.a0=-0.72+0.15*sin(2*M_PI*(t-21)/2.2)*sm((t-21)/3); p.warp=0.25; }
    else if(t<40){ double ph=fmod((t-32)*(2.0+0.5*(t-32)/8),1.0); p.g=(ph<0.45)?1.03:0.985; p.d=7; p.a0=-0.8+0.1*sin(2*M_PI*(t-32)/3.1); p.loose=0.1; }
    else { p.g=lerp(1.0,0.99,(t-40)/2); }
    break;
  case SC_FUZZ: if(t>=fz_next){ fz_next+=0.5; fuzz_draw(); } break;
  case SC_LOSSLESS: p.a0=-0.8+0.15*sin(2*M_PI*t/1.3); p.ties=0.3+0.25*sin(2*M_PI*t/0.9); break;
  }
}
static void zap(long t,double at,double pos,double amp,double ms){ if(t==(long)(at*SR)) strike(pos,amp,ms); }
static void scene_audio(int sc,long t){
  switch(sc){
  case SC_RAIL: train_step(); break;
  case SC_ZAPS:
    zap(t,0.2,240,1,0.08);
    zap(t,5.0,200,1,0.08); zap(t,5.8,140,1,0.08); zap(t,6.6,80,1,0.08); zap(t,7.4,30,1,0.08);
    if(t==(long)(9.5*SR)){ p.d=40; p.g=0.9985; }
    zap(t,9.6,240,1.2,0.08);
    if(t==(long)(15.5*SR)){ p.d=6; p.ties=0.14; p.g=0.9992; p.age=0.3; }
    zap(t,15.6,240,1.2,0.08); zap(t,17.6,120,1.2,0.08);
    if(t==(long)(20.5*SR)){ p.d=3; p.ties=0.0; p.warp=0.25; p.swell=0.3; p.g=0.9993; p.age=0.2; }
    zap(t,20.6,240,3,0.1); zap(t,22.4,240,8,0.1); zap(t,24.4,180,20,0.12);
    zap(t,26.0,60,20,0.12); zap(t,26.5,200,20,0.12); zap(t,27.0,120,20,0.12);
    strike_step(); break;
  case SC_HARD:
    // A: supported line, hits from three distances, harder each time
    zap(t,0.2,240,8,0.1); zap(t,2.2,120,14,0.1); zap(t,4.0,240,24,0.12); zap(t,5.6,40,24,0.12);
    // B: plain stiff cable, signal-dependent dispersion, a rhythm of hard hits
    if(t==(long)(8.0*SR)){ p.d=3; p.ties=0.0; p.age=0.2; }
    zap(t,8.1,240,20,0.12); zap(t,9.6,180,20,0.12); zap(t,10.6,60,20,0.12); zap(t,11.1,200,20,0.12); zap(t,11.6,120,20,0.12);
    zap(t,12.6,240,35,0.15); zap(t,13.1,30,35,0.15); zap(t,13.35,150,35,0.15); zap(t,14.1,220,50,0.15);
    // C: strong, rattling supports
    if(t==(long)(16.5*SR)){ p.d=5; p.ties=0.3; p.loose=0.5; p.warp=0.4; p.age=0.45; p.g=0.9994; }
    zap(t,16.6,240,20,0.12); zap(t,18.6,100,40,0.15); zap(t,20.4,200,60,0.15); zap(t,21.4,60,60,0.15); zap(t,22.0,160,60,0.15);
    // D: long line, GRIP on: the soft hit does nothing, hard hits crack and avalanche
    if(t==(long)(25.0*SR)){ p.d=18; p.ties=0.12; p.loose=0.2; p.warp=0.3; p.age=0.35; p.g=0.9988; p.grip=2e-3; p.snap=12; grip_reset(); }
    zap(t,25.2,240,0.3,0.12); zap(t,26.4,240,30,0.15); zap(t,29.2,140,50,0.15); zap(t,31.6,220,60,0.15); zap(t,32.0,60,60,0.15);
    strike_step(); break;
  case SC_LIVE: inject(100,(t<SR*0.3?0.002:2e-5)*nrand()); break;       // thermal seed: without it a live line stays silent
  case SC_LIVE2: inject(100,(t<SR*0.3?0.002:2e-5)*nrand());
    zap(t,12.0,150,4,0.15); zap(t,15.0,40,6,0.15); zap(t,17.5,120,8,0.15); zap(t,24.0,150,6,0.15); zap(t,27.5,80,8,0.15); strike_step(); break;
  case SC_FUZZ: train_step(); strike_step(); inject(p.N*0.5,(urand()<0.3?5.0:0.0)*nrand()); break;
  case SC_LOSSLESS: if(t<4800) inject(100,0.5*nrand()); break;
  }
}
