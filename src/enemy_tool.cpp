#include "entities.hpp"
#include <algorithm>
#include <iostream>
#include <vector>

static std::vector<uint32_t> snapshot(const crash::GameData& data,const crash::Scene& scene,const crash::Player& p,const crash::EntityWorld& world,const crash::RuntimeEntity& focus){
    int cameraX=std::clamp(int(std::lround(focus.x))-120,0,std::max(0,p.terrain.width()-240));
    int cameraY=std::clamp(int(std::lround(focus.y))-80,0,std::max(0,p.terrain.height()-160));
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
        if(argc!=3){std::cerr<<"Usage: CrashEnemyTool ROM OUTPUT_BMP\n";return 2;}
        crash::GameData data(crash::read(argv[1]));
        auto scene=data.scene(0);crash::Terrain terrain(data,scene);crash::Player p(data,scene,terrain);crash::EntityWorld world(data,scene);world.bind(p);
        auto enemy=std::find_if(world.items.begin(),world.items.end(),[](const crash::RuntimeEntity& e){return e.source.type==crash::EntityWorld::EnemyType&&e.patrolRange==40;});
        if(enemy==world.items.end())throw std::runtime_error("Expected room-0 type-40 enemy not found");
        p.x=31;p.y=363;
        std::vector<uint32_t> strip(720*160,0xff000000);
        auto put=[&](int panel){auto shot=snapshot(data,scene,p,world,*enemy);for(int y=0;y<160;y++)std::copy_n(shot.begin()+y*240,240,strip.begin()+y*720+panel*240);};
        put(0);
        for(int i=0;i<24;i++)world.step(p);
        put(1);
        p.x=enemy->x;p.y=enemy->y;p.spinTicks=10;world.step(p);p.spinTicks=0;
        for(int i=0;i<10;i++)world.step(p);
        put(2);
        crash::bmp(argv[2],strip,720,160);
        std::cout<<"level=1 enemy_type=40 bank=13 enemies="<<world.enemyTotal<<" kills="<<world.enemyKills<<" patrol_range="<<enemy->patrolRange<<" exit_markers="<<world.exitMarkers<<"\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
