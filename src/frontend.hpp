#pragma once
#include "run_state.hpp"
#include "gba_art.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace crash {

inline void uiPixel(std::vector<uint32_t>& p,int x,int y,uint32_t c){if(x>=0&&y>=0&&x<240&&y<160)p[size_t(y*240+x)]=c;}
inline void uiRect(std::vector<uint32_t>& p,int x,int y,int w,int h,uint32_t c){for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++)uiPixel(p,xx,yy,c);}
inline void uiLine(std::vector<uint32_t>& p,int x0,int y0,int x1,int y1,uint32_t c){int dx=std::abs(x1-x0),sx=x0<x1?1:-1,dy=-std::abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;for(;;){uiPixel(p,x0,y0,c);if(x0==x1&&y0==y1)break;int e2=2*err;if(e2>=dy){err+=dy;x0+=sx;}if(e2<=dx){err+=dx;y0+=sy;}}}
inline void uiCircle(std::vector<uint32_t>& p,int cx,int cy,int r,uint32_t c){int x=r,y=0,err=0;while(x>=y){const int q[8][2]={{x,y},{y,x},{-y,x},{-x,y},{-x,-y},{-y,-x},{y,-x},{x,-y}};for(auto& a:q)uiPixel(p,cx+a[0],cy+a[1],c);if(err<=0){++y;err+=2*y+1;}if(err>0){--x;err-=2*x+1;}}}
inline void uiDisc(std::vector<uint32_t>& p,int cx,int cy,int r,uint32_t c){for(int y=-r;y<=r;y++)for(int x=-r;x<=r;x++)if(x*x+y*y<=r*r)uiPixel(p,cx+x,cy+y,c);}
inline void uiDigit(std::vector<uint32_t>& p,int x,int y,unsigned d,uint32_t c){static constexpr const char* g[10]={"111101101101111","010110010010111","111001111100111","111001111001111","101101111001001","111100111001111","111100111101111","111001001001001","111101111101111","111101111001111"};for(int yy=0;yy<5;yy++)for(int xx=0;xx<3;xx++)if(g[d%10][yy*3+xx]=='1')uiPixel(p,x+xx,y+yy,c);}
inline void uiNumber(std::vector<uint32_t>& p,int x,int y,unsigned n,uint32_t c){if(n>=100){uiDigit(p,x,y,(n/100)%10,c);uiDigit(p,x+4,y,(n/10)%10,c);uiDigit(p,x+8,y,n%10,c);}else if(n>=10){uiDigit(p,x,y,(n/10)%10,c);uiDigit(p,x+4,y,n%10,c);}else uiDigit(p,x,y,n,c);}

inline const char* glyph5(char ch){
    switch(ch){
    case 'A':return "01110100011000111111100011000110001";case 'B':return "11110100011000111110100011000111110";
    case 'C':return "01111100001000010000100001000001111";case 'D':return "11110100011000110001100011000111110";
    case 'E':return "11111100001000011110100001000011111";case 'F':return "11111100001000011110100001000010000";
    case 'G':return "01111100001000010111100011000101111";case 'H':return "10001100011000111111100011000110001";
    case 'I':return "11111001000010000100001000010011111";case 'J':return "00111000100001000010100101001001100";
    case 'K':return "10001100101010011000101001001010001";case 'L':return "10000100001000010000100001000011111";
    case 'M':return "10001110111010110101100011000110001";case 'N':return "10001110011010110011100011000110001";
    case 'O':return "01110100011000110001100011000101110";case 'P':return "11110100011000111110100001000010000";
    case 'Q':return "01110100011000110001101011001001101";case 'R':return "11110100011000111110101001001010001";
    case 'S':return "01111100001000001110000010000111110";case 'T':return "11111001000010000100001000010000100";
    case 'U':return "10001100011000110001100011000101110";case 'V':return "10001100011000110001100010101000100";
    case 'W':return "10001100011000110101101011101110001";case 'X':return "10001100010101000100010101000110001";
    case 'Y':return "10001100010101000100001000010000100";case 'Z':return "11111000010001000100010001000011111";
    case '0':return "01110100011001110101110011000101110";case '1':return "00100011000010000100001000010001110";
    case '2':return "01110100010000100010001000100011111";case '3':return "11110000010000101110000010000111110";
    case '4':return "00010001100101010010111110001000010";case '5':return "11111100001000011110000010000111110";
    case '6':return "01110100001000011110100011000101110";case '7':return "11111000010001000100010000100001000";
    case '8':return "01110100011000101110100011000101110";case '9':return "01110100011000101111000010000101110";
    case '-':return "00000000000000011111000000000000000";case ':':return "00000001000000000000001000000000000";
    case '.':return "00000000000000000000000000010000100";case '/':return "00001000100001000100010001000000000";
    default:return "00000000000000000000000000000000000";
    }
}
inline void uiChar(std::vector<uint32_t>& p,int x,int y,char ch,uint32_t c,int s=1){if(ch==' ')return;const char* g=glyph5(ch);for(int yy=0;yy<7;yy++)for(int xx=0;xx<5;xx++)if(g[yy*5+xx]=='1')for(int sy=0;sy<s;sy++)for(int sx=0;sx<s;sx++)uiPixel(p,x+xx*s+sx,y+yy*s+sy,c);}
inline int uiTextWidth(const std::string& s,int scale=1){return s.empty()?0:int(s.size())*6*scale-scale;}
inline void uiText(std::vector<uint32_t>& p,int x,int y,std::string s,uint32_t c,int scale=1){for(char& ch:s)if(ch>='a'&&ch<='z')ch=char(ch-'a'+'A');for(char ch:s){uiChar(p,x,y,ch,c,scale);x+=6*scale;}}
inline void uiCentered(std::vector<uint32_t>& p,int y,const std::string& s,uint32_t c,int scale=1){uiText(p,(240-uiTextWidth(s,scale))/2,y,s,c,scale);}

inline const std::array<const char*,25>& levelNames(){static const std::array<const char*,25> n={{
    "JUNGLE JAM","SHIPWRECKED","TEMPLE OF BOOM","FROSTBITE CAVERN","JUST IN SLIME",
    "SNOW CRASH","ROCKET RACKET","JUST HANGIN","SHARK ATTACK","RUINED",
    "SNOW JOE","ACE OF SPACE","SUNKEN CITY","DOWN THE HOLE","BLIMP BONANZA",
    "STAR TO FINISH","AIR SUPPLY","NO-FLY ZONE","DRIP DRIP DRIP","FINAL COUNTDOWN",
    "DINGODILE","N GIN","TINY","NEO CORTEX","SECRET LEVEL"
}};return n;}
// The GBA level table is 0-19 normal levels, 20-23 bosses, 24 secret.
// The level-select UI presents each boss as slot 6 of its corresponding 5-level world.
inline unsigned warpRoomOf(unsigned level){if(level>=24)return 4u;if(level>=20)return level-20;return level/5u;}
inline unsigned warpSlotOf(unsigned level){if(level>=24)return 0u;if(level>=20)return 5u;return level%5u;}
inline unsigned warpLevel(unsigned room,unsigned slot){if(room>=4)return 24u;return std::min(slot,5u)==5u?20u+room:room*5u+std::min(slot,4u);}

inline std::vector<uint32_t> renderTitleScreen(bool savePresent,int choice){
    std::vector<uint32_t> p(240*160,0xff071936u);
    for(int y=0;y<160;y++){unsigned r=5u+unsigned(y)/10,g=18u+unsigned(y)/6,b=48u+unsigned(y)/3;for(int x=0;x<240;x++)p[size_t(y*240+x)]=0xff000000u|(std::min(r,40u)<<16)|(std::min(g,75u)<<8)|std::min(b,115u);}
    for(int i=0;i<38;i++){int x=(i*67+13)%240,y=(i*29+7)%74;uiPixel(p,x,y,0xff9bdcffu);if((i%4)==0)uiPixel(p,x+1,y,0xffe8f8ffu);}
    uiRect(p,20,21,200,39,0xff17223au);uiRect(p,22,23,196,35,0xff2a3550u);
    uiCentered(p,28,"CRASH",0xffff8a20u,2);uiCentered(p,45,"BANDICOOT",0xffffd04au,1);uiCentered(p,64,"THE HUGE ADVENTURE",0xfff4f1d8u,1);
    std::vector<std::string> opts=savePresent?std::vector<std::string>{"CONTINUAR","NOVO JOGO","SAIR"}:std::vector<std::string>{"NOVO JOGO","SAIR"};
    int y0=91;for(size_t i=0;i<opts.size();i++){bool sel=int(i)==choice;if(sel){uiRect(p,54,y0+int(i)*18-4,132,15,0xff59331bu);uiRect(p,56,y0+int(i)*18-2,128,11,0xffa75b24u);}uiCentered(p,y0+int(i)*18,opts[i],sel?0xffffffffu:0xffcdd7e7u,1);}
    uiCentered(p,148,"ENTER CONFIRMA",0xff8fa5c0u,1);return p;
}

inline std::vector<uint32_t> renderLevelMap(const RunState& state,unsigned selected,const RomUiArt* romArt=nullptr){
    selected=std::min(selected,24u);const unsigned room=warpRoomOf(selected),slot=warpSlotOf(selected);
    std::vector<uint32_t> p(240*160,0xff06121du);
    for(int y=0;y<160;y++){unsigned b=24u+unsigned(y)*28u/160u,g=15u+unsigned(y)*18u/160u,r=4u+unsigned(y)*7u/160u;for(int x=0;x<240;x++)p[size_t(y*240+x)]=0xff000000u|(r<<16)|(g<<8)|b;}
    // The original level-select is layered: gMenuSkyBg behind gLevelSelectPageBg.
    if(romArt&&romArt->menuSkyValid())blitMenuSky(p,*romArt);
    // Prefer the original ROM level-select platform; retain the procedural chamber as fallback.
    if(romArt&&romArt->levelSelectValid())blitOriginalLevelSelect(p,*romArt);
    else{uiRect(p,15,25,210,16,0xff382e2bu);uiRect(p,18,28,204,10,0xff6b5747u);uiLine(p,20,41,57,119,0xff705d4du);uiLine(p,220,41,183,119,0xff705d4du);for(int y=41;y<121;y+=8){int half=34+(y-41)*84/80;uiLine(p,120-half,y,120+half,y,0xff25343bu);}for(int x=35;x<=205;x+=17)uiLine(p,120,44,x,119,0xff1b2b34u);}
    uiCentered(p,8,room<4?(std::string("WARP ROOM ")+char('1'+room)):"SECRET WARP",0xffffd75au,1);
    if(room<4){
        static constexpr int px[6]={38,78,120,162,202,120};
        static constexpr int py[6]={91,80,75,80,91,104};
        for(unsigned s=0;s<6;s++){unsigned level=warpLevel(room,s);bool unlocked=state.levelUnlocked(int(level)),done=state.completedLevels.count(int(level))!=0,sel=s==slot;int x=px[s],y=py[s];bool boss=s==5;
            int outer=boss?16:13,inner=boss?12:9;uint32_t ring=sel?0xffffffffu:(unlocked?0xffe7ad43u:0xff4b535bu),fill=done?0xff4fa965u:(unlocked?(boss?0xffa43939u:0xff654291u):0xff252a31u);
            if(boss){uiRect(p,x-18,y-7,36,14,0xff3b2726u);uiRect(p,x-15,y-5,30,10,0xff674139u);}
            uiDisc(p,x,y,outer,0xff211923u);uiCircle(p,x,y,outer,ring);uiCircle(p,x,y,inner,fill);uiDisc(p,x,y,inner-2,fill);
            if(boss)uiChar(p,x-2,y-3,'B',unlocked?0xffffffffu:0xff72787fu,1);else uiNumber(p,x-2,y-2,s+1,unlocked?0xffffffffu:0xff72787fu);if(done){uiDisc(p,x+outer-2,y-outer+2,3,0xff71e38cu);uiPixel(p,x+outer-2,y-outer+1,0xffffffffu);}
            if(boss)uiCentered(p,y+18,"BOSS",unlocked?0xffffbf55u:0xff73777du,1);
        }
        uiCentered(p,43,"5 FASES + BOSS",0xff9db4c5u,1);
    }else{
        bool unlocked=state.levelUnlocked(24),done=state.completedLevels.count(24)!=0;uiDisc(p,120,91,28,0xff21182du);uiCircle(p,120,91,28,unlocked?0xfff0c35eu:0xff62666cu);uiCircle(p,120,91,22,unlocked?0xff9f4dc2u:0xff33363du);uiNumber(p,116,88,25,unlocked?0xffffffffu:0xff777d83u);if(done)uiDisc(p,140,70,5,0xff71e38cu);uiCentered(p,43,"SECRET PORTAL",0xffbba4d1u,1);
    }
    uiRect(p,8,124,224,28,0xff0e151du);uiRect(p,10,126,220,24,0xff202b38u);
    std::string n=levelNames()[selected];if(uiTextWidth(n)>218)n=n.substr(0,35);uiCentered(p,129,n,state.levelUnlocked(int(selected))?0xffffffffu:0xff8a9098u,1);
    std::string status=state.levelUnlocked(int(selected))?(state.completedLevels.count(int(selected))?"CONCLUIDA":"LIBERADA"):"BLOQUEADA";uiCentered(p,141,status,state.levelUnlocked(int(selected))?0xffffd75au:0xff8a9098u,1);
    return p;
}

inline std::vector<uint32_t> renderPauseScreen(const RomUiArt* romArt,unsigned level){
    std::vector<uint32_t> p(240u*160u,0xff101722u);
    if(romArt&&romArt->pauseMenuValid())blitPauseMenu(p,*romArt);
    else{for(int y=0;y<160;y++){unsigned v=unsigned(18+y/8);for(int x=0;x<240;x++)p[size_t(y*240+x)]=0xff000000u|(v<<16)|((v+4)<<8)|(v+12);}}
    uiRect(p,35,8,170,22,0xff101820u);uiRect(p,38,11,164,16,0xff29384au);uiCentered(p,15,"PAUSADO",0xffffd75au,1);
    uiRect(p,27,119,186,34,0xff0d141cu);uiRect(p,30,122,180,28,0xff202c39u);
    uiCentered(p,126,std::string("FASE ")+std::to_string(level+1),0xffffffffu,1);
    uiCentered(p,138,"P CONTINUA  R REINICIA",0xffffd75au,1);
    return p;
}

inline std::vector<uint32_t> scaleUi(const std::vector<uint32_t>& logical,int scale){if(scale<=1)return logical;int w=240*scale;std::vector<uint32_t> out(size_t(w*160*scale));for(int y=0;y<160;y++)for(int x=0;x<240;x++){uint32_t c=logical[size_t(y*240+x)];for(int yy=0;yy<scale;yy++)for(int xx=0;xx<scale;xx++)out[size_t((y*scale+yy)*w+x*scale+xx)]=c;}return out;}

}
