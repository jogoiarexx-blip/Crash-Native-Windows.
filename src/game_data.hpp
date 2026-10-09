#pragma once
#include "rom.hpp"
#include <algorithm>
#include <array>
#include <map>
#include <optional>
namespace crash {
constexpr uint32_t Base=0x08000000;
struct RomData {
 Bytes bytes;
 explicit RomData(Bytes b):bytes(std::move(b)){validate(bytes);}
 size_t offset(uint32_t addr,size_t n=1)const{if(addr<Base || size_t(addr-Base)>bytes.size() || n>bytes.size()-size_t(addr-Base))throw std::runtime_error("ROM pointer bounds");return size_t(addr-Base);}
 uint8_t b(uint32_t a)const{return bytes[offset(a)];}
 uint16_t h(uint32_t a)const{size_t p=offset(a,2);return uint16_t(bytes[p]|(bytes[p+1]<<8));}
 int16_t sh(uint32_t a)const{return int16_t(h(a));}
 uint32_t w(uint32_t a)const{return u32(bytes,offset(a,4));}
 Bytes tagged(uint32_t a)const{size_t p=offset(a,4);uint32_t tag=w(a);if((tag&255)==0){size_t n=tag>>8;offset(a+4,n);return Bytes(bytes.begin()+p+4,bytes.begin()+p+4+n);}return lz10(bytes,p,8*1024*1024).bytes;}
};
struct Animation {uint32_t address;unsigned duration,flags,paletteRecord;std::vector<uint16_t> sequence;};
struct Bank {std::vector<uint32_t> frames;std::vector<Animation> animations;};
struct Frame {int width=1,height=1,x=0,y=0;std::vector<uint32_t> pixels{0};};
struct Layer {unsigned slot,priority,gw,gh,wt,ht;int32_t sx,sy;Bytes tiles;std::vector<uint16_t> cells;};
struct Scene {uint32_t record=0,descriptor=0,palette=0;int kind=0;std::array<uint32_t,256> colors{};std::vector<Layer> layers;};
struct RoomRouteEntry {unsigned level=0,slot=0;uint32_t record=0,descriptor=0;int kind=0;uint16_t category=0,flags=0;bool tileRoom()const{return descriptor!=0&&kind!=3;}};
enum class ExtraRoomKind : unsigned { Bonus=0, GemPath=1 };
struct Entity {unsigned id=0,type=0,x=0,y=0,param=0,column=0;};
struct EntityLink {unsigned from=0,to=0;};
inline uint32_t rgb555(uint16_t c){auto ex=[](unsigned v){return v*255/31;};return 0xff000000|(ex(c&31)<<16)|(ex((c>>5)&31)<<8)|ex((c>>10)&31);}
class GameData {
 public:
 RomData rom;
 // USA ACQE anchors independently discovered and validated against the ROM.
 static constexpr uint32_t Table=0x084a3624,Dimensions=0x0816ae20,Levels=0x0816c3ac;
 std::vector<Bank> banks;
 explicit GameData(Bytes b):rom(std::move(b)){
  if(rom.w(Table)!=Table+16 || rom.h(Table+12)!=56 || rom.h(Table+14)!=125 || rom.w(Levels)!=1 || rom.w(Levels+8)!=355)throw std::runtime_error("Unsupported ROM revision");
  for(unsigned i=0;i<56;i++){
   Bank bank;uint32_t root=rom.w(Table)+12*i,frames=rom.w(root+4),first=Base+unsigned(rom.bytes.size()),p=frames;
   while(p<first){uint32_t f=rom.w(p);rom.offset(f,12);first=std::min(first,f);bank.frames.push_back(f);p+=4;if(bank.frames.size()>4096)throw std::runtime_error("Frame table count");}
   if(p!=first || bank.frames.empty())throw std::runtime_error("Frame table layout");
   unsigned n=rom.h(root+10);if(n>256)throw std::runtime_error("Animation count");
   for(unsigned j=0;j<n;j++){
    uint32_t a=rom.w(root)+28*j;Animation anim{a,rom.b(a+21),rom.b(a+23),rom.b(a+20),{}};unsigned count=rom.b(a+22);
    if(!count || anim.paletteRecord>=125)throw std::runtime_error("Animation bounds");
    for(unsigned k=0;k<count;k++){uint16_t f=rom.h(rom.w(a)+2*k);if(f>=bank.frames.size())throw std::runtime_error("Frame index bounds");anim.sequence.push_back(f);}bank.animations.push_back(std::move(anim));
   }banks.push_back(std::move(bank));
  }
 }
 Frame frame(unsigned bank,unsigned index,unsigned paletteRecord)const{
  const uint32_t a=banks.at(bank).frames.at(index),pos=rom.w(a),ids=rom.w(a+4),packed=rom.w(a+8);unsigned count=packed>>24;
  if(!count)return {};
  struct Piece {int x,y,w,h;uint32_t src;};std::vector<Piece> pieces;uint32_t src=rom.w(Table+4)+(packed&0xffffff);int x0=10000,y0=10000,x1=-10000,y1=-10000;
  for(unsigned i=0;i<count;i++){
   unsigned id=rom.b(ids+i)&15;if(id>=12)throw std::runtime_error("Sprite piece shape");int x=rom.sh(pos+4*i),y=rom.sh(pos+4*i+2),w=rom.b(Dimensions+id),h=rom.b(Dimensions+12+id);rom.offset(src,size_t(w*h/2));
   pieces.push_back({x,y,w,h,src});src+=unsigned(w*h/2);x0=std::min(x0,x);y0=std::min(y0,y);x1=std::max(x1,x+w);y1=std::max(y1,y+h);
  }
  if(x1-x0>512 || y1-y0>512 || x1<=x0 || y1<=y0)throw std::runtime_error("Sprite extent");
  Frame out;out.width=x1-x0;out.height=y1-y0;out.x=x0;out.y=y0;out.pixels.assign(size_t(out.width*out.height),0);
  std::array<uint32_t,16> colors{};uint32_t palette=rom.w(Table+8)+32*paletteRecord;for(unsigned i=0;i<16;i++)colors[i]=rgb555(rom.h(palette+2*i));
  for(auto& piece:pieces){
   for(int y=0;y<piece.h;y++){
    for(int x=0;x<piece.w;x++){
     unsigned tile=unsigned(y/8*(piece.w/8)+x/8);unsigned v=(rom.b(piece.src+tile*32+unsigned(y%8*4+x%8/2))>>((x&1)*4))&15;
     if(v)out.pixels[size_t((piece.y-y0+y)*out.width+piece.x-x0+x)]=colors[v];
    }
   }
  }
  return out;
 }
 static std::vector<uint16_t> chunk(const Bytes& asset,size_t p){
  std::vector<uint16_t> out;out.reserve(128);auto take=[&](){if(p>asset.size() || asset.size()-p<2)throw std::runtime_error("Chunk truncated");uint16_t v=uint16_t(asset[p]|(asset[p+1]<<8));p+=2;return v;};
  while(out.size()<128){uint16_t token=take();unsigned n=token&255;if(!n || out.size()+n>128)throw std::runtime_error("Chunk run bounds");
   if(token&0x8000){uint16_t v=take();out.insert(out.end(),n,v);}
   else if(token&0x4000){if(n<3)throw std::runtime_error("Delta count");uint16_t v=take();out.push_back(v);unsigned remaining=n-1;while(remaining){uint16_t pair=take();for(unsigned j=0;j<2 && remaining;j++,remaining--){v=uint16_t(int(v)+int(int8_t((pair>>(8*j))&255)));out.push_back(v);}}}
   else for(unsigned i=0;i<n;i++)out.push_back(take());
  }return out;
 }
 bool roomHasEntityType(const RoomRouteEntry& room,unsigned wanted)const{
  if(!room.tileRoom())return false;
  uint32_t list=rom.w(room.descriptor+28);unsigned count=rom.h(list),groupCount=rom.h(list+2);uint32_t groups=rom.w(list+4);
  if(count>4096||groupCount>1024)return false;
  for(unsigned g=0;g<groupCount;g++){uint32_t group=groups+8*g,n=rom.h(group+2),item=rom.w(group+4);for(unsigned i=0;i<n;i++,item+=8)if(rom.h(item)==wanted)return true;}
  return false;
 }
 bool nativePlayable(const RoomRouteEntry& room)const{
  if(!room.tileRoom())return false;
  // Native room control modes implemented by the PC runtime:
  // kind 0 = normal Crash (0x00), kind 1 = underwater Crash (0x02),
  // kind 2 = hover vehicle InputCtrl (0x04). All three now execute natively.
  if(room.kind==0)return roomHasEntityType(room,0x00);
  if(room.kind==1)return roomHasEntityType(room,0x02);
  if(room.kind==2)return roomHasEntityType(room,0x04);
  return false;
 }
 std::vector<RoomRouteEntry> roomRoute(unsigned level)const{
  if(level>=25)throw std::runtime_error("Level id");
  uint32_t list=rom.w(Levels+36*level+32);int32_t count=int32_t(rom.w(list));uint32_t rooms=rom.w(list+4);
  if(count<0||count>16)throw std::runtime_error("Room route count");
  std::vector<RoomRouteEntry> out;out.reserve(unsigned(count));
  for(int32_t i=0;i<count;i++){uint32_t record=rom.w(rooms+4*unsigned(i));rom.offset(record,20);out.push_back({level,unsigned(i),record,rom.w(record+4),int32_t(rom.w(record+8)),rom.h(record+16),rom.h(record+18)});}
  return out;
 }
 std::optional<RoomRouteEntry> extraRoom(unsigned level,ExtraRoomKind kind)const{
  if(level>=25)throw std::runtime_error("Level id");
  uint32_t list=rom.w(Levels+36*level+32);
  uint32_t record=rom.w(list+(kind==ExtraRoomKind::Bonus?8:12));
  if(!record)return std::nullopt;
  rom.offset(record,20);
  unsigned slot=kind==ExtraRoomKind::Bonus?100u:101u;
  return RoomRouteEntry{level,slot,record,rom.w(record+4),int32_t(rom.w(record+8)),rom.h(record+16),rom.h(record+18)};
 }
 Scene sceneFromRoom(const RoomRouteEntry& room)const{
  if(room.level>=25||!room.record)throw std::runtime_error("Room route entry");
  Scene scene{};scene.record=room.record;scene.palette=rom.w(scene.record);scene.descriptor=rom.w(scene.record+4);scene.kind=room.kind;
  if(!scene.descriptor||room.kind==3)throw std::runtime_error("This stage uses actor categories; not a tile room");
  for(unsigned i=0;i<256;i++)scene.colors[i]=rgb555(rom.h(scene.palette+2*i));
  uint32_t assetPtr=rom.w(scene.descriptor+20);Bytes asset;
  if(rom.b(scene.descriptor+24))asset=rom.tagged(assetPtr);else{size_t p=rom.offset(assetPtr);asset=Bytes(rom.bytes.begin()+p,rom.bytes.end());}
  for(unsigned slot=0;slot<5;slot++){
   uint32_t a=rom.w(scene.descriptor+4*slot);if(!a)continue;
   Layer layer{};layer.slot=slot;layer.priority=rom.h(a+20)&3;layer.gw=rom.h(a+22);layer.gh=rom.h(a+24);layer.wt=rom.h(a+26);layer.ht=rom.h(a+28);layer.sx=int32_t(rom.w(a+12));layer.sy=int32_t(rom.w(a+16));
   if(!layer.gw || !layer.gh || layer.gw*layer.gh>65536 || rom.h(a+30)!=2 || layer.wt>layer.gw*16 || layer.ht>layer.gh*8)throw std::runtime_error("Layer dimensions");
   if(slot<4)layer.tiles=rom.tagged(rom.w(a+8));
   const uint32_t grid=rom.w(a),section=rom.w(a+4);std::map<uint16_t,std::vector<uint16_t>> cache;layer.cells.resize(size_t(layer.gw*16*layer.gh*8));
   for(unsigned cy=0;cy<layer.gh;cy++){
    for(unsigned cx=0;cx<layer.gw;cx++){
     uint16_t id=rom.h(grid+2*(cy*layer.gw+cx));auto it=cache.find(id);
     if(it==cache.end()){size_t p=size_t(section)+2*id;if(p+2>asset.size())throw std::runtime_error("Chunk table bounds");size_t start=section+4*size_t(asset[p]|(asset[p+1]<<8));it=cache.emplace(id,chunk(asset,start)).first;}
     for(unsigned y=0;y<8;y++)std::copy_n(it->second.begin()+y*16,16,layer.cells.begin()+size_t((cy*8+y)*layer.gw*16+cx*16));
    }
   }
   scene.layers.push_back(std::move(layer));
  }return scene;
 }
 Scene scene(unsigned level=0)const{
  auto route=roomRoute(level);for(const auto& room:route)if(room.tileRoom())return sceneFromRoom(room);
  throw std::runtime_error("This level has no tile room yet");
 }
 Scene scene(unsigned level,unsigned slot)const{
  auto route=roomRoute(level);if(slot>=route.size())throw std::runtime_error("Room slot");return sceneFromRoom(route[slot]);
 }
 std::vector<EntityLink> entityLinks(const Scene& scene)const{
  uint32_t links=rom.w(scene.descriptor+32);if(!links)return {};
  int32_t count=int32_t(rom.w(links));if(count<0||count>4096)throw std::runtime_error("Entity link count");
  std::vector<EntityLink> out;out.reserve(unsigned(count));
  for(int32_t i=0;i<count;i++){
   int32_t from=int32_t(rom.w(links+4+8*unsigned(i))),to=int32_t(rom.w(links+8+8*unsigned(i)));
   if(from<0||to<0)throw std::runtime_error("Entity link id");
   out.push_back({unsigned(from),unsigned(to)});
  }
  return out;
 }
 std::vector<Entity> entities(const Scene& scene)const{
  uint32_t list=rom.w(scene.descriptor+28);unsigned count=rom.h(list),groupCount=rom.h(list+2);uint32_t groups=rom.w(list+4);
  if(count>4096 || groupCount>1024)throw std::runtime_error("Entity list bounds");
  std::vector<Entity> out;out.reserve(count);unsigned id=0;
  for(int column=int(groupCount)-1;column>=0;--column){
   uint32_t g=groups+8*unsigned(column);unsigned n=rom.h(g+2);uint32_t item=rom.w(g+4);if(n>count || (n && !item))throw std::runtime_error("Entity group bounds");
   for(unsigned i=0;i<n;i++,item+=8){unsigned type=rom.h(item);if(type>=93)throw std::runtime_error("Entity type bounds");out.push_back({id++,type,rom.h(item+2),rom.h(item+4),rom.h(item+6),unsigned(column)});}
  }
  if(out.size()!=count)throw std::runtime_error("Entity count mismatch");
  return out;
 }
 static std::vector<uint32_t> viewport(const Scene& scene,int cameraX,int cameraY,int w=240,int h=160,bool collision=false){
  std::vector<uint32_t> pixels(size_t(w*h),scene.colors[0]);std::vector<const Layer*> layers;for(const auto& l:scene.layers)if(l.slot<4)layers.push_back(&l);
  auto bg=[](unsigned slot){return slot==3?0:slot+1;};std::sort(layers.begin(),layers.end(),[&](auto a,auto b){if(a->priority!=b->priority)return a->priority>b->priority;return bg(a->slot)>bg(b->slot);});
  for(const auto* layer:layers){const bool eight=layer->slot==3;size_t stride=eight?64:32;int ox=cameraX*layer->sx/256,oy=cameraY*layer->sy/256;
   for(int y=0;y<h;y++)for(int x=0;x<w;x++){
    int worldX=x+ox,worldY=y+oy;if(worldX<0 || worldY<0 || worldX>=int(layer->wt*8) || worldY>=int(layer->ht*8))continue;
    unsigned tx=unsigned(worldX/8),ty=unsigned(worldY/8);uint16_t cell=layer->cells.at(size_t(ty*layer->gw*16+tx));unsigned tile=cell&(eight?0x3fff:0x3ff);int px=worldX%8,py=worldY%8;
    if(cell&(eight?0x4000:0x400))px=7-px;
    if(cell&(eight?0x8000:0x800))py=7-py;
    size_t address=tile*stride+size_t(py*(eight?8:4)+px/(eight?1:2));if(address>=layer->tiles.size())throw std::runtime_error("Tile data bounds");unsigned v=layer->tiles[address];if(!eight)v=(v>>((px&1)*4))&15;
    if(v)pixels[size_t(y*w+x)]=scene.colors[eight?v:(cell>>12)*16+v];
   }
  }
  if(collision){auto it=std::find_if(scene.layers.begin(),scene.layers.end(),[](auto& l){return l.slot==4;});if(it!=scene.layers.end())for(int y=0;y<h;y++)for(int x=0;x<w;x++){
   int xx=x+cameraX,yy=y+cameraY;if(xx<0 || yy<0 || xx>=int(it->wt*8) || yy>=int(it->ht*8))continue;uint16_t c=it->cells[size_t(yy/8*int(it->gw*16)+xx/8)];if(c&255){uint32_t v=pixels[size_t(y*w+x)];unsigned red=(c&255)>35?255:40,blue=(c&255)>35?40:255;pixels[size_t(y*w+x)]=0xff000000|((((v>>16)&255)+red)/2<<16)|((((v>>8)&255)+40)/2<<8)|(((v&255)+blue)/2);}
  }}return pixels;
 }
 static void blit(std::vector<uint32_t>& canvas,int w,int h,const Frame& f,int anchorX,int anchorY,bool mirror=false){
  for(int y=0;y<f.height;y++)for(int x=0;x<f.width;x++){int xx=mirror?anchorX-f.x-x-1:anchorX+f.x+x,yy=anchorY+f.y+y;uint32_t p=f.pixels[size_t(y*f.width+x)];if((p>>24) && xx>=0 && yy>=0 && xx<w && yy<h)canvas[size_t(yy*w+xx)]=p;}
 }
};
}
