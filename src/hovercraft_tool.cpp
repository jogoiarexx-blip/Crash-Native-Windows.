#include "category0.hpp"
#include <algorithm>
#include <iostream>
#include <vector>

int main(int argc,char** argv){try{
    if(argc!=3){std::cerr<<"Usage: CrashHovercraftTool ROM OUTPUT_BMP\n";return 2;}
    crash::GameData data(crash::read(argv[1]));crash::Category0Stage s(data,6);s.collisions=false;
    std::vector<uint32_t> strip(720*160,0xff000000);
    auto put=[&](int panel){auto shot=s.renderLogical(true);for(int y=0;y<160;y++)std::copy_n(shot.begin()+y*240,240,strip.begin()+y*720+panel*240);};
    s.scroll=544.0;s.updateJetpackSpawns();put(0);
    s.scroll=s.hovercraftZ-70.0;for(int i=0;i<45;i++){++s.ticks;s.updateHovercraft();}put(1);
    s.destroyHoverPart(2);s.destroyHoverPart(3);for(int i=0;i<40;i++){++s.ticks;s.updateHovercraft();}put(2);
    crash::bmp(argv[2],strip,720,160);
    auto a=crash::Category0Stage::hoverAttack(s.hovercraftProfile);
    std::cout<<"category=6 type="<<s.rom.type<<" events="<<s.rom.eventCount<<" trigger="<<s.rom.event(0).threshold
             <<" kind="<<s.kind(0)<<" hover_picture="<<s.rom.hovercraftFrame().width<<"x"<<s.rom.hovercraftFrame().height
             <<" parts=4 hp=15/25/"<<(s.hovercraftProfile?16:24)<<"/"<<(s.hovercraftProfile?16:24)
             <<" side="<<a.sideDelay<<"/"<<a.sideBurst<<"/"<<a.sideRest
             <<" cannon="<<a.cannonDelay<<"/"<<a.cannonBurst<<"/"<<a.cannonRest
             <<" launcher="<<a.launcherDelay<<"/"<<a.launcherBurst<<"/"<<a.launcherRest
             <<" state="<<s.hovercraftState<<" parts_left="<<s.hovercraftPartsLeft<<"\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
