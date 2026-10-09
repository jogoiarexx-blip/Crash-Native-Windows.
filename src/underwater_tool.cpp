#include "entities.hpp"
#include <algorithm>
#include <iostream>

static std::vector<uint32_t> panel(const crash::GameData& data,const crash::Scene& scene,const crash::Player& p,const crash::EntityWorld& world){
    auto cam=p.camera();auto out=crash::GameData::viewport(scene,cam.first,cam.second,240,160,false);world.renderLogical(out,240,160,cam.first,cam.second,true);
    const auto& a=data.banks[p.spriteBank].animations[p.animation];size_t n=p.animationTicks/std::max(1u,a.duration);n=(a.flags&2)?n%a.sequence.size():std::min(n,a.sequence.size()-1);auto f=data.frame(p.spriteBank,a.sequence[n],a.paletteRecord);crash::GameData::blit(out,240,160,f,int(std::lround(p.x))-cam.first,int(std::lround(p.y))-cam.second,p.facingLeft);return out;
}

int main(int argc,char** argv){try{
    if(argc<3){std::cerr<<"Usage: CrashUnderwaterTool ROM OUTPUT_BMP\n";return 2;}
    crash::GameData data(crash::read(argv[1]));auto scene=data.scene(1);crash::Terrain terrain(data,scene);crash::Player p(data,scene,terrain);crash::EntityWorld world(data,scene);world.bind(p);
    std::vector<uint32_t> strip(720*160,0xff101018);auto put=[&](int slot){auto q=panel(data,scene,p,world);for(int y=0;y<160;y++)std::copy_n(q.begin()+size_t(y*240),240,strip.begin()+size_t(y*720+slot*240));};
    put(0);for(int i=0;i<90;i++){p.step({false,true,false,false,i>30&&i<55,i>=55&&i<75});world.step(p);}put(1);
    auto enemy=std::find_if(world.items.begin(),world.items.end(),[](const crash::RuntimeEntity& e){return e.active&&e.source.type==crash::EntityWorld::PufferfishType;});if(enemy!=world.items.end()){p.placeAt(enemy->x-45,enemy->y);for(int i=0;i<20;i++){p.step({false,true,false,false,false,false});world.step(p);}}put(2);
    crash::bmp(argv[2],strip,720,160);std::cout<<"underwater room: kind="<<scene.kind<<" enemies="<<world.enemyTotal<<" puffer=7 shark=8 moray=5 sea-mine=7 seaweed=8\n";
    for(unsigned level:{1u,8u,12u,16u}){auto s=data.scene(level);crash::Terrain t(data,s);crash::Player q(data,s,t);crash::EntityWorld w(data,s);std::cout<<"level="<<level<<" kind="<<s.kind<<" enemies="<<w.enemyTotal<<" native="<<data.nativePlayable(data.roomRoute(level)[0])<<"\n";}
    std::cout<<"dingodile-kind1-native="<<data.nativePlayable(data.roomRoute(20)[0])<<"\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
