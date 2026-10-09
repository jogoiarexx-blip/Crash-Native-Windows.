#include "../src/cfg_audit/gba_ppu_renderer.hpp"
#include <array>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>
static void wr16(uint8_t* p,unsigned a,uint16_t v){p[a]=uint8_t(v);p[a+1]=uint8_t(v>>8);}
static void wr32(uint8_t* p,unsigned a,uint32_t v){for(int i=0;i<4;i++)p[a+unsigned(i)]=uint8_t(v>>(i*8));}
static bool col(const gba_ppu::Frame& f,unsigned x,unsigned y,int r,int g,int b){size_t i=(size_t(y)*240+x)*3;return f.rgb[i]==r&&f.rgb[i+1]==g&&f.rgb[i+2]==b;}
int main(){
 using gba_ppu::render;
 std::array<uint8_t,0x60> io{};std::array<uint8_t,1024> pal{};
 std::array<uint8_t,0x18000> vr{};std::array<uint8_t,1024> oam{};
 const auto reset=[&](unsigned mode){io.fill(0);pal.fill(0);vr.fill(0);oam.fill(0);wr16(io.data(),0,uint16_t(0x0400u|mode));wr16(io.data(),0x20,256);wr16(io.data(),0x26,256);wr16(pal.data(),0,0x7C1Fu);};
 const auto run=[&](){return render(io.data(),pal.data(),vr.data(),oam.data());};
 #define CHECK(e) do{if(!(e)){std::cerr<<"Bitmap affine failed at line "<<__LINE__<<": "<<#e<<'\n';return 1;}}while(0)
 // Mode 3 (15-bit direct color): identity, translation, negative clipping,
 // scaling by 2, and a 90-degree rotation with a shifted reference origin.
 reset(3);wr16(vr.data(),0,0x03E0);wr16(vr.data(),2,0x7C00);wr16(vr.data(),4,0x001F);
 wr16(vr.data(),480,0x001F);
 CHECK(col(run(),0,0,0,255,0)&&col(run(),1,0,0,0,255));
 wr32(io.data(),0x28,256);CHECK(col(run(),0,0,0,0,255));
 wr32(io.data(),0x28,uint32_t(-256));CHECK(col(run(),0,0,255,0,255));
 wr32(io.data(),0x28,0);wr16(io.data(),0x20,512);wr16(io.data(),0x26,512);
 CHECK(col(run(),1,0,255,0,0));
 wr16(io.data(),0x20,0);wr16(io.data(),0x22,256);wr16(io.data(),0x24,uint16_t(-256));wr16(io.data(),0x26,0);wr32(io.data(),0x2C,256);
 CHECK(col(run(),0,0,255,0,0)&&col(run(),1,0,0,255,0));
 // Mode 4 page selection after affine mapping. Keep the palette stable and
 // select x=1 source on screen x=0, including page 1.
 reset(4);wr16(pal.data(),2,0x7C00);wr16(pal.data(),4,0x03E0);
 vr[1]=1;vr[0xA000+1]=2;wr32(io.data(),0x28,256);
 CHECK(col(run(),0,0,0,0,255));wr16(io.data(),0,0x0414);
 CHECK(col(run(),0,0,0,255,0));
 // Mode 5: physical bitmap is 160x128, regardless of bit 13 wrapping.
 reset(5);wr16(io.data(),0x0C,0x2000);wr16(vr.data(),(127*160+159)*2,0x03E0);
 CHECK(col(run(),159,127,0,255,0));
 CHECK(col(run(),160,127,255,0,255));
 wr32(io.data(),0x28,uint32_t(-256));CHECK(col(run(),0,0,255,0,255));
 // HBlank scanline snapshots can carry a different affine reference X for
 // successive rows. The legacy VBlank register image remains unchanged.
 reset(3);wr16(vr.data(),0,0x03E0);wr16(vr.data(),2,0x7C00);
 wr16(vr.data(),480,0x03E0);wr16(vr.data(),482,0x001F);
 std::vector<uint8_t> rows(160u*0x60u);
 for(unsigned y=0;y<160;y++)std::copy(io.begin(),io.end(),rows.begin()+size_t(y)*0x60u);
 wr32(rows.data()+0x60u,0x28,256);
 auto frame=render(io.data(),pal.data(),vr.data(),oam.data(),rows.data());
 CHECK(col(frame,0,0,0,255,0)&&col(frame,0,1,255,0,0));
 CHECK(col(run(),0,1,0,255,0));
 std::cout<<"Bitmap BG2 affine modes 3/4/5, clipping, frame select, HBlank registers PASS\n";
}
