#pragma once
// Display memory address decoding and CPU byte-store policy, grounded in GBATEK.
// This models display RAM, not ARM timing, open-bus reads or bus contention.
#include <cstdint>
namespace gba_video_bus {
enum class Region : uint8_t { Other, Palette, Vram, Oam };
enum class BytePolicy : uint8_t { Ordinary, DuplicateHalfword, Ignore };
struct Location { Region region; uint32_t offset; };
inline Location locate(uint32_t address) {
    const unsigned group=address>>24;
    if(group==0x05u)return {Region::Palette,address&0x3FFu};
    if(group==0x06u){
        const uint32_t off=address&0x1FFFFu;
        // Each 128 KiB bank holds 64 KiB BG and two mirrors of 32 KiB OBJ.
        return {Region::Vram,off>=0x18000u?off-0x8000u:off};
    }
    if(group==0x07u)return {Region::Oam,address&0x3FFu};
    return {Region::Other,0u};
}
inline uint32_t canonical(uint32_t address) {
    const Location p=locate(address);
    switch(p.region){
    case Region::Palette:return 0x05000000u+p.offset;
    case Region::Vram:return 0x06000000u+p.offset;
    case Region::Oam:return 0x07000000u+p.offset;
    default:return address;
    }
}
inline BytePolicy bytePolicy(uint32_t address,unsigned displayMode){
    const Location p=locate(address);
    if(p.region==Region::Oam)return BytePolicy::Ignore;
    if(p.region==Region::Palette)return BytePolicy::DuplicateHalfword;
    if(p.region==Region::Vram){
        // In bitmap modes OBJ VRAM starts at 0x14000; tile modes at 0x10000.
        if(p.offset >= (displayMode>=3u&&displayMode<=5u ? 0x14000u : 0x10000u))return BytePolicy::Ignore;
        return BytePolicy::DuplicateHalfword;
    }
    return BytePolicy::Ordinary;
}
} // namespace gba_video_bus
