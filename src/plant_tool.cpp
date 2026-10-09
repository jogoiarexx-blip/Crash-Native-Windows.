#include "entities.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

static void box(std::vector<uint32_t>& p,int w,int h,const crash::Player::SolidBox& b,int cx,int cy,uint32_t color){
    int l=int(std::floor(b[0]))-cx,t=int(std::floor(b[1]))-cy,r=int(std::ceil(b[2]))-cx,bt=int(std::ceil(b[3]))-cy;
    for(int x=l;x<=r;x++){if(x>=0&&x<w&&t>=0&&t<h)p[size_t(t*w+x)]=color;if(x>=0&&x<w&&bt>=0&&bt<h)p[size_t(bt*w+x)]=color;}
    for(int y=t;y<=bt;y++){if(l>=0&&l<w&&y>=0&&y<h)p[size_t(y*w+l)]=color;if(r>=0&&r<w&&y>=0&&y<h)p[size_t(y*w+r)]=color;}
}
static std::vector<uint32_t> snapshot(const crash::GameData& data,const crash::Scene& scene,const crash::Player& p,const crash::EntityWorld& world,const crash::RuntimeEntity& plant){
    int cameraX=std::clamp(int(std::lround(plant.x))-120,0,std::max(0,p.terrain.width()-240));
    int cameraY=std::clamp(int(std::lround(plant.y))-80,0,std::max(0,p.terrain.height()-160));
    auto frame=crash::GameData::viewport(scene,cameraX,cameraY,240,160);
    world.renderLogical(frame,240,160,cameraX,cameraY,true);
    const auto& a=data.banks[0].animations[p.animation];size_t n=p.animationTicks/std::max(1u,a.duration);n=(a.flags&2)?n%a.sequence.size():std::min(n,a.sequence.size()-1);
    auto pf=data.frame(0,a.sequence[n],a.paletteRecord);crash::GameData::blit(frame,240,160,pf,int(std::lround(p.x))-cameraX,int(std::lround(p.y))-cameraY,p.facingLeft);
    box(frame,240,160,world.plantTriggerBox(plant),cameraX,cameraY,0xffffff00u);
    if(plant.attacking)box(frame,240,160,world.plantHitBox(plant),cameraX,cameraY,0xffff40ffu);
    return frame;
}
int main(int argc,char** argv){try{
    if(argc!=3){std::cerr<<"Usage: CrashPlantTool ROM OUTPUT_BMP\n";return 2;}
    crash::GameData data(crash::read(argv[1]));auto scene=data.scene(0);crash::Terrain terrain(data,scene);crash::Player p(data,scene,terrain);crash::EntityWorld world(data,scene);world.bind(p);
    auto plant=std::find_if(world.items.begin(),world.items.end(),[](const crash::RuntimeEntity& e){return e.source.type==crash::EntityWorld::PlantType;});if(plant==world.items.end())throw std::runtime_error("Expected room-0 type-42 plant not found");
    std::vector<uint32_t> strip(720*160,0xff000000);auto put=[&](int panel){auto shot=snapshot(data,scene,p,world,*plant);for(int y=0;y<160;y++)std::copy_n(shot.begin()+y*240,240,strip.begin()+y*720+panel*240);};
    p.x=plant->x-90;p.y=plant->y;put(0);
    p.x=plant->x-20;p.y=plant->y;world.step(p);put(1);
    p.x=plant->x-20;p.y=plant->y;for(int i=0;i<12;i++)world.step(p);put(2);
    crash::bmp(argv[2],strip,720,160);
    auto trigger=world.plantTriggerBox(*plant),hit=world.plantHitBox(*plant);
    std::cout<<"plant_type=42 bank=10 plants="<<world.plantTotal<<" attacks="<<world.plantAttacks<<" trigger="<<(trigger[2]-trigger[0])<<"x"<<(trigger[3]-trigger[1])<<" attack_box="<<(hit[2]-hit[0])<<"x"<<(hit[3]-hit[1])<<"\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
