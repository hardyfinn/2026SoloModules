// Renders every factory preset from the reference engine: three phrases (seeds s, s+1, s+2) then one single note.
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "presets.h"
static void wav(const char* fn,const float* x,int N,int sr){ FILE* f=fopen(fn,"wb"); int32_t i32; int16_t i16; int ds=N*2;
  fwrite("RIFF",1,4,f); i32=36+ds; fwrite(&i32,4,1,f); fwrite("WAVEfmt ",1,8,f); i32=16; fwrite(&i32,4,1,f); i16=1; fwrite(&i16,2,1,f); fwrite(&i16,2,1,f);
  i32=sr; fwrite(&i32,4,1,f); i32=sr*2; fwrite(&i32,4,1,f); i16=2; fwrite(&i16,2,1,f); i16=16; fwrite(&i16,2,1,f); fwrite("data",1,4,f); fwrite(&ds,4,1,f);
  for(int n=0;n<N;n++){ double v=x[n]/10.0; if(v>1)v=1; if(v<-1)v=-1; i16=(int16_t)(v*32767); fwrite(&i16,2,1,f);} fclose(f); }
int main(int argc,char** argv){ const char* dir=argc>1?argv[1]:"."; int sr=argc>2?atoi(argv[2]):48000;
  static BdPreset pre[32]; int n=bd_presets(pre); static Birdie b; int cap=sr*60; float* buf=malloc(cap*sizeof(float));
  for(int k=0;k<n;k++){ bd_init(&b,sr); BdParams p=pre[k].p; int N=0, wait=(int)(0.2*sr), stage=0; double pk=0, rms=0; int held=p.hold;
    while(N<cap){
      if(!b.phOn && !b.noteOn){
        if(wait>0) wait--;
        else { if(stage<3){ if(held) bd_note(&b,&p); else bd_phrase(&b,&p,pre[k].seed+stage); }
               else if(stage==3) bd_note(&b,&p);
               else if(stage==4) p.hold=0;
               else break;
               stage++; wait=(int)((held?1.5:0.45)*sr); }
      }
      double v=bd_process(&b,&p); buf[N++]=(float)v; if(fabs(v)>pk)pk=fabs(v); rms+=v*v;
    }
    char fn[512], nm[64]; int j=0; for(const char* c=pre[k].name;*c;c++) nm[j++]=(*c==' ')?'-':tolower(*c); nm[j]=0;
    snprintf(fn,512,"%s/%02d-%s.wav",dir,k+1,nm); wav(fn,buf,N,sr);
    printf("%-10s %5.1fs peak %5.2f V rms %5.2f V\n",pre[k].name,N/(double)sr,pk,sqrt(rms/N));
  } return 0; }
