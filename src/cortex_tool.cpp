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
        if(argc!=3){std::cerr<<"Usage: CrashCortexTool ROM OUTPUT_BMP\n";return 2;}
        crash::GameData data(crash::read(argv[1]));auto scene=data.scene(23);crash::Terrain terrain(data,scene);crash::Player p(data,scene,terrain);crash::EntityWorld w(data,scene);w.bind(p);
        auto boss=std::find_if(w.items.begin(),w.items.end(),[](const crash::RuntimeEntity& e){return e.source.type==crash::EntityWorld::CortexType;});
        if(boss==w.items.end())throw std::runtime_error("Cortex not found");
        p.x=-100;p.y=-100;w.step(p);auto initial=capture(scene,p,w,boss->x,boss->y);
        bool opened=false;for(int i=0;i<1000&&!opened;i++){w.step(p);opened=std::any_of(w.items.begin(),w.items.end(),[](const crash::RuntimeEntity& e){return e.source.type==crash::EntityWorld::TimedPlatformType&&e.controllerMode==1;});}
        if(!opened)throw std::runtime_error("Cortex platforms did not open");
        auto tracking=capture(scene,p,w,140,100);
        auto gem=std::find_if(w.items.begin(),w.items.end(),[](const crash::RuntimeEntity& e){return e.source.type==0x0A;});if(gem==w.items.end())throw std::runtime_error("Cortex gem not found");
        boss->cortex.targetState=5;boss->cortex.targetTimer=1;boss->cortex.targetSteps=0;boss->cortex.targetX=gem->x;boss->cortex.targetY=gem->y;w.step(p);if(gem->controllerMode!=1)throw std::runtime_error("Cortex fast shot missed diagnostic gem");auto hit=capture(scene,p,w,140,100);
        std::vector<uint32_t> strip(720*160,0xff000000);for(int y=0;y<160;y++){std::copy_n(initial.begin()+y*240,240,strip.begin()+y*720);std::copy_n(tracking.begin()+y*240,240,strip.begin()+y*720+240);std::copy_n(hit.begin()+y*240,240,strip.begin()+y*720+480);}crash::bmp(argv[2],strip,720,160);
        std::cout<<"cortex bank="<<w.cortexSet.bank<<" clips="<<w.cortexSet.clips.size()<<" gems="<<w.cortexGemTotal<<" platforms=3 hit="<<boss->cortex.counter<<" native="<<data.nativePlayable(data.roomRoute(23)[0])<<"\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
