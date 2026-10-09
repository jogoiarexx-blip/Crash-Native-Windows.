#include "entities.hpp"
#include <iostream>
#include <map>

int main(int argc,char** argv){
    try{
        if(argc!=2){std::cerr<<"Usage: CrashEnemyCoverageTool ROM\n";return 2;}
        crash::GameData data(crash::read(argv[1]));
        unsigned tileRooms=0,totalEnemies=0,generic=0,aquatic=0,legacy=0,spawners=0,bosses=0,unsupported=0;
        std::map<unsigned,unsigned> unsupportedTypes;
        for(unsigned level=0;level<25;++level){
            unsigned levelEnemies=0,levelSupported=0;
            for(const auto& rr:data.roomRoute(level)){
                if(!rr.tileRoom())continue;
                ++tileRooms;auto scene=data.sceneFromRoom(rr);auto list=data.entities(scene);
                for(const auto& e:list){
                    const bool old=e.type==crash::EntityWorld::EnemyType||e.type==crash::EntityWorld::PlantType;
                    const bool aq=crash::EntityWorld::isAquaticEnemy(e.type);
                    const bool gen=crash::EntityWorld::isGenericEnemy(e.type);
                    const bool spawn=crash::EntityWorld::isEnemySpawner(e.type);
                    const bool boss=e.type==crash::EntityWorld::DingodileType||e.type==crash::EntityWorld::TinyType||e.type==crash::EntityWorld::CortexType||e.type==crash::EntityWorld::MegaMixType;
                    const bool enemyFamily=(e.type>=0x28&&e.type<=0x45)||(e.type>=0x47&&e.type<=0x49)||e.type==0x4B||e.type==0x4C||e.type==0x4D;
                    if(!enemyFamily)continue;
                    ++totalEnemies;++levelEnemies;
                    if(old){++legacy;++levelSupported;}
                    else if(aq){++aquatic;++levelSupported;}
                    else if(gen){++generic;++levelSupported;}
                    else if(spawn){++spawners;++levelSupported;}
                    else if(boss){++bosses;++levelSupported;}
                    else if(e.type==0x3D){/* launch pad, not an enemy despite its numeric range */--totalEnemies;--levelEnemies;}
                    else {++unsupported;++unsupportedTypes[e.type];}
                }
            }
            if(levelEnemies)std::cout<<"level="<<(level+1)<<" enemies="<<levelEnemies<<" handled="<<levelSupported<<"\n";
        }
        std::cout<<"summary tile_rooms="<<tileRooms<<" handled_enemy_entities="<<(legacy+aquatic+generic+spawners+bosses)
                 <<" legacy="<<legacy<<" aquatic="<<aquatic<<" generic="<<generic<<" spawners="<<spawners<<" bosses="<<bosses
                 <<" unsupported="<<unsupported<<"\n";
        for(auto [type,count]:unsupportedTypes)std::cout<<"unsupported type=0x"<<std::hex<<type<<std::dec<<" count="<<count<<"\n";
        return unsupported?1:0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
