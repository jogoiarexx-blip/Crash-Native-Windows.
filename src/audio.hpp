#pragma once
#include "rom.hpp"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace crash {

enum class MusicCue : unsigned {
    Silence, MainMenu, WarpRoom, Jungle, Arctic, Sewers, Underwater,
    Rocket, Future, Bonus, Dingodile, NGin, Tiny, Cortex
};

enum class SfxCue : unsigned {
    Navigate, Confirm, Cancel, MapNavigate, MapConfirm, MapOpen, Jump, Spin, Wumpa, Crate, Checkpoint,
    Hurt, Life, Gem, Clock, Shot, Explosion, EnemyDown, BossHit, LevelClear
};

inline const char* musicName(MusicCue c){
    switch(c){
    case MusicCue::MainMenu:return "main menu europe";
    case MusicCue::WarpRoom:return "warp room";
    case MusicCue::Jungle:return "jungle";
    case MusicCue::Arctic:return "arctic";
    case MusicCue::Sewers:return "sewers";
    case MusicCue::Underwater:return "underwater";
    case MusicCue::Rocket:return "rocket crash";
    case MusicCue::Future:return "future";
    case MusicCue::Bonus:return "bonus round";
    case MusicCue::Dingodile:return "dingodile";
    case MusicCue::NGin:return "n gin";
    case MusicCue::Tiny:return "tiny";
    case MusicCue::Cortex:return "neo cortex";
    default:return "silence";
    }
}

inline MusicCue levelMusic(unsigned level,unsigned category=0,bool actorCategory=false){
    if(level==20)return MusicCue::Dingodile;
    if(level==21)return MusicCue::NGin;
    if(level==22)return MusicCue::Tiny;
    if(level==23)return MusicCue::Cortex;
    if(level==24)return MusicCue::Bonus;
    if(actorCategory){
        if(category>=3&&category<=6)return MusicCue::Rocket;
        return MusicCue::Arctic;
    }
    switch(level){
    case 0: case 2: case 7: case 9:return MusicCue::Jungle;
    case 1: case 8: case 12: case 16:return MusicCue::Underwater;
    case 3: case 5: case 10:return MusicCue::Arctic;
    case 4: case 13: case 15: case 18:return MusicCue::Sewers;
    case 6: case 11: case 14: case 17:return MusicCue::Rocket;
    case 19:return MusicCue::Future;
    default:return MusicCue::Jungle;
    }
}

struct GaxTrackInfo {
    size_t metadataOffset=0;
    std::string title;
    std::string author;
};

inline std::string gaxVersion(const Bytes& rom){
    static const std::string key="GAX Sound Engine ";
    for(size_t i=0;i+key.size()<rom.size();++i){
        if(!std::equal(key.begin(),key.end(),rom.begin()+std::ptrdiff_t(i)))continue;
        size_t e=i;while(e<rom.size()&&e<i+96&&rom[e]>=0x20&&rom[e]<0x7f)++e;
        return std::string(reinterpret_cast<const char*>(rom.data()+i),e-i);
    }
    return {};
}

inline std::vector<GaxTrackInfo> scanGaxTracks(const Bytes& rom){
    std::vector<GaxTrackInfo> out;
    for(size_t i=0;i+8<rom.size();++i){
        if(rom[i]!='\"')continue;
        size_t q=i+1;while(q<rom.size()&&q-i<=96&&rom[q]!='\"'&&rom[q]>=0x20&&rom[q]<0x7f)++q;
        if(q>=rom.size()||rom[q]!='\"'||q==i+1)continue;
        size_t p=q+1;while(p<rom.size()&&p-q<=8&&rom[p]==' ')++p;
        if(p>=rom.size()||rom[p]!=0xa9)continue;
        ++p;while(p<rom.size()&&rom[p]==' ')++p;
        size_t a=p;while(p<rom.size()&&p-a<=80&&rom[p]>=0x20&&rom[p]<0x7f&&rom[p]!='\"')++p;
        if(p==a)continue;
        GaxTrackInfo t;t.metadataOffset=i;t.title.assign(reinterpret_cast<const char*>(rom.data()+i+1),q-i-1);t.author.assign(reinterpret_cast<const char*>(rom.data()+a),p-a);
        while(!t.author.empty()&&std::isspace(static_cast<unsigned char>(t.author.back())))t.author.pop_back();
        if(t.author.size()<4||t.author.find(' ')==std::string::npos)continue;
        out.push_back(std::move(t));i=q;
    }
    return out;
}

inline const GaxTrackInfo* findGaxTrack(const std::vector<GaxTrackInfo>& tracks,const std::string& title){
    auto it=std::find_if(tracks.begin(),tracks.end(),[&](const GaxTrackInfo& t){return t.title==title;});
    return it==tracks.end()?nullptr:&*it;
}

struct OriginalSfxEvent {
    unsigned id=0;
    unsigned volumeParam=0x100;
    int forcedVoice=-1;
};

struct AudioCounters {
    unsigned wumpa=0,crates=0,checkpoints=0,hazards=0,lives=0,gems=0,enemies=0,shots=0,bossParts=0;
    bool clock=false,complete=false;
};

inline std::vector<SfxCue> audioDelta(const AudioCounters& before,const AudioCounters& now){
    std::vector<SfxCue> q;
    if(now.wumpa>before.wumpa)q.push_back(SfxCue::Wumpa);
    if(now.crates>before.crates)q.push_back(SfxCue::Crate);
    if(now.checkpoints>before.checkpoints)q.push_back(SfxCue::Checkpoint);
    if(now.hazards>before.hazards)q.push_back(SfxCue::Hurt);
    if(now.lives>before.lives)q.push_back(SfxCue::Life);
    if(now.gems>before.gems)q.push_back(SfxCue::Gem);
    if(now.enemies>before.enemies)q.push_back(SfxCue::EnemyDown);
    if(now.shots>before.shots)q.push_back(SfxCue::Shot);
    if(now.bossParts>before.bossParts)q.push_back(SfxCue::BossHit);
    if(now.clock&&!before.clock)q.push_back(SfxCue::Clock);
    if(now.complete&&!before.complete)q.push_back(SfxCue::LevelClear);
    return q;
}

}
