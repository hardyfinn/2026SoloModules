// Scenes for the reference engine, written in panel terms (knob values through the inverse maps of mapping.h),
// so that every sketch is reachable from the panel. A value outside a knob's range prints a warning once.
static int warned=0;
static double chk(double k,const char*nm){ if((k<-1e-9||k>1.0+1e-9)&&warned<20){ fprintf(stderr,"WARNING: %s knob %.3f is out of range\n",nm,k); warned++; } return lw_clamp(k,0,1); }
#define SET(field,expr) (k->field=chk(expr,#field))
static double seg(double t,double t0,double t1,double a,double b){ if(t<=t0) return a; if(t>=t1) return b; return a+(b-a)*(t-t0)/(t1-t0); }
static double mtof(double m){ return 440.0*pow(2.0,(m-69)/12.0); }
static void src(K*k,double Edc,double Eac){ double E=Edc+Eac; SET(volts,I_VOLTS(E)); if(E>0) k->mains=Eac/E; }
static void kdef(K*k){
  memset(k,0,sizeof(K));
  k->volts=0; k->mains=1; SET(freq,I_FREQ(50)); SET(current,I_CUR(1.0)); SET(choke,I_CHOKE(0.003)); SET(gap,I_GAP(1.0));
  SET(heat,I_HEAT(0.35)); SET(cool,I_COOL(120e-6)); SET(quench,I_QUENCH(3e-3)); SET(memory,I_MEM(0.03)); SET(restrike,I_RESTR(0.85)); SET(jitter,I_JIT(0.08));
  SET(wind,I_WIND(0.02)); SET(climb,0); SET(tune,I_TUNE(2500)); SET(ring,I_RING(4)); SET(load,I_LOAD(6)); SET(iron,0);
  SET(corona,0); SET(hum,0); SET(fb,0); SET(dist,I_DIST(0.002)); k->age=0; SET(arc,I_ARC(1.0)); SET(field,I_FIELD(0.15)); SET(out,I_OUT(1.0)); k->draw=0; k->in=0;
}
static int scene_id(const char*s){ const char*n[]={"arc","sing","kit","kit2","sub"}; for(int i=0;i<5;i++) if(!strcmp(s,n[i])) return i; return -1; }
static double scene_dur(int sc){ double d[]={27,31.2,23.4,23.4,26}; return d[sc]; }

static void scene(int sc,double t,long n,double sr,K*kk,int*ninst){
  K*k=&kk[0]; *ninst=1; kdef(k);
  if(sc==0){ // raw arc: touch the electrodes, pull apart until it sputters and breaks; a ladder climb; then pushed past sense
    double Eac=1.7; SET(hum,I_HUM(0.05)); SET(memory,I_MEM(0.006)); SET(restrike,I_RESTR(0.8));
    if(t<9.2){
      double g=seg(t,0.8,1.0,0.5,0.015); if(t>1.3) g=seg(t,1.3,5.0,0.015,0.5); if(t>5.0) g=seg(t,5.0,8.8,0.5,1.5); SET(gap,I_GAP(g));
      SET(wind,I_WIND(seg(t,2,8,0.01,0.05))); SET(cool,I_COOL(90e-6));
    } else if(t<18){
      double u=t-9.2; Eac=2.0; SET(gap,I_GAP(0.12)); SET(current,I_CUR(1.3)); SET(choke,I_CHOKE(0.005)); SET(cool,I_COOL(150e-6)); SET(memory,I_MEM(0.012));
      SET(climb,I_CLIMB(seg(u,0,8.8,0.5,1.6))); SET(wind,I_WIND(seg(u,0,8.8,0.012,0.03)));
    } else {
      double u=t-18; SET(gap,I_GAP(0.12)); SET(choke,I_CHOKE(0.005)); SET(memory,I_MEM(0.012));
      SET(climb,I_CLIMB(seg(u,0,8,1.5,7.0))); SET(wind,I_WIND(seg(u,0,8,0.025,0.06))); SET(current,I_CUR(seg(u,0,8,1.3,2.6))); Eac=seg(u,0,8,2.0,3.0); SET(cool,I_COOL(seg(u,0,8,150e-6,50e-6)));
      SET(load,I_LOAD(2.5)); SET(ring,I_RING(9)); SET(tune,I_TUNE(seg(u,0,8,700,2600))); SET(fb,I_FB(seg(u,2,7,0,0.6))); SET(dist,I_DIST(0.0023));
      if(u>8.3) Eac=0;
    }
    src(k,0,Eac);
  }
  if(sc==1){ // singing arc: DC arc + tuned circuit plays a line. Current high = silent arc, lower = sings, starved = tears.
    double bpm=124, st=60.0/bpm/4; int step=(int)(t/st); double u=t-step*st; int notes[16]={33,33,45,33,36,33,48,45,33,33,40,33,43,45,31,36};
    int s=step%16, bar=step/16; double m=notes[s]+12; if(bar>=4 && s%5==4) m+=12; if(bar>=8 && s%7==3) m+=19;
    double b=t/(st*16), Edc=1.6, cur, tf=1.0, Zt=0.4, wind=0.004, iron=0, fb=0, gap=0.25;
    SET(choke,I_CHOKE(0.012)); SET(jitter,I_JIT(0.05)); SET(field,I_FIELD(0.45)); SET(arc,I_ARC(0.8)); SET(memory,I_MEM(0.02));
    cur=seg(b,0,4,1.25,0.95);
    if(b>4){ cur=seg(b,4,8,0.9,0.3); wind=seg(b,4,8,0.004,0.02); }
    if(b>8){ cur=seg(b,8,12,0.6,0.4); tf=seg(b,8,12,1.0,1.8); iron=seg(b,8,12,0.2,2.0); Zt=seg(b,8,12,0.4,0.25); wind=0.012; Edc=seg(b,8,12,1.6,2.0); }
    if(b>12){ cur=seg(b,12,16,0.7,1.1); tf=1.3; iron=0.8; Zt=0.4; fb=seg(b,12,15,0.15,1.3); SET(dist,I_DIST(0.0031)); gap=seg(b,12,16,0.25,0.8); wind=seg(b,12,16,0.01,0.05); Edc=seg(b,12,16,1.6,2.6); }
    k->track=1; SET(cool,I_COOL(tf*0.0165/261.6256)); SET(tune,I_TUNE(261.6256)); k->voct=(m-60)/12.0;
    SET(current,I_CUR(cur)); SET(load,I_LOAD(Zt)); SET(wind,I_WIND(wind)); SET(iron,I_IRON(iron)); SET(fb,I_FB(fb)); SET(gap,I_GAP(gap));
    int acc=(s==0||s==6||s==11); double gl=acc?0.95:0.6; k->keyPatched=1; k->key=(u<st*gl)&&(u>0.0005); SET(ring,I_RING(acc?45:28));
    if(b>=16) Edc=0;
    src(k,Edc,0); k->mains=0;
  }
  if(sc==2||sc==3){ // spark kit: charged capacitors fired through the gap by triggers, 124 bpm. Three instances: kick, snare/roll, hats.
    int err=(sc==3);   // kit2: the erratic version (statistical lag, misfires, ghost re-strikes, chatter)
    double bpm=124, st=60.0/bpm/4; long sps=(long)(st*sr); int step=(int)(n/sps); double u=(double)(n-step*sps)/sr; int s=step%16, bar=step/16;
    int fire=(n==step*sps);
    int snare=(s==4||s==12), kick=(!snare)&&((s==0||s==8)||(bar%2==1&&s==14)||(bar%4==2&&s==7)||(s==10&&bar%2==0));
    int hat=(!kick&&!snare)&&((s%2==1)||(s==2)||(s==10)), roll=(bar%4==3&&s>=13);
    int push=(bar>=8); *ninst=3;
    for(int i=0;i<3;i++){ K*q=&kk[i]; kdef(q); k=q;
      SET(gap,I_GAP(4.0)); SET(current,I_CUR(0.04)); SET(choke,I_CHOKE(0.0002)); SET(heat,I_HEAT(0.04)); SET(cool,I_COOL(200e-6)); SET(memory,I_MEM(0.004));
      SET(jitter,I_JIT(0.1)); SET(wind,I_WIND(0.01)); SET(field,I_FIELD(0.6)); SET(arc,I_ARC(0.8)); SET(restrike,I_RESTR(0.5)); SET(quench,I_QUENCH(0.2));
      src(q,bar>=12?0:2.5,0); q->mains=0; }
    k=&kk[0]; // kick
    k->trig=fire&&kick; SET(tune,I_TUNE(44)); SET(load,I_LOAD(0.6)); SET(ring,I_RING(14)); SET(iron,I_IRON(0.22)); SET(cool,I_COOL(1.2e-3)); SET(heat,I_HEAT(0.0012)); SET(field,I_FIELD(1.3)); SET(arc,I_ARC(6));
    if(err){ SET(jitter,I_JIT(0.3)); SET(iron,I_IRON(0.5)); SET(memory,I_MEM(0.06)); SET(restrike,I_RESTR(0.62)); if(bar<12) src(k,3.0,0); }
    k=&kk[1]; // snare, and the relaxation roll
    k->trig=0; SET(tune,I_TUNE(205)); SET(load,I_LOAD(1.0)); SET(ring,I_RING(5)); SET(iron,I_IRON(0.3)); SET(cool,I_COOL(25e-6)); SET(heat,I_HEAT(0.3));
    SET(current,I_CUR((snare&&u<0.09)?1.0:0.04)); SET(gap,I_GAP(1.2)); src(k,(snare&&bar<12)?2.5:0.6,0);   // the snare is a gate on VOLTS and CURRENT, not a trigger SET(wind,I_WIND(0.07)); SET(climb,I_CLIMB(30)); SET(memory,I_MEM(0.002)); SET(restrike,I_RESTR(0.92)); SET(jitter,I_JIT(0.5)); SET(arc,I_ARC(0.11)); SET(field,I_FIELD(0.5)); SET(quench,I_QUENCH(0.02));
    if(err){ SET(corona,I_CORONA(0.08)); SET(wind,I_WIND(0.11)); }
    k=&kk[2]; // hats (and the roll at the end of every fourth bar)
    k->trig=fire&&hat&&!roll; SET(tune,I_TUNE((s==10)?5200:8200)); SET(load,I_LOAD(5)); SET(ring,I_RING((s==10)?25:7)); SET(cool,I_COOL(15e-6)); SET(heat,I_HEAT((s==10)?0.002:0.01));
    SET(current,I_CUR(0.02)); SET(jitter,I_JIT(0.15)); SET(arc,I_ARC(3)); SET(field,I_FIELD(0.07)); SET(quench,I_QUENCH(0.01));
    if(err){ SET(jitter,I_JIT(0.45)); SET(memory,I_MEM(0.02)); SET(restrike,I_RESTR(0.40)); SET(corona,I_CORONA(0.05)); }
    if(roll){ if(bar<12) src(k,2.6,0); SET(gap,I_GAP(0.5)); SET(tune,I_TUNE(500*(1+0.5*(s-12)))); SET(load,I_LOAD(1.5)); SET(ring,I_RING(12)); SET(current,I_CUR(seg(s+u/st,12,16,0.05,0.5))); SET(cool,I_COOL(30e-6)); SET(heat,I_HEAT(0.03)); SET(restrike,I_RESTR(0.3)); SET(arc,I_ARC(0.8)); SET(field,I_FIELD(0.6)); SET(quench,I_QUENCH(0.2)); SET(choke,I_CHOKE(0.0001)); }
    if(push) for(int i=1;i<3;i++){ k=&kk[i]; SET(wind,I_WIND(M_WIND(k->wind)+0.03)); SET(fb,I_FB(0.5)); SET(dist,I_DIST(0.0017)); SET(iron,I_IRON(M_IRON(k->iron)+0.6)); }
  }
  if(sc==4){ // substation: transformer hum, corona sizzle rising with voltage, flashovers, then a fault
    double Eac=seg(t,0,8,0.5,0.95), cur=1.5, climb=0, wind=0.02, cor=seg(t,1.5,9,0,1.6), fb=0, hum=seg(t,0,3,0.08,0.14);
    SET(gap,I_GAP(1.25)); SET(cool,I_COOL(140e-6)); SET(memory,I_MEM(0.008)); SET(jitter,I_JIT(0.2)); SET(choke,I_CHOKE(0.004)); SET(load,I_LOAD(3)); SET(ring,I_RING(10)); SET(tune,I_TUNE(740)); SET(field,I_FIELD(0.3));
    if(t>10) Eac=seg(t,10,16,0.95,1.22);
    if(t>16){ Eac=seg(t,16,21,1.3,2.6); cur=seg(t,16,21,1.5,3.0); climb=seg(t,16,21,0,3); wind=seg(t,16,21,0.01,0.05); cor=2.2; fb=seg(t,18,21,0,0.6); SET(dist,I_DIST(0.004)); }
    if(t>21.6){ Eac=seg(t,21.6,21.62,2.6,0.45); cor=seg(t,21.6,25,0.4,0); hum=seg(t,22,26,0.14,0); climb=0; }
    SET(current,I_CUR(cur)); SET(climb,I_CLIMB(climb)); SET(wind,I_WIND(wind)); SET(corona,I_CORONA(cor)); SET(fb,I_FB(fb)); SET(hum,I_HUM(hum));
    src(k,0,Eac);
  }
}
