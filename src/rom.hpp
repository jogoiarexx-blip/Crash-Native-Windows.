#pragma once
#include <cstdint>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
namespace crash {
using Bytes=std::vector<uint8_t>;
inline Bytes read(const std::string& path) {
 std::ifstream f(path,std::ios::binary); if(!f)throw std::runtime_error("Cannot open "+path);
 return Bytes(std::istreambuf_iterator<char>(f),{});
}
inline uint32_t u32(const Bytes& b,size_t p) {
 if(p>b.size() || b.size()-p<4)throw std::runtime_error("Truncated word");
 return b[p]|(uint32_t(b[p+1])<<8)|(uint32_t(b[p+2])<<16)|(uint32_t(b[p+3])<<24);
}
inline void validate(const Bytes& b) {
 if(b.size()!=8388608 || std::string(b.begin()+0xac,b.begin()+0xb0)!="ACQE")throw std::runtime_error("Expected ACQE 8 MiB ROM");
 uint8_t sum=0x19;for(size_t i=0xa0;i<0xbd;++i)sum=uint8_t(sum+b[i]);
 if(uint8_t(-sum)!=b[0xbd])throw std::runtime_error("Invalid GBA header checksum");
}
struct Decoded {Bytes bytes;size_t consumed;};
inline Decoded lz10(const Bytes& b,size_t p,size_t limit=262144) {
 const size_t start=p;const uint32_t h=u32(b,p);p+=4;
 const size_t size=h>>8;if((h&255)!=16 || size==0 || size>limit)throw std::runtime_error("Invalid LZ10 header/size");
 Bytes out;out.reserve(size);
 auto take=[&](){if(p>=b.size())throw std::runtime_error("Truncated LZ10 stream");return b[p++];};
 while(out.size()<size){const auto flag=take();for(int bit=7;bit>=0 && out.size()<size;--bit){
  if(flag&(1<<bit)){const auto a=take(),c=take();const size_t n=(a>>4)+3,d=((a&15)<<8)+c+1;
   if(d>out.size())throw std::runtime_error("Invalid LZ10 distance");
   for(size_t i=0;i<n && out.size()<size;++i)out.push_back(out[out.size()-d]);
  }else out.push_back(take());
 }}return {out,p-start};
}
inline uint32_t color(uint16_t c){auto expand=[](unsigned v){return (v<<3)|(v>>2);};return (expand(c&31)<<16)|(expand((c>>5)&31)<<8)|expand((c>>10)&31);}
inline std::vector<uint32_t> tiles(const Bytes& b,size_t offset,bool eight,const Bytes* palette=nullptr,size_t palOffset=0,int width=256,int height=256){
 if(width<8 || height<8 || width%8 || height%8)throw std::runtime_error("Invalid tile atlas dimensions");
 std::vector<uint32_t> pixels(size_t(width)*height,0x1a1a24);size_t stride=eight?64:32;
 for(int y=0;y<height;++y){
  for(int x=0;x<width;++x){
   size_t tile=size_t(y/8)*(width/8)+x/8,pos=offset+tile*stride+size_t(y%8)*(eight?8:4)+size_t(x%8)/(eight?1:2);
   if(pos>=b.size())continue;
   unsigned n=eight?b[pos]:((b[pos]>>((x&1)*4))&15);uint32_t rgb;
   if(palette && palOffset+2*n+1<palette->size())rgb=color(uint16_t((*palette)[palOffset+2*n]|((*palette)[palOffset+2*n+1]<<8)));
   else {unsigned v=n*(eight?1:17);rgb=v*0x010101;}
   pixels[size_t(y)*width+x]=rgb;
  }
 }
 return pixels;
}
inline void bmp(const std::string& path,const std::vector<uint32_t>& pixels,int w,int h){
 std::ofstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Cannot write BMP");
 auto word=[&](uint16_t v){f.put(char(v));f.put(char(v>>8));};auto dword=[&](uint32_t v){word(uint16_t(v));word(uint16_t(v>>16));};
 word(0x4d42);dword(54+uint32_t(w*h*4));dword(0);dword(54);dword(40);dword(uint32_t(w));dword(uint32_t(-h));word(1);word(32);dword(0);dword(uint32_t(w*h*4));dword(0);dword(0);dword(0);dword(0);
 for(auto v:pixels)dword(v);
 if(!f)throw std::runtime_error("BMP write failed");
}
}
