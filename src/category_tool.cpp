#include "category0.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>

int main(int argc,char** argv){try{
    unsigned category=0;const char* out=nullptr;
    if(argc==3){out=argv[2];}
    else if(argc==4){category=unsigned(std::stoul(argv[2]));out=argv[3];}
    else{std::cerr<<"Usage: CrashCategoryTool ROM [CATEGORY] OUTPUT_BMP\n";return 2;}
    crash::GameData data(crash::read(argv[1]));crash::Category0Stage stage(data,category);stage.collisions=false;
    std::vector<uint32_t> strip(720*160,0xff000000);
    auto put=[&](int panel){auto shot=stage.renderLogical(true);for(int y=0;y<160;y++)std::copy_n(shot.begin()+y*240,240,strip.begin()+y*720+panel*240);};
    if(category==6){stage.scroll=500;put(0);stage.scroll=544;stage.updateJetpackSpawns();stage.scroll=stage.hovercraftZ-70;for(int i=0;i<45;i++){++stage.ticks;stage.updateHovercraft();}put(1);stage.destroyHoverPart(2);stage.destroyHoverPart(3);for(int i=0;i<40;i++){++stage.ticks;stage.updateHovercraft();}put(2);}else{put(0);if(stage.jetpack()){for(int i=0;i<220;i++)stage.step(false,i<90,i==30,false,i<70);put(1);for(int i=0;i<200;i++)stage.step(i<80,false,i==24,i<90,false,i==55,false);put(2);}else{for(int i=0;i<45;i++)stage.step(false,true,false);put(1);for(int i=0;i<120;i++)stage.step(true,false,true);put(2);}}crash::bmp(out,strip,720,160);
    std::cout<<"category="<<category<<" type="<<stage.rom.type<<" bg="<<stage.rom.bgCols<<"x"<<stage.rom.bgRows<<" bg_frames="<<stage.rom.backgroundFrames<<" events="<<stage.rom.eventCount<<" end="<<stage.rom.routeEndThreshold<<" picture="<<stage.rom.pictureCols<<"x"<<stage.rom.pictureRows<<" sheet="<<stage.rom.spriteAsset.size()<<" rider_frame="<<stage.rom.actorFrameAddress(0,0)<<" steer_x="<<stage.x<<" scroll="<<stage.scroll<<"\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
