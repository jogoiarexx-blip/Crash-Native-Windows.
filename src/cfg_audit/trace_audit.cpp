#include "trace_audit.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <tuple>

uint64_t gba_trace_fingerprint(const GbaRom& rom) {
    uint64_t hash=14695981039346656037ull;
    for(const uint8_t b:rom.bytes()){hash^=b;hash*=1099511628211ull;}
    return hash;
}
std::string gba_trace_header(const GbaRom& rom) {
    std::ostringstream o;
    o << "GBA_TRACE_V1 " << std::hex << std::uppercase << std::setw(16) << std::setfill('0')
      << gba_trace_fingerprint(rom) << std::dec << " " << rom.bytes().size();
    return o.str();
}
namespace {
std::string hex32(uint32_t n) { std::ostringstream o;o << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << n;return o.str(); }
bool parse_u32(const std::string& s,uint32_t& n){
    if(s.empty()||s[0]=='-'||s[0]=='+')return false;
    try{size_t pos=0;const auto v=std::stoull(s,&pos,0);if(pos!=s.size()||v>0xffffffffull)return false;n=static_cast<uint32_t>(v);return true;}
    catch(...){return false;}
}
bool is_ram_word(uint32_t a){return !(a&3u) &&
    ((a>=0x02000000u&&a<=0x0203FFFCu)||(a>=0x03000000u&&a<=0x03007FFCu));}
}
GbaTraceAudit audit_dynamic_trace(const GbaRom& rom,const GbaCfgAnalysis& cfg,const std::filesystem::path& filename){
    GbaTraceAudit a;std::ifstream f(filename);std::ostringstream output;
    output << "Port GBA Builder - Runtime Trace Audit (observations, NOT static proof)\n";
    std::string header;
    const auto h1=gba_trace_header(rom);
    auto h2=h1;h2.replace(0,12,"GBA_TRACE_V2");
    auto h3=h1;h3.replace(0,12,"GBA_TRACE_V3");
    if(!f||!std::getline(f,header)||(header!=h1&&header!=h2&&header!=h3)){
        a.report=output.str()+"REJECTED: missing trace or ROM fingerprint/size mismatch. No entries trusted.\n";
        a.rejected=1;return a;
    }
    const bool v2=header==h2||header==h3;
    const bool v3=header==h3;
    // Only confirmed Thumb BL edges to proven _call_via register veneers.
    std::map<uint32_t,std::set<uint32_t>> expected;
    for(const auto& b:cfg.blocks){
        if(b.mode!=GbaCodeMode::Thumb)continue;
        for(const auto& ins:b.instructions){
            if(ins.address<0x08000000u)continue;
            const size_t off=static_cast<size_t>(ins.address-0x08000000u);
            if(off+4u>rom.bytes().size())continue;
            const uint16_t h=rom.read16(off),l=rom.read16(off+2u);
            if((h&0xF800u)!=0xF000u||(l&0xF800u)!=0xF800u)continue;
            const int32_t high=(int32_t(h&0x7FFu)^0x400)-0x400;
            const int32_t disp=high*4096+int32_t(l&0x7FFu)*2;
            const uint32_t target=static_cast<uint32_t>(int64_t(ins.address)+4+disp);
            for(const auto& call:b.calls)if(call.address==target&&call.mode==GbaCodeMode::Thumb)
                expected[ins.address].insert(target);
        }
    }
    std::set<uint32_t> veneers;
    for(const auto& v:cfg.indirect_veneer_stats)veneers.insert(v.address);
    std::set<std::pair<uint32_t,bool>> knownBlocks;
    for(const auto& b:cfg.blocks)knownBlocks.insert({b.start,b.mode==GbaCodeMode::Thumb});
    std::map<uint32_t,std::set<uint32_t>> targetsPerSite;
    std::set<std::tuple<uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t>> distinct;
    // V3: last observed write for each RAM word. All partial writes overwrite
    // the same word, so a stale vptr generation cannot be reused.
    struct MemWrite{uint32_t serial=0,value=0;};
    std::map<uint32_t,MemWrite> lastWrite;
    uint32_t lastSerial=0;
    std::string line;size_t lineno=1;
    while(std::getline(f,line)){
        ++lineno;if(line.empty()||line[0]=='#')continue;
        std::istringstream in(line);std::string s1,s2,s3,extra;
        if(v3 && line[0]=='W' && line.size()>1 && line[1]==' '){
            uint32_t serial=0,addr=0,value=0;
            // Parse the exact four tokens: W serial address value.
            std::istringstream win(line);std::string tag,ss,sa,sv,tail;
            if(!(win>>tag>>ss>>sa>>sv)||win>>tail || tag!="W" ||
               !parse_u32(ss,serial)||!parse_u32(sa,addr)||!parse_u32(sv,value)||
               !is_ram_word(addr)||serial==0u||serial<=lastSerial){
                ++a.rejected;++a.lifecycle_rejected;output<<"REJECT line "<<lineno<<": invalid/out-of-order RAM write\n";continue;
            }
            lastSerial=serial;lastWrite[addr]={serial,value};++a.memory_writes;
            continue;
        }
        GbaTraceEntry e;
        bool parsed=bool(in>>s1>>s2>>s3) && parse_u32(s1,e.callsite)&&parse_u32(s2,e.veneer)&&parse_u32(s3,e.target);
        if(v2&&parsed){
            uint32_t* extras[]={&e.receiver_register,&e.receiver,&e.object_field,&e.vtable,&e.method_slot,&e.evidence};
            for(uint32_t* dest:extras){std::string token;if(!(in>>token)||!parse_u32(token,*dest)){parsed=false;break;}}
        }
        if(v3&&parsed){std::string token;parsed=bool(in>>token)&&parse_u32(token,e.write_serial);}
        if((in>>extra)||!parsed){++a.rejected;output<<"REJECT line "<<lineno<<": malformed\n";continue;}
        const auto expect=expected.find(e.callsite);
        if(!veneers.count(e.veneer)||expect==expected.end()||!expect->second.count(e.veneer)){
            ++a.rejected;output<<"REJECT line "<<lineno<<": callsite/veneer not a confirmed direct BL edge\n";continue;
        }
        if(v2){
            bool valid=false;
            if(e.evidence==0u||e.evidence==2u){
                valid=e.receiver_register==0xFFFFFFFFu&&e.receiver==0u&&e.object_field==0u&&
                      e.vtable==0u&&e.method_slot==0u&&e.write_serial==0u;
                if(valid&&e.evidence==2u)++a.ambiguous_snapshots;
            }else if((e.evidence==1u||e.evidence==3u)&&e.receiver_register<4u&&
                     is_ram_word(e.receiver)&&!(e.vtable&3u)&&!(e.method_slot&3u)&&
                     e.object_field<=0x1000u && e.receiver<=0xFFFFFFFFu-e.object_field &&
                     is_ram_word(e.receiver+e.object_field)){
                const uint64_t slotAddress=uint64_t(e.vtable)+e.method_slot;
                const uint64_t off=slotAddress>=0x08000000u?slotAddress-0x08000000u:uint64_t(rom.bytes().size());
                if(off<=rom.bytes().size()&&rom.bytes().size()-size_t(off)>=4u&&rom.read32(size_t(off))==e.target){
                    for(const auto& p:cfg.virtual_call_patterns){
                        if(p.callsite!=e.callsite||p.veneer!=e.veneer||
                           p.object_offset!=e.object_field||p.slot_offset!=e.method_slot)continue;
                        if(std::find(p.candidate_tables.begin(),p.candidate_tables.end(),e.vtable)!=p.candidate_tables.end()){
                            valid=true;break;
                        }
                    }
                }
                if(valid&&v3&&e.evidence==3u){
                    const auto it=lastWrite.find(e.receiver+e.object_field);
                    valid=e.write_serial>0u&&it!=lastWrite.end()&&
                          it->second.serial==e.write_serial&&it->second.value==e.vtable;
                    if(!valid)++a.lifecycle_rejected;
                }else if(v3&&e.evidence==1u){valid=valid&&e.write_serial==0u;}
                else if(!v3){valid=valid&&e.evidence==1u;}
            }
            if(!valid){++a.rejected;++a.rejected_snapshots;output<<"REJECT line "<<lineno<<": inconsistent receiver/vtable lifecycle snapshot\n";continue;}
        }
        if(!distinct.insert({e.callsite,e.veneer,e.target,e.receiver,e.vtable,e.method_slot,e.evidence,e.write_serial}).second)continue;
        if(v2&&(e.evidence==1u||e.evidence==3u))++a.consistent_snapshots;
        if(v3&&e.evidence==3u)++a.tracked_snapshots;
        const bool thumb=(e.target&1u)!=0;
        const uint32_t addr=e.target&~1u;
        const bool inRom=addr>=0x08000000u&&static_cast<uint64_t>(addr-0x08000000u)<rom.bytes().size();
        if(knownBlocks.count({addr,thumb})){e.category="known-CFG-block";++a.known;}
        else if(inRom&&((thumb&&!(addr&1u))||(!thumb&&!(addr&3u)))){
            e.category="unmapped-ROM-candidate";++a.candidate;
        }else{e.category="external-or-unaligned";++a.external;}
        ++a.accepted;targetsPerSite[e.callsite].insert(e.target);
        output<<"OBSERVED "<<hex32(e.callsite)<<" via="<<hex32(e.veneer)<<
                " raw-target="<<hex32(e.target)<<" ["<<e.category<<"]";
        if(v2){output<<" evidence="<<e.evidence;
            if(e.evidence==1u||e.evidence==3u)output<<" receiver=r"<<e.receiver_register<<"@"<<hex32(e.receiver)
               <<" field="<<e.object_field<<" vtable="<<hex32(e.vtable)<<" slot="<<e.method_slot;
            if(e.evidence==3u)output<<" tracked-write-serial="<<e.write_serial;
        }
        output<<" (runtime observation, NOT proof of static method identity)\n";
        a.entries.push_back(e);
    }
    a.distinct_callsites=targetsPerSite.size();
    for(const auto& item:targetsPerSite)if(item.second.size()>1u)++a.polymorphic_callsites;
    output<<"SUMMARY accepted_unique="<<a.accepted<<" known-CFG="<<a.known
          <<" unmapped-ROM="<<a.candidate<<" external="<<a.external
          <<" rejected="<<a.rejected<<" observed_callsites="<<a.distinct_callsites
          <<" multiple_targets="<<a.polymorphic_callsites
          <<" consistent_snapshots="<<a.consistent_snapshots
          <<" tracked_snapshots="<<a.tracked_snapshots
          <<" RAM_writes="<<a.memory_writes
          <<" lifecycle_rejected="<<a.lifecycle_rejected
          <<" ambiguous_snapshots="<<a.ambiguous_snapshots
          <<" rejected_snapshots="<<a.rejected_snapshots<<"\n";
    output<<"No observed address was inserted as a CFG root; no global method binding inferred.\n";
    a.report=output.str();return a;
}


bool read_native_frontier(const GbaRom& rom,const std::filesystem::path& file,
                          std::vector<GbaCodeEdge>& roots,std::string& error){
    std::ifstream f(file);std::string h;
    if(!f||!std::getline(f,h)){error="Arquivo de fronteira ausente ou vazio";return false;}
    auto expected=gba_trace_header(rom);expected.replace(0,12,"GBA_FRONTIER_V1");
    if(h!=expected){error="Fronteira pertence a ROM diferente (FNV64/tamanho)";return false;}
    std::set<std::pair<uint32_t,GbaCodeMode>> uniq;
    std::string line;size_t lines=0;
    while(std::getline(f,line)){
        if(line.empty()||line[0]=='#')continue;
        std::istringstream in(line);std::string addr,mode,tail;
        if(!(in>>addr>>mode)||(in>>tail)){error="Linha de fronteira malformada";return false;}
        uint32_t pc=0;
        if(!parse_u32(addr,pc)){error="Endereco de fronteira invalido";return false;}
        if(mode!="THUMB"&&mode!="ARM"){error="Modo de fronteira invalido";return false;}
        GbaCodeMode m=mode=="THUMB"?GbaCodeMode::Thumb:GbaCodeMode::Arm;
        const unsigned size=m==GbaCodeMode::Thumb?2u:4u;
        if((pc&(size-1u))!=0||pc<0x08000000u||
           uint64_t(pc-0x08000000u)+size>rom.bytes().size()){
            error="Fronteira fora da ROM ou desalinhada";return false;
        }
        if(++lines>64u){error="Fronteira tem mais de 64 registros";return false;}
        if(uniq.insert({pc,m}).second)roots.push_back({pc,m});
    }
    if(roots.empty()){error="Fronteira nao possui raizes";return false;}
    return true;
}
