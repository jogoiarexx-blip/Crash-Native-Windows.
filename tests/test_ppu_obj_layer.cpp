#include "../src/cfg_audit/gba_ppu_renderer.hpp"
#include <array>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>
static void wr(uint8_t* p,size_t at,uint16_t x){p[at]=uint8_t(x);p[at+1]=uint8_t(x>>8);}
static bool pixelIs(const gba_ppu::Frame& f,unsigned x,unsigned y,unsigned r,unsigned g,unsigned b){
    size_t k=(size_t(y)*240u+x)*3u;
    return f.rgb[k]==r&&f.rgb[k+1]==g&&f.rgb[k+2]==b;
}
int main(){
    std::array<uint8_t,0x60> io{};
    std::array<uint8_t,0x400> palette{},oam{};
    std::array<uint8_t,0x18000> vram{};
    wr(io.data(),0,0x1043u); // Bitmap mode 3, OBJ, 1-D sprites, BG disabled
    wr(palette.data(),0,0x03E0u); // green backdrop
    wr(palette.data(),0x202,0x001Fu); // red sprite index 1
    wr(palette.data(),0x204,0x7C00u); // blue sprite index 2
    wr(io.data(),0x52,0x0808u); // 50% foreground + 50% second target
    wr(oam.data(),0,0x0400u);wr(oam.data(),2,0);wr(oam.data(),4,512u); // OAM 0: semi red
    wr(oam.data(),8,0);wr(oam.data(),10,0);wr(oam.data(),12,513u);  // OAM 1: blue, directly behind
    wr(vram.data(),0x14000,0x1111u);
    wr(vram.data(),0x14020,0x2222u);
    wr(vram.data(),0x14004,0x1111u);wr(vram.data(),0x14024,0x2222u);
    auto render=[&](){return gba_ppu::render(io.data(),palette.data(),vram.data(),oam.data());};
    // The lower OBJ cannot serve as the second blending target (GBATEK).
    wr(io.data(),0x50,0x1000u); // second target OBJ only
    if(!pixelIs(render(),0,0,255,0,0))return 1;
    wr(io.data(),0x50,0x2000u); // second target backdrop only
    if(!pixelIs(render(),0,0,123,123,0))return 2;
    // Background outranks the OBJ. Only the winner of the OBJ layer can
    // contribute as the second target behind the foreground BG.
    wr(io.data(),0,0x1443u); // enable BG2
    wr(io.data(),0x20,0x0100u);wr(io.data(),0x26,0x0100u);
    wr(vram.data(),0,0x03E0u); // green BG2 pixel
    wr(oam.data(),4,512u|(2u<<10)); // first OBJ behind BG2
    wr(oam.data(),12,513u|(3u<<10));
    wr(io.data(),0x50,0x1044u); // BG2 first + OBJ second + alpha
    if(!pixelIs(render(),0,0,123,123,0))return 3;
    // Per-line OAM snapshots can switch the winning OBJ without stale pixels.
    wr(io.data(),0,0x1043u); // back to no BG2
    wr(io.data(),0x50,0); // plain colors
    std::vector<uint8_t> lines(160u*0x400u,0);
    for(unsigned y=0;y<160u;++y)std::copy(oam.begin(),oam.end(),lines.data()+size_t(y)*0x400u);
    wr(lines.data(),4,512u); // top red, first scanline
    wr(lines.data(),12,513u); // lower blue
    wr(lines.data()+0x400u,0,0x0200u); // top red disabled on line 1
    wr(lines.data()+0x400u,12,513u); // blue visible (still priority 3)
    auto raster=gba_ppu::render(io.data(),palette.data(),vram.data(),oam.data(),nullptr,nullptr,nullptr,lines.data());
    if(!pixelIs(raster,0,0,255,0,0)||!pixelIs(raster,0,1,0,0,255))return 4;
    std::cout<<"Only highest visible OBJ participates in blending and raster pass\n";
}
