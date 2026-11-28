// Scene automation for the sketches. All times in seconds.
static void defaults(P*p){
  p->Edc=0; p->Eac=0; p->fm=50; p->cur=1.0; p->choke=0.003; p->gap=1.0;
  p->ft=2500; p->Zt=6; p->Qt=4; p->tau=120e-6; p->P0=0.35; p->turb=0.02; p->climb=0; p->mem=0.03;
  p->jit=0.08; p->corona=0; p->fb=0; p->fbdel=0.002; p->hum=0; p->sat=0.3; p->arcl=1; p->fieldl=0.15; p->fp=17000; p->iron=0; p->trig=0; p->restr=0.85; p->goff=3e-3; p->key=0;
}
static double scene_dur(int sc){ switch(sc){ case 1: return 27; case 2: return 31.2; case 3: return 23.4; case 4: return 26; case 0: return 3; } return 10; }
static double mtof(double m){ return 440.0*pow(2.0,(m-69)/12.0); }

static void scene(int sc,double t,P*p){
  defaults(p);
  if(sc==0){ p->Edc=getenv("EDC")?atof(getenv("EDC")):1.6; p->Eac=getenv("EAC")?atof(getenv("EAC")):0; p->gap=getenv("GAP")?atof(getenv("GAP")):0.25; p->cur=getenv("CUR")?atof(getenv("CUR")):1; p->ft=getenv("FT")?atof(getenv("FT")):220; p->Zt=getenv("ZT")?atof(getenv("ZT")):1; p->Qt=getenv("QT")?atof(getenv("QT")):30; p->tau=getenv("TAU")?atof(getenv("TAU")):40e-6; p->choke=getenv("CHOKE")?atof(getenv("CHOKE")):0.012; p->P0=getenv("P0")?atof(getenv("P0")):0.35; p->turb=getenv("TURB")?atof(getenv("TURB")):0.01; p->mem=getenv("MEM")?atof(getenv("MEM")):0.03; }
  if(sc==1){ // raw arc: touch the electrodes, pull apart until it sputters and breaks; a ladder climb; then pushed past sense
    p->Eac=1.7; p->hum=0.05; p->mem=0.006; p->restr=0.8;
    if(t<9.2){
      p->gap=seg(t,0.8,1.0,0.5,0.015); if(t>1.3) p->gap=seg(t,1.3,5.0,0.015,0.5); if(t>5.0) p->gap=seg(t,5.0,8.8,0.5,1.5);
      p->turb=seg(t,2,8,0.01,0.05); p->cur=1.0; p->tau=90e-6;
    } else if(t<18){
      double u=t-9.2; p->Eac=2.0; p->gap=0.12; p->cur=1.3; p->choke=0.005; p->tau=150e-6; p->mem=0.012;
      p->climb=seg(u,0,8.8,0.5,1.6); p->turb=seg(u,0,8.8,0.012,0.03);
    } else {
      double u=t-18; p->gap=0.12; p->choke=0.005; p->mem=0.012;
      p->climb=seg(u,0,8,1.5,7.0); p->turb=seg(u,0,8,0.025,0.06); p->cur=seg(u,0,8,1.3,2.6); p->Eac=seg(u,0,8,2.0,3.0); p->tau=seg(u,0,8,150e-6,50e-6);
      p->Zt=2.5; p->Qt=9; p->ft=seg(u,0,8,700,2600); p->fb=seg(u,2,7,0,0.6); p->fbdel=0.0023;
      if(u>8.3) p->Eac=0;
    }
  }
  if(sc==2){ // singing arc: DC arc + tuned circuit plays a line. Current high = silent arc, lower = sings, starved = tears.
    double bpm=124, st=60.0/bpm/4; int step=(int)(t/st); double u=t-step*st; int notes[16]={33,33,45,33,36,33,48,45,33,33,40,33,43,45,31,36};
    int s=step%16, bar=step/16; double m=notes[s]+12; if(bar>=4 && s%5==4) m+=12; if(bar>=8 && s%7==3) m+=19;
    double b=t/(st*16);
    p->Edc=1.6; p->gap=0.25; p->choke=0.012; p->turb=0.004; p->jit=0.05;
    p->ft=mtof(m); p->Zt=0.4; p->fieldl=0.45; p->arcl=0.8; p->mem=0.02;
    double tf=1.0;
    p->cur=seg(b,0,4,1.25,0.95);
    if(b>4){ p->cur=seg(b,4,8,0.9,0.3); p->turb=seg(b,4,8,0.004,0.02); }
    if(b>8){ p->cur=seg(b,8,12,0.6,0.4); tf=seg(b,8,12,1.0,1.8); p->iron=seg(b,8,12,0.2,2.0); p->Zt=seg(b,8,12,0.4,0.25); p->turb=0.012; p->Edc=seg(b,8,12,1.6,2.0); }
    if(b>12){ p->cur=seg(b,12,16,0.7,1.1); tf=1.3; p->iron=0.8; p->Zt=0.4; p->fb=seg(b,12,15,0.15,1.3); p->fbdel=0.0031; p->gap=seg(b,12,16,0.25,0.8); p->turb=seg(b,12,16,0.01,0.05); p->Edc=seg(b,12,16,1.6,2.6); }
    p->tau=tf*0.0165/p->ft;
    { static int last=-1; p->key=(step!=last); last=step; }
    int acc=(s==0||s==6||s==11); double gl=acc?0.95:0.6; p->Qt=(u<st*gl)?(acc?45:28):1.2;
    if(b>=16) p->Edc=0;
  }
  if(sc==3){ // spark kit: a charged capacitor fired through the gap by triggers, 124 bpm. One model, its knobs sequenced per step.
    static int last=-1; double bpm=124, st=60.0/bpm/4; int step=(int)(t/st); double u=t-step*st; int s=step%16, bar=step/16;
    int snare=(s==4||s==12), kick=(!snare)&&((s==0||s==8)||(bar%2==1&&s==14)||(bar%4==2&&s==7)||(s==10&&bar%2==0)), hat=(s%2==1)||(s==2)||(s==10), roll=(bar%4==3&&s>=12);
    int fire=(step!=last); last=step;
    p->Edc=2.5; p->gap=4.0; p->cur=0.04; p->choke=0.0002; p->P0=0.04; p->tau=200e-6; p->mem=0.004; p->jit=0.1; p->turb=0.01; p->fieldl=0.6; p->arcl=0.8; p->restr=0.5; p->goff=0.2;
    if(bar>=12){ p->Edc=0; return; }
    if(kick){ p->trig=fire; p->ft=44; p->Zt=0.6; p->Qt=14; p->iron=0.22; p->tau=1.2e-3; p->P0=0.0012; p->fieldl=1.3; p->arcl=6; }
    else if(snare){ p->trig=fire; p->ft=205; p->Zt=1.0; p->Qt=5; p->iron=0.3; p->tau=25e-6; p->P0=0.3; p->cur=(u<0.09)?1.0:0.04; p->gap=1.2; p->turb=0.07; p->climb=30; p->mem=0.002; p->restr=0.92; p->arcl=0.11; p->fieldl=0.5; p->goff=0.02; }
    else if(roll){ p->trig=0; p->Edc=2.6; p->gap=0.5; p->ft=500*(1+0.5*(s-12)); p->Zt=1.5; p->Qt=12; p->cur=seg(s+u/st,12,16,0.05,0.5); p->tau=30e-6; p->P0=0.03; p->restr=0.3; }
    else if(hat){ p->trig=fire; p->ft=(s==10)?5200:8200; p->Zt=5; p->Qt=(s==10)?25:7; p->tau=15e-6; p->P0=(s==10)?0.01:0.05; p->cur=0.02; p->jit=0.15; p->arcl=3; p->fieldl=0.07; p->goff=0.01; p->P0*=0.2; }
    else { p->Qt=1.5; }
    if(bar>=8 && !kick){ p->turb+=0.03; p->fb=0.5; p->fbdel=0.0017; p->iron+=0.6; } // pushed last bars
  }
  if(sc==4){ // substation: transformer hum, corona sizzle rising with voltage, flashovers, then a fault
    p->Eac=seg(t,0,8,0.5,0.95); p->hum=seg(t,0,3,0.08,0.14); p->sat=seg(t,0,16,0.2,0.8); p->gap=1.25; p->corona=seg(t,1.5,9,0,1.6);
    p->cur=1.5; p->tau=140e-6; p->mem=0.008; p->jit=0.2; p->choke=0.004; p->Zt=3; p->Qt=10; p->ft=740; p->fieldl=0.3;
    if(t>10) p->Eac=seg(t,10,16,0.95,1.22);   // flashovers begin as the peaks reach the gap
    if(t>16){ p->Eac=seg(t,16,21,1.3,2.6); p->cur=seg(t,16,21,1.5,3.0); p->climb=seg(t,16,21,0,3); p->turb=seg(t,16,21,0.01,0.05); p->corona=2.2; p->fb=seg(t,18,21,0,0.6); p->fbdel=0.004; }
    if(t>21.6){ p->Eac=seg(t,21.6,21.62,2.6,0.45); p->corona=seg(t,21.6,25,0.4,0); p->hum=seg(t,22,26,0.14,0); p->climb=0; }
  }
}
