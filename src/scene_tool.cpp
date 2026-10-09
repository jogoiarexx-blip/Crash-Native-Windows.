#include "game_data.hpp"
#include <iostream>
int main(int argc,char** argv){try{if(argc<3){std::cout<<"Crash scene tool v0.2.0: validate ROM | frame ROM BANK INDEX PALETTE OUT.bmp | scene ROM X Y OUT.bmp | map ROM SLOT OUT.bin\n";return 0;}crash::GameData g(crash::read(argv[2]));std::string cmd=argv[1];
 if(cmd=="validate"){size_t frames=0,anims=0;for(unsigned i=0;i<g.banks.size();i++){frames+=g.banks[i].frames.size();anims+=g.banks[i].animations.size();for(auto& a:g.banks[i].animations)for(auto f:a.sequence)g.frame(i,f,a.paletteRecord);}auto scene=g.scene();crash::GameData::viewport(scene,0,240);std::cout<<g.banks.size()<<" banks / "<<anims<<" animations / "<<frames<<" frames / first room decoded\n";}
 else if(cmd=="digests"){
  for(unsigned i=0;i<g.banks.size();i++){std::map<unsigned,bool> palettes;for(auto& a:g.banks[i].animations)palettes[a.paletteRecord]=true;
   for(auto pair:palettes)for(unsigned j=0;j<g.banks[i].frames.size();j++){auto f=g.frame(i,j,pair.first);uint32_t hash=2166136261u;for(auto px:f.pixels)for(unsigned sh=0;sh<32;sh+=8){hash^=(px>>sh)&255;hash*=16777619u;}std::cout<<i<<","<<j<<","<<pair.first<<","<<f.width<<","<<f.height<<","<<f.x<<","<<f.y<<","<<hash<<"\n";}
  }
 }
 else if(cmd=="frame" && argc==7){auto f=g.frame(unsigned(std::stoul(argv[3])),unsigned(std::stoul(argv[4])),unsigned(std::stoul(argv[5])));crash::bmp(argv[6],f.pixels,f.width,f.height);std::cout<<f.x<<" "<<f.y<<"\n";}
 else if(cmd=="scene" && argc==6){auto s=g.scene();auto px=crash::GameData::viewport(s,std::stoi(argv[3]),std::stoi(argv[4]));crash::bmp(argv[5],px,240,160);}
 else if(cmd=="preview" && argc==4){auto s=g.scene();auto px=crash::GameData::viewport(s,0,240);auto& a=g.banks[0].animations[47];auto f=g.frame(0,a.sequence[0],a.paletteRecord);crash::GameData::blit(px,240,160,f,100,116);crash::bmp(argv[3],px,240,160);}
 else if(cmd=="map" && argc==5){auto s=g.scene();unsigned slot=unsigned(std::stoul(argv[3]));for(auto& l:s.layers)if(l.slot==slot){std::ofstream f(argv[4],std::ios::binary);for(auto v:l.cells){f.put(char(v));f.put(char(v>>8));}if(!f)throw std::runtime_error("map write");return 0;}throw std::runtime_error("missing layer");}
 else throw std::runtime_error("invalid command");
 return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
