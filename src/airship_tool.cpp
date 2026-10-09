#include "category0.hpp"
#include <algorithm>
#include <iostream>
#include <vector>

int main(int argc,char** argv){try{
    if(argc!=3){std::cerr<<"Usage: CrashAirshipTool ROM OUTPUT_BMP\n";return 2;}
    crash::GameData data(crash::read(argv[1]));crash::Category0Stage s(data,5);s.collisions=false;
    unsigned idx=0;while(idx<s.rom.eventCount&&s.kind(idx)!=17)++idx;if(idx>=s.rom.eventCount)throw std::runtime_error("category 5 kind-17 airship not found");
    s.scroll=double(s.rom.eventDepth(idx))-160.0;s.updateJetpackSpawns();
    std::vector<uint32_t> strip(720*160,0xff000000);auto put=[&](int panel){auto shot=s.renderLogical(true);for(int y=0;y<160;y++)std::copy_n(shot.begin()+y*240,240,strip.begin()+y*720+panel*240);};
    s.scroll=s.eventZ[idx]-120.0;s.updateJetpackActors();put(0);
    s.eventCooldown[idx]=0;for(int t=0;t<24;t++){++s.ticks;s.updateJetpackActors();s.stepEnemyShots();}put(1);
    s.beginAirshipExplosion(idx);for(int t=0;t<82;t++){++s.ticks;s.updateAirship(idx,17);}put(2);
    crash::bmp(argv[2],strip,720,160);
    std::cout<<"category=5 airship_kind=17 hp_base=45 state="<<unsigned(s.eventState[idx])<<" projectiles="<<s.airshipProjectilesFired<<" explosion_bursts="<<s.airshipExplosionsSpawned<<" events="<<s.rom.eventCount<<" end="<<s.rom.routeEndThreshold<<"\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
