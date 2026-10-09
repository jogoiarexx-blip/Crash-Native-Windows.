#include "entities.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>

static std::vector<uint32_t> snapshot(const crash::GameData& data,const crash::Scene& scene,const crash::Player& p,const crash::EntityWorld& world,const crash::RuntimeEntity& focus){
    int cameraX=std::clamp(int(std::lround(focus.x))-120,0,std::max(0,p.terrain.width()-240));
    int cameraY=std::clamp(int(std::lround(focus.y))-90,0,std::max(0,p.terrain.height()-160));
    auto frame=crash::GameData::viewport(scene,cameraX,cameraY,240,160);
    world.renderLogical(frame,240,160,cameraX,cameraY,true);
    const auto& a=data.banks[0].animations[p.animation];
    size_t n=p.animationTicks/std::max(1u,a.duration);n=(a.flags&2)?n%a.sequence.size():std::min(n,a.sequence.size()-1);
    auto f=data.frame(0,a.sequence[n],a.paletteRecord);
    crash::GameData::blit(frame,240,160,f,int(std::lround(p.x))-cameraX,int(std::lround(p.y))-cameraY,p.facingLeft);
    return frame;
}

int main(int argc,char** argv){
    try{
        if(argc!=3){std::cerr<<"Usage: CrashMoverTool ROM OUTPUT_BMP\n";return 2;}
        crash::GameData data(crash::read(argv[1]));auto scene=data.scene(2);crash::Terrain terrain(data,scene);crash::Player p(data,scene,terrain);crash::EntityWorld world(data,scene);world.bind(p);
        auto mover=std::find_if(world.items.begin(),world.items.end(),[](const crash::RuntimeEntity& e){return e.source.type==crash::EntityWorld::MovingPlatformType&&e.mover.inputDistX==40;});
        if(mover==world.items.end())throw std::runtime_error("Expected level-2 mover not found");
        auto box=world.entityBox(*mover);p.x=mover->x;p.y=box[1]-p.by-p.bh;p.grounded=true;
        std::vector<uint32_t> strip(720*160,0xff000000);
        for(int panel=0;panel<3;panel++){
            int target=panel==0?0:panel==1?60:150;
            static int simulated=0;while(simulated<target){p.step({});world.step(p);++simulated;}
            auto s=snapshot(data,scene,p,world,*mover);for(int y=0;y<160;y++)std::copy_n(s.begin()+y*240,240,strip.begin()+y*720+panel*240);
        }
        crash::bmp(argv[2],strip,720,160);
        std::cout<<"level=2 mover_type=79 distX="<<mover->mover.inputDistX<<" rangeX="<<mover->mover.rangeX<<" carriedTicks="<<world.carriedTicks<<" finalX="<<mover->x<<"\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
