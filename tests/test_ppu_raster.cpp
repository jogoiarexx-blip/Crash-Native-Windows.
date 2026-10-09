#include "gba_ppu_renderer.hpp"
#include <array>
#include <algorithm>
#include <cstdint>
#include <iostream>
static void wr(uint8_t* a,unsigned off,uint16_t v){a[off]=uint8_t(v);a[off+1]=uint8_t(v>>8);}
static bool rgb(const gba_ppu::Frame& f,int x,int y,int r,int g,int b){size_t p=(size_t(y)*240u+size_t(x))*3u;return f.rgb[p]==r&&f.rgb[p+1]==g&&f.rgb[p+2]==b;}
int main(){
 std::array<uint8_t,0x60> io{};std::array<uint8_t,1024> pal{};
 std::array<uint8_t,0x18000> vr{};std::array<uint8_t,1024> oam{};
 std::array<uint8_t,160u*0x60u> scan{};
 wr(io.data(),0x20,0x0100);wr(io.data(),0x26,0x0100);
 wr(io.data(),0,0x0403);wr(pal.data(),0,0x001f); // red backdrop
 wr(vr.data(),0,0x03e0);wr(vr.data(),480,0x03e0); // green at (0,0),(0,1)
 for(unsigned y=0;y<160u;++y)std::copy(io.begin(),io.end(),scan.begin()+size_t(y)*0x60u);
 auto render=[&](){return gba_ppu::render(io.data(),pal.data(),vr.data(),oam.data(),scan.data());};
 if(gba_ppu::fingerprint(render())!=gba_ppu::fingerprint(gba_ppu::render(io.data(),pal.data(),vr.data(),oam.data())))return 1;
 wr(scan.data(),0,0x0003); // disable BG2 on first line
 if(!rgb(render(),0,0,255,0,0)||!rgb(render(),0,1,0,255,0))return 2;
 wr(scan.data(),0,0x0483); // forced blank on first line
 if(!rgb(render(),0,0,255,255,255)||!rgb(render(),0,1,0,255,0))return 3;
 wr(scan.data(),0,0x0403);
 wr(scan.data()+0x60u,0x50,0x0084); // BG2 first target, brightness increase
 wr(scan.data()+0x60u,0x54,16u);
 if(!rgb(render(),0,0,0,255,0)||!rgb(render(),0,1,255,255,255))return 4;
 std::array<uint8_t,160u*1024u> rowPal{};
 for(unsigned y=0;y<160u;++y)std::copy(pal.begin(),pal.end(),rowPal.begin()+size_t(y)*1024u);
 auto paletteRender=[&](){return gba_ppu::render(io.data(),pal.data(),vr.data(),oam.data(),scan.data(),rowPal.data());};
 if(gba_ppu::fingerprint(paletteRender())!=gba_ppu::fingerprint(render()))return 5;
 // Per-line backdrop: the VBlank palette differs from the first two HBlank snapshots.
 wr(io.data(),0,0x0003);
 for(unsigned y=0;y<160u;++y)wr(scan.data()+size_t(y)*0x60u,0,0x0003);
 wr(rowPal.data(),0,0x7C00); // blue on line 0
 wr(rowPal.data()+1024u,0,0x03E0); // green on line 1
 if(!rgb(paletteRender(),0,0,0,0,255)||!rgb(paletteRender(),0,1,0,255,0))return 6;
 if(!rgb(render(),0,0,255,0,0)||!rgb(render(),0,1,255,0,0))return 7;
 // Per-line indexed background palette, not only the backdrop.
 wr(io.data(),0,0x0404); // mode 4, BG2
 for(unsigned y=0;y<160u;++y){wr(scan.data()+size_t(y)*0x60u,0,0x0404);wr(scan.data()+size_t(y)*0x60u,0x50,0);}
 vr[0]=1;vr[240]=1;
 wr(pal.data(),2,0x001F); // VBlank pixel would be red
 wr(rowPal.data(),2,0x7C00);wr(rowPal.data()+1024u,2,0x03E0);
 if(!rgb(paletteRender(),0,0,0,0,255)||!rgb(paletteRender(),0,1,0,255,0))return 8;
 // In bitmap modes, the first OBJ tile is mapped after 0x14000.
 wr(io.data(),0,0x1404);
 for(unsigned y=0;y<160u;++y)wr(scan.data()+size_t(y)*0x60u,0,0x1404);
 wr(oam.data(),4,0x0200); // OBJ #0 4bpp tile 512, size 8x8 at (0,0)
 vr[0x14000u]=0x11;vr[0x14004u]=0x11;
 wr(rowPal.data(),0x202u,0x03E0); // green sprite pixel on row 0
 wr(rowPal.data()+1024u,0x202u,0x7C00); // blue sprite pixel on row 1
 if(!rgb(paletteRender(),0,0,0,255,0)||!rgb(paletteRender(),0,1,0,0,255))return 9;

 // Per-line VRAM: mode 3 BG2 reads each scanline's own VRAM image, not VBlank VRAM.
 wr(io.data(),0,0x0403);
 for(unsigned y=0;y<160u;++y)wr(scan.data()+size_t(y)*0x60u,0,0x0403);
 std::vector<uint8_t> rowVr(160u*0x18000u,0);
 // Keep unchanged baseline rows red; snapshots at lines 0 and 1 are blue/green.
 wr(vr.data(),0,0x001F);wr(vr.data(),480u,0x001F);
 wr(rowVr.data(),0,0x7C00);wr(rowVr.data()+0x18000u,480u,0x03E0);
 auto vramRender=[&](){return gba_ppu::render(io.data(),pal.data(),vr.data(),oam.data(),scan.data(),rowPal.data(),rowVr.data());};
 if(!rgb(vramRender(),0,0,0,0,255)||!rgb(vramRender(),0,1,0,255,0))return 10;
 if(!rgb(render(),0,0,255,0,0)||!rgb(render(),0,1,255,0,0))return 11;
 // Per-line OAM: sprite is disabled on line 0 and enabled on line 1.
 // BG falls back to red; enabled sprite is blue and overlaps at x=0,y=1.
 wr(io.data(),0,0x1403);
 for(unsigned y=0;y<160u;++y)wr(scan.data()+size_t(y)*0x60u,0,0x1403);
 std::vector<uint8_t> rowOam(160u*0x400u,0);
 for(unsigned y=0;y<160u;++y){
   std::copy(oam.begin(),oam.end(),rowOam.begin()+size_t(y)*0x400u);
   wr(rowOam.data()+size_t(y)*0x400u,4,0x0200); // first OBJ tile 512
 }
 wr(oam.data(),0,0x0200); // VBlank OBJ disabled; snapshots differ
 wr(rowOam.data(),0,0x0200); // OBJ 0 disabled on line 0
 wr(rowOam.data()+0x400u,0,0x0000); // OBJ enabled on line 1
 wr(rowPal.data()+0x400u,0x202u,0x7C00); // blue OBJ
 wr(rowVr.data()+0x18000u,0x14004u,0x0011); // sprite pixel y=1
 auto oamRender=[&](){return gba_ppu::render(io.data(),pal.data(),vr.data(),oam.data(),scan.data(),rowPal.data(),rowVr.data(),rowOam.data());};
 if(!rgb(oamRender(),0,0,0,0,255)||!rgb(oamRender(),0,1,0,0,255))return 12;
 // Changed OAM must actually affect the frame; no fabricated equivalence.
 if(gba_ppu::fingerprint(oamRender())==gba_ppu::fingerprint(vramRender()))return 13;
 std::cout<<"Per-line registers, palette, VRAM and OAM snapshot PASS\n";
}
