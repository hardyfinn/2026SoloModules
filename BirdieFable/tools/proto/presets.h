// Birdie factory presets, in engine units. The build turns each into a .vcvm preset.
#include "birdie_ref.h"
typedef struct { const char* name; BdParams p; uint32_t seed; } BdPreset;
#define TUNE_RATIO(r) ((log2(r)+1.0)/3.0)   /* follow on: throat = r x pitch */
static BdParams bd_default(void){ BdParams p; memset(&p,0,sizeof p);
  p.size=1.0; p.tension=0.0; p.pressure=0.22; p.breath=0.25; p.rough=0.0; p.v2mix=0.0; p.v2int=5.0; p.v2slide=-1.0;
  p.length=0.20; p.slide=-6.0; p.shape=SH_FALL; p.attack=0.15; p.release=0.30; p.buzzRate=120.0; p.buzz=0.0; p.chop=0.0;
  p.trachea=0.025; p.tube=0.6; p.throat=0.6; p.tune=1.0/3.0; p.follow=1; p.beak=10000.0;
  p.notes=6; p.rate=5.0; p.pattern=PT_MOTIF; p.vary=0.5; p.drift=0.0; p.accel=0.0; p.swell=0.0; p.time=1.0; p.hold=0; p.clockPeriod=0.0; return p; }
static int bd_presets(BdPreset* o){ int n=0; BdParams p;
  #define PRE(nm,sd) o[n].name=nm; o[n].p=p; o[n].seed=sd; n++;
  p=bd_default(); p.length=0.28; p.slide=-8; p.notes=4; p.rate=2.8; p.pattern=PT_REPEAT; p.vary=0; p.throat=0.7; p.beak=9000; PRE("Whistler",1)
  p=bd_default(); p.size=0.8; p.slide=-5.5; p.shape=SH_LINE; p.length=0.03; p.pressure=0.3; p.notes=24; p.rate=25; p.pattern=PT_REPEAT; p.vary=0; p.accel=0.3; p.drift=-2; PRE("Triller",2)
  p=bd_default(); p.size=0.9; p.tension=1; p.slide=5.5; p.shape=SH_ARCH; p.length=0.10; p.pressure=0.28; p.notes=7; p.rate=5; p.pattern=PT_MOTIF; p.vary=0.5; PRE("Chirper",3)
  p=bd_default(); p.tension=-1; p.slide=-1; p.shape=SH_LINE; p.length=0.5; p.buzzRate=140; p.buzz=0.25; p.chop=0.7; p.notes=4; p.rate=1.7; p.pattern=PT_TAIL; p.vary=0.5; p.throat=0.4; PRE("Buzzer",4)
  p=bd_default(); p.size=1.1; p.tension=-3; p.slide=3; p.v2mix=0.5; p.v2int=5; p.v2slide=-1; p.length=0.5; p.notes=5; p.rate=1.8; p.pattern=PT_CALL; p.vary=0.6; p.throat=0.35; p.trachea=0.03; PRE("Thrush",5)
  p=bd_default(); p.size=8; p.tension=0; p.pressure=0.14; p.slide=2; p.shape=SH_ARCH; p.length=0.7; p.attack=0.3; p.release=0.4; p.buzzRate=22; p.buzz=0.02; p.chop=0.25; p.breath=0.35;
                  p.throat=0.9; p.beak=3000; p.tube=0.3; p.trachea=0.02; p.notes=3; p.rate=1.1; p.pattern=PT_PAIR; p.vary=0.3; PRE("Dove",6)
  p=bd_default(); p.size=2.6; p.tension=-20; p.slide=-3; p.shape=SH_ARCH; p.pressure=0.4; p.rough=0.25; p.breath=0.8; p.length=0.38; p.attack=0.08; p.release=0.35;
                  p.throat=0.5; p.follow=0; p.tune=0.675; p.beak=14000; p.trachea=0.04; p.tube=0.6; p.notes=3; p.rate=1.7; p.pattern=PT_REPEAT; p.vary=0; PRE("Crow",7)
  p=bd_default(); p.size=4; p.tension=-24; p.slide=-3; p.pressure=0.4; p.rough=0.15; p.breath=0.6; p.length=0.16; p.attack=0.1; p.release=0.3; p.throat=0.6; p.follow=0; p.tune=0.72;
                  p.trachea=0.03; p.tube=0.5; p.beak=16000; p.notes=5; p.rate=4; p.pattern=PT_REPEAT; p.vary=0; p.accel=-0.4; p.swell=-0.6; p.drift=-2; PRE("Honker",8)
  p=bd_default(); p.tension=-2; p.slide=-5; p.length=0.12; p.notes=12; p.rate=8; p.pattern=PT_RUN; p.vary=1.0; PRE("Songster",9)
  p=bd_default(); p.size=0.7; p.tension=-1; p.slide=4; p.shape=SH_DOUBLE; p.length=0.08; p.notes=16; p.rate=12; p.pattern=PT_SHUFFLE; p.vary=0.8; PRE("Warbler",10)
  p=bd_default(); p.size=8; p.tension=-2; p.slide=-5; p.length=0.12; p.notes=12; p.rate=8; p.pattern=PT_RUN; p.vary=1.0; p.time=3; PRE("Giant",9)
  p=bd_default(); p.size=6; p.tension=-12; p.slide=16; p.shape=SH_DOUBLE; p.length=4.0; p.pressure=0.35; p.hold=1; p.buzzRate=5; p.buzz=0.3; p.v2mix=0.5; p.v2int=0.2; p.v2slide=1;
                  p.tube=0.75; p.trachea=0.04; p.throat=0.4; p.tune=TUNE_RATIO(1.5); p.beak=16000; p.notes=1; PRE("Drone",12)
  p=bd_default(); p.size=48; p.tension=-6; p.slide=-6; p.length=0.07; p.pressure=0.4; p.breath=0; p.attack=0.05; p.release=0.4; p.notes=16; p.clockPeriod=60.0/124/4; p.pattern=PT_SHUFFLE; p.vary=0.7;
                  p.beak=16000; p.trachea=0.005; p.tube=0.5; p.throat=0.25; p.tune=TUNE_RATIO(1.0); PRE("Bass Bird",13)
  p=bd_default(); p.size=2; p.tension=-14; p.slide=12; p.shape=SH_SWOOP; p.buzzRate=60; p.buzz=0.4; p.chop=0.5; p.rough=0.1; p.length=0.15; p.notes=8; p.rate=6; p.pattern=PT_CALL; p.vary=0.9; PRE("Robot",14)
  p=bd_default(); p.size=0.5; p.tension=-3; p.slide=-2.5; p.shape=SH_LINE; p.length=0.012; p.pressure=0.35; p.attack=0.3; p.notes=32; p.rate=40; p.pattern=PT_REPEAT; p.vary=0; p.swell=0.6; PRE("Tiny",15)
  p=bd_default(); p.size=10; p.tension=-1.5; p.pressure=0.12; p.slide=-1; p.length=0.45; p.attack=0.15; p.release=0.5; p.breath=0.2; p.throat=0.9; p.beak=2500; p.tube=0.3; p.notes=5; p.rate=1.6; p.pattern=PT_MOTIF; p.vary=0.2; p.swell=-0.3; PRE("Owl",16)
  return n; }
