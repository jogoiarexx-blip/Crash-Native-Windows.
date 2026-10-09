#include "../src/cfg_audit/gba_ppu_renderer.hpp"
#include <array>
#include <cstdint>
#include <iostream>
namespace {
template<size_t N> void w16(std::array<uint8_t,N>& v,size_t at,uint16_t x){v[at]=uint8_t(x);v[at+1]=uint8_t(x>>8);}
bool eq(const gba_ppu::Frame& f,int x,int y,int r,int g,int b){size_t k=(size_t(y)*240+x)*3;return f.rgb[k]==r&&f.rgb[k+1]==g&&f.rgb[k+2]==b;}
}
int main(){
 std::array<uint8_t,0x60> io{};std::array<uint8_t,1024> pal{};std::array<uint8_t,0x18000> vr{};std::array<uint8_t,1024> oam{};
 w16(io,0x20,0x0100);w16(io,0x26,0x0100); // BG2 identity
 w16(pal,0,0x001f); // backdrop red
 w16(io,0,0x0403); // mode 3 BG2
 w16(vr,0,0x03e0);w16(vr,2,0x03e0); // green screen at first two pixels
 const auto capture=[&](){return gba_ppu::render(io.data(),pal.data(),vr.data(),oam.data());};
 if(!eq(capture(),0,0,0,255,0))return 1;
 w16(io,0x50,0x2044); // BG2 target1, backdrop target2, alpha mode
 w16(io,0x52,0x0808); // 8/16 + 8/16
 if(!eq(capture(),0,0,123,123,0))return 2;
 w16(io,0x50,0x0084);w16(io,0x54,16); // BG2 target1, brighten by 16
 if(!eq(capture(),0,0,255,255,255))return 3;
 w16(io,0x50,0x00c4); // darken
 if(!eq(capture(),0,0,0,0,0))return 4;
 w16(io,0x50,0);
 w16(io,0,0x2403); // WIN0 + mode 3 BG2
 w16(io,0x40,0x0001);w16(io,0x44,0x0001); // WIN0 only pixel (0,0)
 w16(io,0x48,0x0004);w16(io,0x4a,0x0000);
 if(!eq(capture(),0,0,0,255,0)||!eq(capture(),1,0,255,0,0))return 5;
 w16(io,0,0x6403); // WIN0 and WIN1; WIN0 takes precedence
 w16(io,0x42,0x0002);w16(io,0x46,0x0001);
 w16(io,0x48,0x0400); // BG2 allowed only in WIN1
 if(!eq(capture(),0,0,255,0,0)||!eq(capture(),1,0,0,255,0))return 6;
 w16(io,0,0x2403);w16(io,0x48,0x0004);w16(io,0x4a,0x0024);
 w16(io,0x50,0x0084);w16(io,0x54,16); // effect allowed outside, masked in WIN0
 if(!eq(capture(),0,0,0,255,0)||!eq(capture(),1,0,255,255,255))return 7;
 // Semitransparent OBJ in GBA bitmap mode: OBJ VRAM begins at tile 512.
 io.fill(0);oam.fill(0);w16(io,0x20,0x0100);w16(io,0x26,0x0100);w16(io,0,0x1443); // mode 3, BG2, OBJ, OBJ 1D
 w16(pal,514,0x001f);w16(vr,0x14000,0x0001); // OBJ palette index 1, red
 w16(oam,0,0x0400);w16(oam,2,0);w16(oam,4,512); // semi-transparent OBJ tile 512
 w16(io,0x50,0x0400);w16(io,0x52,0x0808); // BG2 is second target
 if(!eq(capture(),0,0,123,123,0))return 8;
 w16(vr,0x14000,0x0000); // Transparent OBJ restores BG
 if(!eq(capture(),0,0,0,255,0))return 9;
 std::cout<<"GBA PPU windows, BG/OBJ priority, alpha, brightness and semitransparent OBJ PASS\n";
}
