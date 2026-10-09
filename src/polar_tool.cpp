#include "category0.hpp"
#include <algorithm>
#include <iostream>
#include <vector>

namespace {
unsigned findKind(const crash::Category0Stage& s,unsigned kind){unsigned i=0;while(i<s.rom.eventCount&&s.kind(i)!=kind)++i;if(i>=s.rom.eventCount)throw std::runtime_error("Polar kind not found");return i;}
void centerOn(crash::Category0Stage& s,unsigned i){
    const auto eb=s.rom.actorBox(s.kind(i)),pb=s.rom.actorBox(0);
    const double dz=(pb.z+pb.d*.5)-(eb.z+eb.d*.5);
    s.scroll=s.rom.eventDepth(i)-dz;auto q=s.project(i);
    const double ecx=q.sx+(eb.x+eb.w*.5)*q.scale,ecy=q.sy+(eb.y+eb.h*.5)*q.scale;
    s.x=std::clamp(ecx-120.0-(pb.x+pb.w*.5),-50.0,50.0);s.jumpY=ecy-110.0-(pb.y+pb.h*.5);
}
}

int main(int argc,char** argv){try{
    if(argc!=3){std::cerr<<"Usage: CrashPolarTool ROM OUTPUT_BMP\n";return 2;}
    crash::GameData data(crash::read(argv[1]));std::vector<uint32_t> strip(720*160,0xff000000);
    auto put=[&](const crash::Category0Stage& s,int panel){auto shot=s.renderLogical(true);for(int y=0;y<160;y++)std::copy_n(shot.begin()+y*240,240,strip.begin()+y*720+panel*240);};

    crash::Category0Stage boost(data,1);unsigned bi=findKind(boost,12);centerOn(boost,bi);boost.handleCollisions();boost.collisions=false;for(int i=0;i<8;i++)boost.step(false,false,false);put(boost,0);
    crash::Category0Stage launcher(data,0);unsigned li=findKind(launcher,22);centerOn(launcher,li);launcher.handleCollisions();launcher.collisions=false;for(int i=0;i<10;i++)launcher.step(false,false,false);put(launcher,1);
    crash::Category0Stage goal(data,0);unsigned gi=findKind(goal,25);centerOn(goal,gi);goal.handleCollisions();put(goal,2);

    crash::bmp(argv[2],strip,720,160);
    std::cout<<"boost kind=12 frames="<<boost.polarBoostFrames<<" hits="<<boost.polarBoostHits
             <<" | launcher kind=22 active="<<launcher.polarLaunchActive<<" hits="<<launcher.polarLauncherHits
             <<" | goal kind=25 finishTimer="<<goal.polarFinishTimer<<" hits="<<goal.polarGoalHits<<"\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
