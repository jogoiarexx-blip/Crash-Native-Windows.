#pragma once
// Port GBA Builder experimental PPU compositor. Frame state is sampled at a
// VBlank boundary, not a cycle-accurate rasterized GBA framebuffer.
// Implemented: modes 0-5, text/affine BG, affine/normal OBJ, mosaic, BG priority,
// OBJ windows and WIN0/WIN1 masks, alpha/brighten/darken. Optional HBlank-sampled
// per-line LCD registers, palettes, VRAM and OAM at HBlank (approximation).
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace gba_ppu {
struct Frame {
    static constexpr int width=240, height=160;
    std::vector<uint8_t> rgb=std::vector<uint8_t>(width*height*3u);
    uint16_t dispcnt=0;
    unsigned mode=0;
    unsigned unsupported_effects=0;
};
inline uint16_t le16(const uint8_t* p,size_t offset){return uint16_t(p[offset])|uint16_t(p[offset+1])<<8;}
inline int32_t le28(const uint8_t* p,size_t offset){uint32_t u=uint32_t(p[offset])|(uint32_t(p[offset+1])<<8)|(uint32_t(p[offset+2])<<16)|(uint32_t(p[offset+3])<<24);return int32_t(u<<4)>>4;}
inline uint32_t rgb555(uint16_t c){
    const auto expand=[](uint32_t a){return (a<<3)|(a>>2);};
    return (expand(c&31u)<<16)|(expand((c>>5)&31u)<<8)|expand((c>>10)&31u);
}
inline void pixel(Frame& f,unsigned x,unsigned y,uint32_t rgb){
    const auto p=(size_t(y)*Frame::width+x)*3u;
    f.rgb[p]=uint8_t(rgb>>16);f.rgb[p+1]=uint8_t(rgb>>8);f.rgb[p+2]=uint8_t(rgb);
}
inline uint16_t bgPal(const uint8_t* p,uint32_t i){return le16(p,(i&255u)*2u);}
inline uint16_t objPal(const uint8_t* p,uint32_t i){return le16(p,512u+(i&255u)*2u);}
inline bool textPixel(const uint8_t* io,const uint8_t* vr,const uint8_t* pal,int bg,int x,int y,uint32_t& color){
    uint16_t cnt=le16(io,8u+2u*unsigned(bg));
    const unsigned width=(cnt&0x4000u)?512u:256u;
    const unsigned height=(cnt&0x8000u)?512u:256u;
    const uint16_t xscroll=le16(io,0x10u+4u*unsigned(bg));
    const uint16_t yscroll=le16(io,0x12u+4u*unsigned(bg));
    unsigned tx=(unsigned(x)+xscroll)&(width-1u),ty=(unsigned(y)+yscroll)&(height-1u);
    const unsigned mapBase=((cnt>>8)&31u)*0x800u;
    const unsigned mapBlock=(tx/256u)+(ty/256u)*(width/256u);
    const unsigned mapOff=mapBase+mapBlock*0x800u+((ty/8u)%32u)*64u+((tx/8u)%32u)*2u;
    if(mapOff+1u>=0x10000u)return false;
    const uint16_t entry=le16(vr,mapOff);
    const unsigned t=entry&1023u;
    unsigned px=tx&7u,py=ty&7u;
    if(entry&0x400u)px=7u-px;
    if(entry&0x800u)py=7u-py;
    const bool depth8=(cnt&0x80u)!=0;
    const unsigned chr=((cnt>>2)&3u)*0x4000u;
    const unsigned off=chr+t*(depth8?64u:32u)+py*(depth8?8u:4u)+(depth8?px:px/2u);
    if(off>=0x10000u)return false;
    const unsigned idx=depth8?vr[off]:((vr[off]>>(4u*(px&1u)))&15u);
    if(idx==0u)return false;
    const unsigned paletteIndex=depth8?idx:((entry>>12)*16u+idx);
    color=rgb555(bgPal(pal,paletteIndex));return true;
}
// Affine reference points are signed 28-bit 8.8 fixed-point values. The
// register pair and PA/PB/PC/PD matrix apply to text-affine and bitmap BG2.
// Using one coordinate mapper prevents bitmap modes 3-5 from bypassing
// rotation/scale and reading an incorrect straight framebuffer row.
inline void affineSampleXY(const uint8_t* io,int bg,int x,int y,int32_t& sx,int32_t& sy){
    const unsigned reg=(bg==2)?0x20u:0x30u;
    const int pa=int16_t(le16(io,reg)),pb=int16_t(le16(io,reg+2));
    const int pc=int16_t(le16(io,reg+4)),pd=int16_t(le16(io,reg+6));
    const int32_t ox=le28(io,reg+8),oy=le28(io,reg+12);
    sx=int32_t((int64_t(ox)+int64_t(pa)*x+int64_t(pb)*y)>>8);
    sy=int32_t((int64_t(oy)+int64_t(pc)*x+int64_t(pd)*y)>>8);
}
inline bool affinePixel(const uint8_t* io,const uint8_t* vr,const uint8_t* pal,int bg,int x,int y,uint32_t& color){
    const unsigned cnt=le16(io,8u+2u*unsigned(bg));
    const unsigned size=128u<<((cnt>>14)&3u);
    int32_t sx=0,sy=0;
    affineSampleXY(io,bg,x,y,sx,sy);
    if(cnt&0x2000u){sx&=int32_t(size-1);sy&=int32_t(size-1);}
    else if(sx<0||sy<0||uint32_t(sx)>=size||uint32_t(sy)>=size)return false;
    const unsigned mapOff=((cnt>>8)&31u)*0x800u+unsigned(sy/8)*size/8u+unsigned(sx/8);
    if(mapOff>=0x10000u)return false;
    const unsigned tile=vr[mapOff];
    const unsigned texOff=((cnt>>2)&3u)*0x4000u+tile*64u+unsigned(sy&7)*8u+unsigned(sx&7);
    if(texOff>=0x10000u)return false;
    const unsigned idx=vr[texOff];
    if(idx==0)return false;
    color=rgb555(bgPal(pal,idx));return true;
}
inline bool bitmapPixel(const uint8_t* io,const uint8_t* vr,const uint8_t* pal,unsigned mode,int x,int y,uint32_t& color){
    // Unlike tiled affine BGs, bitmap modes do not wrap outside their image.
    const int width=(mode==5u)?160:240,height=(mode==5u)?128:160;
    if(x<0||y<0||x>=width||y>=height)return false;
    if(mode==3u){const unsigned off=unsigned(y*240+x)*2u;if(off+1>=0x18000u)return false;color=rgb555(le16(vr,off));return true;}
    const unsigned frame=(io[0]&0x10u)?0xA000u:0u;
    if(mode==4u){const unsigned off=frame+unsigned(y*240+x);if(off>=0x18000u)return false;color=rgb555(bgPal(pal,vr[off]));return true;}
    if(mode==5u){if(x>=160||y>=128)return false;const unsigned off=frame+unsigned(y*160+x)*2u;if(off+1>=0x18000u)return false;color=rgb555(le16(vr,off));return true;}
    return false;
}
inline uint16_t to555(uint32_t rgb){
    return uint16_t(((rgb>>19)&31u)|(((rgb>>11)&31u)<<5)|(((rgb>>3)&31u)<<10));
}
inline uint16_t alpha555(uint16_t first,uint16_t second,unsigned eva,unsigned evb){
    const unsigned r=std::min(31u,((first&31u)*eva+(second&31u)*evb)>>4);
    const unsigned g=std::min(31u,((((first>>5)&31u)*eva)+(((second>>5)&31u)*evb))>>4);
    const unsigned b=std::min(31u,((((first>>10)&31u)*eva)+(((second>>10)&31u)*evb))>>4);
    return uint16_t(r|(g<<5)|(b<<10));
}
inline uint16_t brightness555(uint16_t input,unsigned evy,bool brighten){
    unsigned out=0;
    for(unsigned shift: {0u,5u,10u}){
        const unsigned c=(input>>shift)&31u;
        const unsigned adjusted=brighten ? c+((31u-c)*evy>>4) : c-(c*evy>>4);
        out|=std::min(31u,adjusted)<<shift;
    }
    return uint16_t(out);
}
inline bool windowContains(int x,int y,uint16_t wh,uint16_t wv){
    const unsigned x1=wh>>8,x2=wh&255u,y1=wv>>8,y2=wv&255u;
    const bool inX=x1<=x2?(unsigned(x)>=x1&&unsigned(x)<x2):(unsigned(x)>=x1||unsigned(x)<x2);
    const bool inY=y1<=y2?(unsigned(y)>=y1&&unsigned(y)<y2):(unsigned(y)>=y1||unsigned(y)<y2);
    return inX&&inY;
}
struct Layer {uint16_t color=0;uint16_t rank=4096;uint8_t kind=5;bool semi=false;};
static constexpr unsigned kObjDims[3][4][2]={
    {{8,8},{16,16},{32,32},{64,64}},
    {{16,8},{32,8},{32,16},{64,32}},
    {{8,16},{8,32},{16,32},{32,64}}
};
// One sampling path is shared by visible OBJ and OBJ-window.  This prevents
// transparent pixels from accidentally becoming opaque window-mask pixels.
inline bool objPixel(const uint8_t* io,const uint8_t* vr,const uint8_t* oam,
                     unsigned obj,int x,int y,unsigned mode,unsigned& paletteIndex){
    const size_t at=size_t(obj)*8u;
    const unsigned a0=le16(oam,at),a1=le16(oam,at+2u),a2=le16(oam,at+4u);
    const bool affine=(a0&0x100u)!=0;
    if(!affine&&(a0&0x200u))return false; // OBJ disabled (not double-size)
    const unsigned shape=(a0>>14)&3u,size=(a1>>14)&3u;
    if(shape==3u)return false;
    const unsigned w=kObjDims[shape][size][0],h=kObjDims[shape][size][1];
    const unsigned dw=affine&&(a0&0x200u)?w*2u:w;
    const unsigned dh=affine&&(a0&0x200u)?h*2u:h;
    unsigned dx=(unsigned(x)+512u-(a1&511u))&511u;
    unsigned dy=(unsigned(y)+256u-(a0&255u))&255u;
    if(dx>=dw||dy>=dh)return false;
    if(a0&0x1000u){ // OBJ mosaic: sample the upper-left dot of each OBJ-relative block
        const uint16_t mosaic=le16(io,0x4Cu);
        const unsigned mh=((mosaic>>8)&15u)+1u,mv=((mosaic>>12)&15u)+1u;
        dx-=dx%mh;dy-=dy%mv;
    }
    unsigned sx=dx,sy=dy;
    if(affine){
        const unsigned base=((a1>>9)&31u)*32u;
        const int pa=int16_t(le16(oam,base+6u));
        const int pb=int16_t(le16(oam,base+14u));
        const int pc=int16_t(le16(oam,base+22u));
        const int pd=int16_t(le16(oam,base+30u));
        const int cx=int(dx)-int(dw)/2,cy=int(dy)-int(dh)/2;
        const int tx=int((int64_t(pa)*cx+int64_t(pb)*cy)>>8)+int(w)/2;
        const int ty=int((int64_t(pc)*cx+int64_t(pd)*cy)>>8)+int(h)/2;
        if(tx<0||ty<0||tx>=int(w)||ty>=int(h))return false;
        sx=unsigned(tx);sy=unsigned(ty);
    }else{
        if(a1&0x1000u)sx=w-1u-sx;
        if(a1&0x2000u)sy=h-1u-sy;
    }
    const bool depth8=(a0&0x2000u)!=0;
    const bool map1d=(le16(io,0)&0x40u)!=0;
    const unsigned tileOffset=(sy/8u)*(map1d?(w/8u)*(depth8?2u:1u):32u)
                              +(sx/8u)*(depth8?2u:1u);
    const unsigned tileId=(((depth8?a2&~1u:a2)&1023u)+tileOffset)&1023u;
    const unsigned off=0x10000u+tileId*32u+(sy&7u)*(depth8?8u:4u)
                       +(depth8?(sx&7u):((sx&7u)>>1));
    if(off>=0x18000u||(mode>=3u&&off<0x14000u))return false;
    const unsigned idx=depth8?vr[off]:((vr[off]>>(4u*(sx&1u)))&15u);
    if(!idx)return false;
    paletteIndex=depth8?idx:((a2>>12)&15u)*16u+idx;
    return true;
}
// Exact clipped horizontal coverage for a sprite on one scanline. Coordinates
// wrap at 512 (X) and 256 (Y), matching OBJ position decoding in objPixel().
// Scanning only the covered columns prevents 128 offscreen/8px sprites from
// causing ~5 million redundant pixel probes per pass and frame.
inline bool objVisibleSpan(const uint8_t* oam,unsigned obj,unsigned y,
                           unsigned& begin,unsigned& end){
    const unsigned at=obj*8u;
    const unsigned a0=le16(oam,at),a1=le16(oam,at+2u);
    const bool affine=(a0&0x100u)!=0;
    if(!affine&&(a0&0x200u))return false;
    const unsigned shape=(a0>>14)&3u,size=(a1>>14)&3u;
    if(shape==3u)return false;
    const unsigned w=kObjDims[shape][size][0],h=kObjDims[shape][size][1];
    const unsigned dw=affine&&(a0&0x200u)?2u*w:w;
    const unsigned dh=affine&&(a0&0x200u)?2u*h:h;
    const unsigned dy=(y+256u-(a0&255u))&255u;
    if(dy>=dh)return false;
    const unsigned ox=a1&511u;
    if(ox<unsigned(Frame::width)){
        begin=ox;end=std::min(unsigned(Frame::width),ox+dw);
    }else if(ox+dw>512u){
        begin=0u;end=std::min(unsigned(Frame::width),ox+dw-512u);
    }else return false;
    return begin<end;
}
// scanlineIo holds 160 x 0x60 LCD-register bytes at visible HBlank.
// scanlinePalette holds 160 x 0x400 palette bytes at the same boundary.
// Optional raster buffers: 160 x 0x18000 VRAM and 160 x 0x400 OAM.
// When absent, use the legacy single VBlank VRAM/OAM snapshot.
inline Frame render(const uint8_t* io,const uint8_t* pal,const uint8_t* vr,const uint8_t* oam,
                    const uint8_t* scanlineIo=nullptr,const uint8_t* scanlinePalette=nullptr,
                    const uint8_t* scanlineVram=nullptr,const uint8_t* scanlineOam=nullptr){
    Frame f;f.dispcnt=le16(io,0);f.mode=f.dispcnt&7u;
    if(!scanlineIo&&(f.dispcnt&0x80u)){std::fill(f.rgb.begin(),f.rgb.end(),255);return f;}
    if(!scanlineIo&&f.mode>5u){++f.unsupported_effects;return f;}
    constexpr unsigned pixels=Frame::width*Frame::height;
    std::array<Layer,pixels> front,behind;
    // The GBA resolves the OBJ layer to ONE top-most visible OBJ before
    // comparing it against the backgrounds. Lower-priority OBJs must never
    // become alpha blending's second target (OBJ-to-OBJ blend is impossible).
    std::array<Layer,pixels> topObj;
    std::array<uint8_t,pixels> mask;
    std::array<uint8_t,pixels> objWindow{};
    const uint16_t windows=f.dispcnt&0xE000u;
    // OBJ window is a raster mask, not a visible sprite.  Its pixels rank
    // below WIN0 and WIN1 but above WINOUT.  It requires OBJ and OBJWIN enabled.
    if(((windows&0x8000u)&&(f.dispcnt&0x1000u))||scanlineIo){
        for(unsigned i=0;i<128u;++i){
            for(unsigned y=0;y<Frame::height;++y){
                const uint8_t* rowOam=scanlineOam?scanlineOam+size_t(y)*0x400u:oam;
                const uint8_t* rowVr=scanlineVram?scanlineVram+size_t(y)*0x18000u:vr;
                if(((le16(rowOam,i*8u)>>10)&3u)!=2u)continue;
                const uint8_t* rowIo=scanlineIo?scanlineIo+size_t(y)*0x60u:io;
                const unsigned rowCnt=le16(rowIo,0),rowMode=rowCnt&7u;
                if((rowCnt&0x9000u)!=0x9000u||rowMode>5u)continue;
                unsigned xBegin=0,xEnd=0;
                if(!objVisibleSpan(rowOam,i,y,xBegin,xEnd))continue;
                for(unsigned x=xBegin;x<xEnd;++x){
                    unsigned pi=0;
                    if(objPixel(rowIo,rowVr,rowOam,i,int(x),int(y),rowMode,pi))objWindow[y*Frame::width+x]=1u;
                }
            }
        }
    }
    const uint16_t wh0=le16(io,0x40u),wh1=le16(io,0x42u);
    const uint16_t wv0=le16(io,0x44u),wv1=le16(io,0x46u);
    const uint16_t winin=le16(io,0x48u),winout=le16(io,0x4Au);
    for(unsigned y=0;y<Frame::height;++y)for(unsigned x=0;x<Frame::width;++x){
        const unsigned pos=y*Frame::width+x;
        const uint8_t* rowPal=scanlinePalette?scanlinePalette+size_t(y)*0x400u:pal;
        front[pos].color=le16(rowPal,0);behind[pos].color=le16(rowPal,0);
        const uint8_t* rowIo=scanlineIo?scanlineIo+size_t(y)*0x60u:io;
        const unsigned rowWindows=le16(rowIo,0)&0xE000u;
        const uint16_t rowWh0=scanlineIo?le16(rowIo,0x40u):wh0;
        const uint16_t rowWh1=scanlineIo?le16(rowIo,0x42u):wh1;
        const uint16_t rowWv0=scanlineIo?le16(rowIo,0x44u):wv0;
        const uint16_t rowWv1=scanlineIo?le16(rowIo,0x46u):wv1;
        const uint16_t rowWinIn=scanlineIo?le16(rowIo,0x48u):winin;
        const uint16_t rowWinOut=scanlineIo?le16(rowIo,0x4Au):winout;
        mask[pos]=!rowWindows ? 0x3Fu
            : (rowWindows&0x2000u)&&windowContains(int(x),int(y),rowWh0,rowWv0) ? uint8_t(rowWinIn&0x3Fu)
            : (rowWindows&0x4000u)&&windowContains(int(x),int(y),rowWh1,rowWv1) ? uint8_t((rowWinIn>>8)&0x3Fu)
            : (rowWindows&0x8000u)&&objWindow[pos] ? uint8_t((rowWinOut>>8)&0x3Fu)
            : uint8_t(rowWinOut&0x3Fu);
    }
    const auto insert=[&](unsigned pos,Layer p){
        if(p.rank<front[pos].rank){behind[pos]=front[pos];front[pos]=p;}
        else if(p.rank<behind[pos].rank){behind[pos]=p;}
    };
    const uint16_t mosaic=le16(io,0x4Cu);
    const unsigned bgMosaicX=(mosaic&15u)+1u,bgMosaicY=((mosaic>>4)&15u)+1u;
    for(int bg=3;bg>=0;--bg){
        if(!scanlineIo&&!(f.dispcnt&(0x100u<<bg)))continue;
        for(unsigned y=0;y<Frame::height;++y){
          const uint8_t* rowIo=scanlineIo?scanlineIo+size_t(y)*0x60u:io;
          const uint8_t* rowVr=scanlineVram?scanlineVram+size_t(y)*0x18000u:vr;
          const unsigned rowCnt=le16(rowIo,0),rowMode=rowCnt&7u;
          if(!(rowCnt&(0x100u<<bg))||rowMode>5u)continue;
          const bool text=(rowMode==0u)||(rowMode==1u&&bg<2);
          const bool affine=(rowMode==1u&&bg==2)||(rowMode==2u&&bg>=2);
          const bool bitmap=(rowMode>=3u&&bg==2);
          if(!text&&!affine&&!bitmap)continue;
          const unsigned pr=le16(rowIo,8u+unsigned(bg)*2u)&3u;
          const bool bgMosaic=(le16(rowIo,8u+unsigned(bg)*2u)&0x40u)!=0;
          const unsigned rowMosaicX=scanlineIo?((le16(rowIo,0x4Cu)&15u)+1u):bgMosaicX;
          const unsigned rowMosaicY=scanlineIo?(((le16(rowIo,0x4Cu)>>4)&15u)+1u):bgMosaicY;
          for(unsigned x=0;x<Frame::width;++x){
            const unsigned pos=y*Frame::width+x;
            if(!(mask[pos]&(1u<<unsigned(bg))))continue;
            uint32_t c=0;
            const int sx=bgMosaic?int(x-x%rowMosaicX):int(x);
            const int sy=bgMosaic?int(y-y%rowMosaicY):int(y);
            const uint8_t* rowPal=scanlinePalette?scanlinePalette+size_t(y)*0x400u:pal;
            // Bitmap BG2 uses the very same signed affine matrix as a tiled
            // affine background; out-of-range bitmap samples are transparent.
            int32_t bitmapX=sx,bitmapY=sy;
            if(bitmap)affineSampleXY(rowIo,2,sx,sy,bitmapX,bitmapY);
            const bool visible=text?textPixel(rowIo,rowVr,rowPal,bg,sx,sy,c)
                :affine?affinePixel(rowIo,rowVr,rowPal,bg,sx,sy,c)
                :bitmapPixel(rowIo,rowVr,rowPal,rowMode,bitmapX,bitmapY,c);
            if(visible)insert(pos,{to555(c),uint16_t(pr*256u+128u+unsigned(bg)),uint8_t(bg),false});
          }
        }
    }
    if((f.dispcnt&0x1000u)||scanlineIo){
        for(int i=127;i>=0;--i){
            for(unsigned y=0;y<Frame::height;++y){
                const uint8_t* rowOam=scanlineOam?scanlineOam+size_t(y)*0x400u:oam;
                const uint8_t* rowVr=scanlineVram?scanlineVram+size_t(y)*0x18000u:vr;
                const unsigned a0=le16(rowOam,unsigned(i)*8u),a2=le16(rowOam,unsigned(i)*8u+4u);
                const unsigned objMode=(a0>>10)&3u;
                if(objMode==2u||objMode==3u)continue;
                const uint8_t* rowIo=scanlineIo?scanlineIo+size_t(y)*0x60u:io;
                const unsigned rowCnt=le16(rowIo,0),rowMode=rowCnt&7u;
                if(!(rowCnt&0x1000u)||rowMode>5u)continue;
                unsigned xBegin=0,xEnd=0;
                if(!objVisibleSpan(rowOam,unsigned(i),y,xBegin,xEnd))continue;
                for(unsigned x=xBegin;x<xEnd;++x){
                    const unsigned pos=y*Frame::width+x;
                    if(!(mask[pos]&0x10u))continue;
                    unsigned pi=0;
                    if(objPixel(rowIo,rowVr,rowOam,unsigned(i),int(x),int(y),rowMode,pi)){
                        const Layer candidate{objPal(scanlinePalette?scanlinePalette+size_t(y)*0x400u:pal,pi),
                                              uint16_t(((a2>>10)&3u)*256u+unsigned(i)),4u,objMode==1u};
                        if(candidate.rank<topObj[pos].rank)topObj[pos]=candidate;
                    }
                }
            }
        }
    }
    // Merge just the winning sprite with the two nearest visible BG/backdrop
    // surfaces. Keeping the losers out also preserves correct 2nd-target
    // selection when an OBJ is behind a foreground BG.
    for(unsigned pos=0;pos<pixels;++pos)
        if(topObj[pos].rank<4096u)insert(pos,topObj[pos]);
    const unsigned bldcnt=le16(io,0x50u);
    const unsigned bldalpha=le16(io,0x52u),bldy=le16(io,0x54u)&31u;
    const unsigned eva=std::min(16u,bldalpha&31u),evb=std::min(16u,(bldalpha>>8)&31u),evy=std::min(16u,bldy);
    const unsigned effect=(bldcnt>>6)&3u;
    for(unsigned y=0;y<Frame::height;++y)for(unsigned x=0;x<Frame::width;++x){
        const unsigned pos=y*Frame::width+x;
        const uint8_t* rowIo=scanlineIo?scanlineIo+size_t(y)*0x60u:io;
        const unsigned rowCnt=le16(rowIo,0);
        if((rowCnt&0x80u)||((rowCnt&7u)>5u)){
            pixel(f,x,y,0xFFFFFFu);
            continue;
        }
        const unsigned rowBldCnt=scanlineIo?le16(rowIo,0x50u):bldcnt;
        const unsigned rowBldAlpha=scanlineIo?le16(rowIo,0x52u):bldalpha;
        const unsigned rowBldY=scanlineIo?(le16(rowIo,0x54u)&31u):bldy;
        const unsigned rowEva=scanlineIo?std::min(16u,rowBldAlpha&31u):eva;
        const unsigned rowEvb=scanlineIo?std::min(16u,(rowBldAlpha>>8)&31u):evb;
        const unsigned rowEvy=scanlineIo?std::min(16u,rowBldY):evy;
        const unsigned rowEffect=scanlineIo?((rowBldCnt>>6)&3u):effect;
        uint16_t c=front[pos].color;
        const bool effectEnabled=(mask[pos]&0x20u)!=0;
        const bool secondEligible=(rowBldCnt&(1u<<(behind[pos].kind+8u)))!=0;
        const bool firstEligible=(rowBldCnt&(1u<<front[pos].kind))!=0;
        if(effectEnabled&&secondEligible&&(front[pos].semi||(rowEffect==1u&&firstEligible)))
            c=alpha555(c,behind[pos].color,rowEva,rowEvb);
        else if(effectEnabled&&firstEligible&&rowEffect==2u&&!front[pos].semi)
            c=brightness555(c,rowEvy,true);
        else if(effectEnabled&&firstEligible&&rowEffect==3u&&!front[pos].semi)
            c=brightness555(c,rowEvy,false);
        pixel(f,x,y,rgb555(c));
    }
    return f;
}
inline uint64_t fingerprint(const Frame& f){uint64_t h=14695981039346656037ull;for(uint8_t b:f.rgb){h^=b;h*=1099511628211ull;}return h;}
inline uint32_t crc32(const uint8_t* data,size_t n){uint32_t crc=0xFFFFFFFFu;for(size_t i=0;i<n;++i){crc^=data[i];for(unsigned k=0;k<8;++k)crc=(crc>>1)^((crc&1u)?0xEDB88320u:0u);}return ~crc;}
inline void be32(std::vector<uint8_t>& out,uint32_t v){out.push_back(uint8_t(v>>24));out.push_back(uint8_t(v>>16));out.push_back(uint8_t(v>>8));out.push_back(uint8_t(v));}
inline void chunk(std::vector<uint8_t>& png,const char name[4],const std::vector<uint8_t>& data){be32(png,uint32_t(data.size()));const auto begin=png.size();for(int i=0;i<4;++i)png.push_back(uint8_t(name[i]));png.insert(png.end(),data.begin(),data.end());be32(png,crc32(png.data()+begin,png.size()-begin));}
inline bool writePng(const std::filesystem::path& path,const Frame& f){
    if(f.rgb.size()!=Frame::width*Frame::height*3u)return false;
    std::vector<uint8_t> scan;scan.reserve(f.rgb.size()+Frame::height);
    for(int y=0;y<Frame::height;++y){scan.push_back(0);const auto beg=f.rgb.begin()+y*Frame::width*3;scan.insert(scan.end(),beg,beg+Frame::width*3);}
    std::vector<uint8_t> z{0x78,0x01};
    size_t off=0;while(off<scan.size()){
        const size_t n=std::min<size_t>(65535,scan.size()-off);const bool last=(off+n==scan.size());
        z.push_back(last?1u:0u);z.push_back(uint8_t(n));z.push_back(uint8_t(n>>8));
        const uint16_t inv=uint16_t(~uint16_t(n));z.push_back(uint8_t(inv));z.push_back(uint8_t(inv>>8));
        z.insert(z.end(),scan.begin()+off,scan.begin()+off+n);off+=n;
    }
    uint32_t s1=1,s2=0;for(uint8_t b:scan){s1=(s1+b)%65521u;s2=(s2+s1)%65521u;}be32(z,(s2<<16)|s1);
    std::vector<uint8_t> png{137,80,78,71,13,10,26,10};std::vector<uint8_t> ihdr;be32(ihdr,240);be32(ihdr,160);ihdr.insert(ihdr.end(),{8,2,0,0,0});
    chunk(png,"IHDR",ihdr);chunk(png,"IDAT",z);chunk(png,"IEND",{});
    std::ofstream out(path,std::ios::binary|std::ios::trunc);if(!out)return false;
    out.write(reinterpret_cast<const char*>(png.data()),std::streamsize(png.size()));return bool(out);
}
} // namespace gba_ppu
