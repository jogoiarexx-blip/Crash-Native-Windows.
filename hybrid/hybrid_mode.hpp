#pragma once
#include <string>
#include <sstream>
namespace crash_hybrid {
enum class EngineMode { Ask, Original, Rebuilt, Check };
inline EngineMode requestedMode(const std::string& commandLine) {
  std::istringstream words(commandLine);
  std::string token;
  EngineMode result=EngineMode::Ask;
  while(words>>token){
    if(token=="--original" || token=="/original") result=EngineMode::Original;
    else if(token=="--reconstruido" || token=="--reconstructed" || token=="/reconstruido") result=EngineMode::Rebuilt;
    else if(token=="--verificar" || token=="--check") return EngineMode::Check;
  }
  return result;
}
inline const char* modeName(EngineMode mode) {
  switch(mode) {
    case EngineMode::Original:return "ROM original ARM/Thumb traduzida";
    case EngineMode::Rebuilt:return "Crash nativo reconstruido";
    case EngineMode::Check:return "Verificar instalacao";
    default:return "Escolher modo";
  }
}
}
