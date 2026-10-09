#include "png.hpp"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "../third_party/stb_image.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
namespace crash {
Image loadPNG(const std::string& path){
 std::ifstream input(path,std::ios::binary|std::ios::ate);
 if(!input)throw std::runtime_error("PNG missing: "+path);
 auto length=input.tellg();if(length<=0||length>32*1024*1024)throw std::runtime_error("PNG file size limit");
 input.seekg(0);Bytes bytes(static_cast<size_t>(length));if(!input.read(reinterpret_cast<char*>(bytes.data()),length))throw std::runtime_error("PNG read error");
 int w=0,h=0,n=0;
 if(!stbi_info_from_memory(bytes.data(),int(bytes.size()),&w,&h,&n)||w<=0||h<=0||w>8192||h>8192||int64_t(w)*h>16*1024*1024)throw std::runtime_error("Invalid PNG or dimensions exceed limits");
 auto rgba=stbi_load_from_memory(bytes.data(),int(bytes.size()),&w,&h,&n,4);if(!rgba)throw std::runtime_error("PNG decode failed");
 Image out;out.width=w;out.height=h;
 try{out.pixels.resize(size_t(w)*h);for(size_t i=0;i<out.pixels.size();i++)out.pixels[i]=(uint32_t(rgba[i*4+3])<<24)|(uint32_t(rgba[i*4])<<16)|(uint32_t(rgba[i*4+1])<<8)|rgba[i*4+2];}catch(...){stbi_image_free(rgba);throw;}
 stbi_image_free(rgba);return out;
}
}
