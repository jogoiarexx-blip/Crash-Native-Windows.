#include "game_data.hpp"
#include <iomanip>
#include <iostream>

int main(int argc,char** argv){
    try{
        if(argc!=2){std::cerr<<"Usage: CrashRouteTool ROM\n";return 2;}
        crash::GameData data(crash::read(argv[1]));
        unsigned tileRooms=0,nativeRooms=0,actorStages=0,multiRoomLevels=0,specialRooms=0,nativeSpecial=0;
        for(unsigned level=0;level<25;++level){
            auto route=data.roomRoute(level);if(route.size()>1)++multiRoomLevels;
            std::cout<<"level="<<(level+1)<<" route="<<route.size();
            for(const auto& r:route){
                std::cout<<" | slot="<<(r.slot+1)<<" kind="<<r.kind;
                if(r.tileRoom()){++tileRooms;bool native=data.nativePlayable(r);if(native)++nativeRooms;std::cout<<" tile desc=0x"<<std::hex<<std::uppercase<<r.descriptor<<std::dec<<" native="<<(native?1:0);}
                else{++actorStages;std::cout<<" actor_category="<<r.category;}
            }
            for(auto kind:{crash::ExtraRoomKind::Bonus,crash::ExtraRoomKind::GemPath}){auto extra=data.extraRoom(level,kind);if(!extra)continue;++specialRooms;bool native=data.nativePlayable(*extra);if(native)++nativeSpecial;std::cout<<" | "<<(kind==crash::ExtraRoomKind::Bonus?"bonus":"gem_path")<<" kind="<<extra->kind<<" desc=0x"<<std::hex<<std::uppercase<<extra->descriptor<<std::dec<<" native="<<(native?1:0);}
            std::cout<<"\n";
        }
        std::cout<<"summary tile_rooms="<<tileRooms<<" native_player_rooms="<<nativeRooms<<" actor_category_stages="<<actorStages<<" multi_room_levels="<<multiRoomLevels<<" special_rooms="<<specialRooms<<" native_special_rooms="<<nativeSpecial<<"\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
