// Offline check (no Rack): load a folder of plain WAVs, run the firmware
// engine, turn Ectoplasm's dedicated controls, write the result to a WAV.
//   see README.md, "Offline test"
#include "../src/Storage.hpp"
#include <cstdio>
#include <cmath>
#include <chrono>
#include <thread>
using namespace ecto;
static Storage *st; static Bank *bk;
static bool rd(void*,unsigned b,unsigned s,unsigned v,uint64_t pos,void *out,size_t n){return st->read(bk,b,s,v,pos,out,n);}
int main(int argc,char **argv){
    if(argc<3){fprintf(stderr,"usage: render <folder> <out.wav>\n");return 2;}
    Storage storage;st=&storage;storage.load(argv[1],0);
    for(int i=0;i<2000&&!storage.ready.pop(bk);++i)std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if(!bk){fprintf(stderr,"load failed: %s\n",storage.status().c_str());return 1;}
    auto &lib=*bk->library;printf("status: %s, plain=%d\n",storage.status().c_str(),lib.plain);
    for(unsigned b=0;b<16;++b)for(unsigned k=0;k<lib.banks[b].count;++k){auto &c=lib.banks[b].samples[k];
        printf("bank %u slot %u: %s bpm=%u slices=%u ticks=%u size=%u oneshot=%d\n",b+1,c.slot+1,lib.entries[b][c.slot]->name.c_str(),c.bpm,c.slice_count,c.splice_ticks,c.size,c.one_shot);}
    CoreEngine *e=core_engine_create(12345);
    core_engine_set_catalogue(e,lib.banks.data(),rd,nullptr);core_engine_set_resident_bank(e,bk->index);
    CoreState s;core_engine_get_state(e,&s);s.tempo=170;s.stopped=false;
    const unsigned R=44100,secs=12;std::vector<int16_t> out;out.reserve(R*secs*2);
    CoreControls c{};c.knobs[3]=.5f;
    double rms[12]={};
    for(unsigned n=0;n<R*secs;++n){
        unsigned t=n/R;
        if(n==R*3)core_haer_filter(e,150);        // 3 s: low-pass
        if(n==R*5)core_haer_filter(e,512);        // 5 s: filter off
        if(n==R*5)core_haer_drive(e,1000);        // 5 s: fuzz
        if(n==R*7)core_haer_drive(e,500);         // 7 s: back to clean
        if(n==R*7)core_haer_gate(e,900);          // 7 s: gating
        if(n==R*8)core_haer_gate(e,0);
        if(n==R*8)core_haer_pitch(e,900);         // 8 s: pitch up
        if(n==R*9)core_haer_pitch(e,512);
        if(n==R*9)core_haer_play_toggle(e);       // 9 s: stop
        if(n==R*10)core_haer_play_toggle(e);      // 10 s: start
        if(n==R*10)core_haer_tempo(e,120);        // and slow down
        core_engine_controls(e,&c);
        int16_t pcm[2];core_engine_process(e,pcm);out.push_back(pcm[0]);out.push_back(pcm[1]);
        rms[t]+=double(pcm[0])*pcm[0];
    }
    for(unsigned t=0;t<secs;++t)printf("second %2u rms %.4f\n",t,std::sqrt(rms[t]/R)/32768.0);
    CoreDisplay d;core_engine_display(e,&d);printf("tempo now %u, stopped %d\n",d.tempo,d.stopped);
    FILE *f=fopen(argv[2],"wb");uint32_t bytes=out.size()*2;
    auto w32=[&](uint32_t v){fwrite(&v,4,1,f);};auto w16=[&](uint16_t v){fwrite(&v,2,1,f);};
    fwrite("RIFF",1,4,f);w32(36+bytes);fwrite("WAVEfmt ",1,8,f);w32(16);w16(1);w16(2);w32(R);w32(R*4);w16(4);w16(16);fwrite("data",1,4,f);w32(bytes);fwrite(out.data(),2,out.size(),f);fclose(f);
    core_engine_destroy(e);delete bk;return 0;
}
