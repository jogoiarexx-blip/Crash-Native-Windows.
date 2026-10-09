#pragma once
#include "rom.hpp"
namespace crash {
struct Image {int width=0,height=0;std::vector<uint32_t> pixels;};
Image loadPNG(const std::string& path);
}
