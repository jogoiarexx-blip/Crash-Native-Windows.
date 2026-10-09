#pragma once
#include "entities.hpp"
#include <map>
#include <set>
#include <utility>
#include <fstream>
#include <iomanip>
#include <string>

namespace crash {

class RunState {
public:
    struct RoomState {
        EntityWorld::Snapshot world;
        bool checkpointActive=false;
        double checkpointX=0,checkpointY=0;
    };

    std::map<std::pair<int,int>,RoomState> rooms;
    std::set<int> completedLevels;
    std::set<int> bonusDoneLevels,gemPathDoneLevels;
    std::map<int,unsigned> bestTimeFrames;
    unsigned akuMasks=0,extraLives=0,respawns=0,powerFlags=0,gemFlags=0;
    unsigned lastLevel=0;
    bool timeTrialActive=false;
    int timeTrialLevel=-1;
    unsigned timeTrialFrames=0;

    void clear(){rooms.clear();completedLevels.clear();bonusDoneLevels.clear();gemPathDoneLevels.clear();bestTimeFrames.clear();akuMasks=extraLives=respawns=powerFlags=gemFlags=lastLevel=0;timeTrialActive=false;timeTrialLevel=-1;timeTrialFrames=0;}
    bool bonusDone(int level)const{return bonusDoneLevels.count(level)!=0;}
    bool gemPathDone(int level)const{return gemPathDoneLevels.count(level)!=0;}
    void markBonusDone(int level){bonusDoneLevels.insert(level);}
    void markGemPathDone(int level){gemPathDoneLevels.insert(level);}
    void syncGlobal(const EntityWorld& world,const Player& player){akuMasks=world.akuMasks;extraLives=world.extraLives;respawns=player.respawns;powerFlags|=player.powerFlags;gemFlags|=world.coloredGemFlags;}
    void capture(int level,int slot,const EntityWorld& world,const Player& player){syncGlobal(world,player);RoomState r;r.world=world.snapshot();r.checkpointActive=player.checkpointActive;r.checkpointX=player.checkpointX;r.checkpointY=player.checkpointY;rooms[{level,slot}]=std::move(r);}
    bool restore(int level,int slot,EntityWorld& world,Player& player)const{auto it=rooms.find({level,slot});if(it!=rooms.end()){world.restore(it->second.world);player.checkpointActive=it->second.checkpointActive;player.checkpointX=it->second.checkpointX;player.checkpointY=it->second.checkpointY;if(player.checkpointActive)player.placeAt(player.checkpointX,player.checkpointY);}world.akuMasks=akuMasks;world.extraLives=extraLives;player.respawns=respawns;player.powerFlags=powerFlags;world.setProgress(gemFlags,bonusDone(level),gemPathDone(level),timeTrialActive&&timeTrialLevel==level);world.bind(player);return it!=rooms.end();}

    bool clockAvailable(int level)const{return completedLevels.count(level)!=0 && !(timeTrialActive&&timeTrialLevel==level);}
    void startTimeTrial(int level){timeTrialActive=true;timeTrialLevel=level;timeTrialFrames=0;}
    void restartTimeTrial(int level){if(timeTrialActive&&timeTrialLevel==level)timeTrialFrames=0;}
    void tickTimeTrial(){if(timeTrialActive)++timeTrialFrames;}
    void finishLevel(int level){
        completedLevels.insert(level);
        // The ROM stores the four warp-room bosses after the 20 normal levels:
        // 20 Dingodile, 21 N. Gin, 22 Tiny, 23 Cortex.
        if(level==20)powerFlags|=Player::SuperBodySlamPower;
        else if(level==21)powerFlags|=Player::DoubleJumpPower;
        else if(level==22)powerFlags|=Player::TornadoSpinPower;
        else if(level==23)powerFlags|=Player::TurboRunPower;
        if(timeTrialActive&&timeTrialLevel==level){auto it=bestTimeFrames.find(level);if(it==bestTimeFrames.end()||timeTrialFrames<it->second)bestTimeFrames[level]=timeTrialFrames;timeTrialActive=false;timeTrialLevel=-1;timeTrialFrames=0;}
    }
    unsigned bestTime(int level)const{auto it=bestTimeFrames.find(level);return it==bestTimeFrames.end()?0:it->second;}


    bool worldUnlocked(unsigned world)const{
        if(world==0)return true;
        if(world>3)return completedLevels.count(23)!=0;
        return completedLevels.count(int(19+world))!=0; // previous world's boss
    }
    bool normalWorldCleared(unsigned world)const{
        if(world>3)return false;
        const int base=int(world*5);
        for(int i=0;i<5;i++)if(completedLevels.count(base+i)==0)return false;
        return true;
    }
    bool levelUnlocked(int level)const{
        if(level<0||level>24)return false;
        if(level==24)return completedLevels.count(23)!=0; // secret portal remains a post-Cortex gate for now
        if(level>=20){
            const unsigned world=unsigned(level-20);
            return world<4&&worldUnlocked(world)&&normalWorldCleared(world);
        }
        return worldUnlocked(unsigned(level/5));
    }
    unsigned suggestedLevelAfter(int level)const{
        if(level==24)return 24;
        if(level>=20&&level<=23){
            const unsigned world=unsigned(level-20);
            return world<3?(world+1)*5:24;
        }
        if(level>=0&&level<20){
            const unsigned world=unsigned(level/5),base=world*5;
            for(unsigned i=0;i<5;i++)if(completedLevels.count(int(base+i))==0)return base+i;
            const unsigned boss=20+world;
            if(completedLevels.count(int(boss))==0)return boss;
            return world<3?(world+1)*5:24;
        }
        return 0;
    }
    unsigned highestUnlocked()const{
        unsigned n=0;for(unsigned i=0;i<=24;i++)if(levelUnlocked(int(i)))n=i;return n;
    }

    bool saveFile(const std::string& path)const{
        std::ofstream f(path,std::ios::binary);if(!f)return false;
        f<<std::setprecision(17);
        f<<"CRASH_NATIVE_SAVE 9\n";
        f<<"GLOBAL "<<akuMasks<<' '<<extraLives<<' '<<respawns<<' '<<lastLevel<<' '<<powerFlags<<"\n";
        f<<"TT "<<(timeTrialActive?1:0)<<' '<<timeTrialLevel<<' '<<timeTrialFrames<<"\n";
        f<<"COMPLETED "<<completedLevels.size();for(int v:completedLevels)f<<' '<<v;f<<"\n";
        f<<"SPECIAL "<<gemFlags<<' '<<bonusDoneLevels.size();for(int v:bonusDoneLevels)f<<' '<<v;f<<' '<<gemPathDoneLevels.size();for(int v:gemPathDoneLevels)f<<' '<<v;f<<"\n";
        f<<"BEST "<<bestTimeFrames.size();for(const auto& q:bestTimeFrames)f<<' '<<q.first<<' '<<q.second;f<<"\n";
        f<<"ROOMS "<<rooms.size()<<"\n";
        for(const auto& it:rooms){const auto& key=it.first;const auto& r=it.second;const auto& s=r.world;
            f<<"ROOM "<<key.first<<' '<<key.second<<' '<<(r.checkpointActive?1:0)<<' '<<r.checkpointX<<' '<<r.checkpointY<<' '<<s.wumpaCollected<<' '<<s.crateBroken<<' '<<s.checkpointHits<<' '<<s.hazardHits<<' '<<s.carriedTicks<<' '<<s.ticks<<' '<<s.enemyKills<<' '<<s.plantAttacks<<' '<<s.gemsCollected<<' '<<(s.exitMarkerArmed?1:0)<<' '<<s.items.size()<<"\n";
            for(const auto& e:s.items){const auto& m=e.mover;
                f<<"E "<<e.source.id<<' '<<e.source.type<<' '<<e.source.x<<' '<<e.source.y<<' '<<e.source.param<<' '<<e.source.column
                 <<' '<<(e.active?1:0)<<' '<<e.stateTicks<<' '<<e.countdown<<' '<<(e.triggered?1:0)<<' '<<(e.dying?1:0)<<' '<<(e.facingLeft?1:0)<<' '<<(e.attacking?1:0)<<' '<<(e.attackDamageDone?1:0)
                 <<' '<<e.hits<<' '<<e.triggerCap<<' '<<e.deathTicks<<' '<<e.attackTicks<<' '<<e.x<<' '<<e.y<<' '<<e.prevX<<' '<<e.prevY<<' '<<e.patrolOrigin<<' '<<e.patrolRange<<' '<<e.patrolSpeed
                 <<' '<<(m.enabled?1:0)<<' '<<m.kind<<' '<<m.inputDistX<<' '<<m.inputDistY<<' '<<m.rangeX<<' '<<m.rangeY<<' '<<m.travelledX<<' '<<m.travelledY<<' '<<(m.dirX?1:0)<<' '<<(m.dirY?1:0)
                 <<' '<<m.xQ8<<' '<<m.yQ8<<' '<<m.vxQ8<<' '<<m.vyQ8<<' '<<m.targetVxQ8<<' '<<m.targetVyQ8<<' '<<m.lastX<<' '<<m.lastY<<' '<<(m.active?1:0)<<' '<<(m.falling?1:0)<<' '<<m.specialTimer<<' '<<m.holdUntil
                 <<' '<<e.stackAbove<<' '<<e.stackBelow<<' '<<(e.stackFalling?1:0)<<' '<<e.stackFallTargetY<<' '<<e.stackFallVelocity
                 <<' '<<e.controllerMode<<' '<<e.controllerTicks<<' '<<(e.childSpawned?1:0)
                 <<' '<<e.dingodile.state<<' '<<e.dingodile.hits<<' '<<e.dingodile.step<<' '<<e.dingodile.timer<<' '<<e.dingodile.nextState<<' '<<e.dingodile.passes<<' '<<e.dingodile.animation<<' '<<e.dingodile.animationTicks
                 <<' '<<e.dingodile.shieldState<<' '<<e.dingodile.shieldBlinkTimer<<' '<<e.dingodile.shieldBlinksLeft<<' '<<e.dingodile.vxQ8<<' '<<e.dingodile.vyQ8<<' '<<e.dingodile.targetVxQ8<<' '<<e.dingodile.targetVyQ8<<' '<<e.dingodile.stepXQ8<<' '<<e.dingodile.stepYQ8
                 <<' '<<(e.dingodile.shieldBlink?1:0)<<' '<<(e.dingodile.stalactiteVulnerable?1:0)<<' '<<(e.dingodile.complete?1:0)<<' '<<(e.dingodile.rewardSpawned?1:0)
                 <<' '<<e.tiny.state<<' '<<e.tiny.round<<' '<<e.tiny.nextState<<' '<<e.tiny.target<<' '<<e.tiny.count<<' '<<e.tiny.animation<<' '<<e.tiny.animationTicks
                 <<' '<<e.tiny.timer<<' '<<e.tiny.stomped<<' '<<e.tiny.steps<<' '<<e.tiny.total<<' '<<e.tiny.x<<' '<<e.tiny.y<<' '<<e.tiny.dx<<' '<<e.tiny.dy
                 <<' '<<(e.tiny.complete?1:0)<<' '<<(e.tiny.rewardSpawned?1:0)<<' '<<(e.tiny.rewardCollected?1:0)
                 <<' '<<e.cortex.state<<' '<<e.cortex.counter<<' '<<e.cortex.bossAnimation<<' '<<e.cortex.bossFrame<<' '<<e.cortex.cannonAnimation<<' '<<e.cortex.cannonFrame
                 <<' '<<e.cortex.targetState<<' '<<e.cortex.targetAnimation<<' '<<e.cortex.targetTicks<<' '<<e.cortex.targetFrame<<' '<<e.cortex.targetNextState
                 <<' '<<e.cortex.targetTimer<<' '<<e.cortex.targetBlink<<' '<<e.cortex.targetSteps<<' '<<e.cortex.targetTotal
                 <<' '<<e.cortex.targetX<<' '<<e.cortex.targetY<<' '<<e.cortex.targetDestX<<' '<<e.cortex.targetDestY<<' '<<e.cortex.targetDeltaX<<' '<<e.cortex.targetDeltaY
                 <<' '<<(e.cortex.targetDirLeft?1:0)<<' '<<(e.cortex.targetHigh?1:0)<<' '<<(e.cortex.targetTop?1:0)<<' '<<(e.cortex.targetBlinking?1:0)
                 <<' '<<(e.cortex.childrenSpawned?1:0)<<' '<<(e.cortex.complete?1:0)<<' '<<(e.cortex.rewardSpawned?1:0)<<' '<<(e.cortex.rewardCollected?1:0)
                 <<' '<<e.megaMix.state<<' '<<e.megaMix.animation<<' '<<e.megaMix.animationTicks<<' '<<e.megaMix.motionMode<<' '<<e.megaMix.stamp
                 <<' '<<e.megaMix.vxQ8<<' '<<e.megaMix.targetVxQ8<<' '<<e.megaMix.stepXQ8<<' '<<(e.megaMix.latch?1:0)<<' '<<(e.megaMix.grabDamageDone?1:0)<<"\n";
            }
        }
        f<<"END\n";return bool(f);
    }

    bool loadFile(const std::string& path){
        std::ifstream f(path,std::ios::binary);if(!f)return false;RunState tmp;std::string tag;unsigned version=0;if(!(f>>tag>>version)||tag!="CRASH_NATIVE_SAVE"||(version<1||version>9))return false;
        unsigned b=0;if(!(f>>tag)||tag!="GLOBAL"||!(f>>tmp.akuMasks>>tmp.extraLives>>tmp.respawns>>tmp.lastLevel))return false;
        if(version>=7){if(!(f>>tmp.powerFlags))return false;}
        if(!(f>>tag)||tag!="TT"||!(f>>b>>tmp.timeTrialLevel>>tmp.timeTrialFrames))return false;
        tmp.timeTrialActive=b!=0;
        size_t n=0;if(!(f>>tag>>n)||tag!="COMPLETED")return false;for(size_t i=0;i<n;i++){int v;if(!(f>>v))return false;tmp.completedLevels.insert(v);}
        // v7 and older used the old interleaved frontend numbering for power awards.
        // Rebuild only these four bits from the ROM's real boss IDs when migrating.
        if(version<=7){tmp.powerFlags=0;if(tmp.completedLevels.count(20))tmp.powerFlags|=Player::SuperBodySlamPower;if(tmp.completedLevels.count(21))tmp.powerFlags|=Player::DoubleJumpPower;if(tmp.completedLevels.count(22))tmp.powerFlags|=Player::TornadoSpinPower;if(tmp.completedLevels.count(23))tmp.powerFlags|=Player::TurboRunPower;}
        if(version>=9){size_t bn=0,gn=0;if(!(f>>tag>>tmp.gemFlags>>bn)||tag!="SPECIAL")return false;for(size_t i=0;i<bn;i++){int v;if(!(f>>v))return false;tmp.bonusDoneLevels.insert(v);}if(!(f>>gn))return false;for(size_t i=0;i<gn;i++){int v;if(!(f>>v))return false;tmp.gemPathDoneLevels.insert(v);}}
        if(!(f>>tag>>n)||tag!="BEST")return false;
        for(size_t i=0;i<n;i++){int k;unsigned v;if(!(f>>k>>v))return false;tmp.bestTimeFrames[k]=v;}
        if(!(f>>tag>>n)||tag!="ROOMS")return false;
        for(size_t ri=0;ri<n;ri++){int level,slot;unsigned cp,exitFlag;RoomState r;size_t count=0;if(!(f>>tag)||tag!="ROOM")return false;if(!(f>>level>>slot>>cp>>r.checkpointX>>r.checkpointY>>r.world.wumpaCollected>>r.world.crateBroken>>r.world.checkpointHits>>r.world.hazardHits>>r.world.carriedTicks>>r.world.ticks>>r.world.enemyKills>>r.world.plantAttacks>>r.world.gemsCollected>>exitFlag>>count))return false;r.checkpointActive=cp!=0;r.world.exitMarkerArmed=exitFlag!=0;r.world.items.reserve(count);
            for(size_t ei=0;ei<count;ei++){RuntimeEntity e;MoverState& m=e.mover;unsigned active,triggered,dying,facing,attacking,damage,menabled,mdirx,mdiry,mactive,mfalling;if(!(f>>tag)||tag!="E")return false;if(!(f>>e.source.id>>e.source.type>>e.source.x>>e.source.y>>e.source.param>>e.source.column>>active>>e.stateTicks>>e.countdown>>triggered>>dying>>facing>>attacking>>damage>>e.hits>>e.triggerCap>>e.deathTicks>>e.attackTicks>>e.x>>e.y>>e.prevX>>e.prevY>>e.patrolOrigin>>e.patrolRange>>e.patrolSpeed>>menabled>>m.kind>>m.inputDistX>>m.inputDistY>>m.rangeX>>m.rangeY>>m.travelledX>>m.travelledY>>mdirx>>mdiry>>m.xQ8>>m.yQ8>>m.vxQ8>>m.vyQ8>>m.targetVxQ8>>m.targetVyQ8>>m.lastX>>m.lastY>>mactive>>mfalling>>m.specialTimer>>m.holdUntil))return false;e.active=active!=0;e.triggered=triggered!=0;e.dying=dying!=0;e.facingLeft=facing!=0;e.attacking=attacking!=0;e.attackDamageDone=damage!=0;m.enabled=menabled!=0;m.dirX=mdirx!=0;m.dirY=mdiry!=0;m.active=mactive!=0;m.falling=mfalling!=0;if(version>=2){unsigned stackFalling;if(!(f>>e.stackAbove>>e.stackBelow>>stackFalling>>e.stackFallTargetY>>e.stackFallVelocity))return false;e.stackFalling=stackFalling!=0;}if(version>=3){unsigned childSpawned,shieldBlink,stalactiteVulnerable,complete,rewardSpawned;if(!(f>>e.controllerMode>>e.controllerTicks>>childSpawned>>e.dingodile.state>>e.dingodile.hits>>e.dingodile.step>>e.dingodile.timer>>e.dingodile.nextState>>e.dingodile.passes>>e.dingodile.animation>>e.dingodile.animationTicks>>e.dingodile.shieldState>>e.dingodile.shieldBlinkTimer>>e.dingodile.shieldBlinksLeft>>e.dingodile.vxQ8>>e.dingodile.vyQ8>>e.dingodile.targetVxQ8>>e.dingodile.targetVyQ8>>e.dingodile.stepXQ8>>e.dingodile.stepYQ8>>shieldBlink>>stalactiteVulnerable>>complete>>rewardSpawned))return false;e.childSpawned=childSpawned!=0;e.dingodile.shieldBlink=shieldBlink!=0;e.dingodile.stalactiteVulnerable=stalactiteVulnerable!=0;e.dingodile.complete=complete!=0;e.dingodile.rewardSpawned=rewardSpawned!=0;}if(version>=4){unsigned complete,rewardSpawned,rewardCollected;if(!(f>>e.tiny.state>>e.tiny.round>>e.tiny.nextState>>e.tiny.target>>e.tiny.count>>e.tiny.animation>>e.tiny.animationTicks>>e.tiny.timer>>e.tiny.stomped>>e.tiny.steps>>e.tiny.total>>e.tiny.x>>e.tiny.y>>e.tiny.dx>>e.tiny.dy>>complete>>rewardSpawned>>rewardCollected))return false;e.tiny.complete=complete!=0;e.tiny.rewardSpawned=rewardSpawned!=0;e.tiny.rewardCollected=rewardCollected!=0;}if(version>=5){unsigned dirLeft,high,top,blinking,children,complete,rewardSpawned,rewardCollected;if(!(f>>e.cortex.state>>e.cortex.counter>>e.cortex.bossAnimation>>e.cortex.bossFrame>>e.cortex.cannonAnimation>>e.cortex.cannonFrame>>e.cortex.targetState>>e.cortex.targetAnimation>>e.cortex.targetTicks>>e.cortex.targetFrame>>e.cortex.targetNextState>>e.cortex.targetTimer>>e.cortex.targetBlink>>e.cortex.targetSteps>>e.cortex.targetTotal>>e.cortex.targetX>>e.cortex.targetY>>e.cortex.targetDestX>>e.cortex.targetDestY>>e.cortex.targetDeltaX>>e.cortex.targetDeltaY>>dirLeft>>high>>top>>blinking>>children>>complete>>rewardSpawned>>rewardCollected))return false;e.cortex.targetDirLeft=dirLeft!=0;e.cortex.targetHigh=high!=0;e.cortex.targetTop=top!=0;e.cortex.targetBlinking=blinking!=0;e.cortex.childrenSpawned=children!=0;e.cortex.complete=complete!=0;e.cortex.rewardSpawned=rewardSpawned!=0;e.cortex.rewardCollected=rewardCollected!=0;}if(version>=6){unsigned latch,grabDamageDone;if(!(f>>e.megaMix.state>>e.megaMix.animation>>e.megaMix.animationTicks>>e.megaMix.motionMode>>e.megaMix.stamp>>e.megaMix.vxQ8>>e.megaMix.targetVxQ8>>e.megaMix.stepXQ8>>latch>>grabDamageDone))return false;e.megaMix.latch=latch!=0;e.megaMix.grabDamageDone=grabDamageDone!=0;}r.world.items.push_back(e);}r.world.stackLinksValid=version>=2;
            tmp.rooms[{level,slot}]=std::move(r);
        }
        if(!(f>>tag)||tag!="END")return false;
        *this=std::move(tmp);return true;
    }

    unsigned totalWumpa()const{unsigned n=0;for(const auto& r:rooms)n+=r.second.world.wumpaCollected;return n;}
    unsigned totalCrates()const{unsigned n=0;for(const auto& r:rooms)n+=r.second.world.crateBroken;return n;}
    unsigned totalEnemies()const{unsigned n=0;for(const auto& r:rooms)n+=r.second.world.enemyKills;return n;}
    unsigned totalGems()const{unsigned n=0;for(const auto& r:rooms)n+=r.second.world.gemsCollected;return n;}
};

}
