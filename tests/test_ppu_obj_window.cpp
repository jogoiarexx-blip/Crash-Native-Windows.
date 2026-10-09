#include "../src/cfg_audit/gba_ppu_renderer.hpp"
#include <array>
#include <cstdint>
#include <iostream>
namespace {
template<size_t N> void wr(std::array<uint8_t,N>& a,size_t n,uint16_t v){a[n]=uint8_t(v);a[n+1]=uint8_t(v>>8);}
bool rgb(const gba_ppu::Frame& f,int x,int y,int r,int g,int b){const size_t at=(size_t(y)*240u+size_t(x))*3u;return f.rgb[at]==r&&f.rgb[at+1]==g&&f.rgb[at+2]==b;}
}
int main(){
    std::array<uint8_t,0x60> io{};
    std::array<uint8_t,1024> pal{};
    std::array<uint8_t,0x18000> vr{};
    std::array<uint8_t,1024> oam{};
    wr(io,0x20,0x0100);wr(io,0x26,0x0100);
    const auto render=[&](){return gba_ppu::render(io.data(),pal.data(),vr.data(),oam.data());};
    wr(pal,0,0x001f); // red backdrop
    wr(vr,0,0x03e0);wr(vr,2,0x03e0);wr(vr,4,0x03e0); // green bitmap3 BG2 pixels 0,1,2
    wr(io,0,0x9443); // OBJWIN, OBJ, BG2, OBJ 1D, mode3
    wr(io,0x4a,0x0400); // only OBJ-window pixels may show BG2
    wr(oam,0,0x0800); // OBJ mode 2: window, not color
    wr(oam,2,0);wr(oam,4,512); // tile 512: bitmap-mode OBJ base
    wr(vr,0x14000,0x0011); // two opaque indices for pixels 0,1
    if(!rgb(render(),0,0,0,255,0)||!rgb(render(),1,0,0,255,0)||!rgb(render(),2,0,255,0,0))return 1;
    // Transparent pixels must not create an OBJ window; OBJWIN disabled by DISPCNT must suppress it.
    wr(vr,0x14000,0x0001);
    if(!rgb(render(),1,0,255,0,0))return 2;
    wr(io,0,0x1443);
    if(!rgb(render(),0,0,0,255,0))return 3; // without windows, BG2 always visible
    // WIN0 wins over OBJWIN at overlapping pixels, even if WIN0 masks BG2 out.
    wr(io,0,0xB443);wr(io,0x40,0x0001);wr(io,0x44,0x0001);wr(io,0x48,0);
    wr(vr,0x14000,0x0011);
    if(!rgb(render(),0,0,255,0,0))return 4;
    wr(io,0x40,0x0102);
    if(!rgb(render(),0,0,0,255,0)||!rgb(render(),1,0,255,0,0))return 5;
    // Mosaic applies to BG samples, not the window geometry: BG mode3 first green and next red.
    io.fill(0);oam.fill(0);vr.fill(0);wr(io,0x20,0x0100);wr(io,0x26,0x0100);
    wr(io,0,0x0403);wr(io,0x0c,0x0040);wr(io,0x4c,0x0011); // 2x2 BG mosaic
    wr(vr,0,0x03e0);wr(vr,2,0x001f);
    if(!rgb(render(),1,0,0,255,0)||!rgb(render(),0,1,0,255,0))return 6;
    wr(io,0x0c,0);
    if(!rgb(render(),1,0,255,0,0))return 7;
    // Sprite OBJ mosaic: source (1,0) and (0,1) inherit upper-left pixel.
    io.fill(0);oam.fill(0);vr.fill(0);wr(io,0x20,0x0100);wr(io,0x26,0x0100);
    wr(io,0,0x1043);wr(io,0x4c,0x1100);wr(pal,514,0x03e0);
    wr(oam,0,0x1000);wr(oam,2,0);wr(oam,4,512);
    vr[0x14000]=1;
    if(!rgb(render(),1,0,0,255,0)||!rgb(render(),0,1,0,255,0))return 8;
    wr(oam,0,0);
    if(!rgb(render(),1,0,255,0,0))return 9;
    // Affine OBJ identity matrix from OAM parameter gaps.
    oam.fill(0);wr(oam,0,0x0100);wr(oam,2,0);wr(oam,4,512);
    wr(oam,6,256);wr(oam,14,0);wr(oam,22,0);wr(oam,30,256);
    if(!rgb(render(),0,0,0,255,0)||!rgb(render(),1,0,255,0,0))return 10;
    // Double-size affine canvas displays source upper-left at (4,4), not at (0,0).
    wr(oam,0,0x0300);
    if(!rgb(render(),4,4,0,255,0)||!rgb(render(),0,0,255,0,0))return 11;
    // Affine OBJ window: nontransparent matrix-sampled pixel enables BG2.
    wr(io,0,0x9443);wr(io,0x4a,0x0400);wr(oam,0,0x0B00); // affine double-size + OBJWIN
    if(!rgb(render(),4,4,0,0,0))return 12; // BG2 bitmap is 0, so black when unmasked
    if(!rgb(render(),0,0,255,0,0))return 13; // backdrop outside OBJ window
    // Disable affine by requesting a shape=3 OBJ: no color, no phantom window.
    wr(oam,0,0xCB00);
    if(!rgb(render(),4,4,255,0,0))return 14;
    // Exhaustive screen X and selected Y coverage, including wrapped coordinates,
    // affine double-size, the disabled bit and invalid shapes. Span pruning must
    // NEVER omit a source pixel that objPixel() could have returned.
    for(unsigned shape=0;shape<4;shape++)for(unsigned size=0;size<4;size++)
      for(unsigned affineState=0;affineState<4;affineState++)
        for(unsigned ox=0;ox<512;ox++){
        const unsigned base0=(shape<<14)|(affineState<<8);
        const unsigned base1=(size<<14)|ox;
        wr(oam,0,base0);wr(oam,2,base1);
        for(unsigned oy:{0u,7u,120u,220u,248u,255u}){
          wr(oam,0,base0|oy);
          for(unsigned row:{0u,1u,7u,31u,80u,127u,159u}){
            unsigned from=99,to=99;
            const bool has=gba_ppu::objVisibleSpan(oam.data(),0,row,from,to);
            const bool aff=(base0&0x100u)!=0;
            const bool enabled=shape!=3u&&(aff||(base0&0x200u)==0);
            const unsigned w=shape==3u?0u:gba_ppu::kObjDims[shape][size][0];
            const unsigned h=shape==3u?0u:gba_ppu::kObjDims[shape][size][1];
            const unsigned width=w*((aff&&(base0&0x200u))?2u:1u);
            const unsigned height=h*((aff&&(base0&0x200u))?2u:1u);
            const bool validY=((row+256u-oy)&255u)<height;
            bool any=false;
            for(unsigned x=0;x<240u;x++){
                const bool expect=enabled&&validY&&(((x+512u-ox)&511u)<width);
                const bool actual=has&&from<=x&&x<to;
                if(expect!=actual){std::cerr<<"OBJ clipped span disagreement shape="<<shape<<" size="<<size<<" x="<<ox<<" y="<<oy<<" row="<<row<<" scan="<<x<<"\n";return 15;}
                any|=expect;
            }
            if(any!=has)return 16;
          }
        }
      }
    std::cout<<"PPU OBJWIN transparency/priority, BG/OBJ mosaic, affine and double-size OBJ PASS\n";
}
