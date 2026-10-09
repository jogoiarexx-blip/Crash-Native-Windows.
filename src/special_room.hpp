#pragma once
#include "entities.hpp"
#include <cmath>
#include <limits>
#include <optional>
namespace crash {
enum class SpecialPadKind { Bonus, GemPath };
struct SpecialPadPlacement {
    double standX=0.0;
    double standY=0.0;
    double topY=0.0;
    unsigned entityId=0;
};
inline bool specialPadType(const RuntimeEntity& e,SpecialPadKind kind){
    return e.active&&(kind==SpecialPadKind::Bonus
        ? e.source.type==EntityWorld::LevelPlatformType
        : EntityWorld::isGemPlatform(e.source.type));
}
// Prefer the exact pad from which the player entered; when loading older saves,
// fall back to the physically nearest matching pad, not the first room entity.
inline std::optional<SpecialPadPlacement> specialPadPlacement(
    const EntityWorld& world,const Player& player,SpecialPadKind kind,
    unsigned preferredId=std::numeric_limits<unsigned>::max()){
    if(player.underwater()||player.hover())return std::nullopt;
    const auto idle=player.animBounds(18);
    if(!idle.valid())return std::nullopt;
    const RuntimeEntity* best=nullptr;
    double nearest=std::numeric_limits<double>::infinity();
    for(const auto& e:world.items){
        if(!specialPadType(e,kind))continue;
        const auto box=world.entityBox(e);
        const double cx=(box[0]+box[2])*.5;
        const double score=std::abs(cx-player.x)+.25*std::abs(box[1]-player.feet());
        if(e.source.id==preferredId){best=&e;break;}
        if(score<nearest){best=&e;nearest=score;}
    }
    if(!best)return std::nullopt;
    const auto box=world.entityBox(*best);
    return SpecialPadPlacement{(box[0]+box[2])*.5,
        box[1]-idle.y-idle.h,box[1],best->source.id};
}
// This is a transient transition/return placement, NOT a respawn checkpoint.
// Preserve the player's actual checkpoint and inventory. Idle collision bounds
// are ROM-derived, so their feet meet the pad's ROM-derived top exactly.
inline void snapPlayerToSpecialPad(Player& player,const SpecialPadPlacement& spot){
    player.lastJump=player.lastSpin=player.lastShoulderR=false;
    player.crouching=false;player.bodySlamming=false;player.doubleJumpUsed=false;player.highJumping=false;
    player.slideTicks=player.spinTicks=player.strokeTicks=player.tornadoCharge=player.slamImpactTicks=player.spinCooldownTicks=0;
    player.lastDownImpact=player.lastUpImpact=0;
    player.vx=0;player.vy=0;player.x=spot.standX;player.y=spot.standY;player.grounded=true;
    player.animation=18;player.animationTicks=0;
}
// After a failed special room, re-entry remains inhibited while standing on
// the pad. Moving/jumping away releases the latch for another intentional try.
inline bool playerStillOnSpecialPad(const EntityWorld& world,const Player& player,
                                   SpecialPadKind kind,unsigned preferredId){
    if(!player.grounded)return false;
    const auto body=player.worldBox(player.terrainBounds());
    for(const auto& e:world.items){
        if(!specialPadType(e,kind)||e.source.id!=preferredId)continue;
        const auto box=world.entityBox(e);
        return body[2]>box[0]+.5&&body[0]<box[2]-.5&&std::abs(body[3]-box[1])<=2.0;
    }
    return false;
}
}
