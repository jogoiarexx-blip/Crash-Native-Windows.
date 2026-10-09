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
        if(argc!=3){std::cerr<<"Usage: CrashDingodileTool ROM OUTPUT_BMP\n";return 2;}
        crash::GameData data(crash::read(argv[1]));auto scene=data.scene(20);crash::Terrain terrain(data,scene);crash::Player p(data,scene,terrain);crash::EntityWorld w(data,scene);w.bind(p);
        auto boss=std::find_if(w.items.begin(),w.items.end(),[](const crash::RuntimeEntity& e){return e.source.type==crash::EntityWorld::DingodileType;});
        if(boss==w.items.end())throw std::runtime_error("Dingodile not found");
        w.step(p);auto initial=capture(scene,p,w,boss->x,boss->y);
        p.placeAt(25,25);boss->dingodile.state=6;boss->dingodile.animation=4;boss->dingodile.animationTicks=39;boss->childSpawned=false;w.step(p);
        auto rocket=std::find_if(w.childEffects.begin(),w.childEffects.end(),[](const crash::EnemyChildEffect& fx){return fx.kind==crash::EnemyChildEffect::DingodileRocket;});
        if(rocket==w.childEffects.end())throw std::runtime_error("Dingodile rocket did not spawn");
        auto attack=capture(scene,p,w,boss->x,boss->y);
        std::vector<uint32_t> strip(480*160,0xff000000);for(int y=0;y<160;y++){std::copy_n(initial.begin()+y*240,240,strip.begin()+y*480);std::copy_n(attack.begin()+y*240,240,strip.begin()+y*480+240);}crash::bmp(argv[2],strip,480,160);
        std::cout<<"dingodile bank="<<w.dingodileSet.bank<<" clips="<<w.dingodileSet.clips.size()<<" state="<<boss->dingodile.state<<" rocket_bank="<<rocket->bank<<" rocket_anim="<<rocket->animation<<" native="<<data.nativePlayable(data.roomRoute(20)[0])<<"\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
