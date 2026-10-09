#include "rom.hpp"
#include <iostream>
int main(int argc,char** argv){try{
 if(argc<3){std::cout<<"Crash ROM tool v0.2.0\n  inspect ROM\n  lz ROM OFFSET OUTPUT\n  tiles INPUT OFFSET OUTPUT.bmp [8]\nOffsets accept 0x hex.\n";return 0;}
 std::string cmd=argv[1];auto b=crash::read(argv[2]);
 if(cmd=="inspect"){crash::validate(b);std::cout<<"ACQE / CRASH / "<<b.size()<<" bytes / header OK\n";}
 else if(cmd=="lz" && argc==5){auto d=crash::lz10(b,std::stoull(argv[3],nullptr,0));std::ofstream f(argv[4],std::ios::binary);f.write(reinterpret_cast<const char*>(d.bytes.data()),std::streamsize(d.bytes.size()));if(!f)throw std::runtime_error("Write failed");std::cout<<d.bytes.size()<<" decoded bytes, "<<d.consumed<<" input bytes\n";}
 else if(cmd=="tiles" && argc>=5){auto px=crash::tiles(b,std::stoull(argv[3],nullptr,0),argc>=6 && std::string(argv[5])=="8");crash::bmp(argv[4],px,256,256);}
 else throw std::runtime_error("Invalid command");
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
