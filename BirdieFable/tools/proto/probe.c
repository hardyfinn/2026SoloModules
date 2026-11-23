#include <stdio.h>
#include <stdlib.h>
#include "presets.h"
// steady HOLD tone: frequency by upward zero crossings, level, and brightness (rms of derivative / (2 pi f rms))
int main(){ static Birdie b; double cases[][3]={{1,-1,0.22},{4,-1,0.22},{16,-1,0.22},{1,6,0.22},{1,-12,0.22},{1,-24,0.3},{1,-36,0.4},{1,-1,0.5},{1,-1,0.08}};
  for(int c=0;c<9;c++){ bd_init(&b,48000); BdParams p=bd_default(); p.size=cases[c][0]; p.tension=cases[c][1]; p.pressure=cases[c][2]; p.hold=1; p.breath=0; p.throat=0; p.tube=0; p.beak=16000;
    int N=48000*3; double prev=0,t0=-1,t1=0; int nc=0; double s2=0,d2=0,pk=0;
    for(int n=0;n<N;n++){ double v=bd_process(&b,&p); if(n>48000){ if(prev<=0&&v>0){ double fr=-prev/(v-prev); double t=(n-1+fr)/48000.0; if(t0<0)t0=t; t1=t; nc++; } s2+=v*v; d2+=(v-prev)*(v-prev)*48000.0*48000.0; if(fabs(v)>pk)pk=fabs(v);} prev=v; }
    double f=(nc-1)/(t1-t0); printf("size %4.0f tension %5.1f pressure %.2f : f0 %8.2f Hz  peak %.2f V  bright %.2f  pitchOut %.3f V\n",p.size,p.tension,p.pressure,f,pk,sqrt(d2/s2)/(2*M_PI*f),b.outPitch); }
  return 0; }
