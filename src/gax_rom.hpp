#pragma once
#include "audio.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace crash {

inline bool gaxPtr(const Bytes& rom,uint32_t p,size_t n=1){return p>=0x08000000u&&size_t(p-0x08000000u)<=rom.size()&&n<=rom.size()-size_t(p-0x08000000u);}
inline size_t gaxOff(const Bytes& rom,uint32_t p,size_t n=1){if(!gaxPtr(rom,p,n))throw std::runtime_error("GAX ROM pointer bounds");return size_t(p-0x08000000u);}
inline uint16_t gaxU16(const Bytes& r,uint32_t p){auto o=gaxOff(r,p,2);return uint16_t(r[o]|(uint16_t(r[o+1])<<8));}
inline int16_t gaxS16(const Bytes& r,uint32_t p){return int16_t(gaxU16(r,p));}
inline uint32_t gaxU32(const Bytes& r,uint32_t p){return u32(r,gaxOff(r,p,4));}
inline int32_t gaxS32(const Bytes& r,uint32_t p){return int32_t(gaxU32(r,p));}

struct GaxOrderEntry { uint16_t patternOffset=0; int8_t transpose=0; };
struct GaxDspTapData { unsigned delayMs=0,gainQ4=0; };
struct GaxRomSong {
    unsigned id=0,channels=0,patternRows=0,orderCount=0,loopOrder=0,volume=0,mixRate=0,numSfx=0;
    unsigned dspPrimaryChannels=0;
    std::array<GaxDspTapData,3> dspTaps{};
    uint32_t songPtr=0,mixerTypePtr=0,mixerDspPtr=0,infoTypePtr=0,songDataPtr=0,patternsPtr=0,instrumentTablePtr=0,waveTablePtr=0;
    std::string title;
    std::vector<std::vector<GaxOrderEntry>> orders;
};
struct GaxWaveInfo { uint32_t ptr=0,length=0; };
struct GaxSfxEntry { unsigned slotId=0; int32_t priority=0; unsigned baseVolume=0; };

struct GaxEnvelopePointData { uint16_t pos=0; int16_t slope=0; uint8_t value=0; };
struct GaxEnvelopeData {
    uint8_t sustain=0xff,loopStart=0xff,loopEnd=0xff;
    std::vector<GaxEnvelopePointData> points;
    bool valid=false;
};
struct GaxInstrumentRowData {
    bool sweepEnabled=false,pingPong=false;
    int32_t start=0,sweepMin=0,sweepMax=0,sweepLen=0,sweepStep=0;
    uint16_t sweepRate=0;
    int16_t tune=0;
};
struct GaxInstrumentSeqData {
    uint8_t note=0,fixedPitch=0,wave=0;
    std::array<uint16_t,2> fx{};
};
struct GaxInstrumentData {
    uint32_t ptr=0;
    std::array<uint8_t,4> waveIdx{};
    uint8_t vibratoDelay=0,vibratoDepth=0,vibratoSpeed=0,seqSpeed=0;
    std::array<GaxInstrumentRowData,4> rows{};
    GaxEnvelopeData envelope;
    std::vector<GaxInstrumentSeqData> seq;
    bool valid=false;
};

/* Compatibility summary used by old tools/tests. The v0.22 runtime below
 * uses GaxInstrumentData instead, preserving every sequence/envelope row. */
struct GaxInstrumentInfo {
    uint32_t ptr=0; std::array<uint8_t,4> waveIdx{}; unsigned row=0; int tune=0; int start=0,loopStart=0,loopEnd=0;
    unsigned seqNote=0; bool fixedPitch=false; bool valid=false;
};

inline const std::array<const char*,19>& gaxSongNames(){
    static const std::array<const char*,19> n{{"jungle","underwater","arctic","sewers","future","rocket crash","bonus round","dingodile","n gin","tiny","neo cortex","main menu europe","main menu japan","cutscenes","cutscenes spooky","intro","warp room","credits","drums"}};return n;
}
inline const std::array<unsigned,19>& gaxSongChannels(){
    static const std::array<unsigned,19> n{{4,5,4,5,5,4,6,5,5,5,5,7,1,6,7,6,7,7,3}};return n;
}

inline bool gaxSongLooksValid(const Bytes& rom,uint32_t songPtr,unsigned expectedChannels){
    if(!gaxPtr(rom,songPtr,16+(expectedChannels*4)))return false;
    const unsigned items=gaxU32(rom,songPtr);if(items!=expectedChannels+3)return false;
    const uint32_t info=gaxU32(rom,songPtr+8);if(!gaxPtr(rom,info,28))return false;
    const uint32_t songData=gaxU32(rom,info+24);if(!gaxPtr(rom,songData,28))return false;
    const unsigned rows=gaxU16(rom,songData+2),orders=gaxU16(rom,songData+4),rate=gaxU16(rom,songData+0x18);
    if(rows<1||rows>64||orders<1||orders>128||rate<5000||rate>50000)return false;
    if(!gaxPtr(rom,gaxU32(rom,songData+0x0c))||!gaxPtr(rom,gaxU32(rom,songData+0x10))||!gaxPtr(rom,gaxU32(rom,songData+0x14),8))return false;
    for(unsigned c=0;c<expectedChannels;c++){uint32_t type=gaxU32(rom,songPtr+16+c*4);if(!gaxPtr(rom,type,28))return false;uint32_t orderPtr=gaxU32(rom,type+24);if(!gaxPtr(rom,orderPtr,orders*4))return false;}
    return true;
}

inline size_t findGaxSongTableOffset(const Bytes& rom){
    const auto& ch=gaxSongChannels();
    for(size_t p=0x100000;p+19*4<=std::min<size_t>(rom.size(),0x240000);p+=4){
        uint32_t first=u32(rom,p);if(!gaxPtr(rom,first,32))continue;
        if(!gaxSongLooksValid(rom,first,ch[0]))continue;
        bool ok=true;for(unsigned i=1;i<19&&ok;i++){uint32_t s=u32(rom,p+i*4);ok=gaxSongLooksValid(rom,s,ch[i]);}
        if(ok)return p;
    }
    throw std::runtime_error("GAX 19-song table not found");
}

inline GaxRomSong parseGaxSong(const Bytes& rom,size_t tableOffset,unsigned id){
    if(id>=19)throw std::runtime_error("GAX song id");
    GaxRomSong s;s.id=id;s.title=gaxSongNames()[id];s.songPtr=u32(rom,tableOffset+id*4);s.channels=gaxSongChannels()[id];
    if(!gaxSongLooksValid(rom,s.songPtr,s.channels))throw std::runtime_error("Invalid GAX song layout");
    s.mixerTypePtr=gaxU32(rom,s.songPtr+4);
    if(gaxPtr(rom,s.mixerTypePtr,28)){
        s.mixerDspPtr=gaxU32(rom,s.mixerTypePtr+24);
        if(gaxPtr(rom,s.mixerDspPtr,28)){
            s.dspPrimaryChannels=std::min<unsigned>(gaxU32(rom,s.mixerDspPtr),s.channels);
            for(unsigned i=0;i<3;i++){s.dspTaps[i].delayMs=gaxU32(rom,s.mixerDspPtr+4+i*8);s.dspTaps[i].gainQ4=gaxU32(rom,s.mixerDspPtr+8+i*8);}
        }
    }
    uint32_t info=gaxU32(rom,s.songPtr+8);s.infoTypePtr=info;s.songDataPtr=gaxU32(rom,info+24);
    s.patternRows=gaxU16(rom,s.songDataPtr+2);s.orderCount=gaxU16(rom,s.songDataPtr+4);s.loopOrder=gaxU16(rom,s.songDataPtr+6);s.volume=gaxU16(rom,s.songDataPtr+8);
    s.patternsPtr=gaxU32(rom,s.songDataPtr+0x0c);s.instrumentTablePtr=gaxU32(rom,s.songDataPtr+0x10);s.waveTablePtr=gaxU32(rom,s.songDataPtr+0x14);s.mixRate=gaxU16(rom,s.songDataPtr+0x18);s.numSfx=rom[gaxOff(rom,s.songDataPtr+0x1a)];
    s.orders.resize(s.channels);
    for(unsigned c=0;c<s.channels;c++){uint32_t type=gaxU32(rom,s.songPtr+16+c*4),op=gaxU32(rom,type+24);auto& v=s.orders[c];v.reserve(s.orderCount);for(unsigned j=0;j<s.orderCount;j++){uint32_t a=op+j*4;GaxOrderEntry e;e.patternOffset=gaxU16(rom,a);e.transpose=int8_t(rom[gaxOff(rom,a+2)]);v.push_back(e);}}
    return s;
}

inline std::vector<GaxRomSong> parseGaxSongs(const Bytes& rom,size_t* tableOffsetOut=nullptr){
    const size_t t=findGaxSongTableOffset(rom);if(tableOffsetOut)*tableOffsetOut=t;std::vector<GaxRomSong> out;out.reserve(19);for(unsigned i=0;i<19;i++)out.push_back(parseGaxSong(rom,t,i));return out;
}
inline int gaxSongId(MusicCue cue){std::string n=musicName(cue);const auto& names=gaxSongNames();for(unsigned i=0;i<names.size();i++)if(n==names[i])return int(i);return -1;}

inline std::vector<GaxWaveInfo> parseWaveTable(const Bytes& rom,uint32_t table,unsigned maxEntries){
    std::vector<GaxWaveInfo> w;w.reserve(maxEntries);for(unsigned i=0;i<maxEntries;i++){uint32_t p=gaxU32(rom,table+i*8),n=gaxU32(rom,table+i*8+4);if(p&&(!gaxPtr(rom,p,n)||n>2*1024*1024))break;w.push_back({p,n});}return w;
}

inline GaxInstrumentData parseInstrumentData(const Bytes& rom,uint32_t table,unsigned id,unsigned maxCount){
    GaxInstrumentData x;
    if(id>=maxCount||!gaxPtr(rom,table+id*4,4))return x;
    const uint32_t ip=gaxU32(rom,table+id*4);if(!gaxPtr(rom,ip,0x8c))return x;
    x.ptr=ip;for(unsigned i=0;i<4;i++)x.waveIdx[i]=rom[gaxOff(rom,ip+1+i)];
    x.vibratoDelay=rom[gaxOff(rom,ip+8)];x.vibratoDepth=rom[gaxOff(rom,ip+9)];x.vibratoSpeed=rom[gaxOff(rom,ip+0x0a)];
    for(unsigned i=0;i<4;i++){
        const uint32_t rp=ip+0x0c+i*28;auto& row=x.rows[i];row.sweepEnabled=rom[gaxOff(rom,rp)]!=0;row.pingPong=rom[gaxOff(rom,rp+1)]!=0;
        row.start=gaxS32(rom,rp+4);row.sweepMin=gaxS32(rom,rp+8);row.sweepMax=gaxS32(rom,rp+12);row.sweepLen=gaxS32(rom,rp+16);row.sweepStep=gaxS32(rom,rp+20);row.sweepRate=gaxU16(rom,rp+24);row.tune=gaxS16(rom,rp+26);
    }
    const uint32_t ep=gaxU32(rom,ip+0x7c);
    if(gaxPtr(rom,ep,4)){
        const unsigned count=rom[gaxOff(rom,ep)];
        if(count>0&&count<=64&&gaxPtr(rom,ep+4,count*8)){
            x.envelope.sustain=rom[gaxOff(rom,ep+1)];x.envelope.loopStart=rom[gaxOff(rom,ep+2)];x.envelope.loopEnd=rom[gaxOff(rom,ep+3)];x.envelope.points.reserve(count);
            bool sorted=true;uint16_t last=0;
            for(unsigned i=0;i<count;i++){uint32_t q=ep+4+i*8;GaxEnvelopePointData pt{gaxU16(rom,q),gaxS16(rom,q+2),rom[gaxOff(rom,q+4)]};if(i&&pt.pos<last)sorted=false;last=pt.pos;x.envelope.points.push_back(pt);}
            x.envelope.valid=sorted;
        }
    }
    x.seqSpeed=rom[gaxOff(rom,ip+0x84)];const unsigned seqLen=rom[gaxOff(rom,ip+0x85)];const uint32_t sp=gaxU32(rom,ip+0x88);
    if(seqLen&&seqLen<=128&&gaxPtr(rom,sp,seqLen*8)){
        x.seq.reserve(seqLen);for(unsigned i=0;i<seqLen;i++){uint32_t q=sp+i*8;GaxInstrumentSeqData e;e.note=rom[gaxOff(rom,q)];e.fixedPitch=rom[gaxOff(rom,q+1)];e.wave=rom[gaxOff(rom,q+2)];e.fx[0]=gaxU16(rom,q+4);e.fx[1]=gaxU16(rom,q+6);x.seq.push_back(e);}
    }
    x.valid=x.envelope.valid&&!x.seq.empty();return x;
}

inline std::vector<GaxInstrumentData> parseInstrumentBank(const Bytes& rom,uint32_t table,unsigned count){std::vector<GaxInstrumentData> out;out.reserve(count);for(unsigned i=0;i<count;i++)out.push_back(parseInstrumentData(rom,table,i,count));return out;}

inline GaxInstrumentInfo parseInstrument(const Bytes& rom,uint32_t table,unsigned id,unsigned maxCount){
    GaxInstrumentInfo x;auto d=parseInstrumentData(rom,table,id,maxCount);if(!d.valid)return x;x.ptr=d.ptr;x.waveIdx=d.waveIdx;unsigned row=0;if(!d.seq.empty()&&d.seq.front().wave>=1&&d.seq.front().wave<=4){row=d.seq.front().wave-1;x.seqNote=d.seq.front().note;x.fixedPitch=d.seq.front().fixedPitch!=0;}if(!x.waveIdx[row])for(unsigned i=0;i<4;i++)if(x.waveIdx[i]){row=i;break;}if(!x.waveIdx[row])return x;x.row=row;const auto& r=d.rows[row];x.start=std::max(0,r.start);x.loopStart=std::max(0,r.sweepMin);x.loopEnd=std::max(0,r.sweepMax);x.tune=r.tune;x.valid=true;return x;
}

inline uint32_t findGaxSfxSongData(const Bytes& rom,const GaxRomSong& anySong){
    const uint32_t musicInst=anySong.instrumentTablePtr;const size_t hi=gaxOff(rom,musicInst);
    const size_t begin=hi>0x20000?hi-0x20000:0,end=std::min(hi,rom.size()-36);
    for(size_t p=begin;p<=end;p+=4){uint32_t a=u32(rom,p);if(!gaxPtr(rom,a,28))continue;bool same=true;for(unsigned i=1;i<9;i++)if(u32(rom,p+i*4)!=a){same=false;break;}if(!same)continue;
        uint32_t sd=gaxU32(rom,a+24);if(!gaxPtr(rom,sd,28))continue;uint32_t it=gaxU32(rom,sd+0x10),wt=gaxU32(rom,sd+0x14);if(!gaxPtr(rom,it,88*4)||!gaxPtr(rom,wt,88*8))continue;if(gaxU16(rom,sd+8)!=256)continue;return sd;
    }
    return 0;
}
inline std::vector<GaxSfxEntry> parseGaxSfxTable(const Bytes& rom,size_t songTableOffset){
    const size_t p=songTableOffset+19*4;if(p+99*12>rom.size())return {};std::vector<GaxSfxEntry> out;out.reserve(99);for(unsigned i=0;i<99;i++){size_t q=p+i*12;uint32_t slot=u32(rom,q),prio=u32(rom,q+4),vol=u32(rom,q+8);if(slot>=88||vol>0x400)return {};out.push_back({slot,int32_t(prio),vol});}return out;
}

inline size_t findGaxPeriodTableOffset(const Bytes& rom){
    static constexpr std::array<uint32_t,4> sig{{0x00105A02u,0x00106192u,0x00106926u,0x001070BDu}};
    constexpr size_t Count=0xEF4u;
    for(size_t p=0;p+Count*4u<=rom.size();p+=4){
        bool ok=true;for(size_t i=0;i<sig.size();++i)if(u32(rom,p+i*4u)!=sig[i]){ok=false;break;}
        if(!ok)continue;
        if(u32(rom,p+0x600u*4u)<=u32(rom,p)||u32(rom,p+(Count-1u)*4u)<=u32(rom,p+0x600u*4u))continue;
        return p;
    }
    throw std::runtime_error("GAX period table not found");
}

inline int originalSfxId(SfxCue c){
    switch(c){case SfxCue::Navigate:return 0x46;case SfxCue::Confirm:return 0x49;case SfxCue::Cancel:return 0x49;case SfxCue::MapNavigate:return 0x48;case SfxCue::MapConfirm:return 0x52;case SfxCue::MapOpen:return 0x51;case SfxCue::Jump:return 0x0d;case SfxCue::Spin:return 0x0a;case SfxCue::Wumpa:return 0x08;case SfxCue::Crate:return 0x03;case SfxCue::Life:return 0x07;case SfxCue::Clock:return 0x01;case SfxCue::Checkpoint:return 0x17;case SfxCue::Shot:return 0x24;case SfxCue::Explosion:return 0x42;case SfxCue::EnemyDown:return 0x14;case SfxCue::BossHit:return 0x43;case SfxCue::LevelClear:return 0x1c;default:return -1;}
}

struct GaxBankStats {unsigned valid=0,envelopes=0,vibrato=0,multiSequence=0,loopRows=0,pingPongRows=0,sweepRows=0;};
inline GaxBankStats gaxBankStats(const std::vector<GaxInstrumentData>& bank){GaxBankStats s;for(const auto& i:bank){if(!i.valid)continue;++s.valid;if(i.envelope.valid)++s.envelopes;if(i.vibratoDepth)++s.vibrato;if(i.seq.size()>1)++s.multiSequence;for(unsigned r=0;r<4;r++)if(i.waveIdx[r]){const auto& q=i.rows[r];if(q.sweepMin<q.sweepMax)++s.loopRows;if(q.pingPong)++s.pingPongRows;if(q.sweepEnabled)++s.sweepRows;}}return s;}

inline int gaxAsr(int value,unsigned bits){
    if(!bits)return value;
    const int d=1<<bits;
    return value>=0?value/d:-int((unsigned(-value)+unsigned(d-1))/unsigned(d));
}
inline int gaxWrap16(int value){const unsigned u=unsigned(value)&0xffffu;return u<=0x7fffu?int(u):int(u)-0x10000;}
inline int gaxResampleContribution(int sampleSigned8,unsigned volumeQ8){return gaxAsr(sampleSigned8*int(volumeQ8),8);}
inline int gaxDownmix8(int mixed16,unsigned handlers){
    const unsigned step=handlers==1?0x400u:(handlers+8u)<<6;
    return std::clamp(gaxAsr(mixed16*int(step),10),-128,127);
}
inline unsigned originalSfxVolumeParam(SfxCue c){return c==SfxCue::Shot?0xA0u:0x100u;}
inline int originalSfxVoice(SfxCue c){return c==SfxCue::Shot?2:-1;}

class GaxLitePlayer {
    static constexpr double OutputRate=44100.0;
    static constexpr double GbaHz=59.7275;
    static constexpr int NoNote=0x8ad0;
    static constexpr std::array<int,64> Vibrato{{0,12,24,37,48,60,70,80,90,98,106,112,117,122,125,126,127,126,125,122,117,112,106,98,90,80,70,60,48,37,24,12,0,-12,-24,-37,-48,-60,-70,-80,-90,-98,-106,-112,-117,-122,-125,-126,-127,-126,-125,-122,-117,-112,-106,-98,-90,-80,-70,-60,-48,-37,-24,-12}};

    struct Voice {
        bool bound=false,playing=false,fixedPitch=false,released=false,sweepOn=false;
        unsigned instrument=0,row=0;
        int patternPitch=0,pitchStep=0,note=NoNote,noteStep=0,vibratoOffset=0,slideTarget=0,slideRate=0,transpose=0;
        int vol15=255,vol17=255,envOut=255,volStep15=0,volStep17=0,volume=255;
        unsigned envPos=0,vibratoPhase=0,vibratoDelay=0,seqPos=0,seqLoopCount=0,cutDelay=0,cutTimer=0;
        int64_t samplePosQ11=0;
        uint32_t sampleStepQ11=0;
        double gain=1.0;
        int sampleDir=1,sweepPos=0,sweepDir=1,sweepTimer=0;
        uint32_t wavePtr=0,waveLen=0;
        int priority=std::numeric_limits<int>::min();
    };
    struct Chan {Voice v;size_t patternOff=0;unsigned rowSkip=0;bool empty=true;unsigned retriggerDelay=0,delayedNote=0,delayedInstrument=0;};

    const Bytes* rom_=nullptr;
    std::vector<GaxRomSong> songs_;
    size_t songTableOffset_=0,periodTableOffset_=std::numeric_limits<size_t>::max();
    std::vector<GaxWaveInfo> musicWaves_,sfxWaves_;
    std::vector<GaxInstrumentData> musicInst_,sfxInst_;
    std::vector<GaxSfxEntry> sfxTable_;
    uint32_t sfxInstTable_=0;
    std::vector<Chan> ch_;
    std::array<Voice,3> fx_{};
    unsigned fxToggle_=0;
    int songId_=-1;
    unsigned order_=0,row_=0;
    uint16_t speed_=6;
    unsigned tickCounter_=0;
    bool started_=false,patternBreak_=false;
    double tickAccum_=0,hostAccum_=0;
    int heldOutput_=0;
    std::vector<int16_t> echoBuf_;
    size_t echoPos_=0;

    static int clampByte(int v){return std::max(0,std::min(255,v));}
    unsigned activeMixRate()const{return songId_>=0?songs_[unsigned(songId_)].mixRate:21025u;}
    uint32_t q5StepQ11(int q5)const{
        const unsigned rate=activeMixRate();
        if(rom_&&periodTableOffset_!=std::numeric_limits<size_t>::max()){
            uint32_t idx=uint32_t(q5);
            if(idx>0xEF3u)idx=0xEF3u;
            const uint32_t period=u32(*rom_,periodTableOffset_+size_t(idx)*4u);
            const uint64_t reciprocal=(uint64_t(1)<<32)/uint64_t(rate);
            return uint32_t((uint64_t(period)*reciprocal)>>32);
        }
        const double semis=double(q5)/32.0-48.0;
        const double hz=8363.0*std::pow(2.0,semis/12.0);
        return uint32_t(std::max(1.0,std::floor(hz*2048.0/double(rate))));
    }
    static unsigned speedLow(uint16_t s){return s&0xffu;}
    void stopNote(Voice& v){v.note=NoNote;v.noteStep=0;v.playing=false;v.priority=std::numeric_limits<int>::min();}

    void setNote(Voice& v,unsigned note,const std::vector<GaxInstrumentData>& bank){
        if(note==1){
            if(v.bound&&v.instrument<bank.size()&&bank[v.instrument].valid&&bank[v.instrument].envelope.sustain==0xff)stopNote(v);
            v.released=true;return;
        }
        if(note>1){v.patternPitch=int(note-2)<<5;v.released=false;}
    }
    void bindInstrument(Voice& v,unsigned id,const std::vector<GaxInstrumentData>& bank){
        if(!id)return;
        if(id>=bank.size()||!bank[id].valid){v.bound=false;v.playing=false;return;}
        v.instrument=id;v.bound=true;const auto& ins=bank[id];v.envPos=0;v.released=false;v.vibratoPhase=0;v.vibratoDelay=ins.vibratoDelay;v.seqPos=0;v.seqLoopCount=0;v.cutTimer=0;v.vol15=255;v.cutDelay=ins.seqSpeed;v.slideRate=0;v.slideTarget=0;v.volStep17=0;v.noteStep=0;
    }
    void selectWave(Voice& v,const GaxInstrumentData& ins,const std::vector<GaxWaveInfo>& waves,unsigned row){
        if(row>=4||ins.waveIdx[row]>=waves.size()){v.playing=false;return;}const auto& w=waves[ins.waveIdx[row]];if(!w.ptr||!w.length){v.playing=false;return;}
        v.row=row;v.wavePtr=w.ptr;v.waveLen=w.length;const auto& r=ins.rows[row];
        const int start=std::clamp(r.start,0,int(w.length-1));v.samplePosQ11=int64_t(start)<<11;v.sampleDir=1;v.vol17=255;v.sweepOn=false;
        if(r.sweepEnabled&&r.sweepMin<r.sweepMax&&r.sweepLen>0&&r.sweepRate&&r.sweepStep>0){v.sweepOn=true;v.sweepPos=r.start;const int sp=std::clamp(v.sweepPos,0,int(w.length-1));v.samplePosQ11=int64_t(sp)<<11;v.sweepTimer=r.sweepRate;v.sweepDir=(v.sweepPos+r.sweepLen>r.sweepMax)?-1:1;}
        v.playing=true;
    }
    void stepSequence(Voice& v,const std::vector<GaxInstrumentData>& bank,const std::vector<GaxWaveInfo>& waves){
        if(!v.bound||v.instrument>=bank.size())return;
        const auto& ins=bank[v.instrument];
        if(v.seqPos>=ins.seq.size()){v.cutDelay=0;return;}
        const auto e=ins.seq[v.seqPos++];
        if(e.note){v.note=int(e.note-2)<<5;v.fixedPitch=e.fixedPitch!=0;if(e.wave>=1&&e.wave<=4)selectWave(v,ins,waves,e.wave-1);}
        v.volStep17=0;v.noteStep=0;
        for(uint16_t fx:e.fx){unsigned cmd=fx>>8,param=fx&0xff;switch(cmd){case 1:v.noteStep=int(param);break;case 2:v.noteStep=-int(param);break;case 5:if(v.seqLoopCount==0||--v.seqLoopCount!=0)v.seqPos=std::min<unsigned>(param,unsigned(ins.seq.size()));break;case 6:if(v.seqLoopCount==0)v.seqLoopCount=param?param+1:0;break;case 10:v.volStep17=int(param);break;case 11:v.volStep17=-int(param);break;case 12:v.vol17=int(param);break;case 15:v.cutDelay=param;break;default:break;}}
    }
    int envelopeTick(Voice& v,const GaxEnvelopeData& env){
        if(!env.valid||env.points.empty())return 255;
        unsigned pos=v.envPos++;
        const unsigned last=unsigned(env.points.size()-1);
        if(env.sustain!=0xff&&env.sustain<env.points.size()&&!v.released&&pos==env.points[env.sustain].pos)v.envPos=pos;
        if(pos>=env.points[last].pos){if(env.points[last].value==0&&(env.loopEnd==0xff||env.loopEnd<last))stopNote(v);v.envPos=pos;pos=env.points[last].pos;}
        if(!v.released&&env.loopStart!=0xff&&env.loopEnd!=0xff&&env.loopStart<env.points.size()&&env.loopEnd<env.points.size()&&pos==env.points[env.loopEnd].pos)v.envPos=env.points[env.loopStart].pos;
        unsigned i=0;while(i+1<env.points.size()&&env.points[i].pos<pos)++i;if(pos==env.points[i].pos)return env.points[i].value;if(i==0)return env.points[0].value;const int slope=env.points[i].slope;--i;return clampByte(int(env.points[i].value)+int((int(pos)-int(env.points[i].pos))*slope>>8));
    }
    void vibratoTick(Voice& v,const GaxInstrumentData& ins){
        int result=0;if(ins.vibratoDepth){if(v.vibratoDelay==0)v.vibratoPhase=(v.vibratoPhase+ins.vibratoSpeed)&0x3f;else --v.vibratoDelay;result=(Vibrato[v.vibratoPhase]*int(ins.vibratoDepth))>>8;}v.vibratoOffset=result;
    }
    void sweepTick(Voice& v,const GaxInstrumentData& ins){
        if(!v.sweepOn||v.row>=4)return;
        if(--v.sweepTimer!=0)return;
        const auto& r=ins.rows[v.row];
        const int old=v.sweepPos;
        v.sweepTimer=r.sweepRate;
        if(v.sweepDir>0){v.sweepPos=old+r.sweepStep;if(v.sweepPos+r.sweepLen>r.sweepMax){v.sweepPos-=r.sweepStep*2;v.sweepDir=-1;}}
        else{v.sweepPos=old-r.sweepStep;if(v.sweepPos<r.sweepMin){v.sweepPos+=r.sweepStep*2;v.sweepDir=1;}}
        v.samplePosQ11+=int64_t(v.sweepPos-old)<<11;
    }
    void updateStep(Voice& v,const std::vector<GaxInstrumentData>& bank){
        if(!v.bound||v.instrument>=bank.size()||v.row>=4||v.note==NoNote){v.sampleStepQ11=0;return;}const auto& ins=bank[v.instrument];int pitch=v.note+v.vibratoOffset+ins.rows[v.row].tune;if(!v.fixedPitch)pitch+=v.patternPitch+(v.transpose*32);v.sampleStepQ11=q5StepQ11(pitch);
    }
    void tickVoice(Voice& v,const std::vector<GaxInstrumentData>& bank,const std::vector<GaxWaveInfo>& waves){
        if(!v.bound||v.instrument>=bank.size())return;
        const auto& ins=bank[v.instrument];
        if(v.cutTimer==0){if(v.cutDelay){stepSequence(v,bank,waves);v.cutTimer=v.cutDelay?v.cutDelay-1:0;}}else --v.cutTimer;
        if(v.bound){v.envOut=envelopeTick(v,ins.envelope);vibratoTick(v,ins);sweepTick(v,ins);}
        v.vol15=clampByte(v.vol15+v.volStep15);v.vol17=clampByte(v.vol17+v.volStep17);v.patternPitch+=v.pitchStep;if(v.note!=NoNote)v.note+=v.noteStep;
        if(v.slideRate){const bool before=(v.slideTarget-v.patternPitch)<0;v.patternPitch+=v.slideRate;const bool after=(v.slideTarget-v.patternPitch)<0;if(before!=after||v.patternPitch==v.slideTarget){v.slideRate=0;v.patternPitch=v.slideTarget;v.slideTarget=0;}}
        updateStep(v,bank);
    }
    unsigned voiceVolumeQ8(const Voice& v,unsigned songVolume=256)const{
        unsigned q=v.envOut!=255?unsigned(v.envOut):256u;
        if(v.vol17!=255)q=(q*unsigned(v.vol17))>>8;
        if(v.vol15!=255)q=(q*unsigned(v.vol15))>>8;
        if(v.volume!=255)q=(q*unsigned(v.volume))>>8;
        q=(q*songVolume)>>8;
        return std::min(q,0x100u);
    }
    int sampleVoiceRaw(Voice& v,const std::vector<GaxInstrumentData>& bank,unsigned songVolume=256){
        if(!v.playing||v.note==NoNote||!v.wavePtr||!v.waveLen||!v.sampleStepQ11||!v.bound||v.instrument>=bank.size())return 0;
        const auto& ins=bank[v.instrument];
        const auto& r=ins.rows[v.row];
        const int64_t waveEnd=int64_t(v.waveLen)<<11;
        if(v.samplePosQ11<0||v.samplePosQ11>=waveEnd){v.playing=false;v.priority=std::numeric_limits<int>::min();return 0;}
        const unsigned pos=std::min<unsigned>(unsigned(v.samplePosQ11>>11),v.waveLen-1);
        const int sample=int(int8_t((*rom_)[gaxOff(*rom_,v.wavePtr+pos)]));
        const int out=gaxResampleContribution(sample,voiceVolumeQ8(v,songVolume));
        v.samplePosQ11+=int64_t(v.sampleStepQ11)*v.sampleDir;
        if(!r.sweepEnabled&&r.sweepMin<r.sweepMax){
            const int64_t lo=int64_t(std::clamp(r.sweepMin,0,int(v.waveLen)))<<11,hi=int64_t(std::clamp(r.sweepMax,0,int(v.waveLen)))<<11;
            if(hi>lo){if(r.pingPong){if(v.sampleDir>0&&v.samplePosQ11>=hi){v.samplePosQ11=std::max(lo,hi-(v.samplePosQ11-hi));v.sampleDir=-1;}else if(v.sampleDir<0&&v.samplePosQ11<lo){v.samplePosQ11=std::min(hi,lo+(lo-v.samplePosQ11));v.sampleDir=1;}}else if(v.samplePosQ11>=hi)v.samplePosQ11=lo+(v.samplePosQ11-lo)%(hi-lo);}
        }else if(v.sweepOn&&r.sweepLen>0){
            const int64_t lo=int64_t(std::clamp(v.sweepPos,0,int(v.waveLen)))<<11,hi=int64_t(std::clamp(v.sweepPos+r.sweepLen,0,int(v.waveLen)))<<11;
            if(hi>lo&&v.samplePosQ11>=hi)v.samplePosQ11=lo+(v.samplePosQ11-lo)%(hi-lo);
        }else if(v.samplePosQ11>=waveEnd){v.playing=false;v.priority=std::numeric_limits<int>::min();}
        return out;
    }

    void setPattern(const GaxRomSong& s,unsigned ci){Chan& c=ch_[ci];const auto& e=s.orders[ci][order_];uint32_t ptr=s.patternsPtr+e.patternOffset;if(!gaxPtr(*rom_,ptr,1)){c.empty=true;return;}c.patternOff=gaxOff(*rom_,ptr);c.empty=(*rom_)[c.patternOff++]!=0;c.rowSkip=0;c.v.transpose=e.transpose;}
    void decodeRow(const GaxRomSong&,unsigned ci,bool retrigger=false){
        Chan& c=ch_[ci];Voice& v=c.v;v.volStep15=0;v.pitchStep=0;c.retriggerDelay=0;unsigned note=0,inst=0,cmd=0,param=0;
        if(retrigger){note=c.delayedNote;inst=c.delayedInstrument;}
        else{
            if(c.empty)return;
            if(c.rowSkip){--c.rowSkip;return;}
            if(c.patternOff>=rom_->size())return;
            size_t p=c.patternOff;uint8_t b=(*rom_)[p];
            if(b==0xff){if(p+1>=rom_->size())return;c.rowSkip=(*rom_)[p+1]?(*rom_)[p+1]-1:0;c.patternOff=p+2;return;}
            if(b&0x80){unsigned packed=b&0x7f;if(!packed){c.patternOff=p+1;return;}if(packed<=121){if(p+1>=rom_->size())return;note=packed;inst=(*rom_)[p+1];c.patternOff=p+2;}else{if(p+2>=rom_->size())return;cmd=(*rom_)[p+1];param=(*rom_)[p+2];c.patternOff=p+3;}}
            else{if(p+3>=rom_->size())return;note=b;inst=(*rom_)[p+1];cmd=(*rom_)[p+2];param=(*rom_)[p+3];c.patternOff=p+4;}
            if(cmd==14&&(param>>4)==13){c.retriggerDelay=param&15;c.delayedNote=note;c.delayedInstrument=inst;return;}
        }
        if(cmd!=3)setNote(v,note,musicInst_);
        bindInstrument(v,inst,musicInst_);
        switch(cmd){case 1:v.pitchStep=int(param);break;case 2:v.pitchStep=-int(param);break;case 3:if(param&&note>1){v.slideTarget=int(note-2)<<5;v.slideRate=(v.slideTarget-v.patternPitch)/int(param);}break;case 7:speed_=uint16_t((param>>4)|((param<<8)&0x0f00));tickCounter_=speedLow(speed_)?speedLow(speed_)-1:0;break;case 10:v.volStep15=int(param);break;case 11:v.volStep15=-int(param);break;case 12:v.vol15=int(param);break;case 13:patternBreak_=true;break;case 15:speed_=uint16_t(param);tickCounter_=param?param-1:0;break;default:break;}
    }
    void tickMusic(){
        if(songId_<0)return;
        const GaxRomSong& s=songs_[unsigned(songId_)];
        bool newRow=false,newOrder=false;
        if(!started_){started_=true;order_=0;row_=0;speed_=6;tickCounter_=speedLow(speed_)-1;patternBreak_=false;newRow=newOrder=true;}
        else if(speedLow(speed_)&&tickCounter_==0){if(patternBreak_)row_=s.patternRows;else ++row_;patternBreak_=false;if((speed_>>8)!=0)speed_=uint16_t((speed_>>8)|(speed_<<8));tickCounter_=speedLow(speed_)?speedLow(speed_)-1:0;if(row_>=s.patternRows){row_=0;++order_;if(order_>=s.orderCount)order_=std::min<unsigned>(s.loopOrder,s.orderCount-1);newOrder=true;}newRow=true;}
        else if(tickCounter_)--tickCounter_;
        if(newOrder)for(unsigned c=0;c<s.channels;c++)setPattern(s,c);
        for(unsigned c=0;c<s.channels;c++){
            Chan& q=ch_[c];if(q.retriggerDelay&&--q.retriggerDelay==0)decodeRow(s,c,true);if(newRow)decodeRow(s,c,false);tickVoice(q.v,musicInst_,musicWaves_);
        }
    }
    void tickFx(){for(auto& v:fx_)if(v.bound)tickVoice(v,sfxInst_,sfxWaves_);}
    void tick(){tickMusic();tickFx();}
    void resetEcho(){
        echoBuf_.clear();
        echoPos_=0;
        if(songId_<0)return;
        const auto& s=songs_[unsigned(songId_)];
        unsigned maxDelay=0;
        for(const auto& t:s.dspTaps)maxDelay=std::max(maxDelay,t.delayMs);
        if(maxDelay){
            size_t n=size_t((uint64_t(maxDelay)*uint64_t(s.mixRate))/1000u);
            if(n)echoBuf_.assign(n,0);
        }
    }
    int applyEcho(int mixed,const GaxRomSong& s){
        if(echoBuf_.empty())return mixed;
        int contribution=0;
        const size_t n=echoBuf_.size();
        for(const auto& t:s.dspTaps){
            if(!t.delayMs||!t.gainQ4)continue;
            size_t delay=size_t((uint64_t(t.delayMs)*uint64_t(s.mixRate))/1000u);
            if(!delay)continue;
            if(delay>n)delay=n;
            size_t idx=echoPos_+n-delay;
            if(idx>=n)idx-=n;
            contribution+=gaxAsr(int(echoBuf_[idx])*int(t.gainQ4),4);
        }
        const int wet=gaxWrap16(mixed+contribution);
        echoBuf_[echoPos_]=int16_t(wet);
        if(++echoPos_>=n)echoPos_=0;
        return wet;
    }
    int renderInternal(){
        const unsigned rate=activeMixRate();tickAccum_+=GbaHz/double(rate);while(tickAccum_>=1.0){tickAccum_-=1.0;tick();}
        int mixed=0;unsigned handlers=unsigned(fx_.size());
        if(songId_>=0){const auto& s=songs_[unsigned(songId_)];handlers+=s.channels;const unsigned primary=std::min<unsigned>(s.dspPrimaryChannels,unsigned(ch_.size()));
            for(unsigned i=0;i<primary;i++)mixed=gaxWrap16(mixed+sampleVoiceRaw(ch_[i].v,musicInst_,s.volume));
            if(!echoBuf_.empty())mixed=applyEcho(mixed,s);
            for(unsigned i=primary;i<ch_.size();i++)mixed=gaxWrap16(mixed+sampleVoiceRaw(ch_[i].v,musicInst_,s.volume));
        }
        for(auto& v:fx_)mixed=gaxWrap16(mixed+sampleVoiceRaw(v,sfxInst_,256));
        return gaxDownmix8(mixed,handlers);
    }

public:
    bool load(const Bytes& rom){
        try{rom_=&rom;periodTableOffset_=findGaxPeriodTableOffset(rom);songs_=parseGaxSongs(rom,&songTableOffset_);musicWaves_=parseWaveTable(rom,songs_[0].waveTablePtr,30);musicInst_=parseInstrumentBank(rom,songs_[0].instrumentTablePtr,52);sfxTable_=parseGaxSfxTable(rom,songTableOffset_);uint32_t sd=findGaxSfxSongData(rom,songs_[0]);if(sd){sfxInstTable_=gaxU32(rom,sd+0x10);sfxWaves_=parseWaveTable(rom,gaxU32(rom,sd+0x14),88);sfxInst_=parseInstrumentBank(rom,sfxInstTable_,88);}return songs_.size()==19&&musicWaves_.size()>=30&&gaxBankStats(musicInst_).valid==52;}
        catch(...){rom_=nullptr;periodTableOffset_=std::numeric_limits<size_t>::max();songs_.clear();musicInst_.clear();sfxInst_.clear();return false;}
    }
    bool ready()const{return rom_&&periodTableOffset_!=std::numeric_limits<size_t>::max()&&songs_.size()==19&&musicWaves_.size()>=30&&musicInst_.size()==52;}
    size_t songTableOffset()const{return songTableOffset_;}
    size_t periodTableOffset()const{return periodTableOffset_;}
    uint32_t periodValue(unsigned idx)const{if(!rom_||periodTableOffset_==std::numeric_limits<size_t>::max())return 0;idx=std::min(idx,0xEF3u);return u32(*rom_,periodTableOffset_+size_t(idx)*4u);}
    const std::vector<GaxRomSong>& songs()const{return songs_;}
    const std::vector<GaxWaveInfo>& musicWaves()const{return musicWaves_;}
    const std::vector<GaxSfxEntry>& sfxTable()const{return sfxTable_;}
    const std::vector<GaxInstrumentData>& musicInstruments()const{return musicInst_;}
    const std::vector<GaxInstrumentData>& sfxInstruments()const{return sfxInst_;}
    void select(MusicCue cue){int id=gaxSongId(cue);if(id==songId_)return;songId_=id;ch_.clear();started_=false;tickAccum_=0;hostAccum_=0;heldOutput_=0;if(id>=0)ch_.resize(songs_[unsigned(id)].channels);resetEcho();}
    bool playSfxId(unsigned id,unsigned volumeParam=0x100,int forcedVoice=-1){
        if(!ready()||sfxInst_.empty()||sfxWaves_.empty()||id>=sfxTable_.size())return false;
        const auto e=sfxTable_[id];
        if(!e.slotId||e.slotId>=sfxInst_.size()||!sfxInst_[e.slotId].valid)return false;
        unsigned voice=0;int priority=e.priority;
        if(forcedVoice>=0){voice=unsigned(forcedVoice);if(voice>=fx_.size())return false;priority=0;}
        else{voice=fxToggle_&1u;if(priority<fx_[voice].priority){voice^=1u;if(priority<fx_[voice].priority)return false;}fxToggle_=voice^1u;}
        const unsigned volume=std::min(255u,(e.baseVolume*volumeParam)>>8);
        Voice v;v.priority=priority;v.gain=1.0;v.volume=int(volume);setNote(v,8,sfxInst_);bindInstrument(v,e.slotId,sfxInst_);v.volume=int(volume);v.priority=priority;fx_[voice]=v;return true;
    }
    void playSfx(SfxCue cue){
        int id=originalSfxId(cue);
        if(id>=0)playSfxId(unsigned(id),originalSfxVolumeParam(cue),originalSfxVoice(cue));
    }
    double next(){
        if(!ready())return 0;
        hostAccum_+=double(activeMixRate())/OutputRate;
        while(hostAccum_>=1.0){hostAccum_-=1.0;heldOutput_=renderInternal();}
        return double(heldOutput_)/128.0;
    }
};

}
