#pragma once
#include "rom.hpp"
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace crash {

inline std::vector<uint8_t> gbaLz77(const Bytes& rom,size_t off){
    if(off+4>rom.size()||rom[off]!=0x10)throw std::runtime_error("GBA LZ77 header");
    const size_t outSize=size_t(rom[off+1])|(size_t(rom[off+2])<<8)|(size_t(rom[off+3])<<16);
    if(!outSize||outSize>4*1024*1024)throw std::runtime_error("GBA LZ77 size");
    std::vector<uint8_t> out;out.reserve(outSize);size_t src=off+4;
    while(out.size()<outSize){
        if(src>=rom.size())throw std::runtime_error("GBA LZ77 flags");
        uint8_t flags=rom[src++];
        for(int bit=7;bit>=0&&out.size()<outSize;--bit){
            if(flags&(1u<<bit)){
                if(src+2>rom.size())throw std::runtime_error("GBA LZ77 copy");
                uint8_t a=rom[src++],b=rom[src++];size_t len=(a>>4)+3,disp=(size_t(a&15)<<8|b)+1;
                if(disp>out.size())throw std::runtime_error("GBA LZ77 displacement");
                for(size_t i=0;i<len&&out.size()<outSize;i++)out.push_back(out[out.size()-disp]);
            }else{if(src>=rom.size())throw std::runtime_error("GBA LZ77 literal");out.push_back(rom[src++]);}
        }
    }
    return out;
}

inline uint32_t gbaRgb555(uint16_t v){v&=0x7fffu;unsigned r=(v&31u)*255u/31u,g=((v>>5)&31u)*255u/31u,b=((v>>10)&31u)*255u/31u;return 0xff000000u|(r<<16)|(g<<8)|b;}
inline std::vector<uint32_t> gbaPalette(const std::vector<uint8_t>& p,unsigned count){if(p.size()<count*2u)throw std::runtime_error("GBA palette bounds");std::vector<uint32_t> c(count);for(unsigned i=0;i<count;i++)c[i]=gbaRgb555(uint16_t(p[i*2]|(uint16_t(p[i*2+1])<<8)));return c;}

inline std::vector<uint32_t> compose8bpp(const std::vector<uint8_t>& tiles,const std::vector<uint8_t>& map,const std::vector<uint32_t>& colors,unsigned mw,unsigned mh,bool transparentZero){
    if(colors.size()<256||map.size()<size_t(mw)*mh*2u)throw std::runtime_error("GBA 8bpp package bounds");
    std::vector<uint32_t> out(size_t(mw)*8u*mh*8u,0);
    const unsigned stride=mw*8u;
    for(unsigned ty=0;ty<mh;ty++)for(unsigned tx=0;tx<mw;tx++){
        size_t mo=(size_t(ty)*mw+tx)*2u;uint16_t e=uint16_t(map[mo]|(uint16_t(map[mo+1])<<8));unsigned tile=e&0x3ffu;bool hf=(e&0x400u)!=0,vf=(e&0x800u)!=0;if((tile+1u)*64u>tiles.size())continue;
        for(unsigned py=0;py<8;py++)for(unsigned px=0;px<8;px++){unsigned sx=hf?7u-px:px,sy=vf?7u-py:py;uint8_t ci=tiles[tile*64u+sy*8u+sx];uint32_t c=(transparentZero&&ci==0)?0u:colors[ci];out[size_t(ty*8u+py)*stride+tx*8u+px]=c;}
    }
    return out;
}
inline std::vector<uint32_t> compose4bpp(const std::vector<uint8_t>& tiles,const std::vector<uint8_t>& map,const std::vector<uint32_t>& colors,unsigned mw,unsigned mh,bool transparentZero){
    if(colors.size()<16||map.size()<size_t(mw)*mh*2u)throw std::runtime_error("GBA 4bpp package bounds");
    std::vector<uint32_t> out(size_t(mw)*8u*mh*8u,0);const unsigned stride=mw*8u;
    for(unsigned ty=0;ty<mh;ty++)for(unsigned tx=0;tx<mw;tx++){
        size_t mo=(size_t(ty)*mw+tx)*2u;uint16_t e=uint16_t(map[mo]|(uint16_t(map[mo+1])<<8));unsigned tile=e&0x3ffu,pal=(e>>12)&15u;bool hf=(e&0x400u)!=0,vf=(e&0x800u)!=0;if((tile+1u)*32u>tiles.size())continue;
        for(unsigned py=0;py<8;py++)for(unsigned px=0;px<8;px++){unsigned sx=hf?7u-px:px,sy=vf?7u-py:py;uint8_t packed=tiles[tile*32u+sy*4u+sx/2u];unsigned ci=(sx&1u)?(packed>>4):(packed&15u);unsigned pi=pal*16u+ci;if(pi>=colors.size())pi=ci;uint32_t c=(transparentZero&&ci==0)?0u:colors[pi];out[size_t(ty*8u+py)*stride+tx*8u+px]=c;}
    }
    return out;
}

struct RomUiArt {
    std::vector<uint32_t> menuSky;             // 256x160, original gMenuSkyBg
    std::vector<uint32_t> pauseMenu;           // 240x160, original gPauseMenuBg
    std::vector<uint32_t> levelSelectPlatform; // 256x256, original gLevelSelectPageBg
    bool valid()const{return levelSelectValid();}
    bool menuSkyValid()const{return menuSky.size()==256u*160u;}
    bool pauseMenuValid()const{return pauseMenu.size()==240u*160u;}
    bool levelSelectValid()const{return levelSelectPlatform.size()==256u*256u;}
};

/* Crash: The Huge Adventure USA ACQE7D Rev.00. Offsets below are the
 * original graphics packages in the user's ROM revision. Each package is
 * independently validated, so a different revision can still use any package
 * whose layout happens to match and falls back cleanly for the others. */
inline RomUiArt loadRomUiArt(const Bytes& rom){
    RomUiArt art;
    try{
        auto pal=gbaLz77(rom,0x619b54),tiles=gbaLz77(rom,0x61a384),map=gbaLz77(rom,0x62dd00);
        if(pal.size()==32&&tiles.size()==16288&&map.size()==1280){auto colors=gbaPalette(pal,16);colors.resize(256);for(unsigned b=1;b<16;b++)for(unsigned i=0;i<16;i++)colors[b*16+i]=colors[i];art.menuSky=compose4bpp(tiles,map,colors,32,20,false);}
    }catch(...){art.menuSky.clear();}
    try{
        auto pal=gbaLz77(rom,0x619b7c),tiles=gbaLz77(rom,0x61c670),map=gbaLz77(rom,0x62e1d0);
        if(pal.size()==512&&tiles.size()==38400&&map.size()==1200)art.pauseMenu=compose8bpp(tiles,map,gbaPalette(pal,256),30,20,false);
    }catch(...){art.pauseMenu.clear();}
    try{
        auto pal=gbaLz77(rom,0x619dc0),tiles=gbaLz77(rom,0x6235e4),map=gbaLz77(rom,0x62e718);
        if(pal.size()==512&&tiles.size()==20288&&map.size()==2048)art.levelSelectPlatform=compose8bpp(tiles,map,gbaPalette(pal,256),32,32,true);
    }catch(...){art.levelSelectPlatform.clear();}
    return art;
}

inline void blitMenuSky(std::vector<uint32_t>& dst,const RomUiArt& art){if(dst.size()!=240u*160u||!art.menuSkyValid())return;for(int y=0;y<160;y++)for(int x=0;x<240;x++)dst[size_t(y*240+x)]=art.menuSky[size_t(y*256+x)];}
inline void blitPauseMenu(std::vector<uint32_t>& dst,const RomUiArt& art){if(dst.size()!=240u*160u||!art.pauseMenuValid())return;dst=art.pauseMenu;}
inline void blitOriginalLevelSelect(std::vector<uint32_t>& dst,const RomUiArt& art){
    if(dst.size()!=240u*160u||!art.levelSelectValid())return;
    /* Crop x=8..247, y=48..207 from the original 256x256 composition.
     * Palette index 0 is transparent, preserving gMenuSkyBg beneath it. */
    for(int y=0;y<160;y++)for(int x=0;x<240;x++){uint32_t c=art.levelSelectPlatform[size_t((y+48)*256+(x+8))];if(c>>24)dst[size_t(y*240+x)]=c;}
}

}
