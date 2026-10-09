#pragma once
#include "game_data.hpp"
#include "audio.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace crash {

struct CategoryAnimRecord { unsigned duration=1; int frameIndex=0,loopThreshold=1,loopBase=0; uint16_t attr=0; };
struct CategoryEvent { int threshold=0; uint8_t variantA=0,variantB=0,variantC=0; int offsetX=0,offsetY=0; };
struct CategoryBox { int x=0,y=0,z=0,w=0,h=0,d=0; bool valid()const{return w>0&&h>0&&d>0;} };

class Category0Data {
public:
    static constexpr uint32_t CategoryTable=0x081736A8;
    static constexpr uint32_t HovercraftPalette=0x08169628;
    static constexpr uint32_t HovercraftPicture=0x08169828;
    static constexpr unsigned DescriptorSize=0x34, AnimRecordSize=0x28;

    const GameData& data;
    unsigned categoryIndex=0,type=0,maskAssistDeaths=0,bonusKindDeaths=0,retryBossDeaths=0,bossLevel=0,retryBossLevel=0,animSlotCount=0;
    uint32_t descriptor=0,background=0,conditionalPicture=0,palette=0,subEffects=0,animTable=0,spriteSheet=0;
    Bytes spriteAsset;
    unsigned backgroundSize=0,backgroundFrames=0,eventCount=0;
    int routeEndThreshold=0,bgCols=0,bgRows=0,pictureCols=0,pictureRows=0;
    unsigned pictureTiles=0;
    size_t bgTileBytes=0,bgPaletteBytes=0,bgStride=0;

    explicit Category0Data(const GameData& gd,unsigned category=0):data(gd),categoryIndex(category){
        if(category>6)throw std::runtime_error("Actor category not supported by this build");
        descriptor=CategoryTable+category*DescriptorSize;
        type=data.rom.w(descriptor);
        background=data.rom.w(descriptor+4);backgroundSize=data.rom.w(descriptor+8);conditionalPicture=data.rom.w(descriptor+0x0C);
        palette=data.rom.w(descriptor+0x10);subEffects=data.rom.w(descriptor+0x14);animTable=data.rom.w(descriptor+0x18);spriteSheet=data.rom.w(descriptor+0x1C);
        maskAssistDeaths=data.rom.w(descriptor+0x20);bonusKindDeaths=data.rom.w(descriptor+0x24);retryBossDeaths=data.rom.w(descriptor+0x28);bossLevel=data.rom.w(descriptor+0x2C);retryBossLevel=data.rom.w(descriptor+0x30);
        if((type!=0&&type!=1&&type!=2)||!background||!palette||!subEffects||!animTable||!spriteSheet)throw std::runtime_error("Unsupported actor-category descriptor");
        if(type==0&&(backgroundSize!=0x75B94||conditionalPicture!=0))throw std::runtime_error("Unexpected polar category descriptor");
        if((type==1||type==2)&&(backgroundSize!=0x3E784||!conditionalPicture))throw std::runtime_error("Unexpected jetpack category descriptor");
        animSlotCount=type==0?41u:47u;
        spriteAsset=data.rom.tagged(spriteSheet);if(spriteAsset.empty()||spriteAsset.size()>4*1024*1024)throw std::runtime_error("Category sprite sheet");
        bgCols=data.rom.sh(background+0x200);bgRows=data.rom.sh(background+0x202);if(bgCols<=0||bgRows<=0||bgCols>64||bgRows>64)throw std::runtime_error("Category background dimensions");
        const size_t cells=size_t(bgCols)*size_t(bgRows);bgTileBytes=cells*32;bgPaletteBytes=type==0?(((cells+1)/2+3)&~size_t(3)):0;bgStride=bgTileBytes+bgPaletteBytes;
        if(backgroundSize<0x204||(backgroundSize-0x204)%bgStride)throw std::runtime_error("Category background stride");
        backgroundFrames=unsigned((backgroundSize-0x204)/bgStride);eventCount=unsigned(data.rom.w(subEffects+4));if(!eventCount||eventCount>2048)throw std::runtime_error("Category event count");
        routeEndThreshold=int32_t(data.rom.w(subEffects+eventCount*0x14));if(type!=2&&routeEndThreshold<=0)throw std::runtime_error("Category route end");if(type==2&&routeEndThreshold<0)throw std::runtime_error("Category boss route end");
        if(data.rom.w(animTable)!=0) throw std::runtime_error("Category player record index");
        data.rom.offset(data.rom.w(animTable+4),12);data.rom.offset(data.rom.w(animTable+8),4);
        if(conditionalPicture){pictureCols=data.rom.sh(conditionalPicture+0x200);pictureRows=data.rom.sh(conditionalPicture+0x202);pictureTiles=data.rom.w(conditionalPicture+0x204);if(pictureCols<=0||pictureRows<=0||pictureCols>64||pictureRows>64||pictureTiles>4096)throw std::runtime_error("Category BG picture");}
    }

    CategoryAnimRecord animation(unsigned actorSlot,unsigned animIndex)const{
        if(actorSlot>=animSlotCount)throw std::runtime_error("Category actor slot");
        const uint32_t rec=animTable+actorSlot*AnimRecordSize,a=data.rom.w(rec+4)+animIndex*12;data.rom.offset(a,12);CategoryAnimRecord out;
        out.duration=std::max(1u,unsigned(data.rom.h(a)));out.frameIndex=data.rom.sh(a+2);out.loopThreshold=data.rom.sh(a+4);out.loopBase=data.rom.sh(a+6);out.attr=data.rom.h(a+8);
        if(out.frameIndex<0||out.loopThreshold<=0||out.loopBase<0||out.loopBase>=out.loopThreshold) throw std::runtime_error("Category animation record");
        return out;
    }
    unsigned actorPaletteBank(unsigned actorSlot)const{if(actorSlot>=animSlotCount)throw std::runtime_error("Category actor slot");const uint32_t rec=animTable+actorSlot*AnimRecordSize;data.rom.offset(rec,AnimRecordSize);unsigned b=data.rom.b(rec+0x0C);if(b>=16)throw std::runtime_error("Category palette bank");return b;}
    CategoryBox actorBox(unsigned actorSlot)const{if(actorSlot>=animSlotCount)throw std::runtime_error("Category actor slot");const uint32_t rec=animTable+actorSlot*AnimRecordSize;data.rom.offset(rec,AnimRecordSize);return {data.rom.sh(rec+0x14),data.rom.sh(rec+0x16),data.rom.sh(rec+0x18),data.rom.sh(rec+0x1A),data.rom.sh(rec+0x1C),data.rom.sh(rec+0x1E)};}
    double actorBaseDepth(unsigned actorSlot)const{if(actorSlot>=animSlotCount)throw std::runtime_error("Category actor slot");return double(data.rom.w(animTable+actorSlot*AnimRecordSize+0x10))/256.0;}
    double actorSpawnX(unsigned actorSlot)const{if(actorSlot>=animSlotCount)throw std::runtime_error("Category actor slot");return double(int32_t(data.rom.w(animTable+actorSlot*AnimRecordSize+0x20)))/256.0;}
    double actorSpawnY(unsigned actorSlot)const{if(actorSlot>=animSlotCount)throw std::runtime_error("Category actor slot");return double(int32_t(data.rom.w(animTable+actorSlot*AnimRecordSize+0x24)))/256.0;}
    uint32_t actorFrameReference(unsigned actorSlot,int frameOffset)const{if(actorSlot>=animSlotCount)throw std::runtime_error("Category actor slot");const uint32_t rec=animTable+actorSlot*AnimRecordSize,tableB=data.rom.w(rec+8);if(frameOffset<0||frameOffset>4096)throw std::runtime_error("Category frame offset");data.rom.offset(tableB+4u*unsigned(frameOffset),4);return data.rom.w(tableB+4u*unsigned(frameOffset));}

    static Frame decodeTileFrame(const Bytes& src,size_t p,const std::array<uint32_t,16>& colors,bool rle){
        if(p>src.size()||src.size()-p<4) throw std::runtime_error("Category frame header");
        unsigned wt=src[p],ht=src[p+1],tag=src[p+2],pad=src[p+3];
        if(!wt||!ht||wt>16||ht>16||pad!=0||((rle&&tag!=0x30)||(!rle&&tag!=0x10)))throw std::runtime_error("Category frame format");
        size_t total=size_t(wt)*ht*16;std::vector<uint16_t> hw;hw.reserve(total);p+=4;
        if(rle){bool zero=true;while(hw.size()<total){if(p+2>src.size())throw std::runtime_error("Category RLE count");unsigned n=uint16_t(src[p]|(src[p+1]<<8));p+=2;if(hw.size()+n>total)throw std::runtime_error("Category RLE overflow");if(zero)hw.insert(hw.end(),n,0);else{if(p+2ull*n>src.size())throw std::runtime_error("Category RLE literal");for(unsigned i=0;i<n;i++){hw.push_back(uint16_t(src[p]|(src[p+1]<<8)));p+=2;}}zero=!zero;}}
        else{if(p+total*2>src.size())throw std::runtime_error("Category raw frame");for(size_t i=0;i<total;i++)hw.push_back(uint16_t(src[p+2*i]|(src[p+2*i+1]<<8)));}
        Frame out;out.width=int(wt*8);out.height=int(ht*8);out.x=-out.width/2;out.y=-out.height/2;out.pixels.assign(size_t(out.width*out.height),0);
        for(unsigned t=0;t<wt*ht;t++)for(unsigned py=0;py<8;py++)for(unsigned px=0;px<8;px++){
            uint16_t word=hw[size_t(t)*16+py*2+px/4];unsigned v=(word>>((px&3)*4))&15;
            if(!v)continue;
            unsigned tx=(t%wt)*8+px,ty=(t/wt)*8+py;out.pixels[size_t(ty*out.width+tx)]=colors[v];
        }
        return out;
    }
    std::array<uint32_t,16> actorPalette(unsigned actorSlot,bool flashPalette10=false)const{std::array<uint32_t,16> c{};unsigned bank=actorPaletteBank(actorSlot);for(unsigned i=0;i<16;i++)c[i]=rgb555(data.rom.h(palette+32*bank+2*i));if(flashPalette10&&bank==10)c[15]=0xffffffff;c[0]&=0x00ffffff;return c;}
    Frame actorFrame(unsigned actorSlot,int frameOffset,bool flashPalette10=false)const{uint32_t ref=actorFrameReference(actorSlot,frameOffset);auto colors=actorPalette(actorSlot,flashPalette10);if(ref>=Base&&ref<Base+data.rom.bytes.size())return decodeTileFrame(data.rom.bytes,data.rom.offset(ref,4),colors,true);if(ref>=spriteAsset.size())throw std::runtime_error("Category sheet frame offset");return decodeTileFrame(spriteAsset,size_t(ref),colors,false);}
    uint32_t actorFrameAddress(unsigned actorSlot,int frameOffset)const{return actorFrameReference(actorSlot,frameOffset);}Frame rleFrame(uint32_t a)const{return decodeTileFrame(data.rom.bytes,data.rom.offset(a,4),actorPalette(0),true);}
    Frame actorAnimationFrame(unsigned actorSlot,unsigned animIndex,unsigned tick,bool flashPalette10=false)const{auto a=animation(actorSlot,animIndex);unsigned base=unsigned((uint64_t(tick)*a.duration)>>8);if(base>=unsigned(a.loopThreshold)){unsigned span=unsigned(a.loopThreshold-a.loopBase);base=unsigned(a.loopBase)+(base-unsigned(a.loopBase))%span;}return actorFrame(actorSlot,a.frameIndex+int(base),flashPalette10);}
    unsigned animationTicks(unsigned actorSlot,unsigned animIndex)const{auto a=animation(actorSlot,animIndex);return std::max(1u,unsigned((uint64_t(a.loopThreshold)*256u+a.duration-1)/a.duration));}

    static unsigned timeTrialFold(unsigned kind){if(kind==11)return 0;if(kind==3||kind==8||kind==28||kind==29||kind==30||kind==31||kind==35)return 1;return kind;}
    unsigned selectedKind(const CategoryEvent& e,bool timeTrial=false,bool useBonus=false)const{if(timeTrial){unsigned k=e.variantB;if(type==0)return timeTrialFold(k);if(k==0x17)return 0x14;return k;}return useBonus?e.variantC:e.variantA;}
    unsigned bossLevelForDeaths(unsigned deaths)const{return deaths>=retryBossDeaths?retryBossLevel:bossLevel;}
    bool spawnableKind(unsigned kind)const{
        if(kind>=animSlotCount)return false;
        if(type==0){if(kind==0||kind==32||kind==33||kind==34||(kind>=36&&kind<=39))return false;}
        else{if(kind==0||kind==0x3e||(kind>=0x20&&kind<=0x25)||(type==2&&kind==10))return false;}
        const uint32_t rec=animTable+kind*AnimRecordSize;try{data.rom.offset(rec,AnimRecordSize);return data.rom.w(rec+4)!=0&&data.rom.w(rec+8)!=0;}catch(...){return false;}
    }
    int eventDepth(unsigned index)const{if(index>=eventCount)throw std::runtime_error("Category event depth");if(type==2&&index+1==eventCount)return event(index).threshold;return index+1<eventCount?event(index+1).threshold:routeEndThreshold;}
    int eventTarget(unsigned index)const{if(index>=eventCount)throw std::runtime_error("Category event target");uint32_t p=subEffects+index*0x14+0x18;data.rom.offset(p,4);return int32_t(data.rom.w(p));}
    CategoryEvent event(unsigned index)const{if(index>=eventCount)throw std::runtime_error("Category event index");uint32_t p=subEffects+index*0x14;return {int32_t(data.rom.w(p)),data.rom.b(p+8),data.rom.b(p+9),data.rom.b(p+10),int32_t(data.rom.w(p+12)),int32_t(data.rom.w(p+16))};}

    std::array<uint32_t,256> bgPalette(uint32_t base)const{std::array<uint32_t,256> c{};for(unsigned i=0;i<256;i++)c[i]=rgb555(data.rom.h(base+2*i));return c;}
    std::vector<uint32_t> pictureLogical()const{
        std::vector<uint32_t> out(240*160,0xff000000);if(!conditionalPicture)return out;auto colors=bgPalette(conditionalPicture);const size_t cells=size_t(pictureCols)*pictureRows;size_t mapBytes=(cells*2+3)&~size_t(3);uint32_t mapAddr=conditionalPicture+0x208,tileAddr=mapAddr+uint32_t(mapBytes),nibAddr=tileAddr+pictureTiles*32;data.rom.offset(nibAddr,(cells+1)/2);
        const int cropX=std::max(0,(pictureCols*8-240)/2);for(int cy=0;cy<pictureRows;cy++)for(int cx=0;cx<pictureCols;cx++){size_t cell=size_t(cy*pictureCols+cx);unsigned tile=data.rom.h(mapAddr+uint32_t(cell*2));if(tile>=pictureTiles)continue;uint8_t nb=data.rom.b(nibAddr+uint32_t(cell/2));unsigned bank=(nb>>((cell&1)*4))&15;for(int py=0;py<8;py++)for(int px=0;px<8;px++){uint8_t v=data.rom.b(tileAddr+tile*32+py*4+px/2);v=(v>>((px&1)*4))&15;int dx=cx*8+px-cropX,dy=cy*8+py;if(dx>=0&&dx<240&&dy>=0&&dy<160)out[size_t(dy*240+dx)]=colors[bank*16+v];}}return out;
    }
    std::vector<uint32_t> backgroundLogical(unsigned frameIndex)const{
        frameIndex%=backgroundFrames;uint32_t start=background+0x204+uint32_t(frameIndex*bgStride);size_t tileBase=data.rom.offset(start,bgTileBytes);auto colors=bgPalette(background);
        if(type!=0){auto out=pictureLogical();const int cropX=std::max(0,(bgCols*8-240)/2),baseY=126;for(int cy=0;cy<bgRows;cy++)for(int cx=0;cx<bgCols;cx++){size_t cell=size_t(cy*bgCols+cx);for(int py=0;py<8;py++)for(int px=0;px<8;px++){uint8_t v=data.rom.bytes[tileBase+cell*32+py*4+px/2];v=(v>>((px&1)*4))&15;int dx=cx*8+px-cropX,dy=baseY+cy*8+py;if(dx>=0&&dx<240&&dy>=0&&dy<160)out[size_t(dy*240+dx)]=colors[v];}}return out;}
        size_t palBase=data.rom.offset(start+uint32_t(bgTileBytes),bgPaletteBytes);int fullW=bgCols*16,fullH=bgRows*16;std::vector<uint32_t> full(size_t(fullW*fullH),colors[0]);auto bankFor=[&](size_t cell){uint8_t bb=data.rom.bytes[palBase+cell/2];return unsigned((bb>>((cell&1)*4))&15);};auto sample=[&](size_t cell,int xx,int yy){size_t q=tileBase+cell*32+size_t(yy*4+xx/2);unsigned v=data.rom.bytes[q];v=(v>>((xx&1)*4))&15;return colors[bankFor(cell)*16+v];};
        for(int cy=0;cy<bgRows;cy++)for(int cx=0;cx<bgCols;cx++){size_t cell=size_t(cy*bgCols+cx);for(int py=0;py<8;py++)for(int px=0;px<8;px++){uint32_t cc=sample(cell,px,py);int xx=cx*8+px,yy=cy*8+py;full[size_t(yy*fullW+xx)]=cc;full[size_t(yy*fullW+(fullW-1-xx))]=cc;full[size_t((fullH-1-yy)*fullW+xx)]=cc;full[size_t((fullH-1-yy)*fullW+(fullW-1-xx))]=cc;}}
        std::vector<uint32_t> out(240*160,0xff000000);int ox=(fullW-240)/2,oy=(fullH-160)/2;for(int yy=0;yy<160;yy++)for(int xx=0;xx<240;xx++){int sx=xx+ox,sy=yy+oy;if(sx>=0&&sy>=0&&sx<fullW&&sy<fullH)out[size_t(yy*240+xx)]=full[size_t(sy*fullW+sx)];}return out;
    }
    Frame hovercraftFrame(bool flash=false)const{
        const int cols=data.rom.sh(HovercraftPicture),rows=data.rom.sh(HovercraftPicture+2);const unsigned tiles=data.rom.w(HovercraftPicture+4);
        if(cols<=0||rows<=0||cols>32||rows>32||!tiles||tiles>1024)throw std::runtime_error("Hovercraft picture");
        const size_t cells=size_t(cols)*size_t(rows);const uint32_t map=HovercraftPicture+8,tileBase=map+uint32_t(cells*2);data.rom.offset(tileBase,size_t(tiles)*32);
        std::array<uint32_t,16> colors{};for(unsigned i=0;i<16;i++)colors[i]=rgb555(data.rom.h(HovercraftPalette+i*2));if(flash)colors[15]=0xffffffff;colors[0]&=0x00ffffff;
        Frame out;out.width=cols*8;out.height=rows*8;out.x=-out.width/2;out.y=-out.height/2;out.pixels.assign(size_t(out.width*out.height),0);
        for(int cy=0;cy<rows;cy++)for(int cx=0;cx<cols;cx++){unsigned tile=data.rom.h(map+uint32_t((cy*cols+cx)*2));if(tile>=tiles)continue;for(int py=0;py<8;py++)for(int px=0;px<8;px++){uint8_t v=data.rom.b(tileBase+tile*32+py*4+px/2);v=(v>>((px&1)*4))&15;if(v)out.pixels[size_t((cy*8+py)*out.width+cx*8+px)]=colors[v];}}
        return out;
    }
};

class Category0Stage {
public:
    struct Projected { int sx=0,sy=0; double scale=1,dz=0,modelX=0,modelY=0; unsigned kind=0; };
    struct JetShot {double x=0,y=0,z=0,vx=0,vy=0,vz=5.0;unsigned age=0,damage=1,actorKind=0;};
    struct JetBlast {double x=0,y=0,z=0;unsigned age=0,maxAge=28;};
    struct HoverPart {double ox=0,oy=0,oz=0;int hp=0,maxHp=0;unsigned kind=0,cooldown=0,count=0;bool dead=false,active=false,dying=false;unsigned animIndex=0,dyingTicks=0;};
    struct JetSpawnedBomber {double x=0,y=0,z=0,homeX=0,homeY=0;unsigned kind=5,stateTicks=0,dyingTicks=0;int hp=2;bool dying=false;};
    struct HoverFireball {double x=0,y=0,z=0;int speedRaw=0x95,hp=2;unsigned dyingTicks=0;bool dying=false;};
    struct JetCollectedWumpa {double x=0,y=0,vx=0,vy=0;unsigned reward=1,age=0;};
    struct JetReleasedBalloon {double x=0,y=0,z=0,vy=0;unsigned kind=40,age=0;int hp=2;};
    struct JetPoppedBalloon {double x=0,y=0,z=0;unsigned kind=40,age=0;};
    const Category0Data rom;
    double x=0.0,y=0.0,jumpY=0.0,vy=0.0,jetVx=0.0,jetVy=0.0,scroll=0.0,checkpointScroll=0.0,checkpointX=0.0,checkpointY=0.0;
    unsigned steerTicks=0,ticks=0,wumpa=0,crates=0,lives=5,hazards=0,checkpointHits=0,freezeFrames=0,clockFreezeFrames=0,hp=100,jetpackMaxHp=100,enemiesDestroyed=0,shotsFired=0,enemyShotsFired=0,ringPasses=0,shotCooldown=0,invulnFrames=0,rollTicks=0,rollCooldown=0,airshipProjectilesFired=0,airshipExplosionsSpawned=0,airshipsDefeated=0;
    unsigned categoryDeaths=0,bossDeaths=0,activeBossLevel=0;
    unsigned jetpackRingChain=0,jetpackQueuedWumpa=0,jetpackWumpaDispenseTimer=0,jetpackRingRewards=0;
    int jetpackRingLastFrame=-0xbe,jetpackBomberSfxTimer=0;
    unsigned jetPlayerState=1,jetPlayerStateTicks=0;
    double jetPlayerDepth=28.0;
    bool jetFadeStarted=false;
    unsigned maskLevel=0,polarBoostFrames=0,polarShockFrames=0,polarFinishTimer=0,polarFinishHaltFrames=0,polarBoostHits=0,polarLauncherHits=0,polarGoalHits=0,nitroChains=0;
    unsigned hovercraftState=0,hovercraftPhase=0,hovercraftFrameCount=0,hovercraftPartsLeft=0,hovercraftProfile=0,hovercraftFireballsFired=0,hovercraftFireballsDestroyed=0,hovercraftCannonballsFired=0,hovercraftLauncherSpawns=0,hovercraftPartsDestroyed=0;
    unsigned hovercraftHitFlashTimer=0;
    bool hovercraftHitFlashOn=false,hovercraftPalette15White=false;
    int hovercraftEvent=-1,checkpointEvent=-1;
    double hovercraftX=0,hovercraftY=0,hovercraftZ=0,hovercraftVelX=0,hovercraftVelY=0,hovercraftVelZ=0,hovercraftOrbitRadius=0;
    bool hovercraftSpawned=false,hovercraftGone=false,hovercraftFinishTriggered=false;
    std::array<HoverPart,4> hoverParts{};
    int rollDir=0;
    bool complete=false,grounded=true,jumpHeld=false,actionHeld=false,timeTrialVariant=false,bonusVariant=false,collisions=true,timeTrialStartRequested=false,polarLaunchActive=false;
    std::vector<uint8_t> eventKind,consumed,eventAnim,eventPose,eventState;
    std::vector<unsigned> eventStateTicks,eventPhase,eventCooldown,eventHits;
    std::vector<int> eventHp,eventChildHp,eventSpawnTick,planeVelXRaw,planeVelYRaw,planeSpeedRaw,planeAccXRaw,planeAccYRaw,planeSteps,planeNextTarget;
    std::vector<double> eventX,eventY,eventZ,eventHomeX,eventHomeY,eventHomeZ,eventLimitY,eventStepY,eventVelX,eventVelY,eventVelZ;
    std::vector<JetShot> shots,enemyShots;
    std::vector<JetBlast> blasts;
    std::vector<JetSpawnedBomber> spawnedBombers;
    std::vector<HoverFireball> hoverFireballs;
    std::vector<JetCollectedWumpa> jetpackCollectedWumpas;
    std::vector<JetReleasedBalloon> releasedBalloons;
    std::vector<JetPoppedBalloon> poppedBalloons;
    std::vector<OriginalSfxEvent> sfxEvents;
    uint32_t fxRandSeed=0x12345678u;
    double scrollPerTick=36.0/60.0;

    explicit Category0Stage(const GameData& data,unsigned category=0):rom(data,category),eventKind(rom.eventCount,0),consumed(rom.eventCount,0),eventAnim(rom.eventCount,0),eventPose(rom.eventCount,0),eventState(rom.eventCount,0),eventStateTicks(rom.eventCount,0),eventPhase(rom.eventCount,0),eventCooldown(rom.eventCount,0),eventHits(rom.eventCount,0),eventHp(rom.eventCount,0),eventChildHp(rom.eventCount,0),eventSpawnTick(rom.eventCount,-1),planeVelXRaw(rom.eventCount,0),planeVelYRaw(rom.eventCount,0),planeSpeedRaw(rom.eventCount,0),planeAccXRaw(rom.eventCount,0),planeAccYRaw(rom.eventCount,0),planeSteps(rom.eventCount,0),planeNextTarget(rom.eventCount,-1),eventX(rom.eventCount,0),eventY(rom.eventCount,0),eventZ(rom.eventCount,0),eventHomeX(rom.eventCount,0),eventHomeY(rom.eventCount,0),eventHomeZ(rom.eventCount,0),eventLimitY(rom.eventCount,0),eventStepY(rom.eventCount,0),eventVelX(rom.eventCount,0),eventVelY(rom.eventCount,0),eventVelZ(rom.eventCount,0){scrollPerTick=rom.type?40.0/60.0:36.0/60.0;refreshCategoryVariant(false);refreshPlayerAssist();initEventRuntime();}
    bool jetpack()const{return rom.type!=0;}
    bool clockFrozen()const{return clockFreezeFrames>0;}
    void emitSfx(unsigned id,unsigned volumeParam=0x100,int forcedVoice=-1){sfxEvents.push_back({id,volumeParam,forcedVoice});}
    std::vector<OriginalSfxEvent> takeSfxEvents(){std::vector<OriginalSfxEvent> out;out.swap(sfxEvents);return out;}
    void refreshCategoryVariant(bool preserveSpawned=true){bonusVariant=!timeTrialVariant&&categoryDeaths>=rom.bonusKindDeaths;activeBossLevel=rom.bossLevelForDeaths(bossDeaths);for(unsigned i=0;i<rom.eventCount;i++){if(preserveSpawned&&eventSpawnTick[i]>=0&&!consumed[i])continue;eventKind[i]=uint8_t(rom.selectedKind(rom.event(i),timeTrialVariant,bonusVariant));}}
    void refreshPlayerAssist(){jetpackMaxHp=(rom.type&&categoryDeaths>=rom.maskAssistDeaths)?120u:100u;if(hp>jetpackMaxHp)hp=jetpackMaxHp;}
    unsigned kind(unsigned i)const{if(i>=eventKind.size())throw std::runtime_error("Category event kind");return eventKind[i];}
    void configureEventRuntime(unsigned i,bool resetLifecycle){auto e=rom.event(i);unsigned k=kind(i);if(resetLifecycle){consumed[i]=eventAnim[i]=eventPose[i]=eventState[i]=0;eventStateTicks[i]=eventCooldown[i]=eventHits[i]=0;eventSpawnTick[i]=-1;planeVelXRaw[i]=planeVelYRaw[i]=planeAccXRaw[i]=planeAccYRaw[i]=planeSteps[i]=0;planeSpeedRaw[i]=0;planeNextTarget[i]=-1;eventVelX[i]=eventVelY[i]=eventVelZ[i]=0.0;}double ex=double(e.offsetX)+rom.actorSpawnX(k),ey=double(e.offsetY)+rom.actorSpawnY(k),ez=double(rom.eventDepth(i));eventHomeX[i]=eventX[i]=ex;eventHomeY[i]=eventY[i]=ey;eventHomeZ[i]=eventZ[i]=ez;eventLimitY[i]=ey;eventStepY[i]=0.0;eventPhase[i]=(i*73u+rom.categoryIndex*41u)&255u;if(rom.type==1&&k==27){eventY[i]=-250.0;}else if(rom.type==1&&k==28){eventLimitY[i]=std::clamp(ey,-63.0,63.0);eventHomeX[i]=eventX[i]=std::clamp(ex,-128.0,128.0);eventY[i]=250.0;eventStepY[i]=(eventLimitY[i]-250.0)/198.0;}eventHp[i]=rom.type?jetHitPoints(k):0;eventChildHp[i]=crateBalloonKind(k)?2:0;}
    void initEventRuntime(){for(unsigned i=0;i<rom.eventCount;i++)configureEventRuntime(i,true);}
    void resetEventHealth(){for(unsigned i=0;i<rom.eventCount;i++){eventHp[i]=rom.type?jetHitPoints(kind(i)):0;eventChildHp[i]=crateBalloonKind(kind(i))?2:0;}}
    void setTimeTrialVariant(bool on){if(timeTrialVariant==on)return;timeTrialVariant=on;refreshCategoryVariant(true);for(unsigned i=0;i<rom.eventCount;i++)if(eventSpawnTick[i]<0&&!consumed[i])configureEventRuntime(i,false);refreshPlayerAssist();}
    void setRetryCounts(unsigned deaths,unsigned boss){categoryDeaths=deaths;bossDeaths=boss;refreshCategoryVariant(false);refreshPlayerAssist();initEventRuntime();}
    void recordPlayerDeath(){if(timeTrialVariant)return;++categoryDeaths;if(rom.type)++bossDeaths;}
    void resetEventsForRetry(){refreshCategoryVariant(false);refreshPlayerAssist();unsigned first=checkpointEvent>=0?unsigned(checkpointEvent+1):0u;for(unsigned i=first;i<rom.eventCount;i++)configureEventRuntime(i,true);}
    std::array<double,3> eventWorld(unsigned i)const{
        if(rom.type&&i<eventX.size())return {eventX[i],eventY[i],eventZ[i]};
        auto e=rom.event(i);unsigned k=kind(i);return {double(e.offsetX)+rom.actorSpawnX(k),double(e.offsetY)+rom.actorSpawnY(k),double(rom.eventDepth(i))};
    }
    Projected project(unsigned i)const{Projected p;p.kind=kind(i);auto pos=eventWorld(i);p.dz=pos[2]-scroll;p.modelX=pos[0];p.modelY=pos[1];if(rom.type){double depth=std::max(1.0,p.dz);double base=std::max(1.0,rom.actorBaseDepth(p.kind));p.scale=std::clamp(base/depth,0.20,3.0);double posScale=28.0/depth;p.sx=120+int(std::lround(p.modelX*posScale));p.sy=103+int(std::lround(p.modelY*posScale));}else{double depth01=std::clamp((190.0-p.dz)/245.0,0.0,1.0);p.scale=0.48+0.62*depth01;p.sx=120+int(std::lround(p.modelX*(0.42+0.58*depth01)));p.sy=48+int(std::lround(depth01*88.0))+int(std::lround((54.0-p.modelY)*0.25));}return p;}
    enum class PolarCallback : uint8_t { None, Wumpa, Crate, Checkpoint, ExtraLife, Mask, Freeze1, Freeze2, Freeze3, Nitro, BoostPad, Launcher, Goal, HazardPersistent, ShockFence };
    enum class JetAiProfile : uint8_t { Static, CannonPlane, Tracker, DepthRunner, BalloonOrbit, Parachute, Rocket, Airship, Ring };
    static bool crateKind(unsigned k){return k==1||k==3||k==8||k==9||k==10||(k>=28&&k<=31)||k==35||k==5||k==6||k==7;}
    static unsigned crateReward(unsigned k){if(k==10)return 4;if(k==29)return 3;if(k==30)return 5;if(k==1||k==28)return 1;return 0;}
    static PolarCallback polarCallback(unsigned k){
        if(k==11)return PolarCallback::Wumpa;
        if(k==3)return PolarCallback::Checkpoint;
        if(k==8||k==35)return PolarCallback::ExtraLife;
        if(k==9||k==31)return PolarCallback::Mask;
        if(k==5)return PolarCallback::Freeze1;
        if(k==6)return PolarCallback::Freeze2;
        if(k==7)return PolarCallback::Freeze3;
        if(k==4)return PolarCallback::Nitro;
        if(k==12)return PolarCallback::BoostPad;
        if(k==22)return PolarCallback::Launcher;
        if(k==25)return PolarCallback::Goal;
        if(k==23)return PolarCallback::ShockFence;
        if(k==13||k==16||k==18||k==20||k==24)return PolarCallback::HazardPersistent;
        if(crateKind(k))return PolarCallback::Crate;
        return PolarCallback::None;
    }
    static bool jetEnemy(unsigned k){return k==1||(k>=4&&k<=9)||(k>=16&&k<=18)||k==27||k==28;}
    static bool jetCrate(unsigned k){return (k>=19&&k<=26)||k==29;}
    static bool airshipKind(unsigned k){return k>=16&&k<=18;}
    static int jetHitPoints(unsigned k){if(k==1)return 4;if(k>=4&&k<=9)return 2;if(k==16)return 30;if(k==17)return 45;if(k==18)return 60;if(k>=19&&k<=27)return 2;if(k==28)return 1;if(k==29)return 2;if(k==31)return 100000;return 0;}
    static unsigned jetContactDamage(unsigned k){if(k>=4&&k<=9)return 10;if(k==27)return 20;if(k==28)return 14;if(k==1)return 6;return 12;}
    static JetAiProfile jetAiProfile(unsigned k){if(k==1)return JetAiProfile::CannonPlane;if(k>=5&&k<=8)return JetAiProfile::Tracker;if(k==9)return JetAiProfile::DepthRunner;if((k>=19&&k<=26)||k==29)return JetAiProfile::BalloonOrbit;if(k==27)return JetAiProfile::Parachute;if(k==28)return JetAiProfile::Rocket;if(airshipKind(k))return JetAiProfile::Airship;if(k==31)return JetAiProfile::Ring;return JetAiProfile::Static;}
    struct AirshipAttack {int hp,fireGap,fireVolley,initialTimer,cannonGap,cannonVolley,cannonRest;};
    static AirshipAttack airshipAttack(unsigned k,unsigned bossLevel=0){static constexpr AirshipAttack a[6]={{30,120,1,120,15,7,90},{45,90,3,120,15,8,80},{60,60,5,120,15,9,70},{30,120,1,150,15,5,110},{40,90,3,150,15,6,100},{50,60,5,150,15,7,90}};unsigned base=bossLevel>=3?3u:0u;return a[base+std::clamp<unsigned>(k,16,18)-16];}
    struct HoverAttack {int sideDelay,sideBurst,sideRest,cannonDelay,cannonBurst,cannonRest,launcherDelay,launcherBurst,launcherRest;};
    static HoverAttack hoverAttack(unsigned profile){static constexpr HoverAttack a[2]={{45,6,210,20,5,90,32,3,160},{70,4,230,20,3,90,40,2,160}};return a[std::min(profile,1u)];}
    CategoryBox jetBox(unsigned k)const{if(airshipKind(k))return {-102,-12,-2,51,68,4};return rom.actorBox(k);}
    static CategoryBox jetpackRocketTriggeredBox(){return {-21,-21,-2,42,42,4};}

    void decay(double& v,double step){if(v>0)v=std::max(0.0,v-step);else if(v<0)v=std::min(0.0,v+step);}
    bool jetPlayerInactive()const{return jetPlayerState==4||jetPlayerState==5;}
    void queueJetpackWumpa(unsigned amount){if(timeTrialVariant||!amount)return;if(!jetpackQueuedWumpa)jetpackWumpaDispenseTimer=0xf;jetpackQueuedWumpa+=amount;}
    void spawnJetpackCollectedWumpa(unsigned amount){
        JetCollectedWumpa q;q.x=120.0+x;q.y=103.0+y;q.reward=amount;
        const double tx=16.0,ty=16.0;double manhattan=std::abs(q.x-tx)+std::abs(q.y-ty);unsigned steps=std::max(1u,unsigned(std::floor(manhattan/8.0)));q.vx=(tx-q.x)/double(steps);q.vy=(ty-q.y)/double(steps);jetpackCollectedWumpas.push_back(q);
    }
    void updateJetpackCollectedWumpas(){
        for(auto& q:jetpackCollectedWumpas){q.x+=q.vx;q.y+=q.vy;++q.age;if(q.x<=16.0||q.y<=16.0){wumpa+=q.reward;emitSfx(0x0e);q.age=999;}}
        jetpackCollectedWumpas.erase(std::remove_if(jetpackCollectedWumpas.begin(),jetpackCollectedWumpas.end(),[](const JetCollectedWumpa& q){return q.age>=999||q.age>180;}),jetpackCollectedWumpas.end());
    }
    void dispenseJetpackWumpa(){
        if(!jetpackQueuedWumpa)return;
        if(jetPlayerInactive()){wumpa+=jetpackQueuedWumpa;jetpackQueuedWumpa=0;jetpackWumpaDispenseTimer=0;return;}
        if(jetpackWumpaDispenseTimer){--jetpackWumpaDispenseTimer;return;}
        jetpackWumpaDispenseTimer=0xf;unsigned amount=jetpackQueuedWumpa<=9?1u:(jetpackQueuedWumpa<=0x13?2u:(jetpackQueuedWumpa<=0x27?4u:8u));spawnJetpackCollectedWumpa(amount);jetpackQueuedWumpa-=amount;
    }
    void updateJetpackBomberSfx(){
        unsigned count=0;
        for(unsigned i=0;i<rom.eventCount;i++)if(eventSpawnTick[i]>=0&&!consumed[i]&&kind(i)>=4&&kind(i)<=9&&eventState[i]!=6)++count;
        for(const auto& b:spawnedBombers)if(!b.dying)++count;
        if(!count)return;
        if(jetpackBomberSfxTimer--<=0){jetpackBomberSfxTimer=0x16;emitSfx(0x37,std::min(0x100u,count*48u));}
    }
    void passJetpackRing(unsigned i){
        if(i>=rom.eventCount)return;
        x=eventX[i];y=eventY[i];jetVx=jetVy=0;jetPlayerState=6;jetPlayerStateTicks=0;
        if(timeTrialVariant)return;
        int delta=int(ticks)-jetpackRingLastFrame;
        if(delta<=0x14)return;
        if(delta>0xbe)jetpackRingChain=0;
        switch(jetpackRingChain){case 0:queueJetpackWumpa(1);break;case 1:queueJetpackWumpa(5);break;case 2:queueJetpackWumpa(0x14);break;case 3:{unsigned heal=std::max(1u,jetpackMaxHp/5u);hp=std::min(jetpackMaxHp,hp+heal);break;}case 4:++lives;emitSfx(0x07);break;default:break;}
        jetpackRingLastFrame=int(ticks);jetpackRingChain=(jetpackRingChain+1)%5u;++jetpackRingRewards;
    }
    void respawnJetpack(){if(rom.type==2)resetHovercraftFight();resetEventsForRetry();hp=jetpackMaxHp;scroll=checkpointScroll;x=checkpointX;y=checkpointY;jetVx=jetVy=0;jetPlayerState=1;jetPlayerStateTicks=0;jetPlayerDepth=28.0;jetFadeStarted=false;rollTicks=0;rollDir=0;jetpackRingChain=0;jetpackRingLastFrame=-0xbe;jetpackQueuedWumpa=0;jetpackWumpaDispenseTimer=0;jetpackBomberSfxTimer=0;shots.clear();enemyShots.clear();spawnedBombers.clear();hoverFireballs.clear();jetpackCollectedWumpas.clear();releasedBalloons.clear();poppedBalloons.clear();}
    void startJetpackFinish(){if(jetPlayerState==5)return;jetPlayerState=5;jetPlayerStateTicks=0;jetPlayerDepth=28.0;jetVx=jetVy=0;rollTicks=0;rollDir=0;emitSfx(0x3b);}
    void step(bool left,bool right,bool action=false,bool up=false,bool down=false,bool rollLeft=false,bool rollRight=false){
        if(complete)return;
        ++ticks;if(invulnFrames)--invulnFrames;if(shotCooldown)--shotCooldown;
        if(rom.type){dispenseJetpackWumpa();
            const double accel=.25,lim=2.25;
            if(jetPlayerState==1){
                if(up&&!down)jetVy-=accel;else if(down&&!up)jetVy+=accel;else decay(jetVy,accel);
                jetVy=std::clamp(jetVy,-lim,lim);
                if(left&&!right)jetVx-=accel;else if(right&&!left)jetVx+=accel;else decay(jetVx,accel);
                jetVx=std::clamp(jetVx,-lim,lim);
                if(rollLeft!=rollRight){jetPlayerState=rollLeft?2u:3u;jetPlayerStateTicks=0;rollDir=rollLeft?-1:1;rollTicks=rom.animationTicks(0,rollLeft?1u:2u);}
                else if(action&&!shotCooldown){JetShot s;s.x=x+18;s.y=y-24;s.z=scroll+28.0+10.0/256.0;s.vx=x*409.0/4096.0;s.vy=y*409.0/4096.0;shots.push_back(s);shotCooldown=0x12;++shotsFired;emitSfx(0x24,0xA0,2);}
            }else if(jetPlayerState==2||jetPlayerState==3){
                if(up&&!down)jetVy-=accel;else if(down&&!up)jetVy+=accel;else decay(jetVy,accel);
                jetVy=std::clamp(jetVy,-lim,lim);
                ++jetPlayerStateTicks;
                rollDir=jetPlayerState==2?-1:1;
                if(jetPlayerStateTicks<=5){jetVx+=double(rollDir);jetVx=std::clamp(jetVx,-5.0,5.0);}
                else{jetVx-=double(rollDir)*(45.0/256.0);if(std::abs(jetVx)<=45.0/256.0)jetVx=0;}
                if(jetPlayerStateTicks>0x21){
                    if(left&&!right)jetVx-=accel;else if(right&&!left)jetVx+=accel;else decay(jetVx,accel);
                    jetVx=std::clamp(jetVx,-lim,lim);
                    if(rollLeft!=rollRight){jetPlayerState=rollLeft?2u:3u;jetPlayerStateTicks=0;rollDir=rollLeft?-1:1;}
                    else if(jetPlayerStateTicks>=rom.animationTicks(0,jetPlayerState==2?1u:2u)){jetPlayerState=1;jetPlayerStateTicks=0;rollDir=0;}
                }
                if(jetPlayerState==1)rollTicks=0;else{unsigned total=rom.animationTicks(0,jetPlayerState==2?1u:2u);rollTicks=total-std::min(total,jetPlayerStateTicks);}
            }else if(jetPlayerState==4){
                ++jetPlayerStateTicks;
                jetVy+=9.0/256.0;
                if(std::abs(jetVy)>double(0x140)/256.0)jetVy=double(0x140)/256.0;
                if(!jetFadeStarted&&y>double(0x7080)/256.0)jetFadeStarted=true;
            }else if(jetPlayerState==5){
                ++jetPlayerStateTicks;
                jetPlayerDepth+=2.0;
                if(!jetFadeStarted&&jetPlayerDepth>double(0x8200)/256.0)jetFadeStarted=true;
                if(jetPlayerDepth>double(0xa000)/256.0)complete=true;
            }else if(jetPlayerState==6){
                ++jetPlayerStateTicks;
                jetVx=jetVy=0;
                if(jetPlayerStateTicks>=0x32){jetPlayerState=1;jetPlayerStateTicks=0;}
            }
            if(jetPlayerInactive()){x+=jetVx;y+=jetVy;}else{x=std::clamp(x+jetVx,-128.0,128.0);y=std::clamp(y+jetVy,-75.0,75.0);}actionHeld=action;
            scroll+=scrollPerTick;updateJetpackSpawns();updateJetpackActors();updateSpawnedBombers();updateJetpackBomberSfx();updateHoverFireballs();updateJetpackCollectedWumpas();updateReleasedBalloons();updatePoppedBalloons();stepShots();stepBlasts();if(collisions)stepEnemyShots();else enemyShots.clear();updateJetpackStates();if(collisions)handleJetpackCollisions();
            if(jetPlayerState==4&&y>double(0xe100)/256.0)respawnJetpack();
        }else{
            const bool steeringLocked=polarBoostFrames||polarLaunchActive||polarShockFrames||polarFinishHaltFrames;
            if(!steeringLocked&&left!=right){++steerTicks;double d=steerTicks>13?3.5:double(0x2cd)/256.0;x+=left?-d:d;x=std::clamp(x,-50.0,50.0);}else steerTicks=0;
            if(!steeringLocked&&action&&!jumpHeld&&grounded){grounded=false;vy=-4.75;}jumpHeld=action;
            if(!grounded){jumpY+=vy;vy+=0.34;if(jumpY>=0){jumpY=0;vy=0;grounded=true;if(polarLaunchActive)polarLaunchActive=false;}}
            if(polarShockFrames){if(--polarShockFrames==0){if(lives)--lives;recordPlayerDeath();resetEventsForRetry();resetPolarToCheckpoint();}}
            else if(polarFinishHaltFrames){if(--polarFinishHaltFrames==0)complete=true;}
            else if(freezeFrames)--freezeFrames;
            else{double mul=1.0;if(polarBoostFrames)mul=double(0x55)/double(0x24);else if(polarLaunchActive)mul=double(0x1c)/double(0x24);scroll+=scrollPerTick*mul;}
            if(polarBoostFrames)--polarBoostFrames;
            if(polarFinishTimer&&--polarFinishTimer==0)polarFinishHaltFrames=rom.animationTicks(0,10)+polarFinishLeapFrames();
            if(collisions&&!polarShockFrames&&!polarFinishHaltFrames)handleCollisions();
            updatePolarStates();
        }
        if(clockFreezeFrames)--clockFreezeFrames;
        if(rom.type!=2&&scroll>=rom.routeEndThreshold){scroll=rom.routeEndThreshold;if(rom.type)startJetpackFinish();else complete=true;}
    }

    void resetPolarToCheckpoint(){scroll=checkpointScroll;x=checkpointX;jumpY=0;vy=0;grounded=true;polarLaunchActive=false;polarBoostFrames=0;polarShockFrames=0;}
    bool hurtPolar(bool shock=false){
        if(invulnFrames)return false;
        if(maskLevel){--maskLevel;invulnFrames=0x4b;return true;}
        ++hazards;
        if(shock){polarShockFrames=0x2d;polarBoostFrames=0;polarLaunchActive=false;return true;}
        if(lives)--lives;
        recordPlayerDeath();
        resetEventsForRetry();
        resetPolarToCheckpoint();
        return true;
    }
    static CategoryBox nitroBlastBox(){return {-15,-15,-2,30,30,5};}
    static constexpr unsigned polarFinishLeapFrames(){return unsigned((0x2f00-0x03ff+0x3c-1)/0x3c);}
    static bool overlapWorldBox(const std::array<double,3>& a,const CategoryBox& ab,const std::array<double,3>& b,const CategoryBox& bb){return a[0]+ab.x<b[0]+bb.x+bb.w&&a[0]+ab.x+ab.w>b[0]+bb.x&&a[1]+ab.y<b[1]+bb.y+bb.h&&a[1]+ab.y+ab.h>b[1]+bb.y&&a[2]+ab.z<b[2]+bb.z+bb.d&&a[2]+ab.z+ab.d>b[2]+bb.z;}
    void startPolarAnim(unsigned i,unsigned anim){eventAnim[i]=uint8_t(anim);eventStateTicks[i]=0;}
    void detonateNearbyPolarNitros(unsigned source){const auto a=eventWorld(source);const auto box=nitroBlastBox();for(unsigned j=0;j<rom.eventCount;j++){if(j==source||consumed[j]||eventAnim[j]||kind(j)!=4)continue;if(overlapWorldBox(a,box,eventWorld(j),box)){startPolarAnim(j,18);++crates;++nitroChains;}}}
    void updatePolarStates(){for(unsigned i=0;i<rom.eventCount;i++)if(eventAnim[i]){++eventStateTicks[i];if(kind(i)==4&&eventAnim[i]==18&&eventStateTicks[i]==0x14)detonateNearbyPolarNitros(i);if(eventStateTicks[i]>=rom.animationTicks(kind(i),eventAnim[i])){consumed[i]=1;eventAnim[i]=0;}}}
    void handleCollisions(){
        const double pcx=120.0+x,pcy=110.0+jumpY;const CategoryBox pbox=rom.actorBox(0);const std::array<double,4> pb{pcx+pbox.x,pcy+pbox.y,pcx+pbox.x+pbox.w,pcy+pbox.y+pbox.h};const double pz0=pbox.z,pz1=pbox.z+pbox.d;
        for(unsigned i=0;i<rom.eventCount;i++){if(consumed[i]||eventAnim[i])continue;unsigned k=kind(i);if(!rom.spawnableKind(k))continue;auto p=project(i);CategoryBox box=rom.actorBox(k);if(!box.valid())continue;const double ez0=p.dz+box.z,ez1=ez0+box.d;if(!(pz0<ez1&&pz1>ez0))continue;std::array<double,4> eb{p.sx+box.x*p.scale,p.sy+box.y*p.scale,p.sx+(box.x+box.w)*p.scale,p.sy+(box.y+box.h)*p.scale};bool hit=pb[0]<eb[2]&&pb[2]>eb[0]&&pb[1]<eb[3]&&pb[3]>eb[1];if(!hit)continue;
            const PolarCallback cb=polarCallback(k);
            if(cb==PolarCallback::Wumpa){consumed[i]=1;++wumpa;continue;}
            if(cb==PolarCallback::Crate||cb==PolarCallback::Checkpoint||cb==PolarCallback::ExtraLife||cb==PolarCallback::Mask||cb==PolarCallback::Freeze1||cb==PolarCallback::Freeze2||cb==PolarCallback::Freeze3){consumed[i]=1;++crates;wumpa+=crateReward(k);if(cb==PolarCallback::Checkpoint){checkpointScroll=scroll;checkpointX=x;checkpointEvent=int(i);++checkpointHits;}if(cb==PolarCallback::ExtraLife)++lives;if(cb==PolarCallback::Mask){maskLevel=std::min(3u,maskLevel+1);if(maskLevel==3)invulnFrames=500;}if(cb==PolarCallback::Freeze1)freezeFrames=60;if(cb==PolarCallback::Freeze2)freezeFrames=120;if(cb==PolarCallback::Freeze3)freezeFrames=180;continue;}
            if(cb==PolarCallback::BoostPad){x=std::clamp(p.modelX,-50.0,50.0);polarBoostFrames=0x1e;++polarBoostHits;continue;}
            if(cb==PolarCallback::Launcher){polarBoostFrames=0;polarLaunchActive=true;grounded=false;jumpY=0;vy=-7.5;startPolarAnim(i,1);++polarLauncherHits;continue;}
            if(cb==PolarCallback::Goal){if(!polarFinishTimer&&!polarFinishHaltFrames){polarFinishTimer=0x16;++polarGoalHits;}continue;}
            if(cb==PolarCallback::Nitro){++crates;if(hurtPolar(false)){startPolarAnim(i,18);break;}continue;}
            if(cb==PolarCallback::ShockFence){if(hurtPolar(true))break;continue;}
            if(cb==PolarCallback::HazardPersistent){if(hurtPolar(false))break;continue;}
        }
    }

    bool worldOverlap(unsigned i,double px,double py,double pz,const CategoryBox& pb)const{unsigned k=kind(i);auto pos=eventWorld(i);CategoryBox b=(k==28&&eventState[i]!=0)?jetpackRocketTriggeredBox():jetBox(k);double ex=pos[0],ey=pos[1],ez=pos[2];return px+pb.x<ex+b.x+b.w&&px+pb.x+pb.w>ex+b.x&&py+pb.y<ey+b.y+b.h&&py+pb.y+pb.h>ey+b.y&&pz+pb.z<ez+b.z+b.d&&pz+pb.z+pb.d>ez+b.z;}
    void resetHovercraftFight(){if(hovercraftEvent>=0&&unsigned(hovercraftEvent)<rom.eventCount){consumed[unsigned(hovercraftEvent)]=0;eventSpawnTick[unsigned(hovercraftEvent)]=-1;}hovercraftEvent=-1;hovercraftSpawned=false;hovercraftGone=false;hovercraftFinishTriggered=false;hovercraftState=hovercraftPhase=hovercraftFrameCount=hovercraftPartsLeft=hovercraftPartsDestroyed=0;hovercraftHitFlashTimer=0;hovercraftHitFlashOn=false;hovercraftPalette15White=false;hovercraftX=hovercraftY=hovercraftZ=hovercraftVelX=hovercraftVelY=hovercraftVelZ=hovercraftOrbitRadius=0;hoverParts={};spawnedBombers.clear();hoverFireballs.clear();jetpackCollectedWumpas.clear();releasedBalloons.clear();poppedBalloons.clear();}
    void damageJetpack(unsigned amount){
        if(jetPlayerInactive())return;
        if((jetPlayerState==2||jetPlayerState==3)&&jetPlayerStateTicks<=0x10)return;
        if(invulnFrames)return;
        hp=amount>=hp?0:hp-amount;
        if(hp==0){if(!timeTrialVariant&&lives)--lives;recordPlayerDeath();jetPlayerState=4;jetPlayerStateTicks=0;jetFadeStarted=false;jetVy=0;jetVx=std::clamp(jetVx,-double(0x240)/256.0,double(0x240)/256.0)/2.0;rollTicks=0;rollDir=0;invulnFrames=0;}
        else{invulnFrames=45;emitSfx(0x42);}
    }
    static unsigned crateBalloonKind(unsigned k){if(k==19)return 42;if(k>=20&&k<=23)return 41;if((k>=24&&k<=26)||k==29)return 40;return 0;}
    void releaseJetpackCrateBalloon(unsigned i,unsigned k){unsigned bk=crateBalloonKind(k);if(!bk)return;JetReleasedBalloon b;b.kind=bk;b.x=eventX[i];b.y=eventY[i]+double(int32_t(0xffffc24a))/256.0;b.z=eventZ[i];releasedBalloons.push_back(b);}
    void updateReleasedBalloons(){for(auto& b:releasedBalloons){if(b.hp<=0)continue;b.y+=b.vy;b.vy-=6.0/256.0;if(b.vy>-double(0x12c)/256.0)b.vy=-double(0x12c)/256.0;++b.age;}releasedBalloons.erase(std::remove_if(releasedBalloons.begin(),releasedBalloons.end(),[](const JetReleasedBalloon& b){return b.hp<=0||b.y<-double(0xe100)/256.0||b.age>360;}),releasedBalloons.end());}
    std::array<double,3> attachedBalloonWorld(unsigned i)const{return {eventX[i],eventY[i]+double(int32_t(0xffffc24a))/256.0,eventZ[i]};}
    void popJetpackCrateBalloon(unsigned i,unsigned k){if(i>=rom.eventCount||eventState[i]!=0||eventAnim[i]||eventChildHp[i]>0)return;unsigned bk=crateBalloonKind(k);if(!bk)return;auto pos=attachedBalloonWorld(i);JetPoppedBalloon b;b.kind=bk;b.x=pos[0];b.y=pos[1];b.z=pos[2];poppedBalloons.push_back(b);eventState[i]=1;eventStateTicks[i]=0;eventVelY[i]=0;eventChildHp[i]=0;++crates;emitSfx(0x2e);}
    void updatePoppedBalloons(){for(auto& b:poppedBalloons)++b.age;poppedBalloons.erase(std::remove_if(poppedBalloons.begin(),poppedBalloons.end(),[&](const JetPoppedBalloon& b){return b.age>=rom.animationTicks(b.kind,1);}),poppedBalloons.end());}
    void breakJetpackCrate(unsigned i,unsigned k){
        if(consumed[i]||eventAnim[i])return;
        const bool alreadyBroken=eventState[i]==1;
        const bool hadAttachedBalloon=eventState[i]==0&&eventChildHp[i]>0;
        eventHp[i]=0;eventChildHp[i]=0;eventState[i]=2;eventAnim[i]=1;eventStateTicks[i]=0;
        if(hadAttachedBalloon)releaseJetpackCrateBalloon(i,k);
        if(k==23)emitSfx(0x07);else if(k==29)emitSfx(0x18);else emitSfx(0x03);
        if(k==19){unsigned heal=std::max(1u,jetpackMaxHp/5u);hp=std::min(jetpackMaxHp,hp+heal);if(!alreadyBroken)++crates;}
        else if(k>=20&&k<=22){static constexpr unsigned reward[3]={1,3,5};queueJetpackWumpa(reward[k-20]);if(!alreadyBroken)++crates;}
        else if(k==23){++lives;if(!alreadyBroken)++crates;}
        else if(k>=24&&k<=26){clockFreezeFrames+=unsigned(k-23)*60;if(!alreadyBroken)++crates;}
        else if(k==29){timeTrialStartRequested=true;}
    }
    void breakJetpackParachute(unsigned i){if(consumed[i]||eventAnim[i])return;eventHp[i]=0;eventState[i]=1;eventAnim[i]=1;eventStateTicks[i]=0;++crates;emitSfx(0x04);}
    void launchJetpackRocket(unsigned i,unsigned anim){if(consumed[i]||eventAnim[i])return;eventState[i]=1;eventAnim[i]=uint8_t(anim);eventStateTicks[i]=0;if(anim==2)eventHits[i]=1;emitSfx(0x04);}
    void resolveJetpackEvent(unsigned i,unsigned k){if(consumed[i])return;if((k>=19&&k<=26)||k==29){breakJetpackCrate(i,k);return;}if(k==27){breakJetpackParachute(i);return;}if(k==28){launchJetpackRocket(i,1);return;}consumed[i]=1;if(jetEnemy(k))++enemiesDestroyed;}
    void handleJetpackCollisions(){if(jetPlayerInactive())return;const CategoryBox pb=rom.actorBox(0);const double pz=scroll+28.0;for(unsigned i=0;i<rom.eventCount;i++){if(consumed[i]||eventSpawnTick[i]<0)continue;unsigned k=kind(i);if(!rom.spawnableKind(k))continue;if(airshipKind(k)&&(eventState[i]<2||eventState[i]>3))continue;if(!worldOverlap(i,x,y,pz,pb))continue;if(jetCrate(k)){if(!eventAnim[i])resolveJetpackEvent(i,k);continue;}if(k==31){if(!eventState[i]){eventState[i]=1;++ringPasses;emitSfx(0x3e);}passJetpackRing(i);continue;}if(k==27){if(!eventAnim[i]){damageJetpack(20);breakJetpackParachute(i);}continue;}if(k==28){if(eventAnim[i]){if(!eventHits[i]){damageJetpack(14);eventHits[i]=1;}continue;}damageJetpack(14);eventHits[i]=1;launchJetpackRocket(i,1);continue;}if(jetEnemy(k)){damageJetpack(jetContactDamage(k));if(k==1){damageJetpackPlane(i);}else if(k>=4&&k<=9){killEventBomber(i);}}}
        if(rom.type==2&&hovercraftSpawned&&!hovercraftGone&&(hovercraftState==2||hovercraftState==3)){CategoryBox b{-102,-12,-2,51,68,4};if(x+pb.x<hovercraftX+b.x+b.w&&x+pb.x+pb.w>hovercraftX+b.x&&y+pb.y<hovercraftY+b.y+b.h&&y+pb.y+pb.h>hovercraftY+b.y&&pz+pb.z<hovercraftZ+b.z+b.d&&pz+pb.z+pb.d>hovercraftZ+b.z)damageJetpack(12);}
    }
    static int64_t asrFloor(int64_t v,unsigned n){if(v>=0)return v>>n;int64_t m=(int64_t(1)<<n)-1;return -(((-v)+m)>>n);}
    static int planeMarkerSpeed(unsigned markerKind){static constexpr int speed[6]={0x1555,0x1155,0x0d55,0x0955,0x0555,0x0155};return markerKind>=0x20&&markerKind<=0x25?speed[markerKind-0x20]:0x0955;}
    void aimJetpackPlane(unsigned i,int target){
        planeNextTarget[i]=-1;planeAccXRaw[i]=planeAccYRaw[i]=0;
        if(target<0||unsigned(target)>=rom.eventCount){if(planeSpeedRaw[i]<=0x0955){eventState[i]=1;eventPose[i]=3;}else{eventState[i]=0;eventPose[i]=0;planeSteps[i]=0x3fffffff;}return;}
        unsigned marker=kind(unsigned(target));planeSpeedRaw[i]=planeMarkerSpeed(marker);
        int64_t curX=std::llround(eventX[i]*256.0),curY=std::llround(eventY[i]*256.0),curZ=std::llround(eventZ[i]*256.0);
        int64_t targetX=int64_t(rom.event(unsigned(target)).offsetX)*256,targetY=int64_t(rom.event(unsigned(target)).offsetY)*256,targetZ=int64_t(rom.eventDepth(unsigned(target)))*256;
        int steps=int(asrFloor(((targetZ-curZ)*256)/std::max(1,planeSpeedRaw[i]),4));if(steps<=0)steps=1;planeSteps[i]=steps;
        int scale=0x8000/steps;if(scale<=0)scale=1;
        int64_t remX=(targetX-curX)-asrFloor(int64_t(planeVelXRaw[i])*steps,4),remY=(targetY-curY)-asrFloor(int64_t(planeVelYRaw[i])*steps,4);
        planeAccXRaw[i]=int(asrFloor(asrFloor(remX*scale,13)*int64_t(scale*2),13));planeAccYRaw[i]=int(asrFloor(asrFloor(remY*scale,13)*int64_t(scale*2),13));
        planeNextTarget[i]=rom.eventTarget(unsigned(target));eventState[i]=0;eventPose[i]=planeSpeedRaw[i]<=0x0955?3:0;
    }
    void initJetpackPlane(unsigned i){planeVelXRaw[i]=planeVelYRaw[i]=0;planeSpeedRaw[i]=0x0955;eventStateTicks[i]=0;eventCooldown[i]=0;eventHits[i]=0;eventHp[i]=4;int target=rom.eventTarget(i);if(target>=0&&unsigned(target)<rom.eventCount&&planeMarkerSpeed(kind(unsigned(target)))>0x0955)eventZ[i]-=double(0x8e00)/256.0;aimJetpackPlane(i,target);}
    bool trySpawnEnemyCannonball(unsigned i){
        if(eventCooldown[i]){--eventCooldown[i];return false;}
        int64_t px=std::llround(x*256.0),py=std::llround(y*256.0),pz=std::llround((scroll+28.0)*256.0),ex=std::llround(eventX[i]*256.0),ey=std::llround(eventY[i]*256.0),ez=std::llround(eventZ[i]*256.0);
        int angle=int((pz-(ez-10))/-0x1aa);if(angle<=0||eventZ[i]-scroll>double(0x8bff)/256.0)return false;int scale=0x1000/angle;if(scale<=0)return false;
        int64_t rawDx=(px-ex)*scale,rawDy=(py-ey)*scale;int64_t dx=asrFloor(rawDx,12),dy=asrFloor(rawDy,12);if(std::llabs(dx)+std::llabs(dy)>0x5ff)return false;
        JetShot shot;shot.x=eventX[i];shot.y=eventY[i];shot.z=eventZ[i]-10.0/256.0;shot.vx=double(dx)/256.0;shot.vy=double(dy)/256.0;shot.vz=-1.0;shot.damage=2;shot.actorKind=3;enemyShots.push_back(shot);++enemyShotsFired;
        if(++eventHits[i]>=3){eventHits[i]=0;eventCooldown[i]=60;}else eventCooldown[i]=20;return true;
    }
    void updateJetpackPlane(unsigned i){
        eventX[i]+=double(asrFloor(planeVelXRaw[i],4))/256.0;eventY[i]+=double(asrFloor(planeVelYRaw[i],4))/256.0;eventZ[i]+=double(asrFloor(planeSpeedRaw[i],4))/256.0;++eventStateTicks[i];
        if(eventState[i]==0){planeVelXRaw[i]+=planeAccXRaw[i];planeVelYRaw[i]+=planeAccYRaw[i];if(--planeSteps[i]<=0)aimJetpackPlane(i,planeNextTarget[i]);}
        else if(eventState[i]==1){int64_t px=std::llround(x*256.0),py=std::llround(y*256.0),ex=std::llround(eventX[i]*256.0),ey=std::llround(eventY[i]*256.0);planeVelXRaw[i]=int(asrFloor(px-ex,4));planeVelYRaw[i]=int(asrFloor(py-ey,4));}
        else if(eventState[i]==2){if(eventStateTicks[i]>=rom.animationTicks(1,eventPose[i])){eventState[i]=3;eventStateTicks[i]=0;planeAccYRaw[i]=0x0a0;eventPose[i]=eventPose[i]==1?2:5;}}
        else if(eventState[i]==3){planeVelYRaw[i]+=planeAccYRaw[i];planeVelYRaw[i]=std::min(planeVelYRaw[i],0x1400);if(eventY[i]>double(0xe100)/256.0){consumed[i]=1;++enemiesDestroyed;return;}}
        if(eventState[i]<2&&eventPose[i]==3)trySpawnEnemyCannonball(i);
    }
    void damageJetpackPlane(unsigned i){if(eventState[i]>=2)return;eventHp[i]=0;planeVelXRaw[i]/=2;if(planeVelYRaw[i]<0)planeVelYRaw[i]/=2;eventState[i]=2;eventStateTicks[i]=0;eventPose[i]=eventPose[i]==0?1:4;emitSfx(0x25);}
    void initAirship(unsigned i,unsigned k){auto a=airshipAttack(k,activeBossLevel);eventState[i]=1;eventStateTicks[i]=0;eventZ[i]=eventHomeZ[i]+160.0;eventVelX[i]=eventVelY[i]=0.0;eventVelZ[i]=double(0x66)/256.0;eventCooldown[i]=unsigned(a.initialTimer);eventHits[i]=0;eventHp[i]=a.hp;}
    void initHovercraft(unsigned i){hovercraftEvent=int(i);hovercraftSpawned=true;hovercraftGone=false;hovercraftFinishTriggered=false;hovercraftState=1;hovercraftPhase=0;hovercraftFrameCount=0;hovercraftOrbitRadius=0;hovercraftPartsLeft=4;hovercraftPartsDestroyed=0;hovercraftHitFlashTimer=0;hovercraftHitFlashOn=false;hovercraftProfile=std::min(activeBossLevel,1u);hovercraftX=eventHomeX[i];hovercraftY=eventHomeY[i];hovercraftZ=eventHomeZ[i];hovercraftVelX=hovercraftVelY=0;hovercraftVelZ=double(0x66)/256.0;auto a=hoverAttack(hovercraftProfile);int sideHp=hovercraftProfile?16:24;hoverParts={{{32,48,-1,15,15,11,unsigned(a.cannonDelay),0,false,false},{30,-48,-1,25,25,12,0,0,false,false},{-65,10,-1,sideHp,sideHp,13,unsigned(a.sideDelay),0,false,true},{132,10,-1,sideHp,sideHp,13,unsigned(a.sideDelay),0,false,true}}};hoverParts[2].animIndex=0;hoverParts[3].animIndex=1;consumed[i]=1;}
    std::array<double,3> hoverPartWorld(unsigned p)const{const auto& q=hoverParts.at(p);return {hovercraftX+q.ox,hovercraftY+q.oy,hovercraftZ+q.oz};}
    void updateJetpackSpawns(){for(unsigned i=0;i<rom.eventCount;i++)if(eventSpawnTick[i]<0&&double(rom.eventDepth(i))-scroll<=169.0){eventSpawnTick[i]=int(ticks);unsigned k=kind(i);if(rom.type==2&&k==10)initHovercraft(i);else if(k==1)initJetpackPlane(i);else if(airshipKind(k))initAirship(i,k);if(k==28)emitSfx(0x2d);}}
    void spawnAirshipProjectile(unsigned i,unsigned damage,bool fireball){JetShot s;s.x=eventX[i]-double(0xCDB)/256.0;s.y=eventY[i]+double(0x516D)/256.0;s.z=eventZ[i]-10.0;double targetZ=scroll+28.0,steps=std::max(18.0,std::abs(targetZ-s.z)/(fireball?2.0:3.0));s.vx=(x-s.x)/steps;s.vy=(y-s.y)/steps;s.vz=(targetZ-s.z)/steps;s.damage=damage;s.actorKind=fireball?38u:3u;enemyShots.push_back(s);++enemyShotsFired;++airshipProjectilesFired;}
    void steerAirship(unsigned i){eventX[i]+=eventVelX[i];eventY[i]+=eventVelY[i];double dx=x-eventX[i],dy=y-eventY[i];if(std::abs(dx)<45.0&&std::abs(dx)>1.0)eventVelX[i]+=dx>0?0.012:-0.012;if(std::abs(dy)<45.0&&std::abs(dy)>1.0)eventVelY[i]+=dy>0?0.008:-0.008;eventVelX[i]=std::clamp(eventVelX[i],-1.5,1.5);eventVelY[i]=std::clamp(eventVelY[i],-1.0,1.0);}
    uint16_t fxRandRange(unsigned max){fxRandSeed=fxRandSeed*0x41C64E6Du+0x3039u;return max?uint16_t((fxRandSeed>>4)&0xffffu)%max:0;}
    void spawnAirshipExplosions(unsigned i,unsigned count){for(unsigned n=0;n<count;n++){JetBlast b;b.x=eventX[i]-102.0+double(fxRandRange(51u*256u))/256.0;b.y=eventY[i]-12.0+double(fxRandRange(68u*256u))/256.0;b.z=eventZ[i]-1.0;b.age=0;b.maxAge=28+(n&3u)*3u;blasts.push_back(b);}airshipExplosionsSpawned+=count;}
    void stepBlasts(){for(auto& b:blasts)++b.age;blasts.erase(std::remove_if(blasts.begin(),blasts.end(),[](const JetBlast& b){return b.age>=b.maxAge;}),blasts.end());}
    void beginAirshipExplosion(unsigned i){eventHp[i]=0;eventState[i]=4;eventStateTicks[i]=0;eventVelX[i]=eventVelY[i]=0.0;eventVelZ[i]=double(0xAA)/256.0;eventCooldown[i]=0;}
    void updateAirship(unsigned i,unsigned k){auto a=airshipAttack(k,activeBossLevel);double depth=eventZ[i]-scroll;unsigned st=eventState[i];++eventStateTicks[i];if(st==1){eventZ[i]+=eventVelZ[i];if(depth<=double(0x81FF)/256.0){eventState[i]=2;eventStateTicks[i]=0;eventVelX[i]=eventVelY[i]=0.0;}}
        else if(st==2){eventZ[i]+=eventVelZ[i];double target=double(0x98)/256.0;eventVelZ[i]+=eventVelZ[i]<=target?1.0/256.0:-1.0/256.0;if(eventCooldown[i])--eventCooldown[i];else{spawnAirshipProjectile(i,6,true);if(++eventHits[i]>=unsigned(a.fireVolley)){eventHits[i]=0;eventCooldown[i]=unsigned(a.initialTimer);}else eventCooldown[i]=unsigned(a.fireGap);}steerAirship(i);if(depth<=double(0x31FF)/256.0){eventState[i]=3;eventStateTicks[i]=0;eventCooldown[i]=unsigned(a.cannonGap);eventHits[i]=0;}}
        else if(st==3){eventZ[i]+=eventVelZ[i];double target=double(0xB2)/256.0;if(eventVelZ[i]<=target)eventVelZ[i]+=1.0/256.0;if(eventCooldown[i])--eventCooldown[i];else{spawnAirshipProjectile(i,2,false);if(++eventHits[i]>=unsigned(a.cannonVolley)){eventHits[i]=0;eventCooldown[i]=unsigned(a.cannonRest);}else eventCooldown[i]=unsigned(a.cannonGap);}steerAirship(i);if(depth>double(0x4300)/256.0){eventState[i]=2;eventStateTicks[i]=0;eventCooldown[i]=unsigned(a.fireGap);eventHits[i]=0;}}
        else if(st==4){eventX[i]+=eventVelX[i];eventY[i]+=eventVelY[i];eventZ[i]+=eventVelZ[i];if(eventStateTicks[i]==10)spawnAirshipExplosions(i,1);else if(eventStateTicks[i]==50)spawnAirshipExplosions(i,2);else if(eventStateTicks[i]==80)spawnAirshipExplosions(i,3);else if(eventStateTicks[i]==110)spawnAirshipExplosions(i,4);if(eventStateTicks[i]>=170){eventState[i]=5;eventStateTicks[i]=0;eventVelZ[i]=double(0x9D)/256.0;eventVelY[i]=0.0;emitSfx(0x42);}}
        else if(st==5){eventX[i]+=eventVelX[i];eventY[i]+=eventVelY[i];eventZ[i]+=eventVelZ[i];eventVelY[i]=std::min(1.25,eventVelY[i]+7.0/256.0);if(eventY[i]>187.5){consumed[i]=1;++enemiesDestroyed;++airshipsDefeated;}}
    }
    bool trySpawnHovercraftCannonball(){
        auto pos=hoverPartWorld(0);int64_t px=std::llround(x*256.0),py=std::llround(y*256.0),pz=std::llround((scroll+28.0)*256.0),ex=std::llround(pos[0]*256.0),ey=std::llround(pos[1]*256.0),ez=std::llround(pos[2]*256.0);
        int angle=int((pz-ez)/-0x1aa);if(angle<=0)return false;int scale=0x1000/angle;if(scale<=0)return false;int64_t rawDx=(px-ex)*scale,rawDy=(py-ey)*scale;int64_t dx=asrFloor(rawDx,12),dy=asrFloor(rawDy,12);if(std::llabs(dx)+std::llabs(dy)>0xfff)return false;
        JetShot s;s.x=pos[0];s.y=pos[1];s.z=pos[2];s.vx=double(dx)/256.0;s.vy=double(dy)/256.0;s.vz=-1.0;s.damage=2;s.actorKind=3;enemyShots.push_back(s);++enemyShotsFired;++hovercraftCannonballsFired;return true;
    }
    void spawnHovercraftFireball(unsigned p){auto pos=hoverPartWorld(p);HoverFireball f;f.x=pos[0];f.y=pos[1];f.z=pos[2];hoverFireballs.push_back(f);++hovercraftFireballsFired;}
    bool hovercraftLauncherCanFire()const{
        auto pos=hoverPartWorld(1);const double playerZ=scroll+28.0;int angle=int(((playerZ-pos[2])*256.0)/-426.0);if(angle<=0)return false;int scale=4096/angle;if(scale<=0)return false;long long dx=static_cast<long long>((x-pos[0])*256.0)*scale/4096;long long dy=static_cast<long long>((y-pos[1])*256.0)*scale/4096;return std::llabs(dx)+std::llabs(dy)<=0x0fff;
    }
    void spawnHovercraftBomber(){
        auto pos=hoverPartWorld(1);static constexpr unsigned kinds[3]={5,6,8};JetSpawnedBomber b;b.x=b.homeX=pos[0];b.y=b.homeY=pos[1];b.z=pos[2];b.kind=kinds[fxRandRange(3)];spawnedBombers.push_back(b);++hovercraftLauncherSpawns;
    }
    void killHoverFireball(HoverFireball& f){if(f.dying)return;f.dying=true;f.dyingTicks=0;++hovercraftFireballsDestroyed;emitSfx(0x04);}
    void updateHoverFireballs(){
        const CategoryBox pb=rom.actorBox(0),fb=rom.actorBox(39);const double pz=scroll+28.0;
        for(auto& f:hoverFireballs){
            if(f.dying){++f.dyingTicks;continue;}
            f.z+=double(f.speedRaw)/256.0;f.speedRaw=std::max(0x14,f.speedRaw-5);
            if(collisions&&!jetPlayerInactive()&&x+pb.x<f.x+fb.x+fb.w&&x+pb.x+pb.w>f.x+fb.x&&y+pb.y<f.y+fb.y+fb.h&&y+pb.y+pb.h>f.y+fb.y&&pz+pb.z<f.z+fb.z+fb.d&&pz+pb.z+pb.d>f.z+fb.z){damageJetpack(6);killHoverFireball(f);}
        }
        hoverFireballs.erase(std::remove_if(hoverFireballs.begin(),hoverFireballs.end(),[&](const HoverFireball& f){return (f.dying&&f.dyingTicks>=rom.animationTicks(39,1))||f.z-scroll>320.0;}),hoverFireballs.end());
    }
    void killSpawnedBomber(JetSpawnedBomber& b){if(b.dying)return;b.dying=true;b.dyingTicks=0;emitSfx(0x04);}
    void killEventBomber(unsigned i){if(i>=rom.eventCount||eventAnim[i])return;eventHp[i]=0;eventState[i]=6;eventAnim[i]=1;eventStateTicks[i]=0;emitSfx(0x04);}
    void updateSpawnedBombers(){
        const double tau=6.28318530717958647692;const CategoryBox pb=rom.actorBox(0);const double pz=scroll+28.0;
        for(auto& b:spawnedBombers){
            if(!b.dying){CategoryBox eb=rom.actorBox(b.kind);if(collisions&&!jetPlayerInactive()&&x+pb.x<b.x+eb.x+eb.w&&x+pb.x+pb.w>b.x+eb.x&&y+pb.y<b.y+eb.y+eb.h&&y+pb.y+pb.h>b.y+eb.y&&pz+pb.z<b.z+eb.z+eb.d&&pz+pb.z+pb.d>b.z+eb.z){damageJetpack(10);killSpawnedBomber(b);}}
            if(b.dying){++b.dyingTicks;b.z+=0x60/256.0;++b.stateTicks;continue;}
            double depth=b.z-scroll;unsigned stateTime=b.stateTicks;
            if(b.kind==5){if(depth<=54.0){b.x+=(x-b.x)/32.0;b.y+=(y-b.y)/32.0;}}
            else if(b.kind==6){if(depth>54.0)b.y=b.homeY+std::sin(tau*double(stateTime&255u)/256.0)*60.0;else{b.x+=(x-b.x)/32.0;b.y+=(y-b.y)/32.0;}}
            else if(b.kind==8){if(depth>54.0){double a=tau*double(stateTime&255u)/256.0;b.x=b.homeX+std::sin(a+tau*.25)*60.0;b.y=b.homeY+std::sin(a)*60.0;}else{b.x+=(x-b.x)/32.0;b.y+=(y-b.y)/32.0;}}
            b.z+=0x60/256.0;++b.stateTicks;
        }
        spawnedBombers.erase(std::remove_if(spawnedBombers.begin(),spawnedBombers.end(),[&](const JetSpawnedBomber& b){if(!b.dying)return false;if(b.dyingTicks<rom.animationTicks(b.kind,1))return false;++enemiesDestroyed;return true;}),spawnedBombers.end());
    }
    void startHovercraftHitFlash(){if(!hovercraftHitFlashOn&&!hovercraftHitFlashTimer){hovercraftHitFlashTimer=1;hovercraftHitFlashOn=true;}}
    void updateHovercraftHitFlash(){if(!hovercraftHitFlashTimer)return;++hovercraftHitFlashTimer;if((hovercraftHitFlashTimer&3u)==0)hovercraftHitFlashOn=!hovercraftHitFlashOn;if(hovercraftHitFlashTimer>0x0b)hovercraftHitFlashTimer=0;}
    void destroyHoverPart(unsigned p){auto& part=hoverParts.at(p);if(part.dead||part.dying)return;emitSfx(0x04);emitSfx(0x04);part.dying=true;part.dyingTicks=0;part.active=false;part.hp=0;part.animIndex=p==0?2u:(p==1?3u:(p==2?0u:1u));++hovercraftPartsDestroyed;if(hovercraftPartsLeft)--hovercraftPartsLeft;if(!hovercraftPartsLeft){hovercraftState=5;hovercraftPhase=0;hovercraftVelY=0;}}
    void updateHoverPartDeaths(){for(auto& part:hoverParts)if(part.dying&&!part.dead){if(++part.dyingTicks>=rom.animationTicks(part.kind,part.animIndex)){part.dead=true;part.dying=false;}}}
    void updateHoverParts(){if(!hovercraftSpawned||hovercraftGone)return;double depth=hovercraftZ-scroll;auto a=hoverAttack(hovercraftProfile);
        updateHovercraftHitFlash();updateHoverPartDeaths();
        for(unsigned p=2;p<4;p++){auto& part=hoverParts[p];if(part.dead||part.dying)continue;part.animIndex=p==2?0u:1u;part.active=hovercraftState!=5&&depth>double(0x2800)/256.0;if(!part.active)continue;if(part.cooldown)--part.cooldown;else{spawnHovercraftFireball(p);if(++part.count>=unsigned(a.sideBurst)){part.count=0;part.cooldown=unsigned(a.sideRest);}else part.cooldown=unsigned(a.sideDelay);}}
        {auto& part=hoverParts[0];if(!part.dead&&!part.dying){bool now=hovercraftState!=5&&depth<=double(0x4aff)/256.0;if(now&&!part.active){part.active=true;part.cooldown=unsigned(a.cannonDelay);part.count=0;}else if(!now)part.active=false;part.animIndex=part.active?1u:0u;if(part.active){if(part.cooldown)--part.cooldown;else if(trySpawnHovercraftCannonball()){if(++part.count>=unsigned(a.cannonBurst)){part.count=0;part.cooldown=unsigned(a.cannonRest);}else part.cooldown=unsigned(a.cannonDelay);}}}}
        {auto& part=hoverParts[1];if(!part.dead&&!part.dying){bool was=part.active;bool now=hovercraftPartsLeft<=2&&(hovercraftState==2||(hovercraftState==3&&depth<=double(0x4aff)/256.0));if(now&&!part.active){part.active=true;part.cooldown=0;part.count=0;}else if(!now)part.active=false;if(part.active)part.animIndex=1;else if(was)part.animIndex=2;if(part.active){if(part.cooldown)--part.cooldown;else if(hovercraftLauncherCanFire()){spawnHovercraftBomber();if(++part.count>=unsigned(a.launcherBurst)){part.count=0;part.cooldown=unsigned(a.launcherRest);}else part.cooldown=unsigned(a.launcherDelay);}}}}
    }
    void updateHovercraftPaletteFlash(){if((hovercraftFrameCount&0x0fu)==0)hovercraftPalette15White=true;else if((hovercraftFrameCount&7u)==0)hovercraftPalette15White=false;}
    void updateHovercraft(){if(!hovercraftSpawned||hovercraftGone)return;++hovercraftFrameCount;updateHovercraftPaletteFlash();double depth=hovercraftZ-scroll;const double q=1.0/256.0;
        if(hovercraftState==1){hovercraftZ+=hovercraftVelZ;if(depth<=double(0x81FF)/256.0){hovercraftState=2;hovercraftVelX=hovercraftVelY=0;hovercraftPhase=0;}}
        else if(hovercraftState==2){hovercraftZ+=hovercraftVelZ;double target=hovercraftPhase==0?double(0x98)*q:(hovercraftPhase==1?double(0x40)*q:double(0x6A)*q);hovercraftVelZ+=(hovercraftVelZ<target?q:(hovercraftVelZ>target?-q:0));if(hovercraftPhase==0){hovercraftX+=hovercraftVelX;hovercraftY+=hovercraftVelY;hovercraftVelX=std::clamp(hovercraftVelX+(x-hovercraftX)/4096.0,-2.0,2.0);hovercraftVelY=std::clamp(hovercraftVelY+(y-hovercraftY)/4096.0,-2.0,2.0);if(hovercraftX<=-128)hovercraftVelX=2;else if(hovercraftX>128)hovercraftVelX=-2;if(hovercraftY<=-60)hovercraftVelY=2;else if(hovercraftY>44)hovercraftVelY=-2;}else if(hovercraftPhase==1){hovercraftX+=hovercraftVelX;if(hovercraftX>256&&hovercraftVelX>0)hovercraftVelX=-4;else if(hovercraftX<=-256&&hovercraftVelX<0)hovercraftVelX=4;}else{hovercraftOrbitRadius=std::min(128.0,hovercraftOrbitRadius+1.5);double a=6.28318530717958647692*double(((hovercraftFrameCount*30u/16u)&255u))/256.0;hovercraftX=std::cos(a)*hovercraftOrbitRadius;hovercraftY=std::sin(a)*hovercraftOrbitRadius;}if(depth<=double(0x27FF)/256.0){hovercraftState=3;hovercraftPhase=0;hovercraftVelZ=double(0xAE)*q;}}
        else if(hovercraftState==3){hovercraftX+=hovercraftVelX;hovercraftY+=hovercraftVelY;hovercraftZ+=hovercraftVelZ;if(hovercraftY>0)hovercraftVelY=-1;else if(hovercraftY<0)hovercraftVelY=1;else hovercraftVelY=0;if(hovercraftPhase<=3){double target=double(0xAE)*q;hovercraftVelZ+=(hovercraftVelZ<target?q:(hovercraftVelZ>target?-q:0));if(hovercraftX>128&&hovercraftVelX>0){hovercraftVelX=-2;++hovercraftPhase;}else if(hovercraftX<=-128&&hovercraftVelX<0){hovercraftVelX=2;++hovercraftPhase;}}else{double target=double(0x1D4)*q;hovercraftVelZ+=(hovercraftVelZ<target?q:-q);hovercraftVelX=hovercraftX>0?-2:(hovercraftX<0?2:0);}if(hovercraftPhase>3&&depth>128.0){hovercraftFrameCount=0;hovercraftOrbitRadius=0;hovercraftState=2;if(hovercraftPartsLeft>2){hovercraftPhase=1;hovercraftVelZ=double(0x40)*q;}else{hovercraftPhase=2;hovercraftVelZ=double(0x6A)*q;}hovercraftVelX=-10;}}
        else if(hovercraftState==5){double target=double(0x99)*q;hovercraftVelZ+=(hovercraftVelZ<target?q:(hovercraftVelZ>target?-q:0));if(hovercraftVelY<1.0)hovercraftVelY=std::min(1.0,hovercraftVelY+1.0);else if(hovercraftVelY>1.0)hovercraftVelY=std::max(1.0,hovercraftVelY-1.0);hovercraftZ+=hovercraftVelZ;hovercraftY+=hovercraftVelY;if(hovercraftY>75.0&&!hovercraftFinishTriggered){hovercraftFinishTriggered=true;startJetpackFinish();}if(hovercraftZ-scroll<=double(0x14ff)/256.0){hovercraftGone=true;++enemiesDestroyed;}}
        updateHoverParts();
    }
    void updateJetpackActors(){const double tau=6.28318530717958647692;if(rom.type==2)updateHovercraft();for(unsigned i=0;i<rom.eventCount;i++){if(consumed[i]||eventSpawnTick[i]<0)continue;unsigned k=kind(i);if(k==1){updateJetpackPlane(i);continue;}if(eventAnim[i]){if(k>=4&&k<=9)eventZ[i]+=0x60/256.0;continue;}unsigned age=ticks-unsigned(eventSpawnTick[i]);double depth=eventZ[i]-scroll;const JetAiProfile profile=jetAiProfile(k);if(profile==JetAiProfile::Airship){updateAirship(i,k);continue;}
        switch(profile){
        case JetAiProfile::Tracker:
            if(depth>54.0){if(k==5){eventX[i]=eventHomeX[i];eventY[i]=eventHomeY[i];}else if(k==6){eventX[i]=eventHomeX[i];eventY[i]=eventHomeY[i]+std::sin(tau*double(age&255u)/256.0)*60.0;}else if(k==7){double phase=double((age*10u/16u)&255u);eventX[i]=eventHomeX[i]+std::sin(tau*(phase+64.0)/256.0)*80.0;eventY[i]=eventHomeY[i];}else{double a=tau*double(age&255u)/256.0;eventX[i]=eventHomeX[i]+std::sin(a+tau*.25)*60.0;eventY[i]=eventHomeY[i]+std::sin(a)*60.0;}}else{eventX[i]+=(x-eventX[i])/32.0;eventY[i]+=(y-eventY[i])/32.0;}break;
        case JetAiProfile::DepthRunner:eventZ[i]-=136.0/256.0;break;
        case JetAiProfile::BalloonOrbit:{if(eventState[i]==0){double phase=double((eventPhase[i]+age)&255u);eventX[i]=eventHomeX[i]+std::sin(tau*double((unsigned(phase)*5u/16u)&255u)/256.0)*17.0;eventY[i]=eventHomeY[i]+std::sin(tau*double((unsigned(phase)*8u/16u)&255u)/256.0)*30.0;}else if(eventState[i]==1){eventY[i]+=eventVelY[i];eventVelY[i]=std::min(double(0x4c0)/256.0,eventVelY[i]+double(0x12)/256.0);++eventStateTicks[i];if(eventY[i]>double(0xe100)/256.0)consumed[i]=1;}break;}
        case JetAiProfile::Parachute:eventX[i]=eventHomeX[i];if(eventY[i]<eventLimitY[i])eventY[i]=std::min(eventLimitY[i],eventY[i]+double(0x140)/256.0);break;
        case JetAiProfile::Rocket:if(eventState[i]==0){eventX[i]=eventHomeX[i]+std::sin(tau*double(((age*4u)+64u)&255u)/256.0)*16.0;if(eventY[i]>eventLimitY[i])eventY[i]+=eventStepY[i];else launchJetpackRocket(i,1);}break;
        case JetAiProfile::CannonPlane:break;
        case JetAiProfile::Ring:break;
        case JetAiProfile::Static:break;
        case JetAiProfile::Airship:break;
        }
        if(k>=4&&k<=9)eventZ[i]+=0x60/256.0;
    }}
    void updateJetpackStates(){for(unsigned i=0;i<rom.eventCount;i++)if(eventAnim[i]){unsigned k=kind(i);if(++eventStateTicks[i]>=rom.animationTicks(k,eventAnim[i])){consumed[i]=1;eventAnim[i]=0;if(jetEnemy(k))++enemiesDestroyed;}}}
    void stepEnemyShots(){const CategoryBox pb=rom.actorBox(0);for(auto& s:enemyShots){s.x+=s.vx;s.y+=s.vy;s.z+=s.vz;++s.age;if(s.age>180||jetPlayerInactive())continue;double pz=scroll+28.0;bool hit=false;if(s.actorKind){CategoryBox sb=rom.actorBox(s.actorKind);hit=s.x+sb.x<x+pb.x+pb.w&&s.x+sb.x+sb.w>x+pb.x&&s.y+sb.y<y+pb.y+pb.h&&s.y+sb.y+sb.h>y+pb.y&&s.z+sb.z<pz+pb.z+pb.d&&s.z+sb.z+sb.d>pz+pb.z;}else hit=s.x>=x+pb.x&&s.x<=x+pb.x+pb.w&&s.y>=y+pb.y&&s.y<=y+pb.y+pb.h&&s.z>=pz+pb.z&&s.z<=pz+pb.z+pb.d;if(hit){damageJetpack(s.damage);s.age=999;}}enemyShots.erase(std::remove_if(enemyShots.begin(),enemyShots.end(),[](const JetShot& s){return s.age>180;}),enemyShots.end());}
    void stepHovercraftShots(){if(!hovercraftSpawned||hovercraftGone||!(hovercraftState==2||hovercraftState==3))return;for(auto& s:shots){if(s.age>150)continue;for(unsigned p=0;p<hoverParts.size();p++){auto& part=hoverParts[p];if(part.dead||part.dying||part.hp<=0||(p==1&&!part.active))continue;auto pos=hoverPartWorld(p);CategoryBox b=rom.actorBox(part.kind);if(s.x>=pos[0]+b.x&&s.x<=pos[0]+b.x+b.w&&s.y>=pos[1]+b.y&&s.y<=pos[1]+b.y+b.h&&s.z>=pos[2]+b.z&&s.z<=pos[2]+b.z+b.d){s.age=999;startHovercraftHitFlash();if(--part.hp<=0)destroyHoverPart(p);else emitSfx(0x45);break;}}}}
    void stepShots(){
        for(auto& s:shots){s.x+=s.vx;s.y+=s.vy;s.z+=s.vz;++s.age;}
        if(rom.type==2)stepHovercraftShots();
        for(auto& s:shots){
            if(s.age>150)continue;
            for(auto& f:hoverFireballs){if(f.dying)continue;CategoryBox fb=rom.actorBox(39);if(s.x>=f.x+fb.x&&s.x<=f.x+fb.x+fb.w&&s.y>=f.y+fb.y&&s.y<=f.y+fb.y+fb.h&&s.z>=f.z+fb.z&&s.z<=f.z+fb.z+fb.d){s.age=999;if(--f.hp<=0)killHoverFireball(f);break;}}
            if(s.age>150)continue;
            for(auto& b:spawnedBombers){if(b.dying)continue;CategoryBox bb=rom.actorBox(b.kind);if(s.x>=b.x+bb.x&&s.x<=b.x+bb.x+bb.w&&s.y>=b.y+bb.y&&s.y<=b.y+bb.y+bb.h&&s.z>=b.z+bb.z&&s.z<=b.z+bb.z+bb.d){s.age=999;if(--b.hp<=0)killSpawnedBomber(b);break;}}
            if(s.age>150)continue;
            for(auto& rb:releasedBalloons){if(rb.hp<=0)continue;auto b=rom.actorBox(rb.kind);if(s.x>=rb.x+b.x&&s.x<=rb.x+b.x+b.w&&s.y>=rb.y+b.y&&s.y<=rb.y+b.y+b.h&&s.z>=rb.z+b.z&&s.z<=rb.z+b.z+b.d){s.age=999;if(--rb.hp<=0){JetPoppedBalloon pb;pb.kind=rb.kind;pb.x=rb.x;pb.y=rb.y;pb.z=rb.z;poppedBalloons.push_back(pb);emitSfx(0x2e);}break;}}
            if(s.age>150)continue;
            for(unsigned i=0;i<rom.eventCount;i++){if(consumed[i]||eventSpawnTick[i]<0||eventState[i]!=0||eventAnim[i]||eventChildHp[i]<=0)continue;unsigned k=kind(i),bk=crateBalloonKind(k);if(!bk)continue;auto pos=attachedBalloonWorld(i);auto b=rom.actorBox(bk);if(s.x>=pos[0]+b.x&&s.x<=pos[0]+b.x+b.w&&s.y>=pos[1]+b.y&&s.y<=pos[1]+b.y+b.h&&s.z>=pos[2]+b.z&&s.z<=pos[2]+b.z+b.d){s.age=999;if(--eventChildHp[i]<=0)popJetpackCrateBalloon(i,k);break;}}
            if(s.age>150)continue;
            for(unsigned i=0;i<rom.eventCount;i++){if(consumed[i]||eventSpawnTick[i]<0||eventHp[i]<=0)continue;unsigned k=kind(i);if(!rom.spawnableKind(k)||(!jetEnemy(k)&&!jetCrate(k)))continue;if(airshipKind(k)&&(eventState[i]<2||eventState[i]>3))continue;auto pos=eventWorld(i);auto b=(k==28&&eventState[i]!=0)?jetpackRocketTriggeredBox():jetBox(k);double ex=pos[0],ey=pos[1],ez=pos[2];if(s.x>=ex+b.x&&s.x<=ex+b.x+b.w&&s.y>=ey+b.y&&s.y<=ey+b.y+b.h&&s.z>=ez+b.z&&s.z<=ez+b.z+b.d){s.age=999;if(--eventHp[i]<=0){if(k==28){launchJetpackRocket(i,2);}else if(k==27){breakJetpackParachute(i);}else if(airshipKind(k))beginAirshipExplosion(i);else if(k==1){damageJetpackPlane(i);}else if(k>=4&&k<=9){killEventBomber(i);}else resolveJetpackEvent(i,k);}else if(airshipKind(k))emitSfx(0x43);break;}}
        }
        shots.erase(std::remove_if(shots.begin(),shots.end(),[](const JetShot& s){return s.age>150;}),shots.end());
    }

    static void blitScaled(std::vector<uint32_t>& c,int w,int h,const Frame& f,int ax,int ay,double scale){int dw=std::max(1,int(std::lround(f.width*scale))),dh=std::max(1,int(std::lround(f.height*scale)));int left=ax+int(std::lround(f.x*scale)),top=ay+int(std::lround(f.y*scale));for(int yy=0;yy<dh;yy++){int sy=std::clamp(int(double(yy)/scale),0,f.height-1);for(int xx=0;xx<dw;xx++){int sx=std::clamp(int(double(xx)/scale),0,f.width-1);uint32_t px=f.pixels[size_t(sy*f.width+sx)];int dx=left+xx,dy=top+yy;if((px>>24)&&dx>=0&&dy>=0&&dx<w&&dy<h)c[size_t(dy*w+dx)]=px;}}}
    static void pixel(std::vector<uint32_t>& c,int xx,int yy,uint32_t p){if(xx>=0&&yy>=0&&xx<240&&yy<160)c[size_t(yy*240+xx)]=p;}
    static void digit(std::vector<uint32_t>& c,int xx,int yy,unsigned d){static constexpr const char* p[10]={"111101101101111","010110010010111","111001111100111","111001111001111","101101111001001","111100111001111","111100111101111","111001001001001","111101111101111","111101111001111"};for(int y0=0;y0<5;y0++)for(int x0=0;x0<3;x0++)if(p[d%10][y0*3+x0]=='1')pixel(c,xx+x0,yy+y0,0xfff8e76c);}
    static void number(std::vector<uint32_t>& c,int xx,int yy,unsigned v){if(v>=100)digit(c,xx,yy,(v/100)%10);digit(c,xx+(v>=100?4:0),yy,(v/10)%10);digit(c,xx+(v>=100?8:4),yy,v%10);}

    std::vector<uint32_t> renderLogical(bool debug=false)const{
        auto canvas=rom.backgroundLogical(unsigned(std::floor(scroll))%rom.backgroundFrames);
        for(unsigned i=0;i<rom.eventCount;i++){if(consumed[i])continue;unsigned k=kind(i);if(!rom.spawnableKind(k))continue;auto p=project(i);if(p.dz<-55||p.dz>(rom.type?220:190))continue;try{unsigned ai=eventAnim[i]?eventAnim[i]:eventPose[i];if(k==31){Frame deco=rom.actorAnimationFrame(43,0,ticks);double dx43=rom.actorSpawnX(43)-rom.actorSpawnX(31);int dsx=p.sx+int(std::lround(dx43*28.0/std::max(1.0,p.dz)));blitScaled(canvas,240,160,deco,dsx,p.sy,std::clamp(rom.actorBaseDepth(43)/std::max(1.0,p.dz),0.20,3.0));}Frame f=rom.actorAnimationFrame(k,ai,(eventAnim[i]||eventPose[i])?eventStateTicks[i]:ticks);blitScaled(canvas,240,160,f,p.sx,p.sy,p.scale);}catch(...){if(debug)pixel(canvas,p.sx,p.sy,0xffff00ff);}}
        for(unsigned i=0;i<rom.eventCount;i++){if(consumed[i]||eventAnim[i]||eventState[i]!=0)continue;unsigned k=kind(i),bk=crateBalloonKind(k);if(!bk)continue;double bz=eventZ[i]-scroll;if(bz<=1||bz>220)continue;double ps=28.0/bz;int bx=120+int(std::lround(eventX[i]*ps)),by=103+int(std::lround((eventY[i]+double(int32_t(0xffffc24a))/256.0)*ps));try{Frame bf=rom.actorAnimationFrame(bk,0,ticks);blitScaled(canvas,240,160,bf,bx,by,std::clamp(rom.actorBaseDepth(bk)/bz,0.20,3.0));}catch(...){if(debug)pixel(canvas,bx,by,0xffff70ff);}}
        for(const auto& rb:releasedBalloons){if(rb.hp<=0)continue;double bz=rb.z-scroll;if(bz<=1||bz>240)continue;double ps=28.0/bz;int bx=120+int(std::lround(rb.x*ps)),by=103+int(std::lround(rb.y*ps));try{Frame bf=rom.actorAnimationFrame(rb.kind,0,rb.age);blitScaled(canvas,240,160,bf,bx,by,std::clamp(rom.actorBaseDepth(rb.kind)/bz,0.20,3.0));}catch(...){if(debug)pixel(canvas,bx,by,0xffff70ff);}}
        for(const auto& pb:poppedBalloons){double bz=pb.z-scroll;if(bz<=1||bz>240)continue;double ps=28.0/bz;int bx=120+int(std::lround(pb.x*ps)),by=103+int(std::lround(pb.y*ps));try{Frame bf=rom.actorAnimationFrame(pb.kind,1,pb.age);blitScaled(canvas,240,160,bf,bx,by,std::clamp(rom.actorBaseDepth(pb.kind)/bz,0.20,3.0));}catch(...){if(debug)pixel(canvas,bx,by,0xffff70ff);}}
        for(const auto& b:spawnedBombers){double dz=b.z-scroll;if(dz<=1||dz>220)continue;double ps=28.0/dz;int bx=120+int(std::lround(b.x*ps)),by=103+int(std::lround(b.y*ps));try{Frame f=rom.actorAnimationFrame(b.kind,b.dying?1u:0u,b.dying?b.dyingTicks:b.stateTicks);blitScaled(canvas,240,160,f,bx,by,std::clamp(rom.actorBaseDepth(b.kind)/dz,0.20,3.0));}catch(...){if(debug)pixel(canvas,bx,by,0xffff00ff);}}
        for(const auto& fball:hoverFireballs){double dz=fball.z-scroll;if(dz<=1||dz>260)continue;double ps=28.0/dz;int fx=120+int(std::lround(fball.x*ps)),fy=103+int(std::lround(fball.y*ps));try{Frame f=rom.actorAnimationFrame(39,fball.dying?1u:0u,fball.dying?fball.dyingTicks:ticks);blitScaled(canvas,240,160,f,fx,fy,std::clamp(rom.actorBaseDepth(39)/dz,0.20,3.0));}catch(...){if(debug)pixel(canvas,fx,fy,0xffff4000);}}
        if(rom.type==2&&hovercraftSpawned&&!hovercraftGone){double dz=hovercraftZ-scroll;if(dz>1&&dz<260){double posScale=28.0/dz,sc=std::clamp(60.0/dz,0.28,2.2);int sx=120+int(std::lround(hovercraftX*posScale)),sy=103+int(std::lround(hovercraftY*posScale));try{Frame boss=rom.hovercraftFrame(hovercraftPalette15White);blitScaled(canvas,240,160,boss,sx,sy,sc);}catch(...){if(debug)pixel(canvas,sx,sy,0xffff00ff);}for(unsigned p=0;p<hoverParts.size();p++){const auto& part=hoverParts[p];if(part.dead)continue;auto pos=hoverPartWorld(p);double pdz=pos[2]-scroll;if(pdz<=1||pdz>240)continue;double ps=28.0/pdz;int px=120+int(std::lround(pos[0]*ps)),py=103+int(std::lround(pos[1]*ps));try{Frame f=rom.actorAnimationFrame(part.kind,part.animIndex,part.dying?part.dyingTicks:ticks,hovercraftPalette15White);blitScaled(canvas,240,160,f,px,py,std::clamp(rom.actorBaseDepth(part.kind)/pdz,0.20,3.0));}catch(...){if(debug)pixel(canvas,px,py,0xffff00ff);}}}}
        for(const auto& q:jetpackCollectedWumpas){try{Frame f=rom.actorAnimationFrame(44,0,q.age);blitScaled(canvas,240,160,f,int(std::lround(q.x)),int(std::lround(q.y)),1.0);}catch(...){if(debug)pixel(canvas,int(std::lround(q.x)),int(std::lround(q.y)),0xffffd030);}}
        if(rom.type){unsigned anim=0;if(jetPlayerState==2)anim=1;else if(jetPlayerState==3)anim=2;else if(jetPlayerState==4)anim=3;else if(jetPlayerState==5)anim=4;else if(jetPlayerState==6&&jetPlayerStateTicks<rom.animationTicks(0,5))anim=5;Frame rider=rom.actorAnimationFrame(0,anim,ticks);if(jetPlayerState==5){double depth=std::max(1.0,jetPlayerDepth),ps=28.0/depth;int rx=120+int(std::lround(x*ps)),ry=103+int(std::lround(y*ps));blitScaled(canvas,240,160,rider,rx,ry,std::clamp(28.0/depth,0.20,3.0));}else GameData::blit(canvas,240,160,rider,120+int(std::lround(x)),103+int(std::lround(y)),false);for(const auto& s:shots){double dz=s.z-scroll;if(dz<=1||dz>220)continue;double sc=28.0/dz;int sx=120+int(std::lround(s.x*sc)),sy=103+int(std::lround(s.y*sc));for(int yy=-1;yy<=1;yy++)for(int xx=-1;xx<=1;xx++)pixel(canvas,sx+xx,sy+yy,0xfffff080);}for(const auto& s:enemyShots){double dz=s.z-scroll;if(dz<=1||dz>220)continue;double ps=28.0/dz;int sx=120+int(std::lround(s.x*ps)),sy=103+int(std::lround(s.y*ps));if(s.actorKind){try{Frame f=rom.actorAnimationFrame(s.actorKind,0,s.age);blitScaled(canvas,240,160,f,sx,sy,std::clamp(rom.actorBaseDepth(s.actorKind)/dz,0.20,3.0));}catch(...){if(debug)pixel(canvas,sx,sy,0xffff7040);}}else for(int yy=-2;yy<=2;yy++)for(int xx=-2;xx<=2;xx++)if(xx*xx+yy*yy<=4)pixel(canvas,sx+xx,sy+yy,0xffff7040);}for(const auto& b:blasts){double dz=b.z-scroll;if(dz<=1||dz>240)continue;double sc=28.0/dz;int sx=120+int(std::lround(b.x*sc)),sy=103+int(std::lround(b.y*sc));int r=1+int((b.age*5u)/std::max(1u,b.maxAge));for(int yy=-r;yy<=r;yy++)for(int xx=-r;xx<=r;xx++){int rr=xx*xx+yy*yy;if(rr>r*r)continue;uint32_t col=rr<(r*r)/3?0xffffffff:0xffff9c38;pixel(canvas,sx+xx,sy+yy,col);}}number(canvas,10,12,hp);number(canvas,54,12,enemiesDestroyed);number(canvas,98,12,lives);number(canvas,142,12,ringPasses);if(rom.type==2){number(canvas,186,12,hovercraftPartsLeft);unsigned curHp=0,maxHp=0;for(const auto& part:hoverParts){curHp+=unsigned(std::max(0,part.hp));maxHp+=unsigned(std::max(0,part.maxHp));}int fill=maxHp?int((40u*curHp)/maxHp):0;uint32_t hc=hovercraftHitFlashOn?0xffffffff:0xffb8d070;for(int yy=22;yy<26;yy++)for(int xx=186;xx<226;xx++)pixel(canvas,xx,yy,xx<186+fill?hc:0xff303030);}}else{unsigned riderAnim=grounded?0u:3u;Frame rider=rom.actorAnimationFrame(0,riderAnim,ticks);GameData::blit(canvas,240,160,rider,120+int(std::lround(x)),110+int(std::lround(jumpY)),false);number(canvas,10,12,wumpa);number(canvas,54,12,crates);number(canvas,98,12,lives);if(freezeFrames){int end=142+int(std::min(60u,freezeFrames/3));for(int xx=142;xx<end;xx++)pixel(canvas,xx,15,0xff80e8ff);}}
        if(debug){int bar=rom.routeEndThreshold>0?std::clamp(int(scroll*220.0/rom.routeEndThreshold),0,220):int(hovercraftPartsDestroyed*55u);for(int yy=4;yy<8;yy++)for(int xx=10;xx<10+bar;xx++)pixel(canvas,xx,yy,0xffffffff);}
        return canvas;
    }
    std::vector<uint32_t> render(int scale,bool debug=false)const{auto logical=renderLogical(debug);if(scale<=1)return logical;std::vector<uint32_t> out(size_t(240*scale*160*scale));int ww=240*scale;for(int yy=0;yy<160;yy++)for(int xx=0;xx<240;xx++){uint32_t p=logical[size_t(yy*240+xx)];for(int y0=0;y0<scale;y0++)for(int x0=0;x0<scale;x0++)out[size_t((yy*scale+y0)*ww+xx*scale+x0)]=p;}return out;}
};

}
