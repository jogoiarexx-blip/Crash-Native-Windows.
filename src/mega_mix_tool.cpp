#include "entities.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

static std::vector<uint32_t> capture(const crash::Scene& scene,const crash::Player& p,const crash::EntityWorld& w,double x,double y){
    int cx=std::clamp(int(std::lround(x))-120,0,std::max(0,p.terrain.width()-240));
    int cy=std::clamp(int(std::lround(y))-80,0,std::max(0,p.terrain.height()-160));
    auto frame=crash::GameData::viewport(scene,cx,cy,240,160);w.renderLogical(frame,240,160,cx,cy,false);return frame;
}

int main(int argc,char** argv){
    try{
        if(argc!=3){std::cerr<<"Usage: CrashMegaMixTool ROM OUTPUT_BMP\n";return 2;}
        crash::GameData data(crash::read(argv[1]));auto scene=data.scene(24);crash::Terrain terrain(data,scene);crash::Player p(data,scene,terrain);crash::EntityWorld w(data,scene);w.bind(p);
        auto boss=std::find_if(w.items.begin(),w.items.end(),[](const crash::RuntimeEntity& e){return e.source.type==crash::EntityWorld::MegaMixType;});
        if(boss==w.items.end())throw std::runtime_error("Mega-Mix not found");
        p.placeAt(boss->x+220,boss->y);auto initial=capture(scene,p,w,boss->x,boss->y);for(int i=0;i<45;i++)w.step(p);auto chase=capture(scene,p,w,boss->x,boss->y);
        auto crate=std::find_if(w.items.begin(),w.items.end(),[](const crash::RuntimeEntity& e){return e.active&&e.source.type==21;});if(crate==w.items.end())throw std::runtime_error("Mega-Mix diagnostic crate not found");crate->x=boss->x+20;crate->y=boss->y;w.step(p);if(crate->active)throw std::runtime_error("Mega-Mix did not break diagnostic crate");
        p.placeAt(boss->x+20,boss->y);w.step(p);if(boss->megaMix.state!=2)throw std::runtime_error("Mega-Mix did not enter grab state");auto grab=capture(scene,p,w,boss->x,boss->y);
        std::vector<uint32_t> strip(720*160,0xff000000);for(int y=0;y<160;y++){std::copy_n(initial.begin()+y*240,240,strip.begin()+y*720);std::copy_n(chase.begin()+y*240,240,strip.begin()+y*720+240);std::copy_n(grab.begin()+y*240,240,strip.begin()+y*720+480);}crash::bmp(argv[2],strip,720,160);
        std::cout<<"mega_mix bank="<<w.megaMixSet.bank<<" clips="<<w.megaMixSet.clips.size()<<" state="<<boss->megaMix.state<<" motion="<<boss->megaMix.motionMode<<" vx_q8="<<boss->megaMix.vxQ8<<" target_q8="<<boss->megaMix.targetVxQ8<<" crates_destroyed="<<w.megaMixCratesDestroyed<<" exit_auto="<<w.canAutoExit()<<" native="<<data.nativePlayable(data.roomRoute(24)[0])<<"\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
