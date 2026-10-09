#pragma once
#include "player.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace crash {

struct MoverState {
    bool enabled=false;
    unsigned kind=0;
    int inputDistX=0,inputDistY=0;
    int rangeX=0,rangeY=0;
    int travelledX=0,travelledY=0;
    bool dirX=false,dirY=false;
    int32_t xQ8=0,yQ8=0;
    int32_t vxQ8=0,vyQ8=0;
    int32_t targetVxQ8=0,targetVyQ8=0;
    int lastX=0,lastY=0;
    bool active=false,falling=false;
    unsigned specialTimer=0,holdUntil=0;
};

struct DingodileRuntime {
    unsigned state=0,hits=0,step=0,timer=0,nextState=0,passes=0;
    unsigned animation=0,animationTicks=0,shieldState=5,shieldBlinkTimer=0,shieldBlinksLeft=0;
    int32_t vxQ8=0,vyQ8=0,targetVxQ8=0,targetVyQ8=0,stepXQ8=0,stepYQ8=0;
    bool shieldBlink=false,stalactiteVulnerable=false,complete=false,rewardSpawned=false;
};

struct TinyRuntime {
    unsigned state=0,round=0,nextState=0,target=0,count=0,animation=0,animationTicks=0;
    int timer=0,stomped=-1,steps=0,total=0;
    double x=0,y=0,dx=0,dy=0;
    bool complete=false,rewardSpawned=false,rewardCollected=false;
};

struct CortexRuntime {
    unsigned state=0,counter=0;
    unsigned bossAnimation=1,bossFrame=0,cannonAnimation=3,cannonFrame=0;
    unsigned targetState=0,targetAnimation=15,targetTicks=0,targetFrame=0,targetNextState=0;
    int targetTimer=0,targetBlink=0,targetSteps=0,targetTotal=0;
    double targetX=0,targetY=0,targetDestX=0,targetDestY=0,targetDeltaX=0,targetDeltaY=0;
    bool targetDirLeft=false,targetHigh=false,targetTop=false,targetBlinking=false;
    bool childrenSpawned=false,complete=false,rewardSpawned=false,rewardCollected=false;
};

struct MegaMixRuntime {
    unsigned state=1,animation=0,animationTicks=0,motionMode=1;
    int stamp=-1;
    int32_t vxQ8=0,targetVxQ8=0,stepXQ8=32;
    bool latch=false,grabDamageDone=false;
};

struct RuntimeEntity {
    Entity source;
    bool active=true;
    unsigned stateTicks=0,countdown=0;
    bool triggered=false,dying=false,facingLeft=false,attacking=false,attackDamageDone=false;
    unsigned hits=0,triggerCap=0,deathTicks=0,attackTicks=0;
    double x=0,y=0,prevX=0,prevY=0,patrolOrigin=0,patrolRange=0,patrolSpeed=.8;
    unsigned stackAbove=std::numeric_limits<unsigned>::max(),stackBelow=std::numeric_limits<unsigned>::max();
    bool stackFalling=false;
    double stackFallTargetY=0,stackFallVelocity=0;
    MoverState mover{};
    unsigned controllerMode=0,controllerTicks=0;
    bool childSpawned=false;
    DingodileRuntime dingodile{};
    TinyRuntime tiny{};
    CortexRuntime cortex{};
    MegaMixRuntime megaMix{};
    RuntimeEntity()=default;
    explicit RuntimeEntity(const Entity& e):source(e),x(e.x),y(e.y),prevX(e.x),prevY(e.y){mover.xQ8=int32_t(e.x)<<8;mover.yQ8=int32_t(e.y)<<8;mover.lastX=int(e.x);mover.lastY=int(e.y);}
};

struct EntityEffect { double x=0,y=0; unsigned animation=0,ticks=0; };
struct EnemyChildEffect {
    enum Kind : unsigned { Generic=0, DingodileRocket=1, DingodileStalactite=2, DingodileShark=3, TinyLeaf=4, TinyReward=5, CortexSlowShot=6, CortexFastShot=7, CortexReward=8 };
    unsigned kind=Generic,bank=0,animation=0,ownerId=0,ticks=0,maxTicks=0,state=0;
    double x=0,y=0,vx=0,vy=0,targetVy=0,accelY=0;
    bool active=true,facingLeft=false,harmful=true,followOwnerX=false;
};
struct EntityClip { std::vector<Frame> frames; unsigned duration=1,flags=0; };
struct EntitySpriteSet { unsigned bank=0; std::vector<EntityClip> clips; std::vector<Player::SolidBox> bounds; };

class EntityWorld {
 public:
    static constexpr unsigned WumpaType=6,CrystalType=7,GemPathGemType=8,BlueGemType=9,RedGemType=10,GreenGemType=11,YellowGemType=12,GemType=CrystalType,TimeTrialType=16,CrateFirst=21,CrateLast=39,EnemyType=40,PlantType=42;
    static constexpr unsigned VultureType=0x29,PatrollingJungleType=0x2B,BlowgunType=0x2C,PenguinType=0x2D,SealSpawnerType=0x2E,PolarBearType=0x2F;
    static constexpr unsigned PufferfishType=0x30,SharkType=0x31,MorayType=0x32,ElectricEelType=0x33,SquidType=0x34,JellyfishType=0x35;
    static constexpr unsigned LaserBarrierType=0x37,StationarySpaceType=0x38,PatrollingSpaceType=0x39,SaucerType=0x3A,PistonCrusherType=0x3B;
    static constexpr unsigned FlamethrowerType=0x40,HomingSewerType=0x41,PatrollingSewerType=0x42,RatType=0x43,FrogType=0x44,DingodileType=0x45,TinyType=0x47,CortexType=0x48,MegaMixType=0x49;
    static constexpr unsigned SeaMineType=0x4B,SeaMineAltType=0x4C,WoodenCrusherType=0x4D,FlameType=0x5A,SeaweedType=0x5B,SealRuntimeType=0x100;
    static constexpr unsigned MovingPlatformType=79,RedGemPlatformType=81,YellowGemPlatformType=82,GreenGemPlatformType=83,BlueGemPlatformType=84,ExitMarkerType=85,LevelPlatformType=86,TimedPlatformType=87,FallingPlatformType=88;
    static constexpr unsigned CheckpointCrateType=22,AkuCrateType=23,LifeCrateType=30,NitroCrateType=31,QuestionCrateType=32,BounceCrateType=33,TntCrateType=35,CounterCrateType=36;

    std::vector<RuntimeEntity> items;
    std::vector<Frame> wumpaFrames;
    std::array<EntityClip,36> crateClips{};
    std::array<Player::SolidBox,36> crateBounds{};
    std::array<EntityClip,13> levelObjectClips{};
    std::array<Player::SolidBox,13> levelObjectBounds{};
    std::array<EntityClip,2> enemyClips{};
    std::array<Player::SolidBox,2> enemyBounds{};
    std::array<EntityClip,2> plantClips{};
    std::array<EntitySpriteSet,7> aquaticSets{};
    std::array<EntitySpriteSet,17> genericEnemySets{};
    EntitySpriteSet flameSet{},seaweedSet{},dingodileSet{},tinySet{},cortexSet{},cortexGemSet{},megaMixSet{},powerSet{};
    EntityClip gemClip{},timeTrialClip{};
    Player::SolidBox gemBounds{},timeTrialBounds{};
    std::array<Player::SolidBox,2> plantBodyBounds{};
    std::array<Player::SolidBox,2> plantHitBounds{};
    std::vector<EntityEffect> effects;
    std::vector<RuntimeEntity> spawnedEnemies;
    std::vector<EnemyChildEffect> childEffects;
    std::vector<Player::SolidBox> solids;
    std::vector<EntityLink> crateLinks;

    unsigned wumpaCollected=0,wumpaTotal=0,wumpaPlaced=0;
    unsigned crateBroken=0,crateTotal=0,checkpointHits=0,akuMasks=0,extraLives=0,hazardHits=0;
    unsigned levelObjectTotal=0,moverTotal=0,carriedTicks=0,ticks=0,crateLinkCount=0;
    unsigned enemyTotal=0,enemyKills=0,genericEnemyTotal=0,enemySpawnerTotal=0,enemyChildEffectsSpawned=0,enemyChildEffectHits=0,plantTotal=0,plantAttacks=0,gemTotal=0,gemsCollected=0,timeTrialTotal=0,exitMarkers=0,dingodileTotal=0,dingodileHits=0,tinyTotal=0,tinyHits=0,cortexTotal=0,cortexHits=0,cortexGemTotal=0,megaMixTotal=0,megaMixGrabs=0,megaMixCratesDestroyed=0;
    int worldWidth=0,worldHeight=0;
    bool levelExitTriggered=false,exitMarkerArmed=false,autoExitCandidate=false,timeTrialAvailable=false,timeTrialStarted=false,cortexArena=false,megaMixArena=false;
    bool bonusRequested=false,gemPathRequested=false,bonusDone=false,gemPathDone=false,timeTrialMode=false;
    unsigned coloredGemFlags=0;
    const GameData* dataRef=nullptr;
    uint32_t sceneDescriptor=0;
    int sceneKind=0;

    EntityWorld(const GameData& data,const Scene& scene){
        dataRef=&data;sceneDescriptor=scene.descriptor;sceneKind=scene.kind;
        for(const auto& layer:scene.layers)if(layer.slot==4){worldWidth=int(layer.wt*8);worldHeight=int(layer.ht*8);}
        const auto sceneEntities=data.entities(scene);
        cortexArena=std::any_of(sceneEntities.begin(),sceneEntities.end(),[](const Entity& e){return e.type==CortexType;});
        megaMixArena=std::any_of(sceneEntities.begin(),sceneEntities.end(),[](const Entity& e){return e.type==MegaMixType;});
        for(auto e:sceneEntities){
            RuntimeEntity r(e);
            if(e.type==QuestionCrateType||e.type==CounterCrateType)r.triggerCap=parameterByte(data,scene,e.param,6);
            if(e.type==EnemyType){
                r.patrolOrigin=e.x;r.patrolRange=std::max(8.0,double(std::abs(parameterSWord(data,scene,e.param,4))));
                const unsigned flags=parameterByte(data,scene,e.param,0);r.facingLeft=(((flags>>1)^1)&1)!=0;
            } else if(e.type==PlantType) {
                const unsigned flags=parameterByte(data,scene,e.param,0);r.facingLeft=(((flags>>1)^1)&1)!=0;
            } else if(e.type==DingodileType) {
                const unsigned flags=parameterByte(data,scene,e.param,0);r.facingLeft=(((flags>>1)^1)&1)!=0;
                r.dingodile.animation=0;r.dingodile.state=0;
            } else if(e.type==TinyType) {
                const unsigned flags=parameterByte(data,scene,e.param,0);r.facingLeft=(((flags>>1)^1)&1)!=0;
                r.tiny.animation=0;r.tiny.state=0;r.tiny.stomped=-1;
            } else if(e.type==CortexType) {
                const unsigned flags=parameterByte(data,scene,e.param,0);r.facingLeft=(((flags>>1)^1)&1)!=0;
                r.cortex.bossAnimation=1;r.cortex.cannonAnimation=3;r.cortex.targetAnimation=15;
            } else if(e.type==MegaMixType) {
                r.facingLeft=false;r.megaMix=MegaMixRuntime{};
            } else if(isAquaticEnemy(e.type)||isGenericEnemy(e.type)) {
                const unsigned flags=parameterByte(data,scene,e.param,0);r.facingLeft=(((flags>>1)^1)&1)!=0;
                if(e.type==MorayType)r.facingLeft=!r.facingLeft;
                if(e.type==FlamethrowerType||e.type==LaserBarrierType)r.facingLeft=!r.facingLeft;
                if(e.type==SaucerType||e.type==0x3F){r.y-=40.0;r.prevY=r.y;r.mover.yQ8=int32_t(r.y*256.0);}
            }
            if(e.type==MovingPlatformType){
                const unsigned recordType=parameterWord(data,scene,e.param,4);
                if(recordType==1||recordType==5||recordType==6||recordType==7)configureMover(r,recordType,parameterSWord(data,scene,e.param,8),parameterSWord(data,scene,e.param,12),parameterSHalf(data,scene,e.param,16)!=0,parameterSHalf(data,scene,e.param,18)!=0);
            } else if(e.type==TimedPlatformType) {
                if(cortexArena){r.mover.enabled=true;r.mover.kind=106;r.controllerMode=0;r.controllerTicks=26;}
                else configureMover(r,6,0,0,false,false);
            } else if(e.type==FallingPlatformType) {
                configureMover(r,7,0,0,false,false);
            }
            items.push_back(r);
            if(e.type==WumpaType){++wumpaPlaced;++wumpaTotal;}
            if(e.type>=CrystalType&&e.type<=YellowGemType)++gemTotal;
            if(cortexArena&&isCortexGem(e.type))++cortexGemTotal;
            if(e.type==TimeTrialType){++timeTrialTotal;items.back().active=false;}
            if(isCrate(e.type)){++crateTotal;wumpaTotal+=r.triggerCap;}
            if(isLevelObject(e.type))++levelObjectTotal;
            if(e.type==EnemyType||e.type==PlantType||isAquaticEnemy(e.type)||isGenericEnemy(e.type)||e.type==DingodileType||e.type==TinyType||e.type==CortexType||e.type==MegaMixType){++enemyTotal;if(isGenericEnemy(e.type))++genericEnemyTotal;if(e.type==DingodileType)++dingodileTotal;if(e.type==TinyType)++tinyTotal;if(e.type==CortexType)++cortexTotal;if(e.type==MegaMixType)++megaMixTotal;}
            if(e.type==SealSpawnerType)++enemySpawnerTotal;
            if(e.type==PlantType)++plantTotal;
            if(e.type==ExitMarkerType){++exitMarkers;if(worldWidth>0&&int(e.x)>=worldWidth-300)autoExitCandidate=true;}
            if(r.mover.enabled&&(r.mover.rangeX||r.mover.rangeY||r.mover.kind>=5))++moverTotal;
        }
        loadWumpaFrames(data);
        loadCrateClips(data);
        loadLevelObjectClips(data);
        loadEnemyClips(data);
        loadPlantClips(data);
        loadAquaticClips(data);
        loadGenericEnemyClips(data);
        loadFlameClip(data);
        loadSeaweedClip(data);
        loadDingodileClip(data);
        loadTinyClip(data);
        loadCortexClip(data);
        loadCortexGemClip(data);
        loadMegaMixClip(data);
        loadPowerClip(data);
        loadGemClip(data);
        loadTimeTrialClip(data);
        crateLinks=data.entityLinks(scene);
        applyCrateLinks();
        if(wumpaPlaced&&wumpaFrames.size()!=14)throw std::runtime_error("Wumpa frame set incomplete");
        if(data.banks.at(31).animations.size()!=36)throw std::runtime_error("Crate bank revision");
        rebuildSolids();
    }

    static bool isCrate(unsigned type){return type>=CrateFirst&&type<=CrateLast;}
    static bool isAquaticEnemy(unsigned type){return (type>=PufferfishType&&type<=JellyfishType)||type==SeaMineType||type==SeaMineAltType;}
    static bool isGenericEnemy(unsigned type){switch(type){case VultureType:case PatrollingJungleType:case BlowgunType:case PenguinType:case PolarBearType:case LaserBarrierType:case StationarySpaceType:case PatrollingSpaceType:case SaucerType:case 0x3F:case PistonCrusherType:case FlamethrowerType:case HomingSewerType:case PatrollingSewerType:case RatType:case FrogType:case WoodenCrusherType:return true;default:return false;}}
    static bool isEnemySpawner(unsigned type){return type==SealSpawnerType;}
    static bool isCortexGem(unsigned type){return type==0x0A||type==0x0B||type==0x0C;}
    static bool genericInvulnerable(unsigned type){return type==LaserBarrierType||type==PistonCrusherType||type==WoodenCrusherType;}
    static bool isColoredGem(unsigned type){return type>=BlueGemType&&type<=YellowGemType;}
    static unsigned coloredGemBit(unsigned type){switch(type){case BlueGemType:return 8;case RedGemType:return 1;case GreenGemType:return 4;case YellowGemType:return 2;default:return 0;}}
    static unsigned coloredGemAnimation(unsigned type){switch(type){case GemPathGemType:return 1;case BlueGemType:return 4;case RedGemType:return 3;case GreenGemType:return 2;case YellowGemType:return 0;default:throw std::runtime_error("Not a gem pickup");}}
    static bool isGemPlatform(unsigned type){return type>=RedGemPlatformType&&type<=BlueGemPlatformType;}
    static unsigned gemPlatformBit(unsigned type){switch(type){case RedGemPlatformType:return 1;case YellowGemPlatformType:return 2;case GreenGemPlatformType:return 4;case BlueGemPlatformType:return 8;default:return 0;}}
    static unsigned gemOutlineAnimation(unsigned type){switch(type){case RedGemPlatformType:return 7;case YellowGemPlatformType:return 5;case GreenGemPlatformType:return 6;case BlueGemPlatformType:return 8;default:throw std::runtime_error("Not a gem platform");}}
    static unsigned gemPlatformActiveAnimation(unsigned type){switch(type){case RedGemPlatformType:return 11;case YellowGemPlatformType:return 3;case GreenGemPlatformType:return 10;case BlueGemPlatformType:return 9;default:throw std::runtime_error("Not a gem platform");}}
    static bool isLevelObject(unsigned type){return type==78||type==79||type==80||isGemPlatform(type)||type==LevelPlatformType||type==TimedPlatformType||type==FallingPlatformType;}
    bool solidLevelObject(unsigned type)const{return type==MovingPlatformType||type==LevelPlatformType||type==TimedPlatformType||type==FallingPlatformType||(isGemPlatform(type)&&(coloredGemFlags&gemPlatformBit(type)));}
    unsigned levelObjectAnimation(const RuntimeEntity& e)const{
        const unsigned type=e.source.type;
        if(isGemPlatform(type)){if(!(coloredGemFlags&gemPlatformBit(type)))return 12;return (gemPathDone||timeTrialMode)?12:gemPlatformActiveAnimation(type);}
        switch(type){case 78:return 0;case 79:return 1;case 80:return 2;case LevelPlatformType:return (bonusDone||timeTrialMode)?7:5;case TimedPlatformType:return 6;case FallingPlatformType:return 8;default:throw std::runtime_error("Not a supported level object");}
    }
    static unsigned crateAnimation(unsigned type){
        // USA ACQE: level entity 21..39 -> level dispatch -> physics box constructor -> bank 31 animation tag.
        static constexpr unsigned map[19]={31,26,23,3,24,21,4,32,2,28,5,0,25,6,17,7,14,15,16};
        if(!isCrate(type))throw std::runtime_error("Not a crate entity type");
        return map[type-CrateFirst];
    }
    static bool metalCrate(unsigned type){return type==28||type==29;}
    static bool metalArrowCrate(unsigned type){return type==29;}
    static bool nitroCrate(unsigned type){return type==NitroCrateType;}
    static bool tntCrate(unsigned type){return type==TntCrateType;}
    static bool repeatedTriggerCrate(unsigned type){return type==QuestionCrateType||type==CounterCrateType;}

    void bind(Player& p){rebuildSolids();p.setDynamicSolids(&solids);}
    void setTimeTrialAvailable(bool enabled){timeTrialAvailable=enabled;for(auto& e:items)if(e.source.type==TimeTrialType)e.active=enabled&&!timeTrialStarted;}
    void setProgress(unsigned gemFlags,bool bonusComplete,bool gemComplete,bool trialMode){
        coloredGemFlags=gemFlags&15u;bonusDone=bonusComplete;gemPathDone=gemComplete;timeTrialMode=trialMode;bonusRequested=gemPathRequested=false;
        for(auto& e:items)if(isColoredGem(e.source.type)&&(coloredGemFlags&coloredGemBit(e.source.type)))e.active=false;
        rebuildSolids();
    }
    void armExitMarker(bool enabled=true){exitMarkerArmed=enabled;if(!enabled)levelExitTriggered=false;}
    bool canAutoExit()const{return autoExitCandidate||megaMixArena;}
    void reset(){
        for(auto& e:items){
            e.active=true;e.stateTicks=e.countdown=e.hits=e.deathTicks=e.attackTicks=e.controllerTicks=0;e.controllerMode=0;e.childSpawned=false;e.triggered=e.dying=e.attacking=e.attackDamageDone=false;e.stackFalling=false;e.stackFallTargetY=e.stackFallVelocity=0;e.x=e.source.x;e.y=e.source.y;e.prevX=e.x;e.prevY=e.y;
            if(e.source.type==EnemyType)e.patrolOrigin=e.source.x;
            if(e.source.type==EnemyType||e.source.type==PlantType||isAquaticEnemy(e.source.type)||isGenericEnemy(e.source.type)||e.source.type==DingodileType||e.source.type==TinyType||e.source.type==CortexType){e.facingLeft=initialFacingLeft(e.source.type,e.source.param);if(e.source.type==FlamethrowerType||e.source.type==LaserBarrierType)e.facingLeft=!e.facingLeft;}
            if(e.source.type==MegaMixType)e.facingLeft=false;
            if(e.source.type==DingodileType)e.dingodile=DingodileRuntime{};
            if(e.source.type==TinyType){e.tiny=TinyRuntime{};e.tiny.stomped=-1;}
            if(e.source.type==CortexType){e.cortex=CortexRuntime{};e.cortex.bossAnimation=1;e.cortex.cannonAnimation=3;e.cortex.targetAnimation=15;}
            if(e.source.type==MegaMixType)e.megaMix=MegaMixRuntime{};
            if(cortexArena&&e.source.type==TimedPlatformType){e.mover.enabled=true;e.mover.kind=106;e.controllerMode=0;e.controllerTicks=26;e.stateTicks=0;}
            if(e.source.type==SaucerType||e.source.type==0x3F){e.y=e.source.y-40.0;e.prevY=e.y;}
            if(e.mover.enabled){e.mover.xQ8=int32_t(e.source.x)<<8;e.mover.yQ8=int32_t(e.source.y)<<8;e.mover.vxQ8=e.mover.vyQ8=0;e.mover.travelledX=e.mover.inputDistX;e.mover.travelledY=e.mover.inputDistY;e.mover.lastX=int(e.source.x);e.mover.lastY=int(e.source.y);e.mover.active=e.mover.falling=false;e.mover.specialTimer=0;e.mover.holdUntil=e.mover.kind==6?120:0;setMoverTargets(e.mover);}
        }
        effects.clear();spawnedEnemies.clear();childEffects.clear();wumpaCollected=crateBroken=checkpointHits=akuMasks=extraLives=hazardHits=carriedTicks=ticks=0;enemyKills=enemyChildEffectsSpawned=enemyChildEffectHits=plantAttacks=gemsCollected=dingodileHits=tinyHits=cortexHits=megaMixGrabs=megaMixCratesDestroyed=0;levelExitTriggered=false;timeTrialStarted=false;bonusRequested=gemPathRequested=false;for(auto& e:items){if(e.source.type==TimeTrialType)e.active=timeTrialAvailable;if(isColoredGem(e.source.type)&&(coloredGemFlags&coloredGemBit(e.source.type)))e.active=false;}applyCrateLinks();rebuildSolids();
    }

    void step(Player& p){
        ++ticks;
        for(auto& fx:effects)++fx.ticks;
        effects.erase(std::remove_if(effects.begin(),effects.end(),[&](const EntityEffect& fx){return fx.ticks>=effectLength(fx.animation);}),effects.end());

        stepMovers(p);
        stepCrateFalls();
        rebuildSolids();

        auto body=p.bodyBox(),attack=p.attackBox(),terrainBody=p.worldBox(p.terrainBounds());
        const bool slamShockwave=p.superSlamImpact();
        for(auto& e:items){
            if(!e.active)continue;
            ++e.stateTicks;
            if(e.source.type==WumpaType){if(overlap(body,entityBox(e))){e.active=false;++wumpaCollected;}continue;}
            if(e.source.type==CrystalType){if(overlap(body,entityBox(e))){e.active=false;++gemsCollected;}continue;}
            if(e.source.type==GemPathGemType){if(overlap(body,entityBox(e))){e.active=false;++gemsCollected;}continue;}
            if(isColoredGem(e.source.type)&&!cortexArena){if(overlap(body,entityBox(e))){e.active=false;coloredGemFlags|=coloredGemBit(e.source.type);++gemsCollected;rebuildSolids();}continue;}
            if(cortexArena&&isCortexGem(e.source.type)){stepCortexGem(e);continue;}
            if(e.source.type==TimeTrialType){if(timeTrialAvailable&&overlap(body,entityBox(e))){e.active=false;timeTrialStarted=true;timeTrialAvailable=false;}continue;}
            if(e.source.type==ExitMarkerType){
                const double cx=(body[0]+body[2])*.5,cy=(body[1]+body[3])*.5;
                if(exitMarkerArmed&&std::abs(cx-e.x)<=50.0&&std::abs(cy-e.y)<=50.0)levelExitTriggered=true;
                continue;
            }
            if(e.source.type==LevelPlatformType||isGemPlatform(e.source.type)){
                const bool bonus=e.source.type==LevelPlatformType;
                const bool available=bonus?(!bonusDone&&!timeTrialMode):((coloredGemFlags&gemPlatformBit(e.source.type))&&!gemPathDone&&!timeTrialMode);
                if(available&&p.grounded){const auto box=entityBox(e);const double cx=(body[0]+body[2])*.5;if(horizontal(body,box)&&std::abs(body[3]-box[1])<=3.0&&std::abs(cx-e.x)<=7.0){if(bonus)bonusRequested=true;else gemPathRequested=true;}}
                continue;
            }
            if(slamShockwave&&std::hypot(e.x-p.x,e.y-p.y)<=64.0){
                const bool regularEnemy=e.source.type==EnemyType||e.source.type==PlantType||isAquaticEnemy(e.source.type)||isGenericEnemy(e.source.type)||e.source.type==SealRuntimeType;
                if(regularEnemy&&!genericInvulnerable(e.source.type)){e.active=false;e.attacking=false;++enemyKills;continue;}
                if(isCrate(e.source.type)&&!metalCrate(e.source.type)&&!nitroCrate(e.source.type)&&!tntCrate(e.source.type)){breakCrate(e,p);body=p.bodyBox();attack=p.attackBox();terrainBody=p.worldBox(p.terrainBounds());continue;}
            }
            if(e.source.type==EnemyType){stepEnemy(e,p,body,attack);body=p.bodyBox();attack=p.attackBox();terrainBody=p.worldBox(p.terrainBounds());continue;}
            if(e.source.type==PlantType){stepPlant(e,p,body,attack);body=p.bodyBox();attack=p.attackBox();terrainBody=p.worldBox(p.terrainBounds());continue;}
            if(e.source.type==DingodileType){stepDingodile(e,p);body=p.bodyBox();attack=p.attackBox();terrainBody=p.worldBox(p.terrainBounds());continue;}
            if(e.source.type==TinyType){stepTiny(e,p);body=p.bodyBox();attack=p.attackBox();terrainBody=p.worldBox(p.terrainBounds());continue;}
            if(e.source.type==CortexType){stepCortex(e,p);body=p.bodyBox();attack=p.attackBox();terrainBody=p.worldBox(p.terrainBounds());continue;}
            if(e.source.type==MegaMixType){stepMegaMix(e,p);body=p.bodyBox();attack=p.attackBox();terrainBody=p.worldBox(p.terrainBounds());continue;}
            if(isAquaticEnemy(e.source.type)){stepAquaticEnemy(e,p,body,attack);body=p.bodyBox();attack=p.attackBox();terrainBody=p.worldBox(p.terrainBounds());continue;}
            if(isGenericEnemy(e.source.type)){stepGenericEnemy(e,p,body,attack);body=p.bodyBox();attack=p.attackBox();terrainBody=p.worldBox(p.terrainBounds());continue;}
            if(isEnemySpawner(e.source.type)){stepEnemySpawner(e,p);continue;}
            if(!isCrate(e.source.type))continue;
            if(e.stackFalling)continue;
            if(e.triggered&&e.countdown){--e.countdown;if(!e.countdown){explode(e,p);body=p.bodyBox();attack=p.attackBox();terrainBody=p.worldBox(p.terrainBounds());}continue;}
            const auto box=entityBox(e);
            const bool spinHit=p.basicAttack()&&overlap(attack,box);
            const bool contact=touch(terrainBody,box,.75);
            const bool onTop=p.lastDownImpact>1.0&&horizontal(terrainBody,box)&&std::abs(terrainBody[3]-box[1])<1.25;
            const bool fromBelow=p.lastUpImpact<-.8&&horizontal(terrainBody,box)&&std::abs(terrainBody[1]-box[3])<3.25;
            if(nitroCrate(e.source.type)){
                if(spinHit||contact){deactivateCrate(e);++crateBroken;effects.push_back({e.x,e.y,35,0});hurt(p);body=p.bodyBox();attack=p.attackBox();terrainBody=p.worldBox(p.terrainBounds());}
                continue;
            }
            if(metalCrate(e.source.type)){if(onTop&&metalArrowCrate(e.source.type))p.bounce(-7.2);continue;}
            if(tntCrate(e.source.type)){
                if(spinHit){e.triggered=true;e.countdown=8;}
                else if(onTop||fromBelow){e.triggered=true;e.countdown=180;if(onTop)p.bounce();}
                continue;
            }
            if(spinHit){breakCrate(e,p);body=p.bodyBox();attack=p.attackBox();terrainBody=p.worldBox(p.terrainBounds());continue;}
            if(onTop&&e.source.type==BounceCrateType){p.bounce(-7.0);continue;}
            if((onTop||fromBelow)&&repeatedTriggerCrate(e.source.type)&&e.triggerCap){
                ++e.hits;++wumpaCollected;e.stateTicks=0;
                if(e.hits>=e.triggerCap)breakCrate(e,p,false);else if(onTop)p.bounce(-5.7);
                body=p.bodyBox();attack=p.attackBox();terrainBody=p.worldBox(p.terrainBounds());continue;
            }
            if(onTop||fromBelow){breakCrate(e,p);if(onTop)p.bounce();body=p.bodyBox();attack=p.attackBox();terrainBody=p.worldBox(p.terrainBounds());}
        }
        stepSpawnedEnemies(p);
        stepEnemyChildEffects(p);
        rebuildSolids();
    }

    void renderLogical(std::vector<uint32_t>& canvas,int w,int h,int cameraX,int cameraY,bool hud=true)const{
        if(!wumpaFrames.empty()){
            const auto& f=wumpaFrames[(ticks/4)%wumpaFrames.size()];
            for(const auto& e:items)if(e.active&&e.source.type==WumpaType)GameData::blit(canvas,w,h,f,int(std::lround(e.x))-cameraX,int(std::lround(e.y))-cameraY);
        }
        if(!gemClip.frames.empty()){const Frame* f=clipFrame(gemClip,ticks);if(f)for(const auto& e:items)if(e.active&&e.source.type==CrystalType)GameData::blit(canvas,w,h,*f,int(std::lround(e.x))-cameraX,int(std::lround(e.y))-cameraY);}
        if(!cortexGemSet.clips.empty())for(const auto& e:items)if(e.active&&!cortexArena&&(e.source.type==GemPathGemType||isColoredGem(e.source.type))){unsigned anim=coloredGemAnimation(e.source.type);if(anim<cortexGemSet.clips.size()){const Frame* f=clipFrame(cortexGemSet.clips[anim],ticks);if(f)GameData::blit(canvas,w,h,*f,int(std::lround(e.x))-cameraX,int(std::lround(e.y))-cameraY);}}
        if(cortexArena)for(const auto& e:items)if(e.active&&isCortexGem(e.source.type))drawCortexGem(canvas,w,h,cameraX,cameraY,e);
        if(!timeTrialClip.frames.empty()){const Frame* f=clipFrame(timeTrialClip,ticks);if(f)for(const auto& e:items)if(e.active&&e.source.type==TimeTrialType)GameData::blit(canvas,w,h,*f,int(std::lround(e.x))-cameraX,int(std::lround(e.y))-cameraY);}
        for(const auto& e:items)if(e.active&&isLevelObject(e.source.type)){
            if(isGemPlatform(e.source.type)&&!(coloredGemFlags&gemPlatformBit(e.source.type))){unsigned anim=gemOutlineAnimation(e.source.type);if(anim<cortexGemSet.clips.size()){const Frame* f=clipFrame(cortexGemSet.clips[anim],ticks);if(f)GameData::blit(canvas,w,h,*f,int(std::lround(e.x))-cameraX,int(std::lround(e.y))-cameraY);}continue;}
            unsigned anim=levelObjectAnimation(e);const Frame* f=clipFrame(levelObjectClips[anim],levelObjectTick(e,anim));
            if(f)GameData::blit(canvas,w,h,*f,int(std::lround(e.x))-cameraX,int(std::lround(e.y))-cameraY);
        }
        for(const auto& e:items)if(e.active&&e.source.type==EnemyType){
            const unsigned anim=e.dying?1u:0u;const Frame* f=clipFrame(enemyClips[anim],e.dying?e.deathTicks:e.stateTicks);
            if(f)GameData::blit(canvas,w,h,*f,int(std::lround(e.x))-cameraX,int(std::lround(e.y))-cameraY,e.facingLeft);
        }
        for(const auto& e:items)if(e.active&&e.source.type==PlantType){
            const unsigned anim=e.attacking?1u:0u;const Frame* f=clipFrame(plantClips[anim],e.attacking?e.attackTicks:e.stateTicks);
            if(f)GameData::blit(canvas,w,h,*f,int(std::lround(e.x))-cameraX,int(std::lround(e.y))-cameraY,e.facingLeft);
        }
        if(!flameSet.clips.empty())for(const auto& e:items)if(e.active&&e.source.type==FlameType){const Frame* f=clipFrame(flameSet.clips[0],ticks);if(f)GameData::blit(canvas,w,h,*f,int(std::lround(e.x))-cameraX,int(std::lround(e.y))-cameraY);}
        if(!seaweedSet.clips.empty())for(const auto& e:items)if(e.active&&e.source.type==SeaweedType){const Frame* f=clipFrame(seaweedSet.clips[0],ticks);if(f)GameData::blit(canvas,w,h,*f,int(std::lround(e.x))-cameraX,int(std::lround(e.y))-cameraY);}
        for(const auto& e:items)if(e.active&&isAquaticEnemy(e.source.type)){
            const EntitySpriteSet* set=aquaticSet(e.source.type);if(!set)continue;unsigned anim=aquaticAnimation(e);if(anim>=set->clips.size())continue;unsigned local=(e.attacking||e.hits==1||e.hits==3||e.hits==5)?e.attackTicks:e.stateTicks;const Frame* f=clipFrame(set->clips[anim],local);
            if(f)GameData::blit(canvas,w,h,*f,int(std::lround(e.x))-cameraX,int(std::lround(e.y))-cameraY,e.facingLeft);
        }
        for(const auto& e:items)if(e.active&&e.source.type==DingodileType)drawDingodile(canvas,w,h,cameraX,cameraY,e);
        for(const auto& e:items)if(e.active&&e.source.type==TinyType)drawTiny(canvas,w,h,cameraX,cameraY,e);
        for(const auto& e:items)if(e.active&&e.source.type==CortexType)drawCortex(canvas,w,h,cameraX,cameraY,e);
        for(const auto& e:items)if(e.active&&e.source.type==MegaMixType)drawMegaMix(canvas,w,h,cameraX,cameraY,e);
        for(const auto& e:items)if(e.active&&isGenericEnemy(e.source.type))drawGenericEnemy(canvas,w,h,cameraX,cameraY,e);
        for(const auto& e:spawnedEnemies)if(e.active)drawGenericEnemy(canvas,w,h,cameraX,cameraY,e);
        for(const auto& fx:childEffects)if(fx.active)drawEnemyChildEffect(canvas,w,h,cameraX,cameraY,fx);
        // GBA OBJ priority: for equal priority, the lower OAM index wins. Crates are
        // queued in entity-id order, so software compositing must run the list backwards
        // to preserve the original overlap order of linked crate stacks.
        for(auto it=items.rbegin();it!=items.rend();++it){const auto& e=*it;if(!e.active||!isCrate(e.source.type))continue;
            unsigned anim=crateAnimation(e.source.type),local=e.stateTicks;
            if(tntCrate(e.source.type)&&e.triggered){anim=e.countdown>120?18:e.countdown>60?19:20;local=e.stateTicks;}
            const Frame* f=clipFrame(anim,local);if(f)GameData::blit(canvas,w,h,*f,int(std::lround(e.x))-cameraX,int(std::lround(e.y))-cameraY);
        }
        for(const auto& fx:effects){const Frame* f=clipFrame(fx.animation,fx.ticks);if(f)GameData::blit(canvas,w,h,*f,int(std::lround(fx.x))-cameraX,int(std::lround(fx.y))-cameraY);}
        if(hud){
            if(!wumpaFrames.empty()){GameData::blit(canvas,w,h,wumpaFrames[(ticks/4)%wumpaFrames.size()],14,15);drawX(canvas,w,h,27,8);drawNumber(canvas,w,h,34,7,wumpaCollected);}
            if(const Frame* box=clipFrame(31,0)){GameData::blit(canvas,w,h,*box,78,15);drawX(canvas,w,h,91,8);drawNumber(canvas,w,h,98,7,crateBroken);}
            if(akuMasks){drawLetterA(canvas,w,h,139,7);drawX(canvas,w,h,146,8);drawNumber(canvas,w,h,153,7,akuMasks);}
            if(gemTotal&&!gemClip.frames.empty()){GameData::blit(canvas,w,h,*clipFrame(gemClip,ticks),198,15);drawX(canvas,w,h,210,8);drawNumber(canvas,w,h,217,7,gemsCollected);}
        }
    }

    Player::SolidBox entityBox(const RuntimeEntity& e)const{
        if(isCrate(e.source.type)){auto q=crateBounds[crateAnimation(e.source.type)];return {e.x+q[0],e.y+q[1],e.x+q[2],e.y+q[3]};}
        if(e.source.type==CrystalType)return {e.x+gemBounds[0],e.y+gemBounds[1],e.x+gemBounds[2],e.y+gemBounds[3]};
        if(!cortexArena&&(e.source.type==GemPathGemType||isColoredGem(e.source.type))){unsigned anim=coloredGemAnimation(e.source.type);if(anim<cortexGemSet.bounds.size())return translatedBox(e,cortexGemSet.bounds[anim]);}
        if(cortexArena&&isCortexGem(e.source.type)){unsigned anim=cortexGemInitialAnimation(e.source.type);const EntitySpriteSet& set=e.controllerMode?cortexSet:cortexGemSet;anim=e.controllerMode?cortexGemHitAnimation(e.source.type):anim;if(anim<set.bounds.size())return translatedBox(e,set.bounds[anim]);}
        if(e.source.type==TimeTrialType)return {e.x+timeTrialBounds[0],e.y+timeTrialBounds[1],e.x+timeTrialBounds[2],e.y+timeTrialBounds[3]};
        if(isGemPlatform(e.source.type)&&!(coloredGemFlags&gemPlatformBit(e.source.type))){unsigned anim=gemOutlineAnimation(e.source.type);if(anim<cortexGemSet.bounds.size())return translatedBox(e,cortexGemSet.bounds[anim]);}
        if(isLevelObject(e.source.type)){auto q=levelObjectBounds[levelObjectAnimation(e)];return {e.x+q[0],e.y+q[1],e.x+q[2],e.y+q[3]};}
        if(e.source.type==EnemyType){auto q=enemyBounds[e.dying?1:0];return translatedBox(e, q);}
        if(e.source.type==PlantType){auto q=plantBodyBounds[e.attacking?1:0];return translatedBox(e,q);}
        if(isAquaticEnemy(e.source.type)){const EntitySpriteSet* set=aquaticSet(e.source.type);if(set&&!set->bounds.empty()){unsigned anim=std::min<unsigned>(aquaticAnimation(e),unsigned(set->bounds.size()-1));return translatedBox(e,set->bounds[anim]);}}
        if(e.source.type==DingodileType&&!dingodileSet.bounds.empty()){unsigned anim=std::min<unsigned>(e.dingodile.animation,unsigned(dingodileSet.bounds.size()-1));return translatedBox(e,dingodileSet.bounds[anim]);}
        if(e.source.type==TinyType&&!tinySet.bounds.empty()){unsigned anim=std::min<unsigned>(e.tiny.animation,unsigned(tinySet.bounds.size()-1));return translatedBox(e,tinySet.bounds[anim]);}
        if(e.source.type==CortexType&&!cortexSet.bounds.empty()){unsigned anim=std::min<unsigned>(e.cortex.bossAnimation,unsigned(cortexSet.bounds.size()-1));return translatedBox(e,cortexSet.bounds[anim]);}
        if(e.source.type==MegaMixType&&!megaMixSet.bounds.empty()){unsigned anim=std::min<unsigned>(e.megaMix.animation,unsigned(megaMixSet.bounds.size()-1));return translatedBox(e,megaMixSet.bounds[anim]);}
        if(isGenericEnemy(e.source.type)||e.source.type==SealRuntimeType){const EntitySpriteSet* set=genericEnemySet(e.source.type);if(set&&!set->bounds.empty()){unsigned anim=std::min<unsigned>(genericAnimation(e),unsigned(set->bounds.size()-1));return translatedBox(e,set->bounds[anim]);}}
        return {e.x-12,e.y-12,e.x+12,e.y+12};
    }

    Player::SolidBox plantTriggerBox(const RuntimeEntity& e)const{Player::SolidBox q{0,-20,45,0};return translatedBox(e,q);}
    Player::SolidBox plantHitBox(const RuntimeEntity& e)const{return translatedBox(e,plantHitBounds[e.attacking?1:0]);}

    struct Snapshot {
        std::vector<RuntimeEntity> items;
        std::vector<RuntimeEntity> spawnedEnemies;
    std::vector<EnemyChildEffect> childEffects;
        unsigned wumpaCollected=0,crateBroken=0,checkpointHits=0,hazardHits=0;
        unsigned carriedTicks=0,ticks=0,enemyKills=0,plantAttacks=0,gemsCollected=0,coloredGemFlags=0;
        bool exitMarkerArmed=false,stackLinksValid=false,bonusDone=false,gemPathDone=false,timeTrialMode=false;
    };
    Snapshot snapshot()const{
        Snapshot s;s.items=items;s.spawnedEnemies=spawnedEnemies;s.stackLinksValid=true;s.wumpaCollected=wumpaCollected;s.crateBroken=crateBroken;s.checkpointHits=checkpointHits;s.hazardHits=hazardHits;s.carriedTicks=carriedTicks;s.ticks=ticks;s.enemyKills=enemyKills;s.plantAttacks=plantAttacks;s.gemsCollected=gemsCollected;s.coloredGemFlags=coloredGemFlags;s.exitMarkerArmed=exitMarkerArmed;s.bonusDone=bonusDone;s.gemPathDone=gemPathDone;s.timeTrialMode=timeTrialMode;return s;
    }
    void restore(const Snapshot& s){
        if(s.items.size()!=items.size())throw std::runtime_error("Entity snapshot revision");
        items=s.items;spawnedEnemies=s.spawnedEnemies;childEffects.clear();wumpaCollected=s.wumpaCollected;crateBroken=s.crateBroken;checkpointHits=s.checkpointHits;hazardHits=s.hazardHits;carriedTicks=s.carriedTicks;ticks=s.ticks;enemyKills=s.enemyKills;plantAttacks=s.plantAttacks;gemsCollected=s.gemsCollected;coloredGemFlags=s.coloredGemFlags;bonusDone=s.bonusDone;gemPathDone=s.gemPathDone;timeTrialMode=s.timeTrialMode;bonusRequested=gemPathRequested=false;dingodileHits=tinyHits=cortexHits=megaMixGrabs=megaMixCratesDestroyed=0;for(const auto& e:items){if(e.source.type==DingodileType)dingodileHits=std::max(dingodileHits,e.dingodile.hits);if(e.source.type==TinyType)tinyHits=std::max(tinyHits,e.tiny.round);if(e.source.type==CortexType)cortexHits=std::max(cortexHits,e.cortex.counter);}exitMarkerArmed=s.exitMarkerArmed;levelExitTriggered=false;effects.clear();if(!s.stackLinksValid)applyCrateLinks();for(auto& e:items){if(e.source.type==TinyType){if(e.tiny.rewardSpawned&&!e.tiny.rewardCollected)spawnTinyReward(e,false);if(e.tiny.complete&&e.tiny.rewardCollected)levelExitTriggered=true;}if(e.source.type==CortexType){if(e.cortex.rewardSpawned&&!e.cortex.rewardCollected)spawnCortexReward(e,false);if(e.cortex.complete&&e.cortex.rewardCollected)levelExitTriggered=true;}}rebuildSolids();
    }

 private:
    static uint32_t parameterAddress(const GameData& data,const Scene& scene,unsigned index){uint32_t list=data.rom.w(scene.descriptor+28),offsets=data.rom.w(list+8),params=data.rom.w(list+12);return params+data.rom.h(offsets+2*index);}
    static unsigned parameterByte(const GameData& data,const Scene& scene,unsigned index,unsigned off){return data.rom.b(parameterAddress(data,scene,index)+off);}
    static uint32_t parameterWord(const GameData& data,const Scene& scene,unsigned index,unsigned off){return data.rom.w(parameterAddress(data,scene,index)+off);}
    static int32_t parameterSWord(const GameData& data,const Scene& scene,unsigned index,unsigned off){return int32_t(parameterWord(data,scene,index,off));}
    static int16_t parameterSHalf(const GameData& data,const Scene& scene,unsigned index,unsigned off){return data.rom.sh(parameterAddress(data,scene,index)+off);}
    uint32_t parameterAddress(unsigned index)const{if(!dataRef)throw std::runtime_error("Entity runtime data missing");uint32_t list=dataRef->rom.w(sceneDescriptor+28),offsets=dataRef->rom.w(list+8),params=dataRef->rom.w(list+12);return params+dataRef->rom.h(offsets+2*index);}
    unsigned parameterByte(unsigned index,unsigned off)const{return dataRef->rom.b(parameterAddress(index)+off);}
    int32_t parameterSWord(unsigned index,unsigned off)const{return int32_t(dataRef->rom.w(parameterAddress(index)+off));}
    bool initialFacingLeft(unsigned type,unsigned param)const{bool left=((((parameterByte(param,0)>>1)^1)&1)!=0);return type==MorayType?!left:left;}
    static double approachDouble(double value,double target,double step){if(value<target)return std::min(target,value+step);if(value>target)return std::max(target,value-step);return value;}
    static int aquaticSetIndex(unsigned type){switch(type){case PufferfishType:return 0;case SharkType:return 1;case MorayType:return 2;case ElectricEelType:return 3;case SquidType:return 4;case JellyfishType:return 5;case SeaMineType:case SeaMineAltType:return 6;default:return -1;}}
    EntitySpriteSet* aquaticSet(unsigned type){int i=aquaticSetIndex(type);return i<0?nullptr:&aquaticSets[size_t(i)];}
    const EntitySpriteSet* aquaticSet(unsigned type)const{int i=aquaticSetIndex(type);return i<0?nullptr:&aquaticSets[size_t(i)];}
    unsigned aquaticAnimation(const RuntimeEntity& e)const{
        switch(e.source.type){
        case PufferfishType:switch(e.hits){case 3:return 0;case 4:return 1;case 5:return 3;default:return 2;}
        case SharkType:return e.attacking?0u:1u;
        case MorayType:return 0;
        case ElectricEelType:return e.hits==1?0u:e.hits==4?1u:2u;
        case SquidType:return e.hits==1?1u:e.hits==6?0u:2u;
        case JellyfishType:case SeaMineType:case SeaMineAltType:return 0;
        default:return 0;
        }
    }
    unsigned aquaticClipLength(unsigned type,unsigned anim)const{const EntitySpriteSet* set=aquaticSet(type);if(!set||anim>=set->clips.size()||set->clips[anim].frames.empty())return 1;return std::max(1u,set->clips[anim].duration)*unsigned(set->clips[anim].frames.size());}
    static int genericEnemySetIndex(unsigned type){switch(type){case VultureType:return 0;case PatrollingJungleType:return 1;case BlowgunType:return 2;case PenguinType:return 3;case SealRuntimeType:return 4;case PolarBearType:return 5;case LaserBarrierType:return 6;case StationarySpaceType:return 7;case PatrollingSpaceType:return 8;case SaucerType:case 0x3F:return 9;case PistonCrusherType:return 10;case FlamethrowerType:return 11;case HomingSewerType:return 12;case PatrollingSewerType:return 13;case RatType:return 14;case FrogType:return 15;case WoodenCrusherType:return 16;default:return -1;}}
    EntitySpriteSet* genericEnemySet(unsigned type){int i=genericEnemySetIndex(type);return i<0?nullptr:&genericEnemySets[size_t(i)];}
    const EntitySpriteSet* genericEnemySet(unsigned type)const{int i=genericEnemySetIndex(type);return i<0?nullptr:&genericEnemySets[size_t(i)];}
    static const std::array<unsigned,8>& genericAnimMap(unsigned type){
        static const std::array<unsigned,8> def{0,1,8,8,8,8,8,8},vulture{0,8,1,8,8,8,8,8},blowgun{3,8,0,4,1,2,8,5},penguin{0,3,8,2,1,4,8,8},stationarySpace{1,8,8,0,0,1,8,8},patrollingSpace{5,2,8,1,3,4,0,8},saucer{2,2,8,3,3,1,3,8},crusher{0,8,8,1,1,0,8,8},flame{1,8,8,2,3,0,8,8},sewer{1,0,8,8,8,8,8,8};
        switch(type){case VultureType:return vulture;case BlowgunType:return blowgun;case PenguinType:return penguin;case StationarySpaceType:return stationarySpace;case PatrollingSpaceType:return patrollingSpace;case SaucerType:case 0x3F:return saucer;case PistonCrusherType:case WoodenCrusherType:return crusher;case FlamethrowerType:return flame;case PatrollingSewerType:case FrogType:return sewer;default:return def;}
    }
    static bool usesGenericController(unsigned type){
        switch(type){case BlowgunType:case PenguinType:case StationarySpaceType:case PatrollingSpaceType:case SaucerType:case 0x3F:case PistonCrusherType:case FlamethrowerType:case WoodenCrusherType:return true;default:return false;}
    }
    unsigned genericAnimation(const RuntimeEntity& e)const{
        if(e.source.type==SealRuntimeType)return 0;
        const auto& map=genericAnimMap(e.source.type);unsigned mode=0;
        if(e.source.type==VultureType&&e.attacking)mode=2;
        else if(usesGenericController(e.source.type))mode=std::min(7u,e.controllerMode);
        unsigned anim=map[mode];if(anim==8)anim=map[0];if(anim==8)anim=0;return anim;
    }
    unsigned genericAnimTick(const RuntimeEntity& e)const{if(usesGenericController(e.source.type))return e.controllerTicks;return e.attacking?e.attackTicks:e.stateTicks;}
    unsigned genericClipLength(unsigned type,unsigned anim)const{const EntitySpriteSet* set=genericEnemySet(type);if(!set||anim>=set->clips.size()||set->clips[anim].frames.empty())return 1;return std::max(1u,set->clips[anim].duration)*unsigned(set->clips[anim].frames.size());}
    static bool overlap(const std::array<double,4>& a,const std::array<double,4>& b){return a[0]<b[2]&&a[2]>b[0]&&a[1]<b[3]&&a[3]>b[1];}
    static bool touch(const std::array<double,4>& a,const std::array<double,4>& b,double eps){return a[0]<=b[2]+eps&&a[2]>=b[0]-eps&&a[1]<=b[3]+eps&&a[3]>=b[1]-eps;}
    static bool horizontal(const std::array<double,4>& a,const std::array<double,4>& b){return a[2]>b[0]+.5&&a[0]<b[2]-.5;}
    static Player::SolidBox translatedBox(const RuntimeEntity& e,const Player::SolidBox& q){if(e.facingLeft)return {e.x-q[2],e.y+q[1],e.x-q[0],e.y+q[3]};return {e.x+q[0],e.y+q[1],e.x+q[2],e.y+q[3]};}

    static void configureMover(RuntimeEntity& e,unsigned kind,int distX,int distY,bool dirX,bool dirY){
        auto& m=e.mover;m.enabled=true;m.kind=kind;m.inputDistX=std::max(0,distX);m.inputDistY=std::max(0,distY);if(kind==6||kind==7)m.inputDistX=m.inputDistY=0;m.rangeX=m.inputDistX*2;m.rangeY=m.inputDistY*2;m.travelledX=m.inputDistX;m.travelledY=m.inputDistY;m.dirX=dirX;m.dirY=dirY;m.xQ8=int32_t(e.source.x)<<8;m.yQ8=int32_t(e.source.y)<<8;m.lastX=int(e.source.x);m.lastY=int(e.source.y);m.holdUntil=kind==6?120:0;setMoverTargets(m);
    }
    static void setMoverTargets(MoverState& m){m.targetVxQ8=m.rangeX?(m.dirX?256:-256):0;m.targetVyQ8=m.rangeY?(m.dirY?256:-256):0;}
    static int32_t approach(int32_t value,int32_t target,int32_t step){if(value<target)return std::min(target,value+step);if(value>target)return std::max(target,value-step);return value;}
    static void integrateAxis(int32_t& posQ8,int32_t& velocityQ8,int32_t targetQ8){velocityQ8=approach(velocityQ8,targetQ8,8);posQ8+=velocityQ8;}
    static void moverDistanceAxis(int currentInt,int& lastInt,int& travelled,int range,bool& dir,int32_t velocityQ8,int32_t& targetQ8){
        if(!range){lastInt=currentInt;return;}
        if(travelled!=-1)travelled+=std::abs(currentInt-lastInt);else if(std::abs(velocityQ8)>=256)travelled=0;
        if(travelled>range){dir=!dir;targetQ8=dir?256:-256;travelled=-1;}
        lastInt=currentInt;
    }
    unsigned levelObjectTick(const RuntimeEntity& e,unsigned anim)const{
        if(cortexArena&&e.source.type==TimedPlatformType&&anim<levelObjectClips.size())return e.controllerTicks*std::max(1u,levelObjectClips[anim].duration);
        if(!e.mover.enabled)return e.stateTicks;
        const auto& m=e.mover;
        if(m.kind==6){if(ticks<m.holdUntil)return 0;return ticks-m.holdUntil;}
        if(m.kind==7&&!m.active)return 0;
        return e.stateTicks;
    }
    unsigned levelObjectClipLength(unsigned anim)const{if(anim>=levelObjectClips.size()||levelObjectClips[anim].frames.empty())return 1;return std::max(1u,levelObjectClips[anim].duration)*unsigned(levelObjectClips[anim].frames.size());}
    void stepMovers(Player& p){
        for(auto& e:items){
            if(!e.active||!e.mover.enabled)continue;
            if(cortexArena&&e.source.type==TimedPlatformType){const unsigned target=e.controllerMode?10u:26u;if(e.controllerTicks!=target&&(ticks%5u)==0)e.controllerTicks=(e.controllerTicks+1u)%33u;continue;}
            auto oldBox=entityBox(e),body=p.bodyBox();
            const bool standing=p.grounded&&horizontal(body,oldBox)&&std::abs(body[3]-oldBox[1])<1.5;
            auto& m=e.mover;
            if(m.kind==1||m.kind==5||m.kind==6)m.active=standing;
            if(m.kind==6){
                const unsigned anim=levelObjectAnimation(e);
                if(ticks>=m.holdUntil&&ticks-m.holdUntil>=levelObjectClipLength(anim)){m.holdUntil=ticks+120;e.stateTicks=0;}
                continue;
            }
            if(m.kind==7){
                if(m.falling){e.prevY=e.y;e.y+=4.0;m.yQ8=int32_t(std::lround(e.y*256.0));if(e.y>=double(worldHeight>0?worldHeight:p.terrain.height())+32.0){e.active=false;m.active=false;m.falling=false;}continue;}
                if(m.active&&e.stateTicks>=levelObjectClipLength(levelObjectAnimation(e))){e.active=false;m.active=false;}
                continue;
            }
            e.prevX=e.x;e.prevY=e.y;
            if(m.kind==5&&!m.falling){
                if(standing&&m.specialTimer==0)m.specialTimer=1;
                else if(m.specialTimer>0)++m.specialTimer;
                if(m.specialTimer>=60){m.falling=true;m.targetVyQ8=1024;}
            }
            if(m.kind==5&&m.falling){
                m.vyQ8=approach(m.vyQ8,1024,40);m.yQ8+=m.vyQ8;e.y=double(m.yQ8)/256.0;
                if(e.y>p.terrain.height()+160){e.active=false;continue;}
            }else{
                if(m.rangeX)integrateAxis(m.xQ8,m.vxQ8,m.targetVxQ8);
                if(m.rangeY)integrateAxis(m.yQ8,m.vyQ8,m.targetVyQ8);
                e.x=double(m.xQ8)/256.0;e.y=double(m.yQ8)/256.0;
                moverDistanceAxis(int(m.xQ8>>8),m.lastX,m.travelledX,m.rangeX,m.dirX,m.vxQ8,m.targetVxQ8);
                moverDistanceAxis(int(m.yQ8>>8),m.lastY,m.travelledY,m.rangeY,m.dirY,m.vyQ8,m.targetVyQ8);
            }
            if(m.kind==5&&m.active&&!m.falling&&m.specialTimer&&m.specialTimer%30<=4)e.y+=(m.specialTimer&1)?3.0:-3.0;
            if(standing){p.x+=e.x-e.prevX;p.y+=e.y-e.prevY;p.grounded=true;++carriedTicks;}
        }
    }
    bool activateLinkedMover(unsigned id){for(auto& e:items)if(e.source.id==id&&e.mover.enabled){e.mover.active=true;e.stateTicks=0;return true;}return false;}
    RuntimeEntity* byId(unsigned id){return id<items.size()&&items[id].source.id==id?&items[id]:nullptr;}
    const RuntimeEntity* byId(unsigned id)const{return id<items.size()&&items[id].source.id==id?&items[id]:nullptr;}
    void applyCrateLinks(){
        const unsigned none=std::numeric_limits<unsigned>::max();for(auto& e:items){e.stackAbove=none;e.stackBelow=none;}crateLinkCount=0;
        for(const auto& link:crateLinks){RuntimeEntity* lower=byId(link.from);RuntimeEntity* upper=byId(link.to);if(!lower||!upper||!isCrate(lower->source.type)||!isCrate(upper->source.type))continue;++crateLinkCount;if(!lower->active||!upper->active)continue;lower->stackAbove=link.to;upper->stackBelow=link.from;}
    }
    void scheduleCratesAboveFall(const RuntimeEntity& removed){
        const unsigned none=std::numeric_limits<unsigned>::max();if(removed.stackAbove==none)return;RuntimeEntity* first=byId(removed.stackAbove);if(!first||!first->active||!isCrate(first->source.type))return;
        const double drop=std::max(0.0,removed.y-first->y);if(drop<=0.0)return;RuntimeEntity* cur=first;unsigned guard=0;
        while(cur&&cur->active&&isCrate(cur->source.type)&&guard++<items.size()){cur->stackFalling=true;cur->stackFallTargetY=cur->y+drop;cur->stackFallVelocity=std::min(0.0,cur->stackFallVelocity);if(cur->stackAbove==none)break;cur=byId(cur->stackAbove);}
    }
    void stepCrateFalls(){
        for(auto& e:items){if(!e.active||!e.stackFalling||!isCrate(e.source.type))continue;e.prevY=e.y;e.stackFallVelocity=std::min(4.0,e.stackFallVelocity+0.45);e.y+=e.stackFallVelocity;if(e.y>=e.stackFallTargetY){e.y=e.stackFallTargetY;e.stackFallVelocity=0;e.stackFalling=false;}}
    }
    void deactivateCrate(RuntimeEntity& e){
        if(!e.active)return;
        const unsigned none=std::numeric_limits<unsigned>::max();const unsigned aboveId=e.stackAbove,belowId=e.stackBelow;
        scheduleCratesAboveFall(e);
        RuntimeEntity* below=belowId==none?nullptr:byId(belowId);RuntimeEntity* above=aboveId==none?nullptr:byId(aboveId);
        if(below)below->stackAbove=above&&above->active?aboveId:none;
        if(above)above->stackBelow=below&&below->active?belowId:none;
        e.stackAbove=e.stackBelow=none;e.stackFalling=false;e.active=false;
    }
    void rebuildSolids(){
        solids.clear();solids.reserve(crateTotal+levelObjectTotal);
        for(const auto& e:items){if(!e.active)continue;if(cortexArena&&e.source.type==TimedPlatformType&&!e.controllerMode)continue;if(isCrate(e.source.type)||solidLevelObject(e.source.type))solids.push_back(entityBox(e));}
    }
    void stepEnemy(RuntimeEntity& e,Player& p,const Player::SolidBox& body,const Player::SolidBox& attack){
        if(e.dying){if(++e.deathTicks>=enemyClipLength(1))e.active=false;return;}
        if(p.basicAttack()&&overlap(attack,entityBox(e))){e.dying=true;e.deathTicks=0;e.stateTicks=0;++enemyKills;return;}
        const double dir=e.facingLeft?-1.0:1.0;double nx=e.x+dir*e.patrolSpeed;
        if(nx<e.patrolOrigin-e.patrolRange||nx>e.patrolOrigin+e.patrolRange){e.facingLeft=!e.facingLeft;nx=e.x;}
        auto q=enemyBounds[0];double floor=p.terrain.floor(nx+q[0],nx+q[2],e.y+q[3]-6,e.y+q[3]+12);
        if(std::isfinite(floor)){e.prevX=e.x;e.x=nx;e.y=floor-q[3];}else e.facingLeft=!e.facingLeft;
        if(overlap(body,entityBox(e)))hurt(p);
    }
    void stepPufferfish(RuntimeEntity& e){
        const int32_t idle=std::max<int32_t>(1,parameterSWord(e.source.param,4)),attack=std::max<int32_t>(1,parameterSWord(e.source.param,8));
        const int32_t amplitude=parameterSWord(e.source.param,16),period=std::max<int32_t>(1,parameterSWord(e.source.param,20)),phase=parameterSWord(e.source.param,24);
        constexpr double Tau=6.28318530717958647692;e.y=e.source.y+std::sin((double(ticks)*Tau/double(period))-(double(phase)*Tau/256.0))*double(amplitude);
        switch(e.hits){
        case 0:if(++e.attackTicks>=unsigned(idle)){e.hits=3;e.attackTicks=0;}break;
        case 3:if(++e.attackTicks>=aquaticClipLength(e.source.type,0)){e.hits=4;e.attackTicks=0;}break;
        case 4:if(++e.attackTicks>=unsigned(attack)){e.hits=5;e.attackTicks=0;}break;
        case 5:if(++e.attackTicks>=aquaticClipLength(e.source.type,3)){e.hits=0;e.attackTicks=0;}break;
        default:e.hits=0;e.attackTicks=0;break;
        }
    }
    void stepShark(RuntimeEntity& e,const Player& p){
        const double rangeX=std::abs(double(parameterSWord(e.source.param,4))),rangeY=std::abs(double(parameterSWord(e.source.param,8)));
        const double maxVy=std::abs(double(parameterSWord(e.source.param,12)))/256.0,accel=std::max(1.0,std::abs(double(parameterSWord(e.source.param,16))))/256.0;
        if(e.attacking){if(++e.attackTicks>=aquaticClipLength(e.source.type,0)){e.attacking=false;e.attackTicks=0;e.facingLeft=!e.facingLeft;}return;}
        const double nx=e.x+(e.facingLeft?-.5:.5);if(nx<e.source.x-rangeX||nx>e.source.x+rangeX){e.attacking=true;e.attackTicks=0;return;}e.prevX=e.x;e.x=nx;
        const double low=e.source.y-rangeY,high=e.source.y+rangeY;double target=0;if(p.y<e.y&&e.y>low)target=-maxVy;else if(p.y>e.y&&e.y<high)target=maxVy;e.stackFallVelocity=approachDouble(e.stackFallVelocity,target,accel);e.y=std::clamp(e.y+e.stackFallVelocity,low,high);
    }
    void stepElectricEel(RuntimeEntity& e){
        const double rangeX=std::abs(double(parameterSWord(e.source.param,4)));const unsigned idle=unsigned(std::max<int32_t>(1,parameterSWord(e.source.param,8))),attack=unsigned(std::max<int32_t>(1,parameterSWord(e.source.param,12))),offset=unsigned(std::max<int32_t>(0,parameterSWord(e.source.param,16)));
        if(e.attacking){e.hits=1;if(++e.attackTicks>=aquaticClipLength(e.source.type,0)){e.attacking=false;e.attackTicks=0;e.facingLeft=!e.facingLeft;}return;}
        const double nx=e.x+(e.facingLeft?-.5:.5);if(nx<e.source.x-rangeX||nx>e.source.x+rangeX){e.attacking=true;e.hits=1;e.attackTicks=0;return;}e.prevX=e.x;e.x=nx;
        const unsigned cycle=idle+attack,phase=cycle?(ticks+offset)%cycle:0;e.hits=phase>=idle?4u:0u;
    }
    void stepSquid(RuntimeEntity& e){
        constexpr double Tau=6.28318530717958647692;const double seed=double(e.source.id%31);e.x=e.source.x+std::sin((double(ticks)+seed)*Tau/150.0)*28.0;e.y=e.source.y+std::sin((double(ticks)+seed*2.0)*Tau/190.0)*18.0;
        const unsigned phase=(ticks/60+e.source.id)%3;e.hits=phase==0?0u:phase==1?6u:1u;e.facingLeft=std::cos((double(ticks)+seed)*Tau/150.0)<0;
    }
    void stepJellyfish(RuntimeEntity& e){
        const int32_t amplitude=parameterSWord(e.source.param,4),period=std::max<int32_t>(1,parameterSWord(e.source.param,8)),phase=parameterSWord(e.source.param,12);constexpr double Tau=6.28318530717958647692;e.y=e.source.y+std::sin((double(ticks)*Tau/double(period))-(double(phase)*Tau/256.0))*double(amplitude);
    }
    void stepSeaMine(RuntimeEntity& e,const Player& p){
        const double rangeY=std::abs(double(parameterSWord(e.source.param,4))),speedY=std::abs(double(parameterSWord(e.source.param,8)))/256.0,accelY=std::max(1.0,std::abs(double(parameterSWord(e.source.param,12))))/256.0;
        const double rangeX=std::abs(double(parameterSWord(e.source.param,16))),speedX=std::abs(double(parameterSWord(e.source.param,20)))/256.0,accelX=std::max(1.0,std::abs(double(parameterSWord(e.source.param,24))))/256.0;
        double tx=0,ty=0;if(p.x<e.x&&e.x>e.source.x-rangeX)tx=-speedX;else if(p.x>e.x&&e.x<e.source.x+rangeX)tx=speedX;if(p.y<e.y&&e.y>e.source.y-rangeY)ty=-speedY;else if(p.y>e.y&&e.y<e.source.y+rangeY)ty=speedY;
        e.stackFallTargetY=approachDouble(e.stackFallTargetY,tx,accelX);e.stackFallVelocity=approachDouble(e.stackFallVelocity,ty,accelY);e.prevX=e.x;e.prevY=e.y;e.x=std::clamp(e.x+e.stackFallTargetY,e.source.x-rangeX,e.source.x+rangeX);e.y=std::clamp(e.y+e.stackFallVelocity,e.source.y-rangeY,e.source.y+rangeY);
    }
    void stepAquaticEnemy(RuntimeEntity& e,Player& p,const Player::SolidBox& body,const Player::SolidBox& attack){
        if(p.basicAttack()&&overlap(attack,entityBox(e))){e.active=false;e.attacking=false;++enemyKills;return;}
        switch(e.source.type){case PufferfishType:stepPufferfish(e);break;case SharkType:stepShark(e,p);break;case MorayType:break;case ElectricEelType:stepElectricEel(e);break;case SquidType:stepSquid(e);break;case JellyfishType:stepJellyfish(e);break;case SeaMineType:case SeaMineAltType:stepSeaMine(e,p);break;default:break;}
        if(overlap(body,entityBox(e)))hurt(p);
    }
    void genericPatrol(RuntimeEntity& e,const Player& p,double speed=.8,bool keepY=false){
        const double range=std::max(8.0,std::abs(double(parameterSWord(e.source.param,4))));double nx=e.x+(e.facingLeft?-speed:speed);
        if(nx<e.source.x-range||nx>e.source.x+range){e.facingLeft=!e.facingLeft;return;}e.prevX=e.x;e.x=nx;
        if(!keepY){const EntitySpriteSet* set=genericEnemySet(e.source.type);if(set&&!set->bounds.empty()){auto q=set->bounds[std::min<unsigned>(genericAnimation(e),unsigned(set->bounds.size()-1))];double floor=p.terrain.floor(e.x+q[0],e.x+q[2],e.y+q[3]-8,e.y+q[3]+16);if(std::isfinite(floor))e.y=floor-q[3];}}
    }
    static int64_t positiveMod(int64_t value,int64_t divisor){if(divisor<=0)return 1;int64_t r=value%divisor;return r<0?r+divisor:r;}
    void setGenericControllerMode(RuntimeEntity& e,unsigned mode){if(e.controllerMode==mode)return;e.controllerMode=mode;e.controllerTicks=0;e.attackTicks=0;e.childSpawned=false;e.attacking=mode!=0&&mode!=4;}
    void genericAttackCycle(RuntimeEntity& e,int32_t idle,int32_t attack,int32_t offset){
        idle=std::max<int32_t>(1,idle);attack=std::max<int32_t>(1,attack);const int64_t cycle=int64_t(idle)+attack;const auto& map=genericAnimMap(e.source.type);
        switch(e.controllerMode){
        case 0:
            if(positiveMod(int64_t(ticks)+cycle*2-offset-idle,cycle)==0)setGenericControllerMode(e,map[3]!=8?3u:4u);
            break;
        case 4:
            if(positiveMod(int64_t(ticks)+idle+attack-offset,cycle)==0)setGenericControllerMode(e,map[5]!=8?5u:0u);
            break;
        case 3:{unsigned anim=map[3];if(anim==8)anim=map[0];if(e.controllerTicks>=genericClipLength(e.source.type,anim))setGenericControllerMode(e,4);break;}
        case 5:{unsigned anim=map[5];if(anim==8)anim=map[0];if(e.controllerTicks>=genericClipLength(e.source.type,anim))setGenericControllerMode(e,0);break;}
        default:break;
        }
        e.attacking=e.controllerMode!=0&&e.controllerMode!=4;e.attackTicks=e.controllerTicks;
    }
    unsigned dingodileClipLength(unsigned anim)const{
        if(anim>=dingodileSet.clips.size()||dingodileSet.clips[anim].frames.empty())return 1;
        const auto& c=dingodileSet.clips[anim];return std::max(1u,c.duration)*unsigned(c.frames.size());
    }
    unsigned dingodileFrameIndex(const RuntimeEntity& e)const{
        const unsigned anim=std::min<unsigned>(e.dingodile.animation,unsigned(dingodileSet.clips.size()-1));
        const auto& c=dingodileSet.clips[anim];if(c.frames.empty())return 0;
        unsigned n=e.dingodile.animationTicks/std::max(1u,c.duration);return (c.flags&2)?n%unsigned(c.frames.size()):std::min(n,unsigned(c.frames.size()-1));
    }
    bool dingodileAnimDone(const RuntimeEntity& e)const{
        const unsigned anim=e.dingodile.animation;if(anim>=dingodileSet.clips.size())return true;
        const auto& c=dingodileSet.clips[anim];return !(c.flags&2)&&e.dingodile.animationTicks>=std::max(1u,c.duration)*unsigned(c.frames.size());
    }
    void setDingodileAnimation(RuntimeEntity& e,unsigned anim){e.dingodile.animation=std::min<unsigned>(anim,unsigned(dingodileSet.clips.size()-1));e.dingodile.animationTicks=0;}
    static int32_t approachQ8(int32_t v,int32_t target,int32_t step){if(step<=0)return target;if(v<target)return std::min(target,v+step);if(v>target)return std::max(target,v-step);return v;}
    void setDingodileMotion(RuntimeEntity& e,unsigned mode){
        static constexpr int32_t startX[4]={0,0,-400,-600},stepX[4]={0,180,30,10},targetX[4]={0,300,0,0};
        static constexpr int32_t startY[4]={0,0,0,-1024},stepY[4]={0,0,0,64},targetY[4]={0,0,0,1024};
        mode=std::min(mode,3u);int32_t sx=startX[mode],tx=targetX[mode];if(e.facingLeft){sx=-sx;tx=-tx;}
        e.dingodile.vxQ8=sx;e.dingodile.stepXQ8=stepX[mode];e.dingodile.targetVxQ8=tx;e.dingodile.vyQ8=startY[mode];e.dingodile.stepYQ8=stepY[mode];e.dingodile.targetVyQ8=targetY[mode];
    }
    void stepDingodileMotion(RuntimeEntity& e){
        auto& d=e.dingodile;d.vxQ8=approachQ8(d.vxQ8,d.targetVxQ8,d.stepXQ8);d.vyQ8=approachQ8(d.vyQ8,d.targetVyQ8,d.stepYQ8);e.prevX=e.x;e.prevY=e.y;e.x+=double(d.vxQ8)/256.0;e.y+=double(d.vyQ8)/256.0;
    }
    void setDingodileShieldState(RuntimeEntity& e,unsigned state){auto& d=e.dingodile;d.shieldState=state;if(state==1||state==2){d.shieldBlinksLeft=2;d.shieldBlinkTimer=0;d.shieldState=state==1?3u:4u;}}
    void stepDingodileShield(RuntimeEntity& e){auto& d=e.dingodile;if(d.shieldState!=3&&d.shieldState!=4)return;if(d.shieldBlinkTimer==0){d.shieldBlinkTimer=20;d.shieldBlink=!d.shieldBlink;if(d.shieldBlinksLeft==0){d.shieldState=5;return;}--d.shieldBlinksLeft;}if(d.shieldBlinkTimer)--d.shieldBlinkTimer;}
    Player::SolidBox dingodileShieldBox(const RuntimeEntity& e)const{
        const auto& q=dingodileSet.bounds[3];const double x=e.x+(e.facingLeft?6.0:-6.0);if(e.facingLeft)return {x-q[2],e.y+q[1],x-q[0],e.y+q[3]};return {x+q[0],e.y+q[1],x+q[2],e.y+q[3]};
    }
    void spawnDingodileRocket(const RuntimeEntity& e){EnemyChildEffect fx;fx.kind=EnemyChildEffect::DingodileRocket;fx.bank=54;fx.animation=7;fx.ownerId=e.source.id;fx.x=e.x+(e.facingLeft?6.0:-6.0);fx.y=e.y-50.0;fx.facingLeft=e.facingLeft;fx.state=1;fx.maxTicks=600;fx.harmful=true;childEffects.push_back(fx);++enemyChildEffectsSpawned;}
    void spawnDingodileShark(const RuntimeEntity& e,double x,double y,bool facingLeft){EnemyChildEffect fx;fx.kind=EnemyChildEffect::DingodileShark;fx.bank=4;fx.animation=1;fx.ownerId=e.source.id;fx.x=x;fx.y=y;fx.facingLeft=facingLeft;fx.vx=facingLeft?-(288.0/256.0):(288.0/256.0);fx.maxTicks=900;fx.harmful=true;childEffects.push_back(fx);++enemyChildEffectsSpawned;}
    EnemyChildEffect makeDingodileStalactite(const EnemyChildEffect& rocket)const{EnemyChildEffect fx;fx.kind=EnemyChildEffect::DingodileStalactite;fx.bank=54;fx.animation=9;fx.ownerId=rocket.ownerId;fx.x=rocket.x;fx.y=rocket.y;fx.facingLeft=rocket.facingLeft;fx.state=3;fx.vy=0;fx.targetVy=736.0/256.0;fx.accelY=4.0/256.0;fx.maxTicks=600;fx.harmful=true;return fx;}
    void setDingodileState(RuntimeEntity& e,unsigned next){
        auto& d=e.dingodile;d.state=next;e.childSpawned=false;
        switch(next){
        case 1:case 14:setDingodileAnimation(e,0);setDingodileMotion(e,1);break;
        case 3:case 13:setDingodileAnimation(e,6);setDingodileMotion(e,0);break;
        case 4:setDingodileAnimation(e,1);setDingodileMotion(e,0);break;
        case 6:d.passes=0;[[fallthrough]];
        case 5:setDingodileAnimation(e,4);setDingodileMotion(e,0);break;
        case 2:setDingodileMotion(e,0);break;
        case 8:setDingodileShieldState(e,2);d.timer=0xD2;break;
        case 9:case 10:setDingodileShieldState(e,1);setDingodileAnimation(e,2);break;
        case 11:d.timer=d.hits>2?1u:0x64u;setDingodileAnimation(e,1);setDingodileMotion(e,2);break;
        case 12:spawnDingodileShark(e,double(worldWidth),40,true);spawnDingodileShark(e,0,70,false);spawnDingodileShark(e,double(worldWidth+70),100,true);setDingodileAnimation(e,0);setDingodileMotion(e,1);break;
        case 16:setDingodileMotion(e,3);break;
        default:break;
        }
    }
    static bool dingodileAtEdge(const RuntimeEntity& e,int width){return e.facingLeft?e.x<=32.0:e.x>=double(width)-32.0;}
    void stepDingodile(RuntimeEntity& e,Player& p){
        auto& d=e.dingodile;if(d.complete)return;++d.animationTicks;stepDingodileShield(e);
        if(d.state==8&&p.spinning()&&overlap(p.attackBox(),entityBox(e))){++d.hits;++dingodileHits;setDingodileState(e,11);}
        const unsigned state=d.state;
        switch(state){
        case 0:setDingodileState(e,1);d.step=0;d.passes=0;d.stalactiteVulnerable=false;break;
        case 1:case 14:{
            static constexpr double left0[4]={240,160,75,0},leftH[6]={260,210,160,110,75,0},right0[4]={75,160,240,1024},rightH[6]={75,110,160,210,250,1024};
            if(state==1){const bool hurt=d.hits>0;const unsigned count=hurt?6u:4u;d.step=std::min(d.step,count-1);const double mark=e.facingLeft?(hurt?leftH[d.step]:left0[d.step]):(hurt?rightH[d.step]:right0[d.step]);const bool crossed=e.facingLeft?e.x<=mark:e.x>=mark;if(crossed){const double ahead=e.facingLeft?p.x-e.x:e.x-p.x;if(ahead<=32.0){setDingodileState(e,5);++d.step;break;}++d.step;if(d.step>=count)d.step=count-1;}}
            if(!dingodileAtEdge(e,worldWidth))break;
            if(state==1){const unsigned lim=d.hits?2u:1u;if(++d.passes>=lim){setDingodileState(e,6);break;}}
            setDingodileState(e,3);break;}
        case 3:case 13:if(dingodileAnimDone(e)){const unsigned prev=state;e.x+=e.facingLeft?6.0:-6.0;e.facingLeft=!e.facingLeft;if(prev==3){d.timer=d.passes==0?0x73u:0x64u;d.nextState=(d.passes==0&&d.hits>1)?15u:1u;d.step=0;setDingodileState(e,2);}else setDingodileState(e,14);}break;
        case 15:spawnDingodileShark(e,e.facingLeft?0.0:double(worldWidth+40),45,!e.facingLeft);setDingodileState(e,1);break;
        case 4:if(dingodileAnimDone(e))setDingodileState(e,1);break;
        case 5:case 6:{const unsigned frame=dingodileFrameIndex(e);if(!e.childSpawned&&frame>=20){spawnDingodileRocket(e);e.childSpawned=true;}if(dingodileAnimDone(e)){if(state==6){d.stalactiteVulnerable=true;setDingodileState(e,3);}else setDingodileState(e,1);}break;}
        case 2:if(d.timer&&--d.timer==0)setDingodileState(e,d.nextState);break;
        case 7:setDingodileMotion(e,0);setDingodileAnimation(e,5);setDingodileShieldState(e,5);setDingodileState(e,8);break;
        case 8:if(d.timer&&--d.timer==0)setDingodileState(e,9);break;
        case 9:if(dingodileAnimDone(e))setDingodileState(e,1);break;
        case 10:if(dingodileAnimDone(e))setDingodileState(e,12);break;
        case 11:if(d.timer){--d.timer;break;}if(d.hits>2){setDingodileState(e,16);break;}if(dingodileAnimDone(e))setDingodileState(e,10);break;
        case 12:{const double lim=d.hits?32.0:64.0;const bool reached=e.facingLeft?e.x<=lim:e.x>=double(worldWidth)-lim;if(reached)setDingodileState(e,13);break;}
        case 16:if(e.y>=double(worldHeight>0?worldHeight:p.terrain.height())+32.0){d.complete=true;d.rewardSpawned=true;d.state=17;++enemyKills;levelExitTriggered=true;}break;
        case 17:break;
        default:break;}
        if(!d.complete)stepDingodileMotion(e);
        if(!d.complete&&d.state!=8&&d.state!=9&&d.state!=10&&d.state!=11&&d.shieldState==5&&!d.shieldBlink&&overlap(p.bodyBox(),dingodileShieldBox(e)))hurt(p);
    }
    void drawDingodile(std::vector<uint32_t>& canvas,int w,int h,int cameraX,int cameraY,const RuntimeEntity& e)const{
        if(dingodileSet.clips.empty())return;
        const unsigned anim=std::min<unsigned>(e.dingodile.animation,unsigned(dingodileSet.clips.size()-1));
        if(const Frame* f=clipFrame(dingodileSet.clips[anim],e.dingodile.animationTicks))GameData::blit(canvas,w,h,*f,int(std::lround(e.x))-cameraX,int(std::lround(e.y))-cameraY,e.facingLeft);
        if(!e.dingodile.complete&&!e.dingodile.shieldBlink&&dingodileSet.clips.size()>3){if(const Frame* shield=clipFrame(dingodileSet.clips[3],ticks)){const double sx=e.x+(e.facingLeft?6.0:-6.0);GameData::blit(canvas,w,h,*shield,int(std::lround(sx))-cameraX,int(std::lround(e.y))-cameraY,e.facingLeft);}}
    }
    static constexpr std::array<unsigned,3> tinyRoundAnchors{{4,1,0}};
    static constexpr std::array<unsigned,77> tinyHopTargets{{
        1,1,2,1,1,0,2,0,3,3,0,0,0,3,3,1,1,2,4,4,3,3,3,3,3,
        1,1,2,1,1,0,2,0,3,3,0,0,0,3,3,1,1,2,2,3,3,3,3,3,3,
        2,2,2,2,2,0,0,0,0,0,0,0,0,3,3,2,2,2,2,3,3,3,3,3,3,0,0
    }};
    RuntimeEntity* tinyAnchor(unsigned index){unsigned n=0;for(auto& a:items)if(a.source.type==FallingPlatformType){if(n++==index)return &a;}return nullptr;}
    const RuntimeEntity* tinyAnchor(unsigned index)const{unsigned n=0;for(const auto& a:items)if(a.source.type==FallingPlatformType){if(n++==index)return &a;}return nullptr;}
    unsigned tinyAnchorCount()const{unsigned n=0;for(const auto& a:items)if(a.source.type==FallingPlatformType)++n;return n;}
    unsigned tinyClipLength(unsigned anim)const{if(anim>=tinySet.clips.size()||tinySet.clips[anim].frames.empty())return 1;const auto& c=tinySet.clips[anim];return std::max(1u,c.duration)*unsigned(c.frames.size());}
    bool tinyAnimDone(const RuntimeEntity& e)const{unsigned anim=e.tiny.animation;if(anim>=tinySet.clips.size())return true;const auto& c=tinySet.clips[anim];return !(c.flags&2)&&e.tiny.animationTicks>=tinyClipLength(anim);}
    void setTinyAnimation(RuntimeEntity& e,unsigned anim,unsigned startFrame=0){anim=std::min<unsigned>(anim,unsigned(tinySet.clips.size()-1));e.tiny.animation=anim;e.tiny.animationTicks=startFrame*std::max(1u,tinySet.clips[anim].duration);}
    void startTinyHop(RuntimeEntity& e){auto& t=e.tiny;e.facingLeft=t.x<=e.x;t.steps=t.total=0x1A;t.dy=e.y-t.y;t.dx=e.x-t.x;}
    void stepTinyHop(RuntimeEntity& e,bool rising){auto& t=e.tiny;if(t.steps<=0||t.total<=0){t.steps=0;return;}const int steps=--t.steps;const double f=double(steps)/double(t.total);e.prevX=e.x;e.prevY=e.y;e.x=t.dx*f+t.x;const int q=std::clamp((steps<<8)/t.total,0,256);const double sq=double((q*q)>>8);if(rising)e.y=(sq*t.dy/256.0)+t.y;else{const int r=256-q;const double inv=256.0-double((r*r)>>8);e.y=(inv*t.dy/256.0)+t.y;}}
    unsigned pickTinyHopTarget(const TinyRuntime& t,const Player& p)const{unsigned nearest=0;double best=1e30,n=0;for(const auto& a:items)if(a.source.type==FallingPlatformType){const double d=std::abs(a.x-p.x)+std::abs(a.y-p.y);if(d<best){best=d;nearest=unsigned(n);}n+=1.0;}const unsigned idx=std::min<unsigned>(unsigned(t.target*5+nearest+t.round*25),unsigned(tinyHopTargets.size()-1));return tinyHopTargets[idx];}
    void spawnTinyLeaf(const RuntimeEntity& e,const Player& p,int n){EnemyChildEffect fx;fx.kind=EnemyChildEffect::TinyLeaf;fx.bank=55;fx.animation=5;fx.ownerId=e.source.id;fx.x=e.x+(p.x-e.x)*double(n-1)/3.0;fx.y=std::max(0.0,p.y-80.0);fx.vy=0.5;fx.harmful=true;fx.maxTicks=tinyClipLength(5);childEffects.push_back(fx);++enemyChildEffectsSpawned;}
    void spawnTinyReward(RuntimeEntity& e,bool countSpawn=true){RuntimeEntity* a=tinyAnchor(2);if(!a)return;EnemyChildEffect fx;fx.kind=EnemyChildEffect::TinyReward;fx.bank=38;fx.animation=0;fx.ownerId=e.source.id;fx.x=a->x;fx.y=a->y-24.0;fx.harmful=false;fx.maxTicks=0;childEffects.push_back(fx);if(countSpawn)++enemyChildEffectsSpawned;e.tiny.rewardSpawned=true;}
    void sinkTinyAnchor(unsigned index){RuntimeEntity* a=tinyAnchor(index);if(!a||!a->active)return;a->mover.enabled=true;a->mover.kind=7;a->mover.active=true;a->mover.falling=true;a->stateTicks=0;}
    void setTinyState(RuntimeEntity& e,unsigned next,const Player* p=nullptr){auto& t=e.tiny;t.state=next;switch(next){
        case 13:t.nextState=0;t.timer=4;[[fallthrough]];
        case 11:if(t.round){t.target=tinyRoundAnchors[std::min<unsigned>(t.round-1,2)];}[[fallthrough]];
        case 1:case 2:{setTinyAnimation(e,next==1?2u:1u,4);RuntimeEntity* a=tinyAnchor(t.target);if(!a)break;t.x=a->x;if(next==11||next==13)e.x=t.x;t.y=a->y-36.0;startTinyHop(e);break;}
        case 3:case 7:{if(p)t.target=pickTinyHopTarget(t,*p);setTinyAnimation(e,4);RuntimeEntity* a=tinyAnchor(t.target);if(!a)break;const double ay=a->y-36.0;t.x=(a->x+e.x)*.5;t.y=ay>=e.y?e.y-89.0:a->y-125.0;startTinyHop(e);break;}
        case 10:t.y=-48.0;t.x=e.x;startTinyHop(e);break;
        case 8:setTinyAnimation(e,6);t.timer=0xB4;break;
        case 9:++t.round;if(t.round>2){t.y=e.y-100.0;t.x=e.x+100.0;startTinyHop(e);}setTinyAnimation(e,7);break;
        case 14:if(!t.rewardSpawned)spawnTinyReward(e);break;
        case 15:t.x=e.x+100.0;t.y=double(worldHeight)+64.0;startTinyHop(e);break;
        default:break;}}
    void stepTiny(RuntimeEntity& e,Player& p){auto& t=e.tiny;if(t.complete){if(t.rewardCollected)levelExitTriggered=true;return;}++t.animationTicks;if(t.stomped>=0){sinkTinyAnchor(unsigned(t.stomped));t.stomped=-1;}
        if(t.state==8){if(p.spinning()&&overlap(p.attackBox(),entityBox(e))){setTinyState(e,9,&p);tinyHits=std::max(tinyHits,t.round);}}
        else if(overlap(p.bodyBox(),entityBox(e)))hurt(p);
        const unsigned state=t.state;switch(state){
        case 0:t.target=1;t.count=0;setTinyState(e,1,&p);break;
        case 1:case 2:case 11:case 15:stepTinyHop(e,false);if(t.steps==0){if(state==15){t.complete=true;t.state=16;++enemyKills;if(t.rewardCollected)levelExitTriggered=true;}else if(state==1)setTinyState(e,6,&p);else if(state==11){t.count=0;setTinyAnimation(e,3);t.nextState=12;setTinyState(e,5,&p);}else setTinyState(e,8,&p);}break;
        case 3:case 7:case 10:case 14:stepTinyHop(e,true);if(t.steps==0){if(state==14)setTinyState(e,15,&p);else if(state==3)setTinyState(e,1,&p);else if(state==7)setTinyState(e,2,&p);else setTinyState(e,13,&p);}break;
        case 13:if(t.nextState==0){--t.timer;if(t.timer==0)setTinyState(e,11,&p);else{t.nextState=0x46;spawnTinyLeaf(e,p,t.timer);}}if(t.state==13)--t.nextState;break;
        case 6:if(tinyAnimDone(e)){if(++t.count>3){t.count=0;setTinyState(e,7,&p);}else setTinyState(e,3,&p);}break;
        case 8:if(t.timer>0){--t.timer;break;}setTinyAnimation(e,0);t.nextState=3;setTinyState(e,5,&p);break;
        case 5:if(tinyAnimDone(e)){setTinyState(e,t.nextState,&p);break;}[[fallthrough]];
        case 4:if(t.timer>0&&--t.timer==0)setTinyState(e,t.nextState,&p);break;
        case 12:if(t.round){t.stomped=int(tinyRoundAnchors[std::min<unsigned>(t.round-1,2)]);}setTinyState(e,3,&p);break;
        case 9:if(t.round>2){setTinyState(e,14,&p);break;}if(tinyAnimDone(e))setTinyState(e,10,&p);break;
        case 16:if(t.rewardCollected)levelExitTriggered=true;break;
        default:break;}}
    void drawTiny(std::vector<uint32_t>& canvas,int w,int h,int cameraX,int cameraY,const RuntimeEntity& e)const{if(tinySet.clips.empty())return;const unsigned anim=std::min<unsigned>(e.tiny.animation,unsigned(tinySet.clips.size()-1));if(const Frame* f=clipFrame(tinySet.clips[anim],e.tiny.animationTicks))GameData::blit(canvas,w,h,*f,int(std::lround(e.x))-cameraX,int(std::lround(e.y))-cameraY,e.facingLeft);}

    static unsigned cortexGemInitialAnimation(unsigned type){switch(type){case 0x0A:return 3;case 0x0B:return 2;case 0x0C:return 0;default:return 0;}}
    static unsigned cortexGemHitAnimation(unsigned type){switch(type){case 0x0A:return 12;case 0x0B:return 11;case 0x0C:return 13;default:return 11;}}
    unsigned cortexClipLength(unsigned anim)const{if(anim>=cortexSet.clips.size()||cortexSet.clips[anim].frames.empty())return 1;const auto& c=cortexSet.clips[anim];return std::max(1u,c.duration)*unsigned(c.frames.size());}
    unsigned cortexGemHitLength(unsigned type)const{return cortexClipLength(cortexGemHitAnimation(type));}
    bool cortexTargetAnimDone(const CortexRuntime& c)const{if(c.targetAnimation>=cortexSet.clips.size())return true;const auto& clip=cortexSet.clips[c.targetAnimation];return !(clip.flags&2)&&c.targetTicks>=cortexClipLength(c.targetAnimation);}
    void setCortexTargetAnimation(CortexRuntime& c,unsigned anim,unsigned frame=0){if(cortexSet.clips.empty())return;c.targetAnimation=std::min<unsigned>(anim,unsigned(cortexSet.clips.size()-1));c.targetTicks=frame*std::max(1u,cortexSet.clips[c.targetAnimation].duration);c.targetFrame=frame;}
    void setCortexPlatformsKind(unsigned kind){for(auto& p:items)if(p.active&&p.source.type==TimedPlatformType)p.controllerMode=kind?1u:0u;}
    void setCortexTargetDest(CortexRuntime& c,double x,double y,unsigned steps){c.targetDestX=x;c.targetDestY=y;c.targetDeltaX=x-c.targetX;c.targetDeltaY=y-c.targetY;c.targetSteps=c.targetTotal=int(std::max(1u,steps));}
    void stepCortexTargetMotion(CortexRuntime& c){if(c.targetSteps<=0||c.targetTotal<=0)return;const int left=--c.targetSteps;const double f=double(left)/double(c.targetTotal);c.targetX=c.targetDestX-c.targetDeltaX*f;c.targetY=c.targetDestY-c.targetDeltaY*f;}
    unsigned cortexPhase(const CortexRuntime& c)const{return std::min(2u,c.counter);}
    void setCortexTargetState(RuntimeEntity& e,unsigned next){auto& c=e.cortex;c.targetState=next;switch(next){
        case 1:setCortexPlatformsKind(0);setCortexTargetAnimation(c,15);c.targetDirLeft=true;c.targetHigh=true;c.targetTop=false;setCortexTargetDest(c,double(worldWidth)-4.0,152.0,std::array<unsigned,4>{16,14,10,32}[std::min(3u,c.counter)]);c.targetNextState=2;break;
        case 2:{c.targetNextState=3;double x=c.targetDestX;if(c.targetDirLeft&&x-24.0<=4.0)c.targetDirLeft=false;else if(!c.targetDirLeft&&x+28.0>=double(worldWidth))c.targetNextState=5;switch(cortexPhase(c)){case 0:c.targetHigh=c.targetDirLeft;break;case 1:c.targetHigh=!c.targetHigh;break;default:c.targetHigh=!c.targetHigh;if(!c.targetHigh)c.targetTop=!c.targetTop;break;}x+=c.targetDirLeft?-24.0:24.0;double y=c.targetHigh?(c.targetTop?62.0:152.0):130.0;setCortexTargetDest(c,x,y,std::array<unsigned,4>{16,14,10,32}[std::min(3u,c.counter)]);break;}
        case 5:c.targetBlinking=false;c.targetBlink=0;setCortexPlatformsKind(1);c.targetTimer=20;break;
        case 8:setCortexTargetDest(c,double(worldWidth)*.5,double(worldHeight)+32.0,std::array<unsigned,4>{16,14,10,32}[std::min(3u,c.counter)]);break;
        default:break;}}
    void spawnCortexShot(const RuntimeEntity& e,bool fast){const auto& c=e.cortex;EnemyChildEffect fx;fx.kind=fast?EnemyChildEffect::CortexFastShot:EnemyChildEffect::CortexSlowShot;fx.bank=53;fx.animation=fast?17u:14u;fx.ownerId=e.source.id;fx.x=c.targetX;fx.y=c.targetY;fx.harmful=true;fx.maxTicks=cortexClipLength(fx.animation);childEffects.push_back(fx);++enemyChildEffectsSpawned;}
    void spawnCortexReward(RuntimeEntity& e,bool countSpawn=true){EnemyChildEffect fx;fx.kind=EnemyChildEffect::CortexReward;fx.bank=38;fx.animation=3;fx.ownerId=e.source.id;fx.x=140.0;fx.y=152.0;fx.harmful=false;fx.maxTicks=0;childEffects.push_back(fx);if(countSpawn)++enemyChildEffectsSpawned;e.cortex.rewardSpawned=true;}
    void setCortexBossState(RuntimeEntity& e,unsigned next){auto& c=e.cortex;c.state=next;if(next==3){setCortexTargetState(e,9);if(!c.rewardSpawned)spawnCortexReward(e);}}
    void stepCortexGem(RuntimeEntity& e){if(!e.controllerMode)return;++e.controllerTicks;if(e.controllerTicks>=cortexGemHitLength(e.source.type))e.active=false;}
    void hitCortexGem(RuntimeEntity& boss,RuntimeEntity& gem){if(!gem.active||gem.controllerMode)return;gem.controllerMode=1;gem.controllerTicks=0;auto& c=boss.cortex;++c.counter;cortexHits=std::max(cortexHits,c.counter);if(c.counter>=3)setCortexBossState(boss,3);else{c.state=1;setCortexTargetState(boss,1);}}
    void stepCortexTarget(RuntimeEntity& e,const Player& p){auto& c=e.cortex;++c.targetTicks;stepCortexTargetMotion(c);switch(c.targetState){
        case 0:setCortexTargetState(e,1);break;
        case 1:case 2:case 6:if(c.targetSteps==0)setCortexTargetState(e,c.targetNextState);break;
        case 3:c.targetNextState=4;setCortexTargetAnimation(c,18);c.targetState=7;break;
        case 4:spawnCortexShot(e,false);setCortexTargetState(e,2);setCortexTargetAnimation(c,15);break;
        case 5:{if(c.targetBlinking&&++c.targetBlink>9){c.targetBlink=0;c.targetFrame=c.targetFrame?0u:1u;}if(c.targetSteps!=0)break;if(c.targetTimer>0)--c.targetTimer;if(c.targetTimer==0){setCortexTargetState(e,1);spawnCortexShot(e,true);break;}if(c.targetTimer==4){setCortexTargetAnimation(c,16);c.targetBlinking=true;c.targetBlink=0;}else if(c.targetTimer==2){setCortexTargetAnimation(c,16,1);c.targetFrame=1;}const unsigned steps=std::array<unsigned,3>{24,22,18}[cortexPhase(c)];setCortexTargetDest(c,p.x,p.y-10.0,steps);break;}
        case 7:if(cortexTargetAnimDone(c))setCortexTargetState(e,c.targetNextState);break;
        case 8:if(c.targetSteps==0)setCortexTargetState(e,c.targetNextState);break;
        case 9:c.targetNextState=10;setCortexTargetState(e,8);break;
        default:break;}}
    void stepCortex(RuntimeEntity& e,Player& p){auto& c=e.cortex;if(c.complete){if(c.rewardCollected)levelExitTriggered=true;return;}if(c.childrenSpawned)stepCortexTarget(e,p);switch(c.state){
        case 0:c.childrenSpawned=true;c.targetX=e.x+32.0;c.targetY=e.y-64.0;c.targetDestX=c.targetX;c.targetDestY=c.targetY;setCortexTargetState(e,1);c.state=1;break;
        case 1:{const double dx=c.targetX-e.x;e.facingLeft=dx<0;c.cannonAnimation=c.targetY<=80.0?5u:(c.targetY<=120.0?4u:3u);const double width=std::max(1.0,double(worldWidth));const unsigned n=std::min(5u,unsigned(std::abs(dx)*12.0/width));c.bossFrame=c.cannonFrame=5u-n;break;}
        case 2:break;
        case 3:e.prevY=e.y;e.y+=0.5;if(e.y>=double(worldHeight)+64.0){c.complete=true;c.state=4;++enemyKills;if(c.rewardCollected)levelExitTriggered=true;}break;
        default:break;}}
    void drawCortexGem(std::vector<uint32_t>& canvas,int w,int h,int cameraX,int cameraY,const RuntimeEntity& e)const{const EntitySpriteSet& set=e.controllerMode?cortexSet:cortexGemSet;const unsigned anim=e.controllerMode?cortexGemHitAnimation(e.source.type):cortexGemInitialAnimation(e.source.type);if(anim>=set.clips.size())return;const unsigned local=e.controllerMode?e.controllerTicks:e.stateTicks;if(const Frame* f=clipFrame(set.clips[anim],local))GameData::blit(canvas,w,h,*f,int(std::lround(e.x))-cameraX,int(std::lround(e.y))-cameraY);}
    void drawCortex(std::vector<uint32_t>& canvas,int w,int h,int cameraX,int cameraY,const RuntimeEntity& e)const{if(cortexSet.clips.empty())return;const auto& c=e.cortex;if(c.childrenSpawned){if(c.cannonAnimation<cortexSet.clips.size()){const auto& clip=cortexSet.clips[c.cannonAnimation];const unsigned tick=c.cannonFrame*std::max(1u,clip.duration);if(const Frame* f=clipFrame(clip,tick))GameData::blit(canvas,w,h,*f,int(std::lround(e.x))-cameraX,int(std::lround(e.y))-cameraY,e.facingLeft);}if(c.targetAnimation<cortexSet.clips.size()){const auto& clip=cortexSet.clips[c.targetAnimation];const unsigned tick=c.targetAnimation==16?c.targetFrame*std::max(1u,clip.duration):c.targetTicks;if(const Frame* f=clipFrame(clip,tick))GameData::blit(canvas,w,h,*f,int(std::lround(c.targetX))-cameraX,int(std::lround(c.targetY))-cameraY);}}if(c.bossAnimation<cortexSet.clips.size()){const auto& clip=cortexSet.clips[c.bossAnimation];const unsigned tick=c.bossFrame*std::max(1u,clip.duration);if(const Frame* f=clipFrame(clip,tick))GameData::blit(canvas,w,h,*f,int(std::lround(e.x))-cameraX,int(std::lround(e.y))-cameraY,e.facingLeft);}}
    unsigned megaMixClipLength(unsigned anim)const{if(anim>=megaMixSet.clips.size()||megaMixSet.clips[anim].frames.empty())return 1;return std::max(1u,megaMixSet.clips[anim].duration)*unsigned(megaMixSet.clips[anim].frames.size());}
    unsigned megaMixFrame(const MegaMixRuntime& m)const{if(m.animation>=megaMixSet.clips.size()||megaMixSet.clips[m.animation].frames.empty())return 0;const auto& clip=megaMixSet.clips[m.animation];return std::min<unsigned>(unsigned(clip.frames.size()-1),m.animationTicks/std::max(1u,clip.duration));}
    bool megaMixAnimDone(const MegaMixRuntime& m)const{return m.animationTicks>=megaMixClipLength(m.animation);}
    void setMegaMixAnimation(RuntimeEntity& e,unsigned anim){e.megaMix.animation=std::min<unsigned>(anim,unsigned(megaMixSet.clips.size()-1));e.megaMix.animationTicks=0;e.megaMix.grabDamageDone=false;}
    void setMegaMixMotion(RuntimeEntity& e,unsigned mode,bool start){static constexpr int32_t rec[4][3]={{0,0,0},{0,32,450},{0,32,620},{450,32,750}};mode=std::min(3u,mode);auto& m=e.megaMix;const int sign=e.facingLeft?-1:1;m.motionMode=mode;m.stepXQ8=rec[mode][1];m.targetVxQ8=sign*rec[mode][2];if(start)m.vxQ8=sign*rec[mode][0];}
    bool megaMixOnScreen(const RuntimeEntity& e,const Player& p)const{return std::abs(e.x-p.x)<=180.0&&std::abs(e.y-p.y)<=130.0;}
    bool megaMixInGrabRange(const RuntimeEntity& e,const Player& p)const{return std::abs(e.x-p.x)<=double(0x27ff)/256.0&&std::abs(e.y-p.y)<=double(0x31ff)/256.0;}
    void stepMegaMixVelocity(RuntimeEntity& e,Player& p){auto& m=e.megaMix;if(m.stepXQ8>0){if(m.vxQ8<m.targetVxQ8)m.vxQ8=std::min(m.targetVxQ8,m.vxQ8+m.stepXQ8);else if(m.vxQ8>m.targetVxQ8)m.vxQ8=std::max(m.targetVxQ8,m.vxQ8-m.stepXQ8);}e.prevX=e.x;e.prevY=e.y;e.x+=double(m.vxQ8)/256.0;if(!megaMixSet.bounds.empty()){const auto& q=megaMixSet.bounds[std::min<unsigned>(m.animation,unsigned(megaMixSet.bounds.size()-1))];double floor=p.terrain.floor(e.x+q[0],e.x+q[2],e.y+q[3]-10,e.y+q[3]+20);if(std::isfinite(floor))e.y=floor-q[3];}}
    void smashMegaMixCrates(RuntimeEntity& e,Player& p){for(auto& c:items){if(!c.active||!isCrate(c.source.type)||metalCrate(c.source.type))continue;if(std::abs(c.x-e.x)>39.0||std::abs(c.y-e.y)>59.0)continue;const bool was=c.active;if(nitroCrate(c.source.type)||tntCrate(c.source.type))explode(c,p);else breakCrate(c,p);if(was&&!c.active)++megaMixCratesDestroyed;}}
    void stepMegaMix(RuntimeEntity& e,Player& p){auto& m=e.megaMix;++m.animationTicks;if(m.stamp==-1){setMegaMixMotion(e,1,false);m.stamp=0;}const bool onScreen=megaMixOnScreen(e,p);if(onScreen&&!m.latch){m.stamp=int(ticks);m.latch=true;}else if(!onScreen&&m.latch){m.stamp=int(ticks);m.latch=false;}switch(m.state){
        case 0:m.state=1;setMegaMixAnimation(e,0);setMegaMixMotion(e,e.facingLeft?3u:1u,e.facingLeft);break;
        case 1:if(m.stamp!=0&&int(ticks)-m.stamp>60){m.stamp=0;if(e.facingLeft)setMegaMixMotion(e,3,true);else setMegaMixMotion(e,m.latch?1u:2u,false);}if(p.x<e.x&&!e.facingLeft){e.facingLeft=true;setMegaMixMotion(e,3,true);}else if(p.x>e.x+10.0&&e.facingLeft){e.facingLeft=false;setMegaMixMotion(e,1,true);}if(megaMixInGrabRange(e,p)){setMegaMixMotion(e,0,false);m.state=2;setMegaMixAnimation(e,1);}else smashMegaMixCrates(e,p);break;
        case 2:if(megaMixFrame(m)==8&&!m.grabDamageDone&&megaMixInGrabRange(e,p)){m.grabDamageDone=true;++megaMixGrabs;hurt(p);}if(megaMixAnimDone(m)){m.state=1;setMegaMixAnimation(e,0);if(e.facingLeft)setMegaMixMotion(e,3,true);else setMegaMixMotion(e,1,false);}break;
        default:m.state=1;setMegaMixAnimation(e,0);break;}stepMegaMixVelocity(e,p);}
    void drawMegaMix(std::vector<uint32_t>& canvas,int w,int h,int cameraX,int cameraY,const RuntimeEntity& e)const{if(megaMixSet.clips.empty())return;const unsigned anim=std::min<unsigned>(e.megaMix.animation,unsigned(megaMixSet.clips.size()-1));if(const Frame* f=clipFrame(megaMixSet.clips[anim],e.megaMix.animationTicks))GameData::blit(canvas,w,h,*f,int(std::lround(e.x))-cameraX,int(std::lround(e.y))-cameraY,e.facingLeft);}
    const EntitySpriteSet* childEffectSet(unsigned bank)const{if(dingodileSet.bank==bank)return &dingodileSet;if(tinySet.bank==bank)return &tinySet;if(cortexSet.bank==bank)return &cortexSet;if(powerSet.bank==bank)return &powerSet;for(const auto& set:aquaticSets)if(set.bank==bank)return &set;for(const auto& set:genericEnemySets)if(set.bank==bank)return &set;return nullptr;}
    Player::SolidBox childEffectBox(const EnemyChildEffect& fx)const{const EntitySpriteSet* set=childEffectSet(fx.bank);if(!set||fx.animation>=set->bounds.size())return {fx.x-4,fx.y-4,fx.x+4,fx.y+4};const auto& q=set->bounds[fx.animation];if(fx.facingLeft)return {fx.x-q[2],fx.y+q[1],fx.x-q[0],fx.y+q[3]};return {fx.x+q[0],fx.y+q[1],fx.x+q[2],fx.y+q[3]};}
    void spawnEnemyChildEffect(const RuntimeEntity& owner,unsigned bank,unsigned animation,int margin,int z,double speed,bool harmful=true){
        const EntitySpriteSet* set=childEffectSet(bank);if(!set||animation>=set->clips.size()||animation>=set->bounds.size())return;
        const auto ownerBox=entityBox(owner);const auto& q=set->bounds[animation];const double childW=q[2]-q[0],ownerW=ownerBox[2]-ownerBox[0];const double dist=childW*.5+ownerW*.5+double(margin);
        EnemyChildEffect fx;fx.bank=bank;fx.animation=animation;fx.ownerId=owner.source.id;fx.facingLeft=owner.facingLeft;fx.harmful=harmful;fx.x=owner.x+(owner.facingLeft?-dist:dist);fx.y=owner.y+z;fx.vx=owner.facingLeft?-speed:speed;const auto& clip=set->clips[animation];fx.maxTicks=(clip.flags&2)?240u:std::max(1u,clip.duration)*unsigned(clip.frames.size());childEffects.push_back(fx);++enemyChildEffectsSpawned;
    }
    void stepBlowgunShooter(RuntimeEntity& e){
        const int32_t idle=std::max<int32_t>(1,parameterSWord(e.source.param,4)),attack=std::max<int32_t>(1,parameterSWord(e.source.param,8)),offset=parameterSWord(e.source.param,12);const int64_t period=std::max<int64_t>(1,(int64_t(idle)+attack)/2),phase=int64_t(offset)+period/4;
        if((e.controllerMode==0||e.controllerMode==4)&&positiveMod(int64_t(ticks)+period-phase,period)==0){setGenericControllerMode(e,e.controllerMode==0?2u:7u);return;}
        if(e.controllerMode!=2&&e.controllerMode!=7)return;
        const unsigned anim=genericAnimMap(e.source.type)[e.controllerMode];
        const EntitySpriteSet* set=genericEnemySet(e.source.type);
        if(!set||anim>=set->clips.size())return;
        const unsigned triggerFrame=e.controllerMode==2?10u:8u;
        const unsigned triggerTick=triggerFrame*std::max(1u,set->clips[anim].duration);
        if(!e.childSpawned&&e.controllerTicks>=triggerTick){spawnEnemyChildEffect(e,12,6,0,e.controllerMode==2?-10:8,4.0,true);e.childSpawned=true;}
        if(e.controllerTicks>=genericClipLength(e.source.type,anim))setGenericControllerMode(e,e.controllerMode==2?0u:4u);
    }
    void stepFlamethrowerChild(RuntimeEntity& e){
        if(e.controllerMode!=3){if(e.controllerMode==0||e.controllerMode==4||e.controllerMode==5)e.childSpawned=false;return;}const unsigned anim=2;const EntitySpriteSet* set=genericEnemySet(FlamethrowerType);if(!set||anim>=set->clips.size())return;const unsigned triggerTick=9u*std::max(1u,set->clips[anim].duration);if(!e.childSpawned&&e.controllerTicks>=triggerTick){spawnEnemyChildEffect(e,23,4,-45,2,0.0,true);e.childSpawned=true;}
    }
    void stepEnemyChildEffects(Player& p){
        auto body=p.bodyBox(),attack=p.attackBox();std::vector<EnemyChildEffect> pending;
        for(auto& fx:childEffects){
            if(!fx.active)continue;
            ++fx.ticks;
            if(fx.kind==EnemyChildEffect::DingodileRocket){
                if(fx.state==1){fx.vy=approachDouble(fx.vy,-4.0,7.0/256.0);fx.y+=fx.vy;if(fx.harmful&&overlap(body,childEffectBox(fx))){++enemyChildEffectHits;hurt(p);body=p.bodyBox();attack=p.attackBox();}if(fx.y<=8.0){pending.push_back(makeDingodileStalactite(fx));++enemyChildEffectsSpawned;fx.state=2;fx.animation=8;fx.ticks=0;fx.vx=fx.vy=0;fx.maxTicks=dingodileClipLength(8);}}
                else if(fx.ticks>=fx.maxTicks)fx.active=false;
                continue;
            }
            if(fx.kind==EnemyChildEffect::DingodileStalactite){
                if(fx.state==3){fx.vy=approachDouble(fx.vy,fx.targetVy,fx.accelY);fx.y+=fx.vy;RuntimeEntity* boss=byId(fx.ownerId);if(boss&&boss->active&&boss->source.type==DingodileType&&boss->dingodile.stalactiteVulnerable&&overlap(childEffectBox(fx),entityBox(*boss))){setDingodileState(*boss,7);fx.state=6;fx.animation=8;fx.ticks=0;fx.vy=0;fx.maxTicks=dingodileClipLength(8);}else if(fx.harmful&&overlap(body,childEffectBox(fx))){++enemyChildEffectHits;hurt(p);body=p.bodyBox();attack=p.attackBox();fx.state=6;fx.animation=8;fx.ticks=0;fx.vy=0;fx.maxTicks=dingodileClipLength(8);}else if(fx.y>=double(worldHeight>0?worldHeight:p.terrain.height())-32.0){fx.state=6;fx.animation=8;fx.ticks=0;fx.vy=0;fx.maxTicks=dingodileClipLength(8);}}
                else if(fx.state==6&&fx.ticks>=fx.maxTicks)fx.active=false;
                continue;
            }
            if(fx.kind==EnemyChildEffect::DingodileShark){
                fx.x+=fx.vx;if(fx.harmful&&overlap(body,childEffectBox(fx))){++enemyChildEffectHits;hurt(p);body=p.bodyBox();attack=p.attackBox();}const int maxX=worldWidth>0?worldWidth:p.terrain.width();if(fx.x<-40||fx.x>maxX+40||fx.ticks>=fx.maxTicks)fx.active=false;continue;
            }
            if(fx.kind==EnemyChildEffect::TinyLeaf){
                fx.y+=fx.vy;if(fx.harmful&&overlap(body,childEffectBox(fx))){fx.active=false;++enemyChildEffectHits;hurt(p);body=p.bodyBox();attack=p.attackBox();continue;}if(fx.ticks>=fx.maxTicks||fx.y>double(worldHeight>0?worldHeight:p.terrain.height())+32.0)fx.active=false;continue;
            }
            if(fx.kind==EnemyChildEffect::TinyReward){
                if(overlap(body,childEffectBox(fx))){if(RuntimeEntity* boss=byId(fx.ownerId)){if(boss->source.type==TinyType){boss->tiny.rewardCollected=true;p.grantPower(Player::TornadoSpinPower);if(boss->tiny.complete)levelExitTriggered=true;}}fx.active=false;}continue;
            }
            if(fx.kind==EnemyChildEffect::CortexReward){
                if(overlap(body,childEffectBox(fx))){if(RuntimeEntity* boss=byId(fx.ownerId)){if(boss->source.type==CortexType){boss->cortex.rewardCollected=true;p.grantPower(Player::TurboRunPower);if(boss->cortex.complete)levelExitTriggered=true;}}fx.active=false;}continue;
            }
            if(fx.kind==EnemyChildEffect::CortexSlowShot||fx.kind==EnemyChildEffect::CortexFastShot){
                const auto box=childEffectBox(fx);bool playerHit=false;
                if(fx.harmful&&overlap(body,box)){++enemyChildEffectHits;hurt(p);body=p.bodyBox();attack=p.attackBox();fx.harmful=false;playerHit=true;}
                if(!playerHit&&fx.kind==EnemyChildEffect::CortexFastShot&&fx.harmful){if(RuntimeEntity* boss=byId(fx.ownerId)){for(auto& gem:items){if(!gem.active||!isCortexGem(gem.source.type)||gem.controllerMode)continue;if(overlap(box,entityBox(gem))){hitCortexGem(*boss,gem);fx.harmful=false;break;}}}}
                if(fx.ticks>=fx.maxTicks)fx.active=false;
                continue;
            }
            if(fx.followOwnerX){const RuntimeEntity* owner=byId(fx.ownerId);if(owner&&owner->active)fx.x=owner->x;}fx.x+=fx.vx;fx.y+=fx.vy;const auto box=childEffectBox(fx);if(p.spinning()&&overlap(attack,box)){fx.active=false;continue;}if(fx.harmful&&overlap(body,box)){fx.active=false;++enemyChildEffectHits;hurt(p);body=p.bodyBox();attack=p.attackBox();continue;}const int maxX=worldWidth>0?worldWidth:p.terrain.width();if(fx.ticks>=fx.maxTicks||fx.x<-48||fx.x>maxX+48||fx.y<-64||fx.y>p.terrain.height()+64)fx.active=false;
        }
        childEffects.insert(childEffects.end(),pending.begin(),pending.end());childEffects.erase(std::remove_if(childEffects.begin(),childEffects.end(),[](const EnemyChildEffect& fx){return !fx.active;}),childEffects.end());
    }
    void drawEnemyChildEffect(std::vector<uint32_t>& canvas,int w,int h,int cameraX,int cameraY,const EnemyChildEffect& fx)const{const EntitySpriteSet* set=childEffectSet(fx.bank);if(!set||fx.animation>=set->clips.size())return;const Frame* f=clipFrame(set->clips[fx.animation],fx.ticks);if(f)GameData::blit(canvas,w,h,*f,int(std::lround(fx.x))-cameraX,int(std::lround(fx.y))-cameraY,fx.facingLeft);}

    void stepVulture(RuntimeEntity& e,const Player& p){
        if(!e.attacking){Player::SolidBox trigger=translatedBox(e,{0,0,100,50});if(overlap(p.bodyBox(),trigger)){e.attacking=true;e.attackTicks=0;e.stackFallVelocity=0;}return;}
        ++e.attackTicks;if(e.attackTicks<36){e.x=approachDouble(e.x,p.x,1.75);e.stackFallVelocity=approachDouble(e.stackFallVelocity,3.5,.18);e.y+=e.stackFallVelocity;}else{e.x=approachDouble(e.x,e.source.x,2.0);e.y=approachDouble(e.y,e.source.y,2.5);if(std::abs(e.x-e.source.x)<1.0&&std::abs(e.y-e.source.y)<1.0){e.x=e.source.x;e.y=e.source.y;e.attacking=false;e.attackTicks=0;e.stackFallVelocity=0;}}
    }
    void stepHomingSewer(RuntimeEntity& e,const Player& p){const double range=std::max(8.0,std::abs(double(parameterSWord(e.source.param,4)))),speed=std::max(.25,std::abs(double(parameterSWord(e.source.param,8)))/256.0),accel=std::max(.05,std::abs(double(parameterSWord(e.source.param,12)))/256.0);const double target=std::clamp(p.x,e.source.x-range,e.source.x+range);const double desired=(target>e.x)?speed:(target<e.x?-speed:0.0);e.stackFallTargetY=approachDouble(e.stackFallTargetY,desired,accel);e.x=std::clamp(e.x+e.stackFallTargetY,e.source.x-range,e.source.x+range);e.facingLeft=e.stackFallTargetY<0;}
    void stepFrog(RuntimeEntity& e,const Player& p){const unsigned phase=(ticks+e.source.id*11)%96;if(phase<64){const double t=double(phase)/64.0;e.y=e.source.y-std::sin(t*3.14159265358979323846)*34.0;e.x+=e.facingLeft?-.65:.65;}else e.y=approachDouble(e.y,e.source.y,2.0);if(std::abs(e.x-e.source.x)>48.0)e.facingLeft=!e.facingLeft;if(phase==0)e.facingLeft=p.x<e.x;}
    void stepGenericEnemy(RuntimeEntity& e,Player& p,const Player::SolidBox& body,const Player::SolidBox& attack){
        if(!genericInvulnerable(e.source.type)&&p.basicAttack()&&overlap(attack,entityBox(e))){e.active=false;e.attacking=false;++enemyKills;return;}
        if(usesGenericController(e.source.type))++e.controllerTicks;
        switch(e.source.type){
        case VultureType:stepVulture(e,p);break;
        case PatrollingJungleType:case PolarBearType:case PatrollingSewerType:case RatType:genericPatrol(e,p);break;
        case BlowgunType:{int32_t a=parameterSWord(e.source.param,4),b=parameterSWord(e.source.param,8),c=parameterSWord(e.source.param,12);genericAttackCycle(e,a,b,c);stepBlowgunShooter(e);break;}
        case PenguinType:genericPatrol(e,p);genericAttackCycle(e,parameterSWord(e.source.param,12),parameterSWord(e.source.param,8),parameterSWord(e.source.param,16));break;
        case LaserBarrierType:break;
        case StationarySpaceType:genericAttackCycle(e,parameterSWord(e.source.param,8),parameterSWord(e.source.param,4),parameterSWord(e.source.param,12));break;
        case PatrollingSpaceType:genericPatrol(e,p,.7);genericAttackCycle(e,parameterSWord(e.source.param,8),parameterSWord(e.source.param,12),parameterSWord(e.source.param,16));break;
        case SaucerType:case 0x3F:genericPatrol(e,p,.55,true);genericAttackCycle(e,0x78,0x5A,parameterSWord(e.source.param,12));e.y=e.source.y-40.0+std::sin((double(ticks)+e.source.id)*.055)*8.0;break;
        case PistonCrusherType:case WoodenCrusherType:genericAttackCycle(e,parameterSWord(e.source.param,4),parameterSWord(e.source.param,8),parameterSWord(e.source.param,12));break;
        case FlamethrowerType:genericAttackCycle(e,parameterSWord(e.source.param,8),parameterSWord(e.source.param,4),parameterSWord(e.source.param,12));stepFlamethrowerChild(e);break;
        case HomingSewerType:stepHomingSewer(e,p);break;
        case FrogType:stepFrog(e,p);break;
        default:break;}
        if(overlap(body,entityBox(e)))hurt(p);
    }
    void stepEnemySpawner(RuntimeEntity& e,const Player& p){if((e.stateTicks%120)!=0||std::abs(p.x-e.x)>340.0)return;RuntimeEntity seal(Entity{unsigned(0x10000+spawnedEnemies.size()),SealRuntimeType,unsigned(std::lround(e.x)),unsigned(std::lround(e.y)),0,0});seal.facingLeft=p.x<e.x;seal.patrolOrigin=e.x;seal.patrolRange=360;spawnedEnemies.push_back(seal);}
    void stepSpawnedEnemies(Player& p){auto body=p.bodyBox(),attack=p.attackBox();for(auto& e:spawnedEnemies){if(!e.active)continue;++e.stateTicks;if(p.basicAttack()&&overlap(attack,entityBox(e))){e.active=false;++enemyKills;continue;}e.prevX=e.x;e.x+=e.facingLeft?-1.5:1.5;const EntitySpriteSet* set=genericEnemySet(SealRuntimeType);if(set&&!set->bounds.empty()){auto q=set->bounds[0];double floor=p.terrain.floor(e.x+q[0],e.x+q[2],e.y+q[3]-8,e.y+q[3]+18);if(std::isfinite(floor))e.y=floor-q[3];}if(e.stateTicks>300||std::abs(e.x-e.patrolOrigin)>e.patrolRange){e.active=false;continue;}if(overlap(body,entityBox(e))){hurt(p);body=p.bodyBox();attack=p.attackBox();}}spawnedEnemies.erase(std::remove_if(spawnedEnemies.begin(),spawnedEnemies.end(),[](const RuntimeEntity& e){return !e.active&&e.stateTicks>30;}),spawnedEnemies.end());}
    void drawGenericEnemy(std::vector<uint32_t>& canvas,int w,int h,int cameraX,int cameraY,const RuntimeEntity& e)const{const EntitySpriteSet* set=genericEnemySet(e.source.type);if(!set||set->clips.empty())return;unsigned anim=std::min<unsigned>(genericAnimation(e),unsigned(set->clips.size()-1));const Frame* f=clipFrame(set->clips[anim],genericAnimTick(e));if(f)GameData::blit(canvas,w,h,*f,int(std::lround(e.x))-cameraX,int(std::lround(e.y))-cameraY,e.facingLeft);}
    unsigned enemyClipLength(unsigned anim)const{if(anim>=enemyClips.size()||enemyClips[anim].frames.empty())return 1;return std::max(1u,enemyClips[anim].duration)*unsigned(enemyClips[anim].frames.size());}
    unsigned plantClipLength(unsigned anim)const{if(anim>=plantClips.size()||plantClips[anim].frames.empty())return 1;return std::max(1u,plantClips[anim].duration)*unsigned(plantClips[anim].frames.size());}
    void stepPlant(RuntimeEntity& e,Player& p,const Player::SolidBox& body,const Player::SolidBox& attack){
        // Venus Flytrap (entity 0x2A / bank 10): its bank only has idle+attack
        // clips. A spin hit therefore removes the enemy immediately instead of
        // borrowing another species' death animation.
        if(p.basicAttack()&&overlap(attack,entityBox(e))){e.active=false;e.attacking=false;e.attackDamageDone=false;++enemyKills;return;}
        // Original state 3 / sub_800C5D4: mode 0 tests the fixed 45x20 trigger box; mode 2 plays bank-10 anim 1 until animDone, then returns to mode 0.
        if(!e.attacking){if(overlap(body,plantTriggerBox(e))){e.attacking=true;e.attackTicks=0;e.attackDamageDone=false;e.stateTicks=0;++plantAttacks;}return;}
        ++e.attackTicks;
        if(!e.attackDamageDone&&overlap(body,plantHitBox(e))){e.attackDamageDone=true;hurt(p);}
        if(e.attackTicks>=plantClipLength(1)){e.attacking=false;e.attackTicks=0;e.attackDamageDone=false;e.stateTicks=0;}
    }
    void breakCrate(RuntimeEntity& e,Player& p,bool reward=true){
        if(!e.active)return;
        deactivateCrate(e);++crateBroken;effects.push_back({e.x,e.y,29,0});
        if(e.source.type==CheckpointCrateType){++checkpointHits;p.setCheckpoint(e.x,e.y);}
        else if(e.source.type==AkuCrateType)akuMasks=std::min(2u,akuMasks+1);
        else if(e.source.type==LifeCrateType)++extraLives;
        else if(reward&&repeatedTriggerCrate(e.source.type)&&e.triggerCap>e.hits){wumpaCollected+=e.triggerCap-e.hits;e.hits=e.triggerCap;}
    }
    void hurt(Player& p){++hazardHits;if(akuMasks)--akuMasks;else p.respawn();}
    void explode(RuntimeEntity& e,Player& p){
        if(!e.active)return;
        deactivateCrate(e);++crateBroken;effects.push_back({e.x,e.y,33,0});
        auto b=p.bodyBox();double cx=(b[0]+b[2])/2,cy=(b[1]+b[3])/2;if(std::hypot(cx-e.x,cy-e.y)<48.0)hurt(p);
        for(auto& other:items){if(!other.active||&other==&e||!isCrate(other.source.type))continue;if(std::hypot(other.x-e.x,other.y-e.y)>42.0)continue;
            if(nitroCrate(other.source.type)){deactivateCrate(other);++crateBroken;effects.push_back({other.x,other.y,35,0});}
            else if(tntCrate(other.source.type)){other.triggered=true;other.countdown=8;}
            else if(!metalCrate(other.source.type))breakCrate(other,p);
        }
    }
    unsigned effectLength(unsigned anim)const{if(anim>=crateClips.size()||crateClips[anim].frames.empty())return 1;return std::max(1u,crateClips[anim].duration)*unsigned(crateClips[anim].frames.size());}
    const Frame* clipFrame(unsigned anim,unsigned tick)const{if(anim>=crateClips.size())return nullptr;return clipFrame(crateClips[anim],tick);}
    static const Frame* clipFrame(const EntityClip& c,unsigned tick){if(c.frames.empty())return nullptr;size_t n=tick/std::max(1u,c.duration);n=(c.flags&2)?n%c.frames.size():std::min(n,c.frames.size()-1);return &c.frames[n];}

    static Frame decodeFramed(const GameData& data,const Bytes& asset,size_t p,unsigned paletteRecord){
        if(p+4>asset.size())throw std::runtime_error("Framed sprite header");
        unsigned wt=asset[p],ht=asset[p+1];if((wt!=1&&wt!=2&&wt!=4&&wt!=8)||(ht!=1&&ht!=2&&ht!=4&&ht!=8)||asset[p+2]!=0x10||asset[p+3]!=0)throw std::runtime_error("Framed sprite format");
        size_t bytes=size_t(wt)*ht*32;if(p+4+bytes>asset.size())throw std::runtime_error("Framed sprite data");Frame out;out.width=int(wt*8);out.height=int(ht*8);out.x=-out.width/2;out.y=-out.height/2;out.pixels.assign(size_t(out.width*out.height),0);
        std::array<uint32_t,16> colors{};uint32_t palette=data.rom.w(GameData::Table+8)+32*paletteRecord;for(unsigned i=0;i<16;i++)colors[i]=rgb555(data.rom.h(palette+2*i));
        const size_t base=p+4;for(unsigned ty=0;ty<ht;ty++)for(unsigned tx=0;tx<wt;tx++)for(unsigned y=0;y<8;y++)for(unsigned x=0;x<8;x++){size_t a=base+(ty*wt+tx)*32+y*4+x/2;unsigned v=(asset[a]>>((x&1)*4))&15;if(v)out.pixels[size_t((ty*8+y)*out.width+tx*8+x)]=colors[v];}
        return out;
    }
    void loadWumpaFrames(const GameData& data){
        Bytes asset=data.rom.tagged(0x080B1C60);if(asset.size()!=213064)throw std::runtime_error("Wumpa sheet revision");std::vector<size_t> offsets;size_t p=0;
        while(p<asset.size()){if(p+4>asset.size())throw std::runtime_error("Framed sheet tail");unsigned wt=asset[p],ht=asset[p+1];if((wt!=1&&wt!=2&&wt!=4&&wt!=8)||(ht!=1&&ht!=2&&ht!=4&&ht!=8)||asset[p+2]!=0x10||asset[p+3]!=0)throw std::runtime_error("Framed sheet revision");offsets.push_back(p);p+=4+size_t(wt)*ht*32;}
        if(p!=asset.size()||offsets.size()!=274)throw std::runtime_error("Framed sheet count");
        for(unsigned i=200;i<214;i++)wumpaFrames.push_back(decodeFramed(data,asset,offsets[i],33));
    }
    void loadCrateClips(const GameData& data){
        const auto& bank=data.banks.at(31);if(bank.animations.size()!=36)throw std::runtime_error("Crate animation count");
        for(unsigned ai=0;ai<bank.animations.size();++ai){const auto& a=bank.animations[ai];auto& clip=crateClips[ai];clip.duration=std::max(1u,a.duration);clip.flags=a.flags;for(unsigned fi:a.sequence)clip.frames.push_back(data.frame(31,fi,a.paletteRecord));double l=data.rom.sh(a.address+4),t=data.rom.sh(a.address+6);crateBounds[ai]={l,t,l+data.rom.b(a.address+8),t+data.rom.b(a.address+9)};}
    }
    void loadLevelObjectClips(const GameData& data){
        const auto& bank=data.banks.at(39);if(bank.animations.size()!=levelObjectClips.size())throw std::runtime_error("Level-object bank revision");
        for(unsigned ai=0;ai<bank.animations.size();++ai){const auto& a=bank.animations[ai];auto& clip=levelObjectClips[ai];clip.duration=std::max(1u,a.duration);clip.flags=a.flags;for(unsigned fi:a.sequence)clip.frames.push_back(data.frame(39,fi,a.paletteRecord));double l=data.rom.sh(a.address+4),t=data.rom.sh(a.address+6);levelObjectBounds[ai]={l,t,l+data.rom.b(a.address+8),t+data.rom.b(a.address+9)};}
    }
    void loadEnemyClips(const GameData& data){
        const auto& bank=data.banks.at(13);if(bank.animations.size()!=enemyClips.size())throw std::runtime_error("Enemy bank 13 revision");
        for(unsigned ai=0;ai<bank.animations.size();++ai){const auto& a=bank.animations[ai];auto& clip=enemyClips[ai];clip.duration=std::max(1u,a.duration);clip.flags=a.flags;for(unsigned fi:a.sequence)clip.frames.push_back(data.frame(13,fi,a.paletteRecord));double l=data.rom.sh(a.address+4),t=data.rom.sh(a.address+6);enemyBounds[ai]={l,t,l+data.rom.b(a.address+8),t+data.rom.b(a.address+9)};}
    }
    void loadPlantClips(const GameData& data){
        const auto& bank=data.banks.at(10);if(bank.animations.size()!=plantClips.size())throw std::runtime_error("Plant bank 10 revision");
        for(unsigned ai=0;ai<bank.animations.size();++ai){const auto& a=bank.animations[ai];auto& clip=plantClips[ai];clip.duration=std::max(1u,a.duration);clip.flags=a.flags;for(unsigned fi:a.sequence)clip.frames.push_back(data.frame(10,fi,a.paletteRecord));double l=data.rom.sh(a.address+4),t=data.rom.sh(a.address+6);plantBodyBounds[ai]={l,t,l+data.rom.b(a.address+8),t+data.rom.b(a.address+9)};l=data.rom.sh(a.address+12);t=data.rom.sh(a.address+14);plantHitBounds[ai]={l,t,l+data.rom.b(a.address+16),t+data.rom.b(a.address+17)};}
    }
    static void loadSpriteSet(const GameData& data,unsigned bankId,EntitySpriteSet& set){
        const auto& bank=data.banks.at(bankId);set.bank=bankId;set.clips.clear();set.bounds.clear();set.clips.resize(bank.animations.size());set.bounds.resize(bank.animations.size());
        for(unsigned ai=0;ai<bank.animations.size();++ai){const auto& a=bank.animations[ai];auto& clip=set.clips[ai];clip.duration=std::max(1u,a.duration);clip.flags=a.flags;for(unsigned fi:a.sequence)clip.frames.push_back(data.frame(bankId,fi,a.paletteRecord));double l=data.rom.sh(a.address+4),t=data.rom.sh(a.address+6);set.bounds[ai]={l,t,l+data.rom.b(a.address+8),t+data.rom.b(a.address+9)};}
    }
    void loadAquaticClips(const GameData& data){
        static constexpr unsigned banks[7]={5,4,3,8,7,9,6};for(unsigned i=0;i<7;i++)loadSpriteSet(data,banks[i],aquaticSets[i]);
        if(aquaticSets[0].clips.size()!=4||aquaticSets[1].clips.size()!=2||aquaticSets[2].clips.size()!=1||aquaticSets[3].clips.size()!=3||aquaticSets[4].clips.size()!=3||aquaticSets[5].clips.size()!=1||aquaticSets[6].clips.size()!=1)throw std::runtime_error("Aquatic enemy bank revision");
    }
    void loadGenericEnemyClips(const GameData& data){static constexpr unsigned banks[17]={11,14,12,15,17,16,25,27,24,29,26,23,22,20,21,19,18};for(unsigned i=0;i<17;i++)loadSpriteSet(data,banks[i],genericEnemySets[i]);}
    void loadFlameClip(const GameData& data){loadSpriteSet(data,44,flameSet);if(flameSet.clips.size()!=1)throw std::runtime_error("Flame bank revision");}
    void loadSeaweedClip(const GameData& data){loadSpriteSet(data,45,seaweedSet);if(seaweedSet.clips.size()!=1)throw std::runtime_error("Seaweed bank revision");}
    void loadDingodileClip(const GameData& data){loadSpriteSet(data,54,dingodileSet);if(dingodileSet.clips.size()!=10)throw std::runtime_error("Dingodile bank revision");}
    void loadTinyClip(const GameData& data){loadSpriteSet(data,55,tinySet);if(tinySet.clips.size()!=9)throw std::runtime_error("Tiny bank revision");}
    void loadCortexClip(const GameData& data){loadSpriteSet(data,53,cortexSet);if(cortexSet.clips.size()!=19)throw std::runtime_error("Cortex bank revision");}
    void loadCortexGemClip(const GameData& data){loadSpriteSet(data,32,cortexGemSet);if(cortexGemSet.clips.size()<4)throw std::runtime_error("Cortex gem bank revision");}
    void loadMegaMixClip(const GameData& data){loadSpriteSet(data,30,megaMixSet);if(megaMixSet.clips.size()!=3)throw std::runtime_error("Mega-Mix bank revision");}
    void loadPowerClip(const GameData& data){loadSpriteSet(data,38,powerSet);if(powerSet.clips.size()<4)throw std::runtime_error("Power bank revision");}
    void loadGemClip(const GameData& data){
        const auto& bank=data.banks.at(37);if(bank.animations.size()!=1)throw std::runtime_error("Gem bank 37 revision");const auto& a=bank.animations[0];gemClip.duration=std::max(1u,a.duration);gemClip.flags=a.flags;for(unsigned fi:a.sequence)gemClip.frames.push_back(data.frame(37,fi,a.paletteRecord));double l=data.rom.sh(a.address+4),t=data.rom.sh(a.address+6);gemBounds={l,t,l+data.rom.b(a.address+8),t+data.rom.b(a.address+9)};
    }
    void loadTimeTrialClip(const GameData& data){
        const auto& bank=data.banks.at(36);if(bank.animations.size()!=1)throw std::runtime_error("Time-trial bank 36 revision");const auto& a=bank.animations[0];timeTrialClip.duration=std::max(1u,a.duration);timeTrialClip.flags=a.flags;for(unsigned fi:a.sequence)timeTrialClip.frames.push_back(data.frame(36,fi,a.paletteRecord));double l=data.rom.sh(a.address+4),t=data.rom.sh(a.address+6);timeTrialBounds={l,t,l+data.rom.b(a.address+8),t+data.rom.b(a.address+9)};
    }

    static void pixel(std::vector<uint32_t>& c,int w,int h,int x,int y,uint32_t color){if(x>=0&&y>=0&&x<w&&y<h)c[size_t(y*w+x)]=color;}
    static void drawX(std::vector<uint32_t>& c,int w,int h,int x,int y){for(int i=0;i<5;i++){pixel(c,w,h,x+i,y+i,0xfff8f8f0);pixel(c,w,h,x+4-i,y+i,0xfff8f8f0);}}
    static void drawLetterA(std::vector<uint32_t>& c,int w,int h,int x,int y){for(int yy=0;yy<5;yy++)for(int xx=0;xx<4;xx++)if((yy==0&&xx>0&&xx<3)||(xx==0&&yy>0)||(xx==3&&yy>0)||(yy==2))pixel(c,w,h,x+xx,y+yy,0xfff8e76c);}
    static void drawDigit(std::vector<uint32_t>& c,int w,int h,int x,int y,unsigned d){
        static constexpr const char* pat[10]={"111101101101111","010110010010111","111001111100111","111001111001111","101101111001001","111100111001111","111100111101111","111001001001001","111101111101111","111101111001111"};const char* p=pat[d%10];
        for(int yy=0;yy<5;yy++)for(int xx=0;xx<3;xx++)if(p[yy*3+xx]=='1'){pixel(c,w,h,x+xx+1,y+yy+1,0xff101018);pixel(c,w,h,x+xx,y+yy,0xfff8e76c);}
    }
    static void drawNumber(std::vector<uint32_t>& c,int w,int h,int x,int y,unsigned value){std::array<unsigned,3> ds{(value/100)%10,(value/10)%10,value%10};unsigned first=value>=100?0:value>=10?1:2;for(unsigned i=first;i<3;i++)drawDigit(c,w,h,x+int((i-first)*5),y,ds[i]);}
};

}
