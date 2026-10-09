#include <cassert>
#include <string>
#include "../hybrid_mode.hpp"
using namespace crash_hybrid;
int main(){
  assert(requestedMode("")==EngineMode::Ask);
  assert(requestedMode("--original")==EngineMode::Original);
  assert(requestedMode("/original")==EngineMode::Original);
  assert(requestedMode("--reconstruido")==EngineMode::Rebuilt);
  assert(requestedMode("--reconstructed")==EngineMode::Rebuilt);
  assert(requestedMode("--invalid")==EngineMode::Ask);
  assert(requestedMode("--original --verbose")==EngineMode::Original);
  assert(requestedMode("--reconstruido --verbose")==EngineMode::Rebuilt);
  assert(requestedMode("--unoriginal")==EngineMode::Ask);
  assert(requestedMode("--check")==EngineMode::Check);
  assert(requestedMode("--original --verificar")==EngineMode::Check);
  assert(std::string(modeName(EngineMode::Original)).find("original")!=std::string::npos);
  return 0;
}
