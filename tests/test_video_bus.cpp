#include "gba_video_bus.hpp"
#include <iostream>
using namespace gba_video_bus;
int main(){
 if(locate(0x05002401u).offset!=1u)return 1;
 if(locate(0x06018000u).offset!=0x10000u)return 2;
 if(locate(0x0601FFFFu).offset!=0x17FFFu)return 3;
 if(locate(0x06200020u).offset!=0x20u)return 4;
 if(locate(0x07000400u).offset!=0u)return 5;
 if(canonical(0x06018004u)!=0x06010004u)return 6;
 if(bytePolicy(0x05000400u,0)!=BytePolicy::DuplicateHalfword)return 7;
 if(bytePolicy(0x06010000u,0)!=BytePolicy::Ignore)return 8;
 if(bytePolicy(0x06014000u,3)!=BytePolicy::Ignore)return 9;
 if(bytePolicy(0x06013FFFu,3)!=BytePolicy::DuplicateHalfword)return 10;
 if(bytePolicy(0x07000000u,0)!=BytePolicy::Ignore)return 11;
 if(bytePolicy(0x02000000u,0)!=BytePolicy::Ordinary)return 12;
 std::cout<<"GBA mirrored video bus and STRB classification PASS\n";
}
