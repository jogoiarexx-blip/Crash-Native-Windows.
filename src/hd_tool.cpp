#include "hd_assets.hpp"
#include <iostream>
int main(int argc,char** argv){try{
 if(argc<4||argc>5){std::cerr<<"Usage: CrashHDTool ROM SCALE OUTPUT.bmp [TICKS]\n";return 2;}
 int scale=crash::integer(argv[2]),ticks=argc==5?crash::integer(argv[4]):0;if(ticks<0||ticks>36000)throw std::runtime_error("Ticks out of bounds");
 crash::GameData data(crash::read(argv[1]));auto scene=data.scene();crash::Terrain terrain(data,scene);crash::Player p(data,scene,terrain);crash::EntityWorld world(data,scene);world.bind(p);crash::ExternalSprites external;external.load(data,"assets/overrides");
 for(int i=0;i<ticks;i++){p.step({false,true,i>=65&&i<84,i==105});world.step(p);}
 auto pixels=crash::renderHD(data,scene,p,external,scale,true,false,&world);crash::bmp(argv[3],pixels,240*scale,160*scale);
 std::cout<<240*scale<<"x"<<160*scale<<" / "<<external.sprites.size()<<" PNG mappings / "<<external.errors.size()<<" asset errors / Wumpa "<<world.wumpaCollected<<'/'<<world.wumpaTotal<<"\n";return external.errors.empty()?0:1;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
