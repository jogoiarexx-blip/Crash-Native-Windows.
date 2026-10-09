#include "category0.hpp"
#include <algorithm>
#include <iostream>
#include <vector>

int main(int argc,char** argv){try{
    if(argc!=3){std::cerr<<"Usage: CrashJetpackTool ROM OUTPUT_BMP\n";return 2;}
    crash::GameData data(crash::read(argv[1]));
    std::vector<uint32_t> strip(1440*160,0xff000000);
    auto put=[&](const crash::Category0Stage& s,int panel){auto shot=s.renderLogical(true);for(int y=0;y<160;y++)std::copy_n(shot.begin()+y*240,240,strip.begin()+y*1440+panel*240);};

    crash::Category0Stage fire(data,3);fire.collisions=false;fire.scroll=320.0;for(int t=0;t<40;t++)fire.step(false,false,true);put(fire,0);

    crash::Category0Stage death(data,3);death.collisions=false;death.scroll=320.0;death.hp=1;death.jetVx=4.0;death.damageJetpack(1);for(int t=0;t<28;t++)death.step(false,false,false);put(death,1);

    crash::Category0Stage weapons(data,6);weapons.collisions=false;weapons.scroll=544.0;weapons.updateJetpackSpawns();weapons.hovercraftState=2;weapons.hovercraftZ=weapons.scroll+60.0;weapons.hoverParts[2].active=true;weapons.hoverParts[2].cooldown=0;weapons.hoverParts[3].active=true;weapons.hoverParts[3].cooldown=999;weapons.hoverParts[0].active=true;weapons.hoverParts[0].cooldown=0;auto cp=weapons.hoverPartWorld(0);weapons.x=cp[0];weapons.y=cp[1];weapons.updateHoverParts();for(int t=0;t<14;t++){++weapons.ticks;weapons.updateHoverFireballs();weapons.stepEnemyShots();}weapons.destroyHoverPart(0);weapons.startHovercraftHitFlash();for(int t=0;t<2;t++)weapons.updateHoverPartDeaths();put(weapons,2);

    crash::Category0Stage finish(data,6);finish.collisions=false;finish.scroll=544.0;finish.updateJetpackSpawns();finish.hovercraftState=5;finish.hovercraftPartsLeft=0;finish.hovercraftY=76.0;finish.hovercraftZ=finish.scroll+40.0;finish.updateHovercraft();for(int t=0;t<24;t++)finish.step(false,false,false);put(finish,3);

    crash::Category0Stage ring(data,4);ring.collisions=false;unsigned ri=0;while(ri<ring.rom.eventCount&&(ring.kind(ri)!=31||ring.eventWorld(ri)[1]<0))++ri;if(ri<ring.rom.eventCount){ring.scroll=double(ring.rom.eventDepth(ri))-28.0;ring.updateJetpackSpawns();ring.ticks=0;ring.passJetpackRing(ri);for(int t=0;t<16;t++)ring.dispenseJetpackWumpa();ring.updateJetpackCollectedWumpas();}put(ring,4);

    crash::Category0Stage balloon(data,3);balloon.collisions=false;unsigned bi=0;while(bi<balloon.rom.eventCount&&balloon.kind(bi)!=20)++bi;if(bi<balloon.rom.eventCount){std::fill(balloon.consumed.begin(),balloon.consumed.end(),uint8_t(1));balloon.consumed[bi]=0;balloon.eventSpawnTick[bi]=0;balloon.ticks=1;balloon.scroll=double(balloon.rom.eventDepth(bi))-70.0;balloon.eventChildHp[bi]=0;balloon.popJetpackCrateBalloon(bi,20);balloon.updateJetpackActors();}put(balloon,5);

    crash::bmp(argv[2],strip,1440,160);
    std::cout<<"held_fire_40_frames="<<fire.shotsFired
             <<" death_state="<<death.jetPlayerState<<" death_y="<<death.y
             <<" hover_fireballs="<<weapons.hovercraftFireballsFired
             <<" hover_cannonballs="<<weapons.hovercraftCannonballsFired
             <<" finish_state="<<finish.jetPlayerState<<" finish_depth="<<finish.jetPlayerDepth
             <<" ring_chain="<<ring.jetpackRingChain<<" queued_wumpa="<<ring.jetpackQueuedWumpa<<" flying_wumpa="<<ring.jetpackCollectedWumpas.size()
             <<" balloon_state="<<(bi<balloon.rom.eventCount?balloon.eventState[bi]:99u)<<" popped_balloon="<<balloon.poppedBalloons.size()
             <<"\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
