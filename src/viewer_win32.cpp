#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include "rom.hpp"
namespace fs=std::filesystem;
struct Asset{std::string name;size_t offset,size;};
static crash::Bytes rom,data;static std::vector<Asset> assets;static size_t selection=0,page=0,palette=0;static bool eight=false,colors=false,raw=false;static HWND windowHandle;
static std::vector<uint32_t> pixels;
void refresh(){pixels=crash::tiles(raw?rom:data,page,eight,colors?&rom:nullptr,palette);std::ostringstream t;t<<"Crash Native v0.2.0 | INSPECAO DE RECURSOS - SEM GAMEPLAY | ";if(raw)t<<"ROM";else t<<(selection+1)<<"/"<<assets.size()<<" "<<assets[selection].name;t<<" | +0x"<<std::hex<<page<<" | "<<(eight?8:4)<<"bpp | palette ROM 0x"<<palette<<(colors?" ON":" OFF");SetWindowTextA(windowHandle,t.str().c_str());InvalidateRect(windowHandle,nullptr,FALSE);}
void load(){data=crash::read("assets/"+assets[selection].name);page=0;refresh();}
LRESULT CALLBACK proc(HWND w,UINT m,WPARAM k,LPARAM l){switch(m){
case WM_KEYDOWN:try{
 if(k==VK_ESCAPE){DestroyWindow(w);return 0;}
 if(k==VK_RIGHT || k==VK_LEFT){raw=false;selection=(selection+assets.size()+(k==VK_RIGHT?1:-1))%assets.size();load();}
 else if(k=='R'){raw=!raw;page=0;refresh();}
 else if(k=='B'){eight=!eight;refresh();}
 else if(k=='P'){colors=!colors;refresh();}
 else if(k==VK_NEXT){size_t size=(raw?rom:data).size(),step=eight?65536:32768;if(page+step<size)page+=step;refresh();}
 else if(k==VK_PRIOR){size_t step=eight?65536:32768;page=page>=step?page-step:0;refresh();}
 else if(k=='Q'||k=='W'){size_t step=(GetKeyState(VK_SHIFT)&0x8000)?512:32;if(k=='Q')palette=palette>=step?palette-step:0;else palette=std::min(palette+step,rom.size()-512);refresh();}
 else if(k=='S'){fs::create_directories("exports");std::ostringstream name;name<<"exports/"<<(raw?"ROM":assets[selection].name)<<"_"<<std::hex<<page<<"_"<<(eight?8:4)<<"bpp_pal_"<<(colors?std::to_string(palette):"gray")<<".bmp";crash::bmp(name.str(),pixels,256,256);MessageBoxA(w,name.str().c_str(),"BMP exportado",MB_OK);}
}catch(const std::exception& e){MessageBoxA(w,e.what(),"Erro",MB_ICONERROR);}return 0;
case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT r;GetClientRect(w,&r);FillRect(dc,&r,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));int scale=int(std::max(1L,std::min(r.right/256,(r.bottom-70)/256)));int size=scale*256,x=(r.right-size)/2;
 BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=256;bi.bmiHeader.biHeight=-256;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;SetStretchBltMode(dc,COLORONCOLOR);StretchDIBits(dc,x,0,size,size,0,0,256,256,pixels.data(),&bi,DIB_RGB_COLORS,SRCCOPY);
 SetTextColor(dc,RGB(220,225,240));SetBkMode(dc,TRANSPARENT);const char* a="Setas: bloco | PgUp/PgDn: pagina | B: 4/8bpp | R: ROM | S: exportar BMP";const char* b="P: paleta da ROM | Q/W: mover paleta (+Shift: 512 bytes) | Esc: sair";const char* c="Blocos candidatos sem identificacao. Cores e ordem ainda precisam ser confirmadas.";TextOutA(dc,12,size+6,a,int(strlen(a)));TextOutA(dc,12,size+26,b,int(strlen(b)));TextOutA(dc,12,size+46,c,int(strlen(c)));EndPaint(w,&ps);return 0;}
case WM_SIZE:InvalidateRect(w,nullptr,FALSE);return 0;
case WM_DESTROY:PostQuitMessage(0);return 0;
}return DefWindowProcA(w,m,k,l);}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE,LPSTR,int show){try{
 char path[MAX_PATH];GetModuleFileNameA(nullptr,path,MAX_PATH);fs::current_path(fs::path(path).parent_path());rom=crash::read("input/crash.gba");crash::validate(rom);std::ifstream idx("assets/index.txt");Asset a;while(idx>>a.name>>a.offset>>a.size)assets.push_back(a);if(assets.empty())throw std::runtime_error("Missing assets/index.txt");data=crash::read("assets/"+assets[0].name);
 WNDCLASSA wc{};wc.lpfnWndProc=proc;wc.hInstance=instance;wc.lpszClassName="CrashNativeInspector";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassA(&wc);windowHandle=CreateWindowA(wc.lpszClassName,"Crash Native",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1060,920,nullptr,nullptr,instance,nullptr);if(!windowHandle)throw std::runtime_error("Cannot create window");refresh();ShowWindow(windowHandle,show);MSG msg;while(GetMessageA(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageA(&msg);}return 0;
}catch(const std::exception& e){MessageBoxA(nullptr,e.what(),"Crash Native - erro",MB_ICONERROR);return 1;}}
