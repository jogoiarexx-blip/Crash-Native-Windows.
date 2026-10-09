#include "../src/game_data.hpp"
#include <iostream>
void require(bool v){if(!v)throw std::runtime_error("test assertion failed");}
int main(){using namespace crash;
 Bytes literal={0x10,3,0,0,0,'A','B','C'};require(lz10(literal,0).bytes==Bytes({'A','B','C'}));
 Bytes overlap={0x10,9,0,0,0x10,'A','B','C',0x30,2};require(lz10(overlap,0).bytes==Bytes({'A','B','C','A','B','C','A','B','C'}));
 for(auto bad:std::vector<Bytes>{{0x10,3,0,0,0x80,0,0},{0x10,3,0,0,0,'A'},{0x10,0,0,0},{0x11,3,0,0}}){bool fail=false;try{lz10(bad,0);}catch(const std::runtime_error&){fail=true;}require(fail);}
 require(color(31)==0xff0000);require(color(31<<5)==0x00ff00);require(color(31<<10)==0x0000ff);
 Bytes tile(32,0x21);auto px=tiles(tile,0,false,nullptr,0,8,8);require(px[0]==0x111111 && px[1]==0x222222 && px[8]==0x111111);
 Bytes tile8(64,0x80);require(tiles(tile8,0,true,nullptr,0,8,8)[63]==0x808080);
 Bytes fill={128,128,7,0};auto cells=GameData::chunk(fill,0);require(cells.size()==128 && cells[127]==7);
 Bytes delta={128,64,10,0};for(int i=0;i<128;++i)delta.push_back(1);auto seq=GameData::chunk(delta,0);require(seq[0]==10 && seq[127]==137);
 for(auto stream:std::vector<Bytes>{{0,0},{129,128,0,0},{128,128},{2,64,0,0}}){bool failed=false;try{GameData::chunk(stream,0);}catch(const std::runtime_error&){failed=true;}require(failed);}
 std::cout<<"Chunk fill/delta and malformed bounds: PASS\n";
 std::cout<<"LZ10 bounds, overlapping references, colors and tile ordering: PASS\n";
}
