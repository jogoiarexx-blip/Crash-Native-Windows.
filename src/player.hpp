#pragma once
#include "game_data.hpp"
#include <cmath>
#include <limits>
#include <vector>
namespace crash {
// Experimental 60 Hz controller. Geometry/assets come from ROM; tuning is native.
class Terrain {
 public:
 const Layer& map;std::array<std::array<uint8_t,36>,51> shapes{};
 explicit Terrain(const GameData& data,const Scene& scene):map(collisionLayer(scene)){
  constexpr uint32_t table=0x081720e8;
  for(unsigned t=0;t<51;t++)for(unsigned i=0;i<36;i++)shapes[t][i]=data.rom.b(table+t*36+i);
  if(shapes[0][0]!=255 || shapes[43][4]!=0 || shapes[1][4]!=7 || shapes[1][11]!=0)throw std::runtime_error("Terrain table revision");
 }
 static const Layer& collisionLayer(const Scene& s){for(const auto& l:s.layers)if(l.slot==4)return l;throw std::runtime_error("No terrain layer");}
 int width()const{return int(map.wt*8);}int height()const{return int(map.ht*8);}
 uint16_t cell(int x,int y)const{if(x<0 || y<0 || x>=width() || y>=height())return 0;return map.cells.at(size_t(y/8*int(map.gw*16)+x/8));}
 static bool enabled(uint16_t c,int mode){constexpr unsigned mask[]={0x4000,0x1000,0x8000,0x2000};return !(c&mask[mode]);}
 bool solid(int x,int y,int mode)const{
  uint16_t c=cell(x,y);unsigned type=c&255;if(!type || type>=51 || !enabled(c,mode))return false;
  auto& s=shapes[type];unsigned top=s[4+(x%8+8)%8],bottom=s[20+(x%8+8)%8];return top!=255 && bottom!=255 && (y%8+8)%8>=int(top) && (y%8+8)%8<=int(bottom);
 }
 // Highest floor surface under the whole body, inside the requested swept range.
 double floor(double left,double right,double low,double high)const{
  double result=std::numeric_limits<double>::infinity();
  int y0=std::max(0,int(std::floor(low/8))),y1=std::min(int(map.ht)-1,int(std::floor(high/8)));
  for(int x=int(std::floor(left+.001));x<=int(std::floor(right-.001));x++){
   for(int row=y0;row<=y1;row++){
    uint16_t c=cell(x,row*8);unsigned t=c&255;if(!t || t>=51 || !enabled(c,0))continue;
    unsigned v=shapes[t][4+(x%8+8)%8];if(v==255)continue;double surface=row*8+v;
    if(surface>=low-.001 && surface<=high+.001)result=std::min(result,surface);
   }
  }
  return result;
 }
};
// up/down are appended so every existing four-field aggregate initializer keeps
// the original {left,right,jump,spin} meaning.
struct Input {
 bool left=false,right=false,jump=false,spin=false,up=false,down=false;
 // GBA shoulders appended after the legacy six fields so all existing aggregate
 // initializers keep their original meaning. On PC they map to E (R) and Q (L).
 bool shoulderR=false,shoulderL=false;
};
class Player {
 public:
 enum class Mode { Normal, Underwater, Hover };
 enum Power : unsigned {
  TurboRunPower=0x10, SuperBodySlamPower=0x20, TornadoSpinPower=0x40, DoubleJumpPower=0x80
 };
 using SolidBox=std::array<double,4>;
 double x=0,y=0,vx=0,vy=0,startX=0,startY=0,checkpointX=0,checkpointY=0;
 bool grounded=false,facingLeft=false,lastJump=false,lastSpin=false,lastShoulderR=false,checkpointActive=false;
 bool crouching=false,bodySlamming=false,doubleJumpUsed=false;
 unsigned ticks=0,animationTicks=0,animation=18,respawns=0,spinTicks=0,strokeTicks=0,tilt=6;
 unsigned powerFlags=0,slideTicks=0,tornadoCharge=0,slamImpactTicks=0,spinCooldownTicks=0;
 bool highJumping=false;
 bool hoverLeftAllowed=false;
 int hoverLeftTimer=0;
 double lastDownImpact=0,lastUpImpact=0;
 const std::vector<SolidBox>* dynamicSolids=nullptr;
 int bx=-7,by=-27,bw=14,bh=41;
 unsigned spriteBank=0;
 Mode mode=Mode::Normal;
 const Terrain& terrain;
 struct HitBounds { int x=0,y=0,w=0,h=0; bool valid()const{return w>0&&h>0;} };
 struct ActionAnim { unsigned duration=1,flags=0; std::vector<uint16_t> sequence; HitBounds terrain; };
 struct FrameBoxes { HitBounds body,attack; };
 std::vector<ActionAnim> actionAnims;
 std::vector<FrameBoxes> frameBoxes;
 Player(const GameData& data,const Scene& scene,const Terrain& t):terrain(t){
  mode=scene.kind==1?Mode::Underwater:(scene.kind==2?Mode::Hover:Mode::Normal);
  spriteBank=mode==Mode::Underwater?1u:(mode==Mode::Hover?2u:0u);
  animation=mode==Mode::Underwater?31u:(mode==Mode::Hover?0u:18u);
  const auto& bank=data.banks.at(spriteBank);
  actionAnims.reserve(bank.animations.size());
  for(const auto& src:bank.animations){
   HitBounds hb{data.rom.sh(src.address+4),data.rom.sh(src.address+6),data.rom.b(src.address+8),data.rom.b(src.address+9)};
   if(!hb.valid()||hb.w>96||hb.h>128)throw std::runtime_error("Player animation bounds");
   actionAnims.push_back({std::max(1u,src.duration),src.flags,src.sequence,hb});
  }
  frameBoxes.reserve(bank.frames.size());
  auto readBox=[&](uint32_t a){return HitBounds{data.rom.sh(a),data.rom.sh(a+2),data.rom.b(a+4),data.rom.b(a+5)};};
  for(uint32_t frame:bank.frames){
   uint32_t ids=data.rom.w(frame+4);unsigned type=data.rom.b(ids)>>4;FrameBoxes boxes{};
   switch(type){case 0:case 2:case 3:case 4:case 6:boxes.body=readBox(frame+12);break;default:break;}
   switch(type){case 0:case 3:case 4:boxes.attack=readBox(frame+20);break;case 5:boxes.attack=readBox(frame+12);break;default:break;}
   frameBoxes.push_back(boxes);
  }
  const auto initial=actionAnims.at(animation).terrain;bx=initial.x;by=initial.y;bw=initial.w;bh=initial.h;
  if(bw<=0 || bh<=0 || bw>64 || bh>96)throw std::runtime_error("Player body bounds");
  const unsigned wanted=mode==Mode::Underwater?0x02u:(mode==Mode::Hover?0x04u:0x00u);
  uint32_t entities=data.rom.w(scene.descriptor+28),groups=data.rom.w(entities+4);bool found=false;
  for(unsigned i=0;i<data.rom.h(entities+2);i++){uint32_t g=groups+8*i,p=data.rom.w(g+4);for(unsigned j=0;j<data.rom.h(g+2);j++,p+=8)if(data.rom.h(p)==wanted){startX=data.rom.h(p+2);startY=data.rom.h(p+4);found=true;break;}if(found)break;}
  if(!found){if(mode==Mode::Underwater)throw std::runtime_error("No type-0x02 underwater start record");if(mode==Mode::Hover)throw std::runtime_error("No type-0x04 hover start record");throw std::runtime_error("No type-0 start record");}
  reset();
 }
 bool underwater()const{return mode==Mode::Underwater;}
 bool hover()const{return mode==Mode::Hover;}
 bool hoverExitRequested()const{return hover()&&x>double(terrain.width())-10.0;}
 bool hasPower(Power p)const{return (powerFlags&unsigned(p))!=0;}
 bool hasTurboRun()const{return hasPower(TurboRunPower);}
 bool hasSuperBodySlam()const{return hasPower(SuperBodySlamPower);}
 bool hasTornadoSpin()const{return hasPower(TornadoSpinPower);}
 bool hasDoubleJump()const{return hasPower(DoubleJumpPower);}
 void grantPower(Power p){powerFlags|=unsigned(p);}
 HitBounds animBounds(unsigned id)const{return id<actionAnims.size()?actionAnims[id].terrain:HitBounds{bx,by,bw,bh};}
 unsigned collisionAnimation()const{
  if(underwater()||hover())return animation;
  if(bodySlamming)return hasSuperBodySlam()?7u:25u;
  if(sliding())return 15u;
  if(crouching)return std::abs(vx)>.05?20u:4u;
  if(spinning())return tornadoCharge?23u:16u;
  return animation;
 }
 unsigned currentFrameIndex()const{
  unsigned anim=collisionAnimation();
  if(anim>=actionAnims.size())return 0;
  const auto& a=actionAnims[anim];
  if(a.sequence.empty())return 0;
  size_t n=animationTicks/std::max(1u,a.duration);
  n=(a.flags&2)?n%a.sequence.size():std::min(n,a.sequence.size()-1);
  return a.sequence[n];
 }
 HitBounds terrainBounds()const{
  if(underwater())return {bx,by,bw,bh};
  if(hover())return animBounds(animation);
  if(sliding())return animBounds(15);
  if(crouching)return animBounds(std::abs(vx)>.05?20:4);
  if(bodySlamming)return animBounds(hasSuperBodySlam()?7:25);
  if(spinning())return animBounds(tornadoCharge?23:16);
  if(!grounded){if(highJumping)return animBounds(doubleJumpUsed?10:11);if(doubleJumpUsed)return animBounds(6);return animBounds(vy<0?12:21);}
  return animBounds((hasTurboRun()&&std::abs(vx)>3.0)?24:(std::abs(vx)>.2?13:18));
 }
 SolidBox worldBox(HitBounds b)const{if(!b.valid())return {x,y,x,y};double l=facingLeft?x-b.x-b.w:x+b.x;return {l,y+b.y,l+b.w,y+b.y+b.h};}
 double feet()const{auto b=terrainBounds();return y+b.y+b.h;}
 bool spinning()const{return spinTicks>0;}
 bool sliding()const{return slideTicks>0;}
 bool slamImpact()const{return slamImpactTicks>0;}
 bool superSlamImpact()const{return slamImpact()&&hasSuperBodySlam();}
 bool basicAttack()const{return spinning()||sliding()||bodySlamming;}
 std::array<double,4> bodyBox()const{
  if(underwater())return {x+bx,y+by,x+bx+bw,y+by+bh};
  if(hover()){unsigned fi=currentFrameIndex();if(fi<frameBoxes.size()&&frameBoxes[fi].body.valid())return worldBox(frameBoxes[fi].body);return worldBox(animBounds(animation));}
  unsigned fi=currentFrameIndex();
  if(fi<frameBoxes.size()&&frameBoxes[fi].body.valid())return worldBox(frameBoxes[fi].body);
  return {x,y,x,y};
 }
 std::array<double,4> attackBox()const{
  if(underwater())return {x+bx-14,y+by-4,x+bx+bw+14,y+by+bh+4};
  if(hover())return {x,y,x,y};
  unsigned fi=currentFrameIndex();
  if(fi<frameBoxes.size()&&frameBoxes[fi].attack.valid())return worldBox(frameBoxes[fi].attack);
  return {x,y,x,y};
 }
 bool canUseAnimation(unsigned id)const{
  HitBounds target=animBounds(id),cur=terrainBounds();if(!target.valid())return false;SolidBox q=worldBox(target);
  if(dynamicSolids)for(const auto& d:*dynamicSolids)if(q[0]<d[2]-.001&&q[2]>d[0]+.001&&q[1]<d[3]-.001&&q[3]>d[1]+.001)return false;
  const int top=int(std::floor(q[1])),left=int(std::floor(q[0]+.001)),right=int(std::floor(q[2]-.001));
  const double curTop=y+cur.y;for(int yy=top;yy<int(std::floor(curTop-.001));yy++)for(int xx=left;xx<=right;xx++)if(terrain.solid(xx,yy,2))return false;return true;
 }
 void placeAt(double px,double py){x=px;y=py;vx=vy=0;grounded=false;lastJump=false;lastSpin=false;lastShoulderR=false;spinTicks=strokeTicks=slideTicks=tornadoCharge=slamImpactTicks=spinCooldownTicks=0;crouching=bodySlamming=doubleJumpUsed=highJumping=false;hoverLeftAllowed=false;hoverLeftTimer=0;tilt=6;animation=underwater()?31u:(hover()?0u:18u);animationTicks=0;if(!underwater()&&!hover()){auto b=terrainBounds();double f=terrain.floor(x+b.x,x+b.x+b.w,feet()-16,feet()+96);if(std::isfinite(f)){y=f-b.y-b.h;grounded=true;}}}
 void reset(){checkpointActive=false;checkpointX=startX;checkpointY=startY;facingLeft=false;placeAt(startX,startY);}
 void setCheckpoint(double px,double py){checkpointActive=true;checkpointX=px;checkpointY=py;}
 void respawn(){++respawns;placeAt(checkpointActive?checkpointX:startX,checkpointActive?checkpointY:startY);}
 void bounce(double speed=-5.2){vy=speed;grounded=false;}
 void setDynamicSolids(const std::vector<SolidBox>* solids){dynamicSolids=solids;}
 bool dynamicOverlap(double nx,double ny)const{
  if(!dynamicSolids)return false;
  auto hb=terrainBounds();
  double l=facingLeft?nx-hb.x-hb.w:nx+hb.x,t=ny+hb.y,r=l+hb.w,b=t+hb.h;
  for(const auto& q:*dynamicSolids)if(l<q[2]-.001&&r>q[0]+.001&&t<q[3]-.001&&b>q[1]+.001)return true;
  return false;
 }
 double floorSurface(double left,double right,double low,double high)const{
  double result=terrain.floor(left,right,low,high);if(dynamicSolids)for(const auto& q:*dynamicSolids)if(right>q[0]+.001&&left<q[2]-.001&&q[1]>=low-.001&&q[1]<=high+.001)result=std::min(result,q[1]);return result;
 }
 bool sideBlocked(double nx,double ny,int mode_)const{auto hb=terrainBounds();double l=facingLeft?nx-hb.x-hb.w:nx+hb.x;int edge=int(std::floor(l+(mode_==1?hb.w-.001:0)));for(int yy=int(std::floor(ny+hb.y));yy<=int(std::floor(ny+hb.y+hb.h-.001));yy++)if(terrain.solid(edge,yy,mode_))return true;return dynamicOverlap(nx,ny);}
 bool ceilingBlocked(double ny)const{auto hb=terrainBounds();double l=facingLeft?x-hb.x-hb.w:x+hb.x;int top=int(std::floor(ny+hb.y));for(int xx=int(std::floor(l));xx<=int(std::floor(l+hb.w-.001));xx++)if(terrain.solid(xx,top,2))return true;return dynamicOverlap(x,ny);}
 bool bottomBlocked(double ny)const{auto hb=terrainBounds();double l=facingLeft?x-hb.x-hb.w:x+hb.x;int bottom=int(std::floor(ny+hb.y+hb.h-.001));for(int xx=int(std::floor(l));xx<=int(std::floor(l+hb.w-.001));xx++)if(terrain.solid(xx,bottom,0))return true;return dynamicOverlap(x,ny);}
 static double approachD(double value,double target,double step){if(value<target)return std::min(target,value+step);if(value>target)return std::max(target,value-step);return value;}
 static unsigned swimTilt(Input in){const bool side=in.left||in.right;if(in.up)return side?3u:0u;if(in.down)return side?9u:12u;return 6u;}
 static unsigned swimAnim(unsigned mode_,unsigned tilt_){
  static constexpr unsigned table[4][13]={
   {31,31,31,31,31,31,31,31,31,31,31,31,31},
   {33,37,36,7,35,34,23,15,16,41,17,18,14},
   {38,37,36,8,35,34,24,15,16,1,17,18,19},
   {39,39,9,9,9,30,30,30,2,2,2,20,20}
  };
  return table[std::min(mode_,3u)][std::min(tilt_,12u)];
 }
 void stepUnderwater(Input in){
  ++ticks;lastDownImpact=lastUpImpact=0;const bool strokePressed=in.jump&&!lastJump,spinPressed=in.spin&&!lastSpin;lastJump=in.jump;lastSpin=in.spin;if(spinTicks)--spinTicks;if(strokeTicks)--strokeTicks;
  const int dxDir=int(in.right)-int(in.left),dyDir=int(in.down)-int(in.up);if(dxDir)facingLeft=dxDir<0;tilt=swimTilt(in);
  if(spinPressed&&spinTicks==0){spinTicks=24;strokeTicks=0;const double s=960.0/256.0,d=720.0/256.0;if(dyDir&&dxDir){vx=(facingLeft?-d:d);vy=dyDir<0?-d:d;}else if(dyDir){vx=0;vy=dyDir<0?-s:s;}else{vx=facingLeft?-s:s;vy=0;}}
  else if(strokePressed&&spinTicks==0){strokeTicks=16;const double s=704.0/256.0,d=528.0/256.0;if(dyDir&&dxDir){vx=facingLeft?-d:d;vy=dyDir<0?-d:d;}else if(dyDir){vx=0;vy=dyDir<0?-s:s;}else{vx=facingLeft?-s:s;vy=0;}}
  const double mag=300.0/256.0,diag=225.0/256.0,slow=5.0/256.0,fast=15.0/256.0,diagStep=11.0/256.0;
  double tx=0,ty=0,ax=slow,ay=slow;
  if(dxDir&&dyDir){tx=(dxDir<0?-diag:diag);ty=(dyDir<0?-diag:diag);ax=ay=diagStep;}
  else if(dxDir){tx=dxDir<0?-mag:mag;ax=fast;}
  else if(dyDir){ty=dyDir<0?-mag:mag;ay=fast;}
  if(!spinTicks&&!strokeTicks){vx=approachD(vx,tx,ax);vy=approachD(vy,ty,ay);}else{vx=approachD(vx,tx,std::min(ax,.035));vy=approachD(vy,ty,std::min(ay,.035));}
  int xs=std::max(1,int(std::ceil(std::abs(vx))));double sx=vx/xs;for(int i=0;i<xs;i++){double nx=std::clamp(x+sx,double(-bx),double(terrain.width()-bx-bw));if(sideBlocked(nx,y,sx>0?1:3)){vx=0;break;}x=nx;}
  int ys=std::max(1,int(std::ceil(std::abs(vy))));double sy=vy/ys;for(int i=0;i<ys;i++){double ny=std::clamp(y+sy,double(-by),double(terrain.height()-by-bh));if((sy<0&&ceilingBlocked(ny))||(sy>0&&bottomBlocked(ny))){vy=0;break;}y=ny;}
  if(y>terrain.height()+80)respawn();
  grounded=false;
  unsigned swimMode=spinning()?3u:(strokeTicks?2u:((dxDir||dyDir)?1u:0u));unsigned next=swimAnim(swimMode,tilt);if(next!=animation){animation=next;animationTicks=0;}else ++animationTicks;
 }
 void stepHover(Input in){
  ++ticks;lastDownImpact=lastUpImpact=0;grounded=false;animation=0;
  // Original InputCtrl: entry 1 cruises at +0x300 Q8, entry 7 slows to
  // +0x100 for at most 30 frames, entry 8 is +0x300; Y entries are +/-0x300.
  if(in.left&&hoverLeftAllowed){vx=256.0/256.0;if(++hoverLeftTimer>30){hoverLeftAllowed=false;hoverLeftTimer=10;}}
  else if(in.right)vx=768.0/256.0;
  else vx=768.0/256.0;
  if(!hoverLeftAllowed){if(--hoverLeftTimer<0){hoverLeftTimer=0;if(!in.left)hoverLeftAllowed=true;}}
  vy=in.up?-768.0/256.0:(in.down?768.0/256.0:0.0);
  auto killAndRespawn=[&](){respawn();};
  const double nx=x+vx;
  if(nx<double(-terrainBounds().x)||sideBlocked(nx,y,1)){killAndRespawn();return;}
  x=nx;
  const double ny=y+vy;
  if((vy<0&&ceilingBlocked(ny))||(vy>0&&bottomBlocked(ny))){killAndRespawn();return;}
  auto hb=terrainBounds();y=std::clamp(ny,double(-hb.y),double(terrain.height()-hb.y-hb.h));
  ++animationTicks;
 }
 void stepNormal(Input in){
  ++ticks;lastDownImpact=lastUpImpact=0;
  const bool crouchHeld=in.shoulderR||in.down;
  const bool jumpPressed=in.jump&&!lastJump,spinPressed=in.spin&&!lastSpin,rPressed=in.shoulderR&&!lastShoulderR;
  lastJump=in.jump;lastSpin=in.spin;lastShoulderR=in.shoulderR;
  if(spinTicks){--spinTicks;if(!spinTicks)spinCooldownTicks=12;}
  else if(spinCooldownTicks)--spinCooldownTicks;
  if(slideTicks)--slideTicks;
  if(slamImpactTicks)--slamImpactTicks;

  // GBA action-controller semantics: Down crouches, while the actual R button
  // crouches from idle, starts a slide only after movement, and starts body slam in air.
  int direction=int(in.right)-int(in.left);if(direction)facingLeft=direction<0;
  if(grounded&&!slideTicks){
   if(in.down)crouching=true;
   else if(in.shoulderR&&std::abs(vx)<.45)crouching=true;
   else if(!crouchHeld&&crouching&&canUseAnimation(2))crouching=false;
  }
  if(rPressed&&grounded&&!crouching&&!slideTicks&&std::abs(vx)>=.45){slideTicks=28;const int slideDir=direction?direction:(facingLeft?-1:1);vx=double(slideDir)*(1408.0/256.0);}
  const bool nearJumpApex=vy<=0x27f/256.0;
  if(in.shoulderR&&!grounded&&nearJumpApex&&!bodySlamming&&!highJumping&&!spinning()){bodySlamming=true;crouching=false;vy=1024.0/256.0;}

  if(spinPressed){
   if(hasTornadoSpin()&&spinTicks){tornadoCharge=std::min(3u,tornadoCharge+1);spinTicks=std::min(96u,spinTicks+18);}
   else if(!spinTicks&&!spinCooldownTicks){spinTicks=24;tornadoCharge=0;}
  }
  if(!spinTicks)tornadoCharge=0;

  const bool turbo=grounded&&hasTurboRun()&&in.shoulderL&&direction&&!slideTicks&&!crouching;
  if(slideTicks){vx=approachD(vx,0,56.0/256.0);}
  else{const double target=crouching?double(direction)*.75:double(direction)*(turbo?4.125:2.75);const double accel=grounded?(crouching?.16:(turbo?.42:.34)):.20;if(vx<target)vx=std::min(target,vx+accel);else if(vx>target)vx=std::max(target,vx-accel);}

  if(jumpPressed&&grounded){
   const bool high=(crouching||slideTicks);if(!high||canUseAnimation(11)){vy=high?-7.5:-6.4;grounded=false;crouching=false;slideTicks=0;bodySlamming=false;doubleJumpUsed=false;highJumping=high;}
  }
  else if(jumpPressed&&!grounded&&nearJumpApex&&hasDoubleJump()&&!doubleJumpUsed&&!bodySlamming){vy=-6.4;doubleJumpUsed=true;highJumping=true;}
  if(!bodySlamming&&!in.jump&&vy<-2.8)vy=-2.8;
  if(bodySlamming)vy=std::min(1536.0/256.0,vy+128.0/256.0);
  else if(spinTicks&&tornadoCharge&&!grounded){static constexpr double g[4]={.28,18.0/256.0,14.0/256.0,6.0/256.0};vy=std::min(5.0,vy+g[tornadoCharge]);}
  else vy=std::min(8.0,vy+.28);

  auto hb=terrainBounds();
  int steps=std::max(1,int(std::ceil(std::abs(vx))));double dx=vx/steps;
  for(int i=0;i<steps;i++){
   hb=terrainBounds();double minX=-hb.x,maxX=double(terrain.width())-hb.x-hb.w;double nx=std::clamp(x+dx,minX,maxX);int mode_=dx>0?1:3;
   if(sideBlocked(nx,y,mode_)){
    bool lifted=false;if(grounded&&!slideTicks)for(int up=1;up<=4;up++)if(!sideBlocked(nx,y-up,mode_)&&!ceilingBlocked(y-up)){y-=up;lifted=true;break;}
    if(!lifted){vx=0;slideTicks=0;break;}
   }
   x=nx;
   if(grounded){hb=terrainBounds();double left=facingLeft?x-hb.x-hb.w:x+hb.x;double f=floorSurface(left,left+hb.w,feet()-4,feet()+6);if(std::isfinite(f))y=f-hb.y-hb.h;else grounded=false;}
  }
  if(grounded)vy=0;
  else{
   int sub=std::max(1,int(std::ceil(std::abs(vy))));double dy=vy/sub;
   for(int i=0;i<sub;i++){
    hb=terrainBounds();double left=facingLeft?x-hb.x-hb.w:x+hb.x;
    if(dy>=0){double f=floorSurface(left,left+hb.w,feet(),feet()+dy);if(std::isfinite(f)){lastDownImpact=vy;y=f-hb.y-hb.h;vy=0;grounded=true;if(bodySlamming){slamImpactTicks=hasSuperBodySlam()?3u:1u;bodySlamming=false;}doubleJumpUsed=false;highJumping=false;break;}}
    else if(ceilingBlocked(y+dy)){lastUpImpact=vy;vy=0;break;}
    y+=dy;
   }
  }
  if(grounded){doubleJumpUsed=false;highJumping=false;if(bodySlamming){slamImpactTicks=hasSuperBodySlam()?3u:1u;bodySlamming=false;}}
  if(y>terrain.height()+80)respawn();

  unsigned next=18u;
  if(bodySlamming)next=hasSuperBodySlam()?7u:25u;
  else if(slamImpact())next=17u;
  else if(slideTicks)next=15u;
  else if(crouching)next=direction?20u:4u;
  else if(spinning()&&tornadoCharge)next=23u;
  else if(spinning())next=16u;
  else if(turbo)next=24u;
  else if(grounded)next=std::abs(vx)>.2?13u:18u;
  else if(doubleJumpUsed)next=10u;
  else if(highJumping)next=11u;
  else next=vy<0?12u:21u;
  if(next!=animation){animation=next;animationTicks=0;}else ++animationTicks;
 }
 void step(Input in){if(underwater())stepUnderwater(in);else if(hover())stepHover(in);else stepNormal(in);}
 std::pair<int,int> camera()const{return {std::clamp(int(std::lround(x))-100,0,std::max(0,terrain.width()-240)),std::clamp(int(std::lround(y))-110,0,std::max(0,terrain.height()-160))};}
 std::vector<uint32_t> render(const GameData& data,const Scene& scene,bool debug=false)const{
  auto cam=camera();auto out=GameData::viewport(scene,cam.first,cam.second,240,160,debug);auto& a=data.banks[spriteBank].animations[animation];size_t n=animationTicks/std::max(1u,a.duration);n=(a.flags&2)?n%a.sequence.size():std::min(n,a.sequence.size()-1);auto f=data.frame(spriteBank,a.sequence[n],a.paletteRecord);GameData::blit(out,240,160,f,int(std::lround(x))-cam.first,int(std::lround(y))-cam.second,facingLeft);
  if(debug){auto body=bodyBox();int left=int(body[0])-cam.first,top=int(body[1])-cam.second,right=int(body[2])-cam.first,bottom=int(body[3])-cam.second;if(right>left&&bottom>top)for(int yy=top;yy<=bottom;yy++)for(int xx=left;xx<=right;xx++)if((xx==left||xx==right||yy==top||yy==bottom)&&xx>=0&&xx<240&&yy>=0&&yy<160)out[size_t(yy*240+xx)]=0xffffff00;if(basicAttack()){auto a=attackBox();int l=int(a[0])-cam.first,t=int(a[1])-cam.second,r=int(a[2])-cam.first,b=int(a[3])-cam.second;if(r>l&&b>t)for(int yy=t;yy<=b;yy++)for(int xx=l;xx<=r;xx++)if((xx==l||xx==r||yy==t||yy==b)&&xx>=0&&xx<240&&yy>=0&&yy<160)out[size_t(yy*240+xx)]=0xffff40ff;}}
  return out;
 }
};
}
