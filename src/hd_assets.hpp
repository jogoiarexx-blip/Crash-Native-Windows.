#pragma once
#include "png.hpp"
#include "entities.hpp"
#include <filesystem>
#include <sstream>
#include <memory>
#include <set>
#include <tuple>
namespace crash {
struct SpriteKey {unsigned bank,frame,palette;bool operator<(const SpriteKey& b)const{return std::tie(bank,frame,palette)<std::tie(b.bank,b.frame,b.palette);}};
struct Replacement {std::shared_ptr<Image> image;int sx,sy,sw,sh,ox,oy,lw,lh;};
inline std::string trim(std::string v){size_t first=v.find_first_not_of(" \t\r\n"),last=v.find_last_not_of(" \t\r\n");return first==std::string::npos?"":v.substr(first,last-first+1);}
inline int integer(const std::string& value){size_t n=0;int result=std::stoi(value,&n);if(n!=value.size())throw std::runtime_error("Expected integer");return result;}
struct DisplaySettings {int scale=4;bool external=true;};
inline DisplaySettings settings(const std::filesystem::path& path){DisplaySettings s;std::ifstream f(path);std::string line;while(std::getline(f,line)){line=trim(line);if(line.empty()||line[0]=='#')continue;auto n=line.find('=');if(n==std::string::npos)continue;auto key=trim(line.substr(0,n)),value=trim(line.substr(n+1));if(key=="render_scale"&&(value=="1"||value=="4"||value=="6"))s.scale=integer(value);if(key=="external_sprites"&&(value=="0"||value=="1"))s.external=value=="1";}return s;}
class ExternalSprites {
 public:
 std::map<SpriteKey,Replacement> sprites;std::vector<std::string> errors;size_t textureBytes=0;
 void load(const GameData& data,const std::filesystem::path& directory){
  sprites.clear();errors.clear();textureBytes=0;std::ifstream f(directory/"manifest.csv");if(!f){errors.push_back("manifest.csv ausente; usando ROM");return;}
  std::map<std::string,std::shared_ptr<Image>> cache;std::set<std::string> failed;std::string line;unsigned row=0;
  while(std::getline(f,line)){
   ++row;line=trim(line);if(line.empty()||line[0]=='#')continue;
   try{
    std::vector<std::string> columns;std::stringstream stream(line);std::string value;while(std::getline(stream,value,','))columns.push_back(trim(value));
    if(!columns.empty()&&columns[0]=="bank")continue;
    if(columns.size()!=12)throw std::runtime_error("Expected 12 CSV columns");
    int bank=integer(columns[0]),frame=integer(columns[1]),palette=integer(columns[2]);
    if(bank<0||bank>=int(data.banks.size())||frame<0||frame>=int(data.banks[size_t(bank)].frames.size())||palette<0||palette>=125)throw std::runtime_error("Sprite key out of ROM bounds");
    std::filesystem::path relative(columns[3]);if(relative.empty()||relative.is_absolute()||relative.has_root_name()||columns[3].find(':')!=std::string::npos||columns[3].find('\\')!=std::string::npos)throw std::runtime_error("Use a relative PNG path with forward slashes");
    for(const auto& part:relative)if(part=="..")throw std::runtime_error("Parent path forbidden");
    if(relative.extension()!=".png")throw std::runtime_error("Only .png accepted");
    Replacement r{};r.sx=integer(columns[4]);r.sy=integer(columns[5]);r.sw=integer(columns[6]);r.sh=integer(columns[7]);r.ox=integer(columns[8]);r.oy=integer(columns[9]);r.lw=integer(columns[10]);r.lh=integer(columns[11]);
    if(r.sx<0||r.sy<0||r.sx>8192||r.sy>8192||r.sw<=0||r.sh<=0||r.sw>8192||r.sh>8192||r.lw<=0||r.lh<=0||r.lw>512||r.lh>512||r.ox< -512||r.ox>512||r.oy< -512||r.oy>512)throw std::runtime_error("Invalid crop or logical bounds");
    SpriteKey key{unsigned(bank),unsigned(frame),unsigned(palette)};if(sprites.count(key))throw std::runtime_error("Duplicate sprite key");
    if(failed.count(columns[3]))continue;
    auto found=cache.find(columns[3]);
    if(found==cache.end()){
     try{auto image=std::make_shared<Image>(loadPNG((directory/relative).string()));size_t bytes=image->pixels.size()*4;if(bytes>128*1024*1024-textureBytes)throw std::runtime_error("Texture budget exceeded");textureBytes+=bytes;found=cache.emplace(columns[3],image).first;}
     catch(const std::exception& e){failed.insert(columns[3]);throw std::runtime_error(columns[3]+": "+e.what());}
    }
    r.image=found->second;if(r.sx+r.sw>r.image->width||r.sy+r.sh>r.image->height)throw std::runtime_error("Crop exceeds PNG bounds");
    sprites.emplace(key,std::move(r));
   }catch(const std::exception& e){if(errors.size()<100)errors.push_back("linha "+std::to_string(row)+": "+e.what());}
  }
 }
 const Replacement* get(unsigned bank,unsigned frame,unsigned palette)const{auto found=sprites.find({bank,frame,palette});return found==sprites.end()?nullptr:&found->second;}
 void report(const std::filesystem::path& path)const{std::ofstream f(path);f<<"Sprites externos carregados: "<<sprites.size()<<"\nMemoria de texturas: "<<textureBytes<<" bytes\n";for(const auto& e:errors)f<<e<<'\n';}
};
inline uint32_t blend(uint32_t source,uint32_t dest){unsigned a=source>>24;if(a==255)return source;if(!a)return dest;unsigned result=0xff000000;for(unsigned shift:{0u,8u,16u})result|=(((((source>>shift)&255)*a+((dest>>shift)&255)*(255-a)+127)/255)<<shift);return result;}
inline void drawReplacement(std::vector<uint32_t>& pixels,int w,int h,const Replacement& r,int scale,int anchorX,int anchorY,bool mirror){
 int left=anchorX+(mirror?-r.ox-r.lw:r.ox)*scale,top=anchorY+r.oy*scale;
 int width=r.lw*scale,height=r.lh*scale;
 for(int yy=std::max(0,top);yy<std::min(h,top+height);yy++)for(int xx=std::max(0,left);xx<std::min(w,left+width);xx++){
  int dx=xx-left,dy=yy-top;if(mirror)dx=width-1-dx;int sx=r.sx+int(int64_t(dx)*r.sw/width),sy=r.sy+int(int64_t(dy)*r.sh/height);
  auto p=r.image->pixels[size_t(sy*r.image->width+sx)];auto& target=pixels[size_t(yy*w+xx)];target=blend(p,target);
 }
}
inline std::vector<uint32_t> renderHD(const GameData& data,const Scene& scene,const Player& p,const ExternalSprites& external,int scale,bool enabled=true,bool debug=false,const EntityWorld* world=nullptr){
 if(scale!=1&&scale!=4&&scale!=6)throw std::runtime_error("Supported render scales: 1, 4, 6");
 int w=240*scale,h=160*scale;auto cam=p.camera();auto base=GameData::viewport(scene,cam.first,cam.second,240,160,debug);if(world)world->renderLogical(base,240,160,cam.first,cam.second,true);
 std::vector<uint32_t> out(size_t(w)*h);
 for(int yy=0;yy<160;yy++){size_t row=size_t(yy*scale*w);for(int xx=0;xx<240;xx++)std::fill_n(out.begin()+row+xx*scale,scale,base[size_t(yy*240+xx)]);for(int repeat=1;repeat<scale;repeat++)std::copy_n(out.begin()+row,w,out.begin()+row+size_t(repeat*w));}
 const auto& a=data.banks[p.spriteBank].animations[p.animation];size_t n=p.animationTicks/std::max(1u,a.duration);n=(a.flags&2)?n%a.sequence.size():std::min(n,a.sequence.size()-1);unsigned index=a.sequence[n];
 int anchorX=(int(std::lround(p.x))-cam.first)*scale,anchorY=(int(std::lround(p.y))-cam.second)*scale;
 const auto* replacement=enabled?external.get(p.spriteBank,index,a.paletteRecord):nullptr;
 if(replacement)drawReplacement(out,w,h,*replacement,scale,anchorX,anchorY,p.facingLeft);
 else{auto f=data.frame(p.spriteBank,index,a.paletteRecord);Replacement r{};r.image=std::make_shared<Image>();r.image->width=f.width;r.image->height=f.height;r.image->pixels=std::move(f.pixels);r.sw=r.lw=f.width;r.sh=r.lh=f.height;r.ox=f.x;r.oy=f.y;drawReplacement(out,w,h,r,scale,anchorX,anchorY,p.facingLeft);}
 if(debug){auto body=p.bodyBox();int left=(int(body[0])-cam.first)*scale,top=(int(body[1])-cam.second)*scale,right=(int(body[2])-cam.first)*scale,bottom=(int(body[3])-cam.second)*scale;if(right>left&&bottom>top)for(int yy=std::max(0,top);yy<std::min(h,bottom+scale);yy++)for(int xx=std::max(0,left);xx<std::min(w,right+scale);xx++)if(xx<left+scale||xx>=right||yy<top+scale||yy>=bottom)out[size_t(yy*w+xx)]=0xffffff00;if(p.basicAttack()){auto a=p.attackBox();int l=(int(a[0])-cam.first)*scale,t=(int(a[1])-cam.second)*scale,r=(int(a[2])-cam.first)*scale,b=(int(a[3])-cam.second)*scale;if(r>l&&b>t)for(int yy=std::max(0,t);yy<std::min(h,b+scale);yy++)for(int xx=std::max(0,l);xx<std::min(w,r+scale);xx++)if(xx<l+scale||xx>=r||yy<t+scale||yy>=b)out[size_t(yy*w+xx)]=0xffff40ff;}}
 return out;
}
}
