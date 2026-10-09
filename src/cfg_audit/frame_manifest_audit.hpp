#pragma once
// ROM-bound independent audit of uncompressed-deflate RGB PNGs produced by
// the experimental Builder PPU. Does not assert LCD/raster accuracy.
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct GbaFrameAudit {bool valid=false;std::string report;size_t frames=0;};
namespace gba_frame_audit_detail {
inline uint64_t fnv(const uint8_t* d,size_t n){uint64_t h=14695981039346656037ull;for(size_t i=0;i<n;++i){h^=d[i];h*=1099511628211ull;}return h;}
inline uint32_t crc32(const uint8_t* d,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;++i){c^=d[i];for(int k=0;k<8;++k)c=(c>>1)^((c&1u)?0xEDB88320u:0u);}return ~c;}
inline uint32_t be32(const std::vector<uint8_t>& d,size_t p){if(p+4u>d.size())throw std::runtime_error("truncated PNG integer");return (uint32_t(d[p])<<24)|(uint32_t(d[p+1])<<16)|(uint32_t(d[p+2])<<8)|d[p+3];}
inline std::vector<uint8_t> load(const std::filesystem::path& file){
 std::ifstream in(file,std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("PNG not found: "+file.string());
 const auto n=in.tellg();if(n<0||n>1024*1024)throw std::runtime_error("PNG exceeds size limit");
 std::vector<uint8_t> data(static_cast<size_t>(n));in.seekg(0);in.read(reinterpret_cast<char*>(data.data()),n);
 if(!in)throw std::runtime_error("PNG read failure");
 return data;
}
inline std::vector<uint8_t> pngPixels(const std::filesystem::path& file){
 const auto b=load(file);const std::array<uint8_t,8> magic{{137,80,78,71,13,10,26,10}};
 if(b.size()<8u||!std::equal(magic.begin(),magic.end(),b.begin()))throw std::runtime_error("PNG signature invalid");
 std::vector<uint8_t> idat;bool gotIHDR=false,gotEnd=false;size_t p=8;
 while(p+12u<=b.size()){
   const auto n=be32(b,p);p+=4;if(n>1024*1024||p+8u+n>b.size())throw std::runtime_error("PNG chunk length invalid");
   const size_t typepos=p;std::string type(reinterpret_cast<const char*>(b.data()+p),4);p+=4;
   if(crc32(b.data()+typepos,n+4u)!=be32(b,p+n))throw std::runtime_error("PNG CRC mismatch");
   if(type=="IHDR"){
     if(gotIHDR||n!=13u||be32(b,p)!=240u||be32(b,p+4)!=160u||b[p+8]!=8||b[p+9]!=2||b[p+10]!=0||b[p+11]!=0||b[p+12]!=0)throw std::runtime_error("PNG size/format unsupported");
     gotIHDR=true;
   }else if(type=="IDAT"){
     if(!gotIHDR)throw std::runtime_error("PNG IDAT before IHDR");
     idat.insert(idat.end(),b.begin()+p,b.begin()+p+n);
   }else if(type=="IEND"){
     if(n!=0u)throw std::runtime_error("PNG IEND invalid");
     gotEnd=true;
     if(p+n+4u!=b.size())throw std::runtime_error("PNG trailing data");
   }else throw std::runtime_error("Unexpected PNG chunk");
   p+=n+4u;if(gotEnd)break;
 }
 if(!gotIHDR||!gotEnd||idat.size()<6u||idat[0]!=0x78u||idat[1]!=0x01u)throw std::runtime_error("Unsupported PNG/zlib");
 std::vector<uint8_t> raw;size_t z=2;
 for(bool end=false;!end;){
   if(z+5u>idat.size()-4u)throw std::runtime_error("truncated deflate");
   const unsigned mode=idat[z++];end=(mode&1u)!=0;
   if((mode&0xFEu)!=0)throw std::runtime_error("only stored deflate supported");
   const unsigned len=unsigned(idat[z])|(unsigned(idat[z+1])<<8);
   const unsigned neg=unsigned(idat[z+2])|(unsigned(idat[z+3])<<8);z+=4;
   if((len^neg)!=65535u||z+len>idat.size()-4u)throw std::runtime_error("stored deflate length invalid");
   raw.insert(raw.end(),idat.begin()+z,idat.begin()+z+len);z+=len;
   if(raw.size()>160u*721u)throw std::runtime_error("inflated PNG too long");
 }
 if(z+4u!=idat.size()||raw.size()!=160u*721u)throw std::runtime_error("inflated PNG size invalid");
 uint32_t a=1,c=0;for(auto b0:raw){a=(a+b0)%65521u;c=(c+a)%65521u;}
 if(((c<<16)|a)!=be32(idat,z))throw std::runtime_error("PNG Adler mismatch");
 std::vector<uint8_t> rgb;rgb.reserve(240u*160u*3u);
 for(size_t y=0;y<160;++y){const size_t off=y*721u;if(raw[off]!=0)throw std::runtime_error("PNG filter unsupported");rgb.insert(rgb.end(),raw.begin()+off+1u,raw.begin()+off+721u);}
 return rgb;
}
inline uint64_t parseU64(const std::string& s,int base=10){size_t n=0;const auto v=std::stoull(s,&n,base);if(n!=s.size())throw std::runtime_error("number malformed");return v;}
inline std::map<uint64_t,uint16_t> evidence(const std::filesystem::path& file){
 std::ifstream in(file);if(!in)throw std::runtime_error("video evidence not found");std::string line;
 std::map<uint64_t,uint16_t> samples;
 while(std::getline(in,line)){
  if(line.rfind("frame=",0)!=0)continue;
  std::istringstream ss(line);std::string word;uint64_t frame=0;uint16_t dispcnt=0;
  while(ss>>word){if(word.rfind("frame=",0)==0)frame=parseU64(word.substr(6));if(word.rfind("dispcnt=",0)==0){const auto v=parseU64(word.substr(8),0);if(v>65535u)throw std::runtime_error("bad DISPCNT");dispcnt=uint16_t(v);}}
  if(!frame||!samples.emplace(frame,dispcnt).second)throw std::runtime_error("duplicate frame evidence");
 }
 return samples;
}
} // namespace gba_frame_audit_detail
inline GbaFrameAudit audit_gba_frame_manifest(const std::vector<uint8_t>& rom,const std::filesystem::path& folder,const std::filesystem::path& videoEvidence){
 using namespace gba_frame_audit_detail;
 try{
  std::ifstream mf(folder/"manifest.tsv");if(!mf)throw std::runtime_error("manifest not found");
  std::string line; if(!std::getline(mf,line))throw std::runtime_error("empty manifest");
  std::istringstream head(line);std::string mark,hash,bytes;
  if(!std::getline(head,mark,'\t')||mark!="GBA_FRAME_V1"||!std::getline(head,hash,'\t')||!std::getline(head,bytes,'\t')||
     hash.rfind("rom_fnv=",0)!=0||bytes.rfind("rom_bytes=",0)!=0)throw std::runtime_error("manifest header invalid");
  if(parseU64(hash.substr(8),0)!=fnv(rom.data(),rom.size())||parseU64(bytes.substr(10))!=rom.size())throw std::runtime_error("ROM fingerprint mismatch");
  const auto evidenceByFrame=evidence(videoEvidence);
  std::ostringstream report;report<<"GBA Frame Audit — experimental compositor; NOT hardware accuracy proof\n";
  size_t count=0;uint64_t previous=0;
  while(std::getline(mf,line)){
   if(line.empty())continue;
   if(++count>16u)throw std::runtime_error("too many exported frames");
   std::istringstream row(line);std::array<std::string,6> cells{};
   for(auto& cell:cells)if(!std::getline(row,cell,'\t'))throw std::runtime_error("manifest row incomplete");
   const auto frame=parseU64(cells[0]),mode=parseU64(cells[1]);const auto disp=parseU64(cells[2],0),pixelHash=parseU64(cells[3],0);
   const auto effects=parseU64(cells[4]);const auto& filename=cells[5];
   if(frame<=previous||mode>5||disp>65535u||((disp&7u)!=mode)||effects>1024u||filename!="vblank_"+std::to_string(frame)+".png")throw std::runtime_error("manifest frame fields invalid");
   const auto i=evidenceByFrame.find(frame);if(i==evidenceByFrame.end()||i->second!=disp)throw std::runtime_error("frame missing from VBlank evidence or DISPCNT mismatch");
   const auto pixels=pngPixels(folder/filename);
   if(fnv(pixels.data(),pixels.size())!=pixelHash)throw std::runtime_error("PNG pixels do not match manifest");
   report<<"frame="<<frame<<" mode="<<mode<<" PNG=240x160 pixels verified unsupported_effects="<<effects<<'\n';previous=frame;
  }
  if(!count)throw std::runtime_error("no frames in manifest");
  report<<"ROM-bound PNG audit PASS: "<<count<<" frames; compositor output, not emulated LCD accuracy.\n";
  return {true,report.str(),count};
 }catch(const std::exception& e){return {false,std::string("GBA Frame Audit REJECTED: ")+e.what(),0};}
}
