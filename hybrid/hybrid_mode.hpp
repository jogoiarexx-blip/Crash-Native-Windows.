#pragma once
#include <string>
namespace crash_hybrid {
enum class EngineMode { Ask, Original, Rebuilt };
inline EngineMode requestedMode(const std::string& commandLine) {
  if(commandLine=="--original" || commandLine=="/original") return EngineMode::Original;
  if(commandLine=="--reconstruido" || commandLine=="--reconstructed" || commandLine=="/reconstruido") return EngineMode::Rebuilt;
  return EngineMode::Ask;
}
inline const char* modeName(EngineMode mode) {
  switch(mode) {
    case EngineMode::Original:return "ROM original ARM/Thumb traduzida";
    case EngineMode::Rebuilt:return "Crash nativo reconstruido";
    default:return "Escolher modo";
  }
}
}
