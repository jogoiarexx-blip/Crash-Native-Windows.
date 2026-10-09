#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <filesystem>
#include <memory>
#include <sstream>
#include <chrono>
#include <cmath>
#include "game_data.hpp"
namespace fs=std::filesystem;
static std::unique_ptr<crash::GameData> data;static crash::Scene scene;static unsigned bank=0,anim=47;static int cameraX=0,cameraY=240;static bool playing=true,showSprite=true,overlay=false,mirror=false,solo=false;static HWND hwnd;static std::vector<uint32_t> pixels;static double clockTicks=0;static std::chrono::steady_clock::time_point last;static bool keys[256]{};
void refresh(){
 auto& a=data->banks.at(bank).animations.at(anim);unsigned duration=std::max(1u,a.duration);size_t step=size_t(clockTicks)/duration;step=(a.flags&2)?step%a.sequence.size():std::min(step,a.sequence.size()-1);unsigned frame=a.sequence[step];
 pixels=solo?std::vector<uint32_t>(240*160,0xff19202c):crash::GameData::viewport(scene,cameraX,cameraY,240,160,overlay);
 if(showSprite){auto f=data->frame(bank,frame,a.paletteRecord);if(solo)crash::GameData::blit(pixels,240,160,f,120,110,mirror);else crash::GameData::blit(pixels,240,160,f,100,116,mirror);}
 std::ostringstream title;title<<"Crash Native v0.2.0 | CENA E ANIMACOES - SEM GAMEPLAY | banco "<<bank<<" anim "<<anim<<" frame "<<frame<<" | camera "<<cameraX<<","<<cameraY;SetWindowTextA(hwnd,title.str().c_str());InvalidateRect(hwnd,nullptr,FALSE);
}
void choose(int delta,bool changeBank){if(changeBank){bank=unsigned((int(bank)+56+delta)%56);anim=0;}else{int n=int(data->banks[bank].animations.size());anim=unsigned((int(anim)+n+delta)%n);}clockTicks=0;refresh();}
void fullscreen(){static bool full=false;static WINDOWPLACEMENT old{};old.length=sizeof(WINDOWPLACEMENT);static LONG_PTR style=0;full=!full;if(full){style=GetWindowLongPtrA(hwnd,GWL_STYLE);GetWindowPlacement(hwnd,&old);MONITORINFO mi{};mi.cbSize=sizeof(MONITORINFO);GetMonitorInfoA(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);SetWindowLongPtrA(hwnd,GWL_STYLE,style&~WS_OVERLAPPEDWINDOW);SetWindowPos(hwnd,HWND_TOP,mi.rcMonitor.left,mi.rcMonitor.top,mi.rcMonitor.right-mi.rcMonitor.left,mi.rcMonitor.bottom-mi.rcMonitor.top,SWP_FRAMECHANGED);}else{SetWindowLongPtrA(hwnd,GWL_STYLE,style);SetWindowPlacement(hwnd,&old);SetWindowPos(hwnd,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);}}
LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM key,LPARAM param){try{switch(msg){
 case WM_KEYDOWN:if(key<256)keys[key]=true;if(param&(1LL<<30))return 0;
  if(key==VK_ESCAPE){DestroyWindow(w);return 0;}if(key==VK_F11){fullscreen();return 0;}if(key=='Q')choose(-1,false);else if(key=='E')choose(1,false);else if(key=='Z')choose(-1,true);else if(key=='X')choose(1,true);else if(key==VK_SPACE)playing=!playing;else if(key=='C')overlay=!overlay;else if(key=='F')mirror=!mirror;else if(key==VK_TAB)solo=!solo;else if(key=='V')showSprite=!showSprite;else if(key=='R'){bank=0;anim=47;cameraX=0;cameraY=240;clockTicks=0;}
  else if(key=='N'){playing=false;clockTicks+=std::max(1u,data->banks[bank].animations[anim].duration);}
  else if(key=='S' && (GetKeyState(VK_CONTROL)&0x8000)){fs::create_directories("exports");crash::bmp("exports/native-scene.bmp",pixels,240,160);MessageBoxA(w,"exports/native-scene.bmp","Imagem exportada",MB_OK);}
  refresh();return 0;
 case WM_KEYUP:if(key<256)keys[key]=false;return 0;
 case WM_KILLFOCUS:std::fill(std::begin(keys),std::end(keys),false);return 0;
 case WM_TIMER:{auto now=std::chrono::steady_clock::now();double dt=std::min(.1,std::chrono::duration<double>(now-last).count());last=now;if(playing)clockTicks+=dt*60.0;int speed=keys[VK_SHIFT]?12:3;
  if(keys[VK_LEFT])cameraX-=speed;
  if(keys[VK_RIGHT])cameraX+=speed;
  if(keys[VK_UP])cameraY-=speed;
  if(keys[VK_DOWN])cameraY+=speed;
  cameraX=std::clamp(cameraX,0,4016-240);cameraY=std::clamp(cameraY,0,480-160);refresh();return 0;}
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT r;GetClientRect(w,&r);FillRect(dc,&r,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));int scale=int(std::max(1L,std::min(r.right/240,(r.bottom-92)/160)));int width=240*scale,height=160*scale,x=(r.right-width)/2;
  BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=240;bi.bmiHeader.biHeight=-160;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;SetStretchBltMode(dc,COLORONCOLOR);StretchDIBits(dc,x,0,width,height,0,0,240,160,pixels.data(),&bi,DIB_RGB_COLORS,SRCCOPY);SetTextColor(dc,RGB(225,230,245));SetBkMode(dc,TRANSPARENT);
  const char* lines[]={"SETAS: camera | Q/E: animacao | Z/X: banco | TAB: sprite isolado | F11: tela cheia","ESPACO: pausar | N: proximo frame | F: espelhar | V: ocultar sprite | R: reiniciar","C: mapa de terreno (visualizacao) | Ctrl+S: imagem BMP | ESC: sair","PREVIA NATIVA: sem movimento do personagem, fisica, inimigos ou progressao."};for(int i=0;i<4;i++)TextOutA(dc,12,height+8+20*i,lines[i],int(strlen(lines[i])));EndPaint(w,&ps);return 0;}
 case WM_SIZE:InvalidateRect(w,nullptr,FALSE);return 0;
 case WM_DESTROY:KillTimer(w,1);PostQuitMessage(0);return 0;
 }}catch(const std::exception& e){KillTimer(w,1);MessageBoxA(w,e.what(),"Crash Native - erro",MB_ICONERROR);DestroyWindow(w);return 0;}return DefWindowProcA(w,msg,key,param);}
int WINAPI WinMain(HINSTANCE h,HINSTANCE,LPSTR,int show){try{SetProcessDPIAware();char path[MAX_PATH];GetModuleFileNameA(nullptr,path,MAX_PATH);fs::current_path(fs::path(path).parent_path());data=std::make_unique<crash::GameData>(crash::read("input/crash.gba"));scene=data->scene();WNDCLASSA wc{};wc.lpfnWndProc=proc;wc.hInstance=h;wc.lpszClassName="CrashNativeScene";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassA(&wc);hwnd=CreateWindowA(wc.lpszClassName,"Crash Native v0.2.0",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1230,930,nullptr,nullptr,h,nullptr);if(!hwnd)throw std::runtime_error("Window creation failed");refresh();last=std::chrono::steady_clock::now();SetTimer(hwnd,1,16,nullptr);ShowWindow(hwnd,show);MSG m;while(GetMessageA(&m,nullptr,0,0)>0){TranslateMessage(&m);DispatchMessageA(&m);}return 0;}catch(const std::exception& e){MessageBoxA(nullptr,e.what(),"Crash Native - erro",MB_ICONERROR);return 1;}}
