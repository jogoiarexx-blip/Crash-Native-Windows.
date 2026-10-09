#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <filesystem>
#include <string>
#include "hybrid_mode.hpp"
extern int WINAPI CrashOriginalWinMain(HINSTANCE,HINSTANCE,LPSTR,int);
extern int WINAPI CrashRebuiltWinMain(HINSTANCE,HINSTANCE,LPSTR,int);
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR arguments,int show) {
  wchar_t path[32768]{};
  const DWORD n=GetModuleFileNameW(nullptr,path,32768);
  if(n && n<32768) SetCurrentDirectoryW(std::filesystem::path(path).parent_path().c_str());
  auto mode=crash_hybrid::requestedMode(arguments?arguments:"");
  if(mode==crash_hybrid::EngineMode::Check){
    const bool romOk=std::filesystem::is_regular_file("input/crash.gba");
    MessageBoxW(nullptr,romOk
      ? L"ROM encontrada em input\\crash.gba. Abra CrashHybrid.exe para escolher o motor. A execucao ainda precisa ser testada."
      : L"ROM ausente. Coloque sua copia legitima em input\\crash.gba. Nenhum compilador e necessario.",
      L"Crash Hybrid - verificacao",MB_OK|(romOk?MB_ICONINFORMATION:MB_ICONWARNING));
    return romOk?0:2;
  }
  if(!std::filesystem::is_regular_file("input/crash.gba")) {
    MessageBoxW(nullptr,L"Coloque sua copia legitima da ROM em input\\crash.gba e abra CrashHybrid.exe novamente.",L"ROM necessaria",MB_OK|MB_ICONERROR);
    return 2;
  }
  if(mode==crash_hybrid::EngineMode::Ask) {
    const int answer=MessageBoxW(nullptr,
      L"Escolha o motor:\n\nSIM = ROM original traduzida (experimental)\nNAO = Crash reconstruido (mais jogavel)\nCANCELAR = sair\n\nSaves e estados permanecem independentes.",
      L"Crash Hibrido",MB_YESNOCANCEL|MB_ICONINFORMATION|MB_DEFBUTTON1);
    if(answer==IDYES)mode=crash_hybrid::EngineMode::Original;
    else if(answer==IDNO)mode=crash_hybrid::EngineMode::Rebuilt;
    else return 0;
  }
  return mode==crash_hybrid::EngineMode::Original
    ?CrashOriginalWinMain(instance,previous,arguments,show)
    :CrashRebuiltWinMain(instance,previous,arguments,show);
}
