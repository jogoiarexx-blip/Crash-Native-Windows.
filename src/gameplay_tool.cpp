#include "entities.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>
int main(int argc,char** argv){try{
 if(argc<3||argc>4){std::cerr<<"Usage: CrashGameplayTool ROM OUTPUT_DIRECTORY [LEVEL]\n";return 2;}
 unsigned level=argc==4?unsigned(std::stoul(argv[3])):0;crash::GameData data(crash::read(argv[1]));auto scene=data.scene(level);crash::Terrain terrain(data,scene);crash::Player p(data,scene,terrain);crash::EntityWorld world(data,scene);world.bind(p);
 std::filesystem::path out=argv[2];std::filesystem::create_directories(out);std::ofstream log(out/"trajectory.csv");log<<"tick,x,y,vx,vy,grounded,spin,wumpa,crates,checkpoint,aku,hazards,camera_x,camera_y\n";
 for(int i=0;i<360;i++){
  auto cam=p.camera();log<<i<<','<<p.x<<','<<p.y<<','<<p.vx<<','<<p.vy<<','<<p.grounded<<','<<p.spinning()<<','<<world.wumpaCollected<<','<<world.crateBroken<<','<<world.checkpointHits<<','<<world.akuMasks<<','<<world.hazardHits<<','<<cam.first<<','<<cam.second<<'\n';
  if(i%3==0){
   std::ostringstream name;name<<"frame-"<<std::setw(3)<<std::setfill('0')<<i/3<<".bmp";
   auto frame=crash::GameData::viewport(scene,cam.first,cam.second,240,160);world.renderLogical(frame,240,160,cam.first,cam.second,true);
   const auto& a=data.banks[0].animations[p.animation];size_t n=p.animationTicks/std::max(1u,a.duration);n=(a.flags&2)?n%a.sequence.size():std::min(n,a.sequence.size()-1);auto f=data.frame(0,a.sequence[n],a.paletteRecord);crash::GameData::blit(frame,240,160,f,int(std::lround(p.x))-cam.first,int(std::lround(p.y))-cam.second,p.facingLeft);
   crash::bmp((out/name.str()).string(),frame,240,160);
  }
  p.step({false,i>=15,(i>=65&&i<84) || (i>=220&&i<239),i==28||i==105||i==352});world.step(p);
 }
 std::cout<<"360 native ticks / 120 rendered frames written. Wumpa "<<world.wumpaCollected<<'/'<<world.wumpaTotal<<", caixas "<<world.crateBroken<<'/'<<world.crateTotal<<". Movers "<<world.moverTotal<<", carregado "<<world.carriedTicks<<" ticks. Experimental controller, not GBA emulation.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
