// Copyright 2026 Zack Scholl. SPDX-License-Identifier: GPL-3.0-only
// Modified for Ectoplasm by Haer, 2026-11-20: plain WAV folders (see "Plain" below).
#include "Storage.hpp"
#include "../engine/lib/start_tempo.h"
#include "../engine/lib/startup_playback.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <cmath>
#include <regex>

namespace ecto {
static void parseSettings(const Library&,CoreState&);
static void require(bool b,const std::string &s){if(!b)throw std::runtime_error(s);}
static uint16_t u16(const uint8_t *p){return p[0]|uint16_t(p[1])<<8;}
static uint32_t u32(const uint8_t *p){return u16(p)|uint32_t(u16(p+2))<<16;}
static std::vector<uint8_t> bytes(const fs::path &p,size_t limit){
    auto n=fs::file_size(p);require(n<=limit,"File exceeds supported size: "+p.string());
    std::vector<uint8_t> result(size_t(n),0);std::ifstream in(p,std::ios::binary);
    require(bool(in.read(reinterpret_cast<char*>(result.data()),std::streamsize(n))),"Cannot read "+p.string());return result;
}
fs::path Storage::path(const Library &l,unsigned b,unsigned s,unsigned v){return l.root/("bank"+std::to_string(b+1))/(std::to_string(s)+"."+std::to_string(v)+".wav");}
Wav Storage::inspect(const fs::path &p){
    std::ifstream in(p,std::ios::binary);uint8_t h[16]{};auto size=fs::file_size(p);
    require(bool(in.read((char*)h,12))&&!memcmp(h,"RIFF",4)&&!memcmp(h+8,"WAVE",4),"Expected RIFF WAV");
    require(uint64_t(u32(h+4))+8==size,"Truncated or extended WAV");Wav w;bool fmt=false,data=false;
    while(uint64_t(in.tellg())+8<=size){
        require(bool(in.read((char*)h,8)),"Truncated chunk");uint32_t n=u32(h+4);auto at=uint64_t(in.tellg());
        require(n<=size-at,"WAV chunk exceeds file");
        if(!memcmp(h,"fmt ",4)){
            require(!fmt&&n>=16&&bool(in.read((char*)h,16)),"Invalid WAV format");fmt=true;
            w.channels=u16(h+2);w.rate=u32(h+4);
            require(u16(h)==1&&u16(h+14)==16&&(w.channels==1||w.channels==2)&&(w.rate==44100||w.rate==88200)&&u16(h+12)==w.channels*2&&u32(h+8)==w.rate*w.channels*2,"Unsupported card PCM");
        }else if(!memcmp(h,"data",4)){require(!data,"Duplicate WAV data");data=true;w.offset=at;w.bytes=n;}
        in.seekg(std::streamoff(at+n+(n&1)));require(bool(in),"Cannot seek WAV");
    }
    require(fmt&&data&&w.bytes%(w.channels*2)==0,"Missing or unaligned PCM");return w;
}
std::shared_ptr<Library> Storage::catalogue(const fs::path &root,uint64_t generation){
    require(fs::is_directory(root),"Sample folder is missing");auto l=std::make_shared<Library>();l->root=fs::absolute(root);l->generation=generation;
    unsigned count=0;
    for(unsigned b=0;b<16;++b)for(unsigned s=0;s<16;++s){
        auto p=path(*l,b,s,0),info=p;info+=".info";
        if(!fs::exists(p)&&!fs::exists(info))continue;
        try{
            auto e=std::make_unique<Entry>();auto data=bytes(info,8192);char error[128];
            require(core_card_decode(data.data(),data.size(),&e->info,error,sizeof error),error);
            e->info.sample.slot=s;e->wav=inspect(p);auto &c=e->info.sample;
            require(e->wav.channels==c.channels&&e->wav.rate==44100u*c.rate_multiple&&e->wav.bytes==uint64_t(c.size)+uint64_t(e->wav.rate)*c.channels*2,"WAV and metadata disagree about padding or format");
            l->banks[b].samples[l->banks[b].count++]=e->info.sample;l->entries[b][s]=std::move(e);++count;
        }catch(const std::exception &e){l->warnings.push_back("Bank "+std::to_string(b+1)+", sample "+std::to_string(s+1)+": "+e.what());}
    }
    if(count==0){
        // Ectoplasm: no sample-manager export here, so read it as plain WAVs.
        l->warnings.clear();cataloguePlain(*l);
        for(auto &b:l->banks)count+=b.count;
        require(count>0,"No WAV files found in this folder");
        parseSettings(*l,l->preparedSettings);return l;
    }
    auto settings=l->root/"settings";
    if(fs::is_directory(settings))for(auto &f:fs::recursive_directory_iterator(settings))if(f.is_regular_file()){
        auto key=fs::relative(f.path(),settings).generic_string();auto dash=key.rfind('-');
        if(dash!=std::string::npos){auto base=key.substr(0,dash),value=key.substr(dash+1);auto it=l->settings.find(base);
            if(it!=l->settings.end()&&it->second!=value){l->warnings.push_back("Conflicting setting: "+base);it->second="";}else l->settings[base]=value;}
    }
    auto mapping=settings/"sample_cv_mapping";if(!fs::exists(mapping))mapping=l->root/"sample_cv_mapping";
    if(fs::exists(mapping)){auto d=bytes(mapping,16);std::string s(d.begin(),d.end());while(!s.empty()&&isspace(static_cast<unsigned char>(s.back())))s.pop_back();l->settings["sample_cv_mapping"]=s=="1voct"?"1voct":"bank";}
    auto tempo=settings/"start_tempo";if(!fs::exists(tempo))tempo=l->root/"start_tempo";
    if(fs::exists(tempo)){
        uint16_t bpm=0;
        try{auto d=bytes(tempo,16);require(start_tempo_parse(reinterpret_cast<const char*>(d.data()),d.size(),&bpm),"Invalid start tempo");}
        catch(const std::exception&){bpm=0;l->warnings.push_back("Invalid start tempo; using Default (no override).");}
        l->settings["start_tempo"]=bpm?std::to_string(bpm):"default";
    }
    auto startup=settings/"startup_playback";if(!fs::exists(startup))startup=l->root/"startup_playback";
    if(fs::exists(startup)){
        bool wait=false;
        try{auto d=bytes(startup,16);require(startup_playback_parse(reinterpret_cast<const char*>(d.data()),d.size(),&wait),"Invalid startup playback");}
        catch(const std::exception&){wait=false;l->warnings.push_back("Invalid startup playback; using Play immediately.");}
        l->settings["startup_playback"]=wait?"wait":"play";
    }
    for(unsigned r=0;r<7;++r)l->runePresent[r]=fs::is_directory(settings/"grimoire"/("rune"+std::to_string(r+1)));
    parseSettings(*l,l->preparedSettings);return l;
}
std::unique_ptr<Bank> Storage::prepare(std::shared_ptr<Library> l,unsigned index){
    if(index>=16||!l->banks[index].count){index=0;while(index<16&&!l->banks[index].count)++index;}
    require(index<16,"Empty library");auto result=std::make_unique<Bank>();result->library=std::move(l);result->index=index;
    for(unsigned s=0;s<16;++s)if(auto &e=result->library->entries[index][s]){
        if(e->plain){result->primary[s]=decodePlain(e->src);result->bytes+=result->primary[s].size();continue;}
        auto &v=result->primary[s];v.resize(size_t(e->wav.bytes));std::ifstream in(path(*result->library,index,s,0),std::ios::binary);in.seekg(std::streamoff(e->wav.offset));
        require(bool(in.read((char*)v.data(),std::streamsize(v.size()))),"Primary WAV changed while loading");result->bytes+=v.size();
    }
    return result;
}
Storage::Storage(){worker=std::thread([this]{run();});}
Storage::~Storage(){stop=true;worker.join();Bank *p;while(ready.pop(p))delete p;while(retired.pop(p))delete p;}
void Storage::load(const std::string &root,unsigned bank,bool importSettings){std::lock_guard<std::mutex> lock(mutex);pendingRoot=root;pendingBank=bank;pendingImport=importSettings;++revision;message="Loading sample folder…";}
std::string Storage::status(){std::lock_guard<std::mutex> lock(mutex);return message;}
std::string Storage::sampleName(unsigned b,unsigned s){
    std::lock_guard<std::mutex> lock(mutex);
    if(!current||b>=16||s>=16||!current->entries[b][s])return "";
    return current->entries[b][s]->name;
}
bool Storage::read(Bank *b,unsigned bank,unsigned slot,unsigned variant,uint64_t offset,void *out,size_t n){
    if(!b || bank!=b->index || slot>=16 || variant!=0 ||
       !b->library->entries[bank][slot] || offset<44)return false;
    offset-=44;auto &data=b->primary[slot];
    if(offset>data.size() || n>data.size()-offset)return false;
    memcpy(out,data.data()+offset,n);return true;
}
void Storage::run(){
    uint64_t seen=0,generation=0;std::map<uint64_t,std::weak_ptr<Library>> libraries;
    std::shared_ptr<Library> latest;
    auto publish=[&](std::unique_ptr<Bank> bank){
        Bank *raw=bank.release();while(!stop&&!ready.push(raw))std::this_thread::sleep_for(std::chrono::milliseconds(1));if(stop)delete raw;
    };
    while(!stop){
        Bank *dead;while(retired.pop(dead))delete dead;
        std::string root;unsigned bank=0;bool import=false;uint64_t rev;
        {std::lock_guard<std::mutex> lock(mutex);rev=revision;if(rev!=seen){root=pendingRoot;bank=pendingBank;import=pendingImport;}}
        try{
            if(rev!=seen){seen=rev;auto l=catalogue(fs::u8path(root),++generation);auto prepared=prepare(l,bank);l->importSettings=import;
                {std::lock_guard<std::mutex> lock(mutex);if(revision!=rev)continue;}
                libraries[l->generation]=l;latest=l;publish(std::move(prepared));
                std::lock_guard<std::mutex> lock(mutex);current=l;message=l->warnings.empty()?(l->plain?"Ready (plain WAVs)":"Ready"):("Ready — "+l->warnings.front());}
            int requested=requestedBank.exchange(-1);auto it=libraries.find(activeGeneration.load());
            if(requested>=0&&it!=libraries.end())if(auto l=it->second.lock())publish(prepare(l,unsigned(requested)));
        }catch(const std::exception &e){std::lock_guard<std::mutex> lock(mutex);message=e.what();}
        for(auto it=libraries.begin();it!=libraries.end();)if(it->second.expired())it=libraries.erase(it);else ++it;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
static void parseSettings(const Library &l,CoreState &s){
    auto get=[&](const char *key,const char *fallback){auto i=l.settings.find(key);return i==l.settings.end()||i->second.empty()?std::string(fallback):i->second;};
    auto tempo=get("start_tempo","default");start_tempo_parse(tempo.data(),tempo.size(),&s.start_tempo);
    s.startup_wait=get("startup_playback","play")=="wait";
    s.clock_stop=get("clock_stop_sync","off")=="on";s.clock_trigger=get("clock_output_trig","off")=="on";s.clock_slice=get("clock_behavior_sync_slice","off")=="on";
    s.bipolar[0]=get("amen_cv","bipolar")=="bipolar";s.bipolar[1]=get("break_cv","bipolar")=="bipolar";s.bipolar[2]=get("sample_cv","bipolar")=="bipolar";
    auto amen=get("amen_behavior","jump");s.amen_behavior=amen=="repeat"?1:amen=="split"?2:0;
    s.sample_mapping=get("sample_cv_mapping","bank")=="1voct";auto reset=get("override_with_reset","none");s.reset_input=reset=="amen"?0:reset=="break"?1:reset=="sample"?2:reset=="clk"?3:-1;
    try{s.brightness=uint8_t(std::clamp(std::stoi(get("brightness","50")),0,100));}catch(...){s.brightness=50;}
    for(unsigned r=0;r<7;++r)for(unsigned fx=0;fx<16;++fx){auto key="grimoire/rune"+std::to_string(r+1)+"/effect"+std::to_string(fx+1);auto it=l.settings.find(key);if(it!=l.settings.end()&&!it->second.empty())s.runes[r][fx]=it->second=="on";}
}
void applySettings(const Library &l,CoreState &s){
    const auto &v=l.preparedSettings;
    s.start_tempo=v.start_tempo;
    s.startup_wait=v.startup_wait;
    s.clock_stop=v.clock_stop;s.clock_trigger=v.clock_trigger;s.clock_slice=v.clock_slice;
    memcpy(s.bipolar,v.bipolar,sizeof s.bipolar);s.amen_behavior=v.amen_behavior;s.sample_mapping=v.sample_mapping;
    s.reset_input=v.reset_input;s.brightness=v.brightness;
    for(unsigned r=0;r<7;++r)if(l.runePresent[r])memcpy(s.runes[r],v.runes[r],sizeof s.runes[r]);
}
}
// ---------------------------------------------------------------------------
// Ectoplasm (Haer, 2026): plain WAV folders.
// A folder of WAVs fills bank 1, then bank 2, ... (16 per bank). A folder of
// sub-folders makes each sub-folder one bank. Each file becomes a sample with
// 16 equal slices; BPM comes from the file name ("amen 172bpm.wav",
// "break_bpm140.wav") or, failing that, from the loop length. Files shorter
// than one second with no BPM in the name play as one-shots. Audio is decoded
// to the card format in memory: 44.1 kHz 16-bit, half a second of circular
// padding at each end. Nothing is written to the folder.
namespace ecto {
static bool isWav(const fs::path &p){auto e=p.extension().string();for(auto &c:e)c=char(std::tolower((unsigned char)c));return e==".wav"||e==".wave";}
static bool naturalLess(const std::string &a,const std::string &b){
    size_t i=0,j=0;
    while(i<a.size()&&j<b.size()){
        if(std::isdigit((unsigned char)a[i])&&std::isdigit((unsigned char)b[j])){
            size_t i2=i,j2=j;while(i2<a.size()&&std::isdigit((unsigned char)a[i2]))++i2;while(j2<b.size()&&std::isdigit((unsigned char)b[j2]))++j2;
            auto x=a.substr(i,i2-i),y=b.substr(j,j2-j);while(x.size()>1&&x[0]=='0')x.erase(0,1);while(y.size()>1&&y[0]=='0')y.erase(0,1);
            if(x.size()!=y.size())return x.size()<y.size();if(x!=y)return x<y;i=i2;j=j2;continue;
        }
        char x=char(std::tolower((unsigned char)a[i])),y=char(std::tolower((unsigned char)b[j]));
        if(x!=y)return x<y;++i;++j;
    }
    return a.size()-i<b.size()-j;
}
static std::vector<fs::path> sortedWavs(const fs::path &dir){
    std::vector<fs::path> out;
    for(auto &f:fs::directory_iterator(dir))if(f.is_regular_file()&&isWav(f.path())&&f.path().filename().string()[0]!='.')out.push_back(f.path());
    std::sort(out.begin(),out.end(),[](const fs::path &a,const fs::path &b){return naturalLess(a.filename().string(),b.filename().string());});
    return out;
}
static double bpmFromName(const std::string &name){
    std::string s=name;for(auto &c:s)c=char(std::tolower((unsigned char)c));
    std::smatch m;
    static const std::regex after(R"((^|[^a-z0-9.])([0-9]{2,3}(\.[0-9]+)?)[ _:-]*bpm)"),before(R"(bpm[ _:=\-]*([0-9]{2,3}(\.[0-9]+)?))");
    double bpm=0;
    if(std::regex_search(s,m,after))bpm=std::atof(m[2].str().c_str());
    else if(std::regex_search(s,m,before))bpm=std::atof(m[1].str().c_str());
    return bpm>=30&&bpm<=300?bpm:0;
}
Plain Storage::inspectPlain(const fs::path &p){
    std::ifstream in(p,std::ios::binary);uint8_t h[40]{};auto size=fs::file_size(p);
    require(bool(in.read((char*)h,12))&&!memcmp(h,"RIFF",4)&&!memcmp(h+8,"WAVE",4),"Not a WAV file");
    Plain w;w.source=p;bool fmt=false,data=false;
    while(uint64_t(in.tellg())+8<=size){
        if(!in.read((char*)h,8))break;uint64_t n=u32(h+4);auto at=uint64_t(in.tellg());
        if(n>size-at)n=size-at; // tolerate a slightly short last chunk
        if(!memcmp(h,"fmt ",4)){
            require(n>=16&&bool(in.read((char*)h,std::min<uint64_t>(n,40))),"Invalid WAV format");fmt=true;
            w.format=u16(h);w.channels=u16(h+2);w.rate=u32(h+4);w.bits=u16(h+14);
            if(w.format==0xFFFE&&n>=26)w.format=u16(h+24); // extensible: sub-format
        }else if(!memcmp(h,"data",4)){data=true;w.offset=at;w.bytes=n;}
        in.clear();in.seekg(std::streamoff(at+n+(n&1)));if(!in)break;
    }
    require(fmt&&data,"WAV has no audio");
    require((w.format==1&&(w.bits==8||w.bits==16||w.bits==24||w.bits==32))||(w.format==3&&(w.bits==32||w.bits==64)),"Unsupported WAV encoding (use PCM or float)");
    require(w.channels>=1&&w.channels<=16&&w.rate>=8000&&w.rate<=384000,"Unsupported WAV channels or rate");
    w.frames=w.bytes/(w.channels*(w.bits/8));require(w.frames>16,"WAV is empty");
    w.outChannels=w.channels>=2?2:1;
    w.outFrames=w.rate==44100?w.frames:uint64_t(std::llround(double(w.frames)*44100.0/w.rate));
    require(w.outFrames*w.outChannels*2<=uint64_t(INT32_MAX)-44100*4,"WAV is too long");
    return w;
}
std::vector<uint8_t> Storage::decodePlain(const Plain &w){
    // Decode to float, keeping (at most) the first two channels.
    std::vector<uint8_t> raw(size_t(w.bytes));
    {std::ifstream in(w.source,std::ios::binary);in.seekg(std::streamoff(w.offset));require(bool(in.read((char*)raw.data(),std::streamsize(raw.size()))),"WAV changed while loading");}
    const unsigned C=w.outChannels,B=w.bits/8,stride=w.channels*B;
    std::vector<float> src(size_t(w.frames)*C);
    for(uint64_t f=0;f<w.frames;++f)for(unsigned c=0;c<C;++c){
        const uint8_t *q=raw.data()+f*stride+c*B;float v=0;
        if(w.format==3){if(B==4){float x;memcpy(&x,q,4);v=x;}else{double x;memcpy(&x,q,8);v=float(x);}}
        else if(B==1)v=(int(q[0])-128)/128.f;
        else if(B==2)v=int16_t(u16(q))/32768.f;
        else if(B==3)v=float(int32_t(uint32_t(q[0])<<8|uint32_t(q[1])<<16|uint32_t(q[2])<<24)>>8)/8388608.f;
        else v=float(int32_t(u32(q)))/2147483648.f;
        src[size_t(f)*C+c]=std::isfinite(v)?v:0.f;
    }
    raw.clear();raw.shrink_to_fit();
    // Resample to 44.1 kHz (4-point Hermite), exactly outFrames frames.
    const uint64_t N=w.outFrames;std::vector<int16_t> pcm(size_t(N)*C);
    const double step=double(w.rate)/44100.0;
    auto at=[&](int64_t i,unsigned c){if(i<0)i=0;if(i>=int64_t(w.frames))i=int64_t(w.frames)-1;return src[size_t(i)*C+c];};
    for(uint64_t n=0;n<N;++n){
        double pos=w.rate==44100?double(n):n*step;int64_t i=int64_t(pos);float t=float(pos-i);
        for(unsigned c=0;c<C;++c){
            float y0=at(i-1,c),y1=at(i,c),y2=at(i+1,c),y3=at(i+2,c);
            float c1=.5f*(y2-y0),c2=y0-2.5f*y1+2.f*y2-.5f*y3,c3=.5f*(y3-y0)+1.5f*(y1-y2);
            float v=((c3*t+c2)*t+c1)*t+y1;v=std::max(-1.f,std::min(1.f,v));
            pcm[size_t(n)*C+c]=int16_t(std::lrint(v*32767.f));
        }
    }
    // Card layout: last half second, the audio, first half second (circular).
    const size_t pad=size_t(22050)*C,len=pcm.size();
    std::vector<uint8_t> out((len+2*pad)*2);
    auto put=[&](size_t k,int16_t v){out[2*k]=uint8_t(v&255);out[2*k+1]=uint8_t((uint16_t(v)>>8)&255);};
    for(size_t k=0;k<pad;++k)put(k,pcm[(len+len*((pad/len)+1)-pad+k)%len]);
    for(size_t k=0;k<len;++k)put(pad+k,pcm[k]);
    for(size_t k=0;k<pad;++k)put(pad+len+k,pcm[k%len]);
    return out;
}
void Storage::cataloguePlain(Library &l){
    l.plain=true;
    std::vector<std::vector<fs::path>> banks;
    std::vector<fs::path> dirs;
    for(auto &f:fs::directory_iterator(l.root))if(f.is_directory()&&f.path().filename().string()[0]!='.')dirs.push_back(f.path());
    std::sort(dirs.begin(),dirs.end(),[](const fs::path &a,const fs::path &b){return naturalLess(a.filename().string(),b.filename().string());});
    for(auto &d:dirs){auto w=sortedWavs(d);if(!w.empty()&&banks.size()<16){if(w.size()>16){l.warnings.push_back(d.filename().string()+": only the first 16 WAVs are used");w.resize(16);}banks.push_back(w);}}
    auto top=sortedWavs(l.root);
    for(size_t i=0;i<top.size()&&banks.size()<16;i+=16)banks.push_back(std::vector<fs::path>(top.begin()+i,top.begin()+std::min(top.size(),i+16)));
    for(unsigned b=0;b<banks.size()&&b<16;++b)for(unsigned s=0;s<banks[b].size()&&s<16;++s){
        const auto &path=banks[b][s];
        try{
            auto e=std::make_unique<Entry>();e->plain=true;e->src=inspectPlain(path);e->name=path.stem().string();
            auto &c=e->info.sample;const auto &w=e->src;
            c.slot=uint8_t(s);c.channels=uint8_t(w.outChannels);c.rate_multiple=1;c.size=uint32_t(w.outFrames*w.outChannels*2);
            double seconds=double(w.outFrames)/44100.0,bpm=bpmFromName(path.filename().string()),beats=0;
            bool oneShot=bpm==0&&seconds<1.0;
            if(oneShot){bpm=120;beats=1;c.one_shot=true;c.tempo_match=false;c.mode=1;}
            else{
                if(bpm==0){
                    // Loop length: the power-of-two beat count whose tempo is nearest 130 BPM.
                    double best=1e9;
                    for(double n:{1.,2.,4.,8.,16.,32.,64.,128.,256.}){double t=n*60.0/seconds;if(t>=70&&t<=200&&std::abs(t-130)<best){best=std::abs(t-130);bpm=t;}}
                    if(bpm==0)bpm=120;
                }
                beats=std::max(1.0,std::round(seconds/(60.0/bpm)));
                c.one_shot=false;c.tempo_match=true;c.mode=0;
            }
            c.bpm=uint16_t(std::lround(std::max(30.0,std::min(300.0,bpm))));
            unsigned slices=oneShot?1:16;
            c.slice_count=uint8_t(slices);c.variable=false;
            double ticks=std::trunc(std::round(192.0*beats/slices*4.0)/4.0);
            c.splice_ticks=uint16_t(std::max(2.0,std::min(32767.0,ticks)));
            const uint32_t frameBytes=w.outChannels*2;
            for(unsigned i=0;i<slices;++i){
                e->info.starts[i]=int32_t(uint64_t(c.size)*i/slices/frameBytes*frameBytes);
                e->info.stops[i]=i+1==slices?int32_t(c.size):int32_t(uint64_t(c.size)*(i+1)/slices/frameBytes*frameBytes);
                e->info.types[i]=0;
            }
            c.starts=e->info.starts;c.stops=e->info.stops;c.types=e->info.types;
            for(unsigned lane=0;lane<3;++lane){c.transient_count[lane]=0;c.transients[lane]=e->info.transients[lane];}
            l.banks[b].samples[l.banks[b].count++]=c;l.entries[b][s]=std::move(e);
        }catch(const std::exception &ex){l.warnings.push_back(path.filename().string()+": "+ex.what());}
    }
    // Entries were moved: re-point the catalogue at their stable storage.
    for(unsigned b=0;b<16;++b)for(unsigned k=0;k<l.banks[b].count;++k){
        auto &c=l.banks[b].samples[k];auto &e=l.entries[b][c.slot];
        c.starts=e->info.starts;c.stops=e->info.stops;c.types=e->info.types;for(unsigned lane=0;lane<3;++lane)c.transients[lane]=e->info.transients[lane];
    }
}
}
