#include "cfg_audit/cfg.hpp"
#include "cfg_audit/gba_rom.hpp"
#include "cfg_audit/trace_audit.hpp"
#include "cfg_audit/video_evidence_audit.hpp"
#include "cfg_audit/frame_manifest_audit.hpp"
#include <algorithm>
#include <cstdint>
#include <deque>
#include <set>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace {
constexpr uint32_t kRomBase = 0x08000000u;

int32_t sign_extend24(uint32_t value) {
    value &= 0x00FFFFFFu;
    if (value & 0x00800000u) value |= 0xFF000000u;
    return static_cast<int32_t>(value);
}

uint32_t entry_target(const GbaRom& rom) {
    const uint32_t ins = rom.read32(0);
    if ((ins & 0x0F000000u) != 0x0A000000u) return 0;
    const int64_t disp = int64_t(sign_extend24(ins)) * 4;
    return static_cast<uint32_t>(int64_t(kRomBase) + 8 + disp);
}

std::string hex8(uint32_t v) {
    std::ostringstream o;
    o << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << v;
    return o.str();
}

std::string build_report(const GbaCfgAnalysis& cfg, uint32_t entry) {
    using Key = std::pair<uint32_t, GbaCodeMode>;
    std::map<Key, size_t> incoming_calls;
    for (const auto& block : cfg.blocks) {
        for (const auto& call : block.calls) ++incoming_calls[{call.address, call.mode}];
    }

    std::map<uint32_t, bool> veneer_addresses;
    for (const auto& v : cfg.indirect_veneer_stats) veneer_addresses[v.address] = true;
    std::vector<std::tuple<size_t, uint32_t, GbaCodeMode>> ranked;
    ranked.reserve(incoming_calls.size());
    for (const auto& [key, count] : incoming_calls) {
        if (veneer_addresses.count(key.first)) continue;
        ranked.emplace_back(count, key.first, key.second);
    }
    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
        if (std::get<0>(a) != std::get<0>(b)) return std::get<0>(a) > std::get<0>(b);
        return std::get<1>(a) < std::get<1>(b);
    });

    std::vector<uint32_t> indirects;
    for (const auto& block : cfg.blocks) {
        if (block.terminator == GbaTerminator::Indirect) indirects.push_back(block.start);
    }
    std::sort(indirects.begin(), indirects.end());

    std::ostringstream o;
    o << "Crash Native v0.66 HBlank VRAM-OAM Crossfeed Audit\n";
    o << "================================================================\n";
    o << "Entry ARM: " << hex8(entry) << "\n";
    o << "CFG blocks (incl. execution-observed roots if supplied): " << cfg.blocks.size() << " (ARM " << cfg.arm_blocks << ", Thumb " << cfg.thumb_blocks << ")\n";
    o << "Confirmed entry/direct-call functions: " << cfg.function_entries.size() << "\n";
    o << "Runtime-observed extra CFG roots (NOT static proof): " << cfg.profiled_roots.size() << "\n";
    o << "Decoded instructions: " << cfg.decoded_instructions << "\n";
    o << "Direct calls: " << cfg.direct_calls << "\n";
    o << "Direct branches: " << cfg.direct_branches << "\n";
    o << "Conditional branches: " << cfg.conditional_branches << "\n";
    o << "Returns: " << cfg.returns << " (compiler pop/BX epilogues " << cfg.compiler_epilogue_returns << ")\n";
    o << "Resolved indirects: " << cfg.resolved_indirects << " (switch tables " << cfg.resolved_switch_tables << ", targets " << cfg.resolved_switch_targets << ")\n";
    o << "Unresolved indirect endpoints: " << cfg.unresolved_indirects << " (call-via veneers " << cfg.indirect_call_veneers << ")\n";
    o << "Dynamic callsites through veneers: " << cfg.indirect_call_sites << "\n";
    o << "Statically recovered dynamic callsites: " << cfg.resolved_dynamic_call_sites << " -> " << cfg.resolved_dynamic_call_targets << " targets\n";
    o << "ROM-rooted method tables: " << cfg.rooted_function_tables.size()
      << " (slots=" << cfg.rooted_table_slots
      << ", distinct methods=" << cfg.rooted_table_distinct_functions << ")\n";
    o << "Must-provenance cross-block virtual patterns (advisory): " << cfg.crossblock_virtual_patterns << "\n";
    o << "Constructor direct-call relationships (advisory): " << cfg.constructor_callers.size() << "\n";
    o << "NOTICE: ROM methods are proven table references, NOT resolved per-veneer call targets.\n";

    o << "Liftable blocks: ARM " << cfg.arm_liftable_blocks << "/" << cfg.arm_blocks
      << ", Thumb " << cfg.thumb_liftable_blocks << "/" << cfg.thumb_blocks << "\n\n";

    o << "Top concrete direct-call targets (native-port priority)\n";
    o << "----------------------------------------------\n";
    const size_t limit = std::min<size_t>(64, ranked.size());
    for (size_t i = 0; i < limit; ++i) {
        const auto [count, address, mode] = ranked[i];
        o << std::setw(2) << (i + 1) << ". " << hex8(address) << " "
          << (mode == GbaCodeMode::Thumb ? "THUMB" : "ARM") << " calls=" << count << "\n";
    }

    o << "\nDynamic call-via veneers\n";
    o << "------------------------\n";
    for (const auto& v : cfg.indirect_veneer_stats) {
        o << hex8(v.address) << " target=r" << unsigned(v.register_index) << " callers=" << v.callers << "\n";
    }

    o << "\nROM-rooted C++ method tables (new native CFG seeds)\n";
    o << "---------------------------------------------------\n";
    for (const auto& table:cfg.rooted_function_tables) {
        o << "table=" << hex8(table.address) << " LDR=" << hex8(table.literal_reference)
          << " slots=" << table.slots << " distinct=" << table.methods.size() << "\n";
        for (const auto& m:table.methods) o << "  method=" << hex8(m.address) << "\n";
    }
    o << "\nProven constructor vtable pointer stores\n";
    o << "---------------------------------------\n";
    for (const auto& store:cfg.vtable_installs) {
        o << hex8(store.instruction) << " table=" << hex8(store.table_address)
          << " object_field=" << store.object_offset << " base=r" << unsigned(store.base_register) << "\n";
    }
    o << "\nVirtual callsite load patterns [CANDIDATES ONLY, NOT CFG ROOTS]\n";
    o << "-------------------------------------------------------------\n";
    for(const auto& pattern:cfg.virtual_call_patterns){
        o << hex8(pattern.callsite) << " via=" << hex8(pattern.veneer)
          << " object_field=" << pattern.object_offset << " method_slot=" << pattern.slot_offset
          << " candidate_tables=" << pattern.candidate_tables.size()
          << " candidate_methods=" << pattern.candidate_methods.size()
          << " cross_block=" << (pattern.crosses_blocks?"YES":"NO") << "\n";
        for(const auto& method:pattern.candidate_methods) o << "  candidate_method=" << hex8(method.address) << "\n";
    }
    o << "\nDirect callers of functions containing constructor stores [CALLGRAPH ONLY]\n";
    o << "-------------------------------------------------------------------------\n";
    for(const auto& link:cfg.constructor_callers){
        o << hex8(link.callsite) << " callee=" << hex8(link.callee_entry)
          << " vptr_store=" << hex8(link.constructor_store)
          << " table=" << hex8(link.table_address)
          << " object_field=" << link.object_offset << " NOT_INSTANCE_PROOF\n";
    }
    std::map<std::pair<uint16_t,uint16_t>,size_t> clusters;
    for(const auto& p:cfg.virtual_call_patterns) ++clusters[{p.object_offset,p.slot_offset}];
    std::vector<std::pair<std::pair<uint16_t,uint16_t>,size_t>> byCount(clusters.begin(),clusters.end());
    std::sort(byCount.begin(),byCount.end(),[](const auto& a,const auto& b){
        if(a.second!=b.second) return a.second>b.second;
        return a.first<b.first;
    });
    o << "\nDynamic dispatch investigation priority (candidate evidence)\n";
    o << "-----------------------------------------------------------\n";
    for(const auto& cl:byCount) o << "object_field=" << cl.first.first
        << " method_slot=" << cl.first.second << " callsites=" << cl.second << "\n";
    o << "\nRuntime-dependent call-via sites (address + target register)\n";
    o << "-----------------------------------------------------------\n";
    for (const auto& block:cfg.blocks) {
        for (const auto& call:block.calls) {
            const auto ven=std::find_if(cfg.indirect_veneer_stats.begin(),cfg.indirect_veneer_stats.end(),
                [&](const GbaIndirectVeneerStat& v){return v.address==call.address;});
            if (ven==cfg.indirect_veneer_stats.end()) continue;
            const uint32_t site=block.instructions.size()>=2u
                ?block.instructions[block.instructions.size()-2].address:block.start;
            o<<hex8(site)<<" via="<<hex8(call.address)<<" r"<<unsigned(ven->register_index);
            if(block.resolved_dynamic_calls.empty())o<<" PENDING_RUNTIME_OBJECT";
            else {o<<" STATIC";for(const auto& t:block.resolved_dynamic_calls)o<<" "<<hex8(t.address);}
            o<<"\n";
        }
    }
    o << "\nUnresolved indirect endpoints\n";
    o << "-----------------------------\n";
    for (uint32_t address : indirects) o << hex8(address) << "\n";
    return o.str();
}
// ROM-bound audit of a generated GBA hardware-progress observation.
// Keep this pure diagnostic: it NEVER changes the Win32 playable engine.
struct HardwareRunAudit {
    bool valid=false;
    std::string explanation;
};
HardwareRunAudit audit_hardware_run(const GbaRom& rom, const std::string& path) {
    std::ifstream in(path);
    if(!in)return {false,"cannot open observation log"};
    std::string line;
    while(std::getline(in,line)) {
        if(line.rfind("Actual generated native block execution:",0)!=0)continue;
        std::istringstream fields(line);
        std::string token;
        std::map<std::string,std::string> values;
        while(fields>>token){const auto split=token.find('=');if(split!=std::string::npos)values[token.substr(0,split)]=token.substr(split+1);}
        try {
            auto field=[&](const char* key)->uint64_t{
                const auto it=values.find(key);
                if(it==values.end()||it->second.empty())throw std::runtime_error(std::string("missing ")+key);
                size_t used=0;const uint64_t n=std::stoull(it->second,&used,0);
                if(used!=it->second.size())throw std::runtime_error(std::string("invalid ")+key);
                return n;
            };
            const uint64_t recordedHash=field("rom_fingerprint");
            const uint64_t bytes=field("rom_bytes");
            const uint64_t cycles=field("estimated_cycles");
            const uint64_t vblanks=field("vblank_events");
            const uint64_t frames=field("completed_frames");
            const uint64_t vcount=field("vcount");
            uint64_t expectedHash=14695981039346656037ull;
            for(uint8_t b:rom.bytes()){expectedHash^=b;expectedHash*=1099511628211ull;}
            if(recordedHash!=expectedHash||bytes!=rom.bytes().size())return {false,"ROM fingerprint or byte count mismatch"};
            if(!cycles)return {false,"zero estimated cycles"};
            constexpr uint64_t lineCycles=1232u,lines=228u,frameCycles=lineCycles*lines;
            constexpr uint64_t vblankBegin=160u*lineCycles;
            const uint64_t expectedFrames=cycles/frameCycles;
            const uint64_t expectedVcount=(cycles/lineCycles)%lines;
            const uint64_t expectedVblanks=cycles<vblankBegin?0u:1u+(cycles-vblankBegin)/frameCycles;
            if(frames!=expectedFrames||vcount!=expectedVcount||vblanks!=expectedVblanks)
                return {false,"inconsistent LCD clock / VCOUNT / VBlank counters"};
            // V1.4 emits actual IRQ exception counts, not just IF bits.
            // Older v1.3 hardware logs remain valid as legacy diagnostics.
            const bool hasIrq=values.count("irq_entries")!=0;
            uint64_t irqIn=0,irqOut=0,irqActive=0,irqMissing=0;
            if(hasIrq){
                irqIn=field("irq_entries");irqOut=field("irq_returns");
                irqActive=field("irq_active");irqMissing=field("irq_unvectored");
                if(irqActive>1u||irqOut>irqIn||irqIn-irqOut!=irqActive)
                    return {false,"IRQ entry/return/active context mismatch"};
                if(irqIn>cycles || irqMissing>cycles)
                    return {false,"impossible IRQ counter exceeds elapsed cycles"};
            }
            std::ostringstream r;
            r<<"\nCROSSFEED: ROM-bound hardware progress (estimated instruction cycles)\n"
             <<"ROM verified: FNV64=0x"<<std::hex<<recordedHash<<std::dec
             <<" bytes="<<bytes<<"\n"
             <<"Estimated cycles="<<cycles<<" VBlank events="<<vblanks
             <<" completed frames="<<frames<<" VCOUNT="<<vcount<<"\n"
             <<"LCD invariants PASS; these events are simulated, not rendered gameplay frames.\n"
             <<(hasIrq?(std::string("IRQ exception BIOS bridge: entries=")+std::to_string(irqIn)+
                " returns="+std::to_string(irqOut)+" active="+std::to_string(irqActive)+
                " missing_vector="+std::to_string(irqMissing)+
                "; context counter invariants PASS.\n"):
                "Legacy hardware log (IRQ requests only; no CPU exception counts).\n");
            return {true,r.str()};
        }catch(const std::exception& e){return {false,std::string("invalid hardware log: ")+e.what()};}
    }
    return {false,"no generated runtime observation in log"};
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Uso: CrashCfgPriorityTool ROM [relatorio.txt] [trace.txt] [--frontier ROM_BOUND.txt] [--iwram-copy SOURCE_ROM DEST_IWRAM SIZE ENTRY] [--iwram-irq-entry IRQ_RAM] [--iwram-extra SOURCE_ROM DEST_IWRAM SIZE ENTRY]... [--iwram-overlay SOURCE_ROM DEST_IWRAM SIZE ENTRY]... [--iwram-root ARM_RAM]... [--hardware-log BuilderRun.txt] [--video-evidence VideoEvidence.txt] [--frame-dir frames/]\n";
        return 1;
    }
    GbaRom rom;
    std::string error;
    if (!rom.load(argv[1], error)) {
        std::cerr << "ERRO: " << error << "\n";
        return 2;
    }
    if (rom.header().game_code != "ACQE") {
        std::cerr << "ERRO: esperado Crash ACQE; encontrado " << rom.header().game_code << "\n";
        return 3;
    }
    const uint32_t entry = entry_target(rom);
    if (!entry) {
        std::cerr << "ERRO: entry point ARM nao reconhecido.\n";
        return 4;
    }
    std::vector<GbaCodeEdge> profiled;
    struct Relocation {uint32_t src=0,dst=0,size=0,entry=0,irqEntry=0;bool overlay=false;};
    Relocation relocation; std::vector<Relocation> extraCopies; std::vector<uint32_t> observedArmRoots; bool hasRelocation=false,hasFrontier=false;
    std::string tracePath,hardwarePath,videoPath,frameDir;
    for(int i=3;i<argc;){
        const std::string opt=argv[i];
        if(opt=="--frontier"&&!hasFrontier&&i+1<argc){
            if(!read_native_frontier(rom,argv[i+1],profiled,error)){
                std::cerr<<"ERRO: fronteira: "<<error<<"\n";return 7;}
            hasFrontier=true;i+=2;
        }else if(opt=="--iwram-copy"&&!hasRelocation&&i+4<argc){
            try{
                auto parse=[](const char* text)->uint32_t{std::string v(text);size_t n=0;
                  const auto r=std::stoull(v,&n,0);if(n!=v.size()||r>0xFFFFFFFFull)throw std::out_of_range("number");return uint32_t(r);};
                relocation.src=parse(argv[i+1]);relocation.dst=parse(argv[i+2]);
                relocation.size=parse(argv[i+3]);relocation.entry=parse(argv[i+4]);
            }catch(const std::exception&){std::cerr<<"ERRO: numeros invalidos para --iwram-copy\n";return 7;}
            if((relocation.src&3u)||(relocation.dst&3u)||(relocation.size&3u)||(relocation.entry&3u)||
               relocation.src<0x08000000u||
               uint64_t(relocation.src-0x08000000u)+relocation.size>rom.bytes().size()||
               relocation.dst<0x03000000u||uint64_t(relocation.dst)+relocation.size>0x03008000ull||
               relocation.size<4u||relocation.size>0x8000u||
               relocation.entry<relocation.dst||
               uint64_t(relocation.entry)+4u>uint64_t(relocation.dst)+relocation.size){
                std::cerr<<"ERRO: mapeamento IWRAM fora de limites/alinhamento\n";return 7;
            }
            hasRelocation=true;i+=5;
        }else if((opt=="--iwram-extra" || opt=="--iwram-overlay") && hasRelocation && i+4<argc){
            Relocation extra;
            try{auto parse=[](const char* text)->uint32_t{std::string t(text);size_t n=0;
                const auto v=std::stoull(t,&n,0);if(n!=t.size()||v>0xFFFFFFFFull)throw std::out_of_range("number");return uint32_t(v);};
                extra.src=parse(argv[i+1]);extra.dst=parse(argv[i+2]);extra.size=parse(argv[i+3]);extra.entry=parse(argv[i+4]);extra.overlay=(opt=="--iwram-overlay");
            }catch(const std::exception&){std::cerr<<"ERRO: --iwram-extra parametros invalidos\n";return 7;}
            if(extraCopies.size()>=16u || (extra.src&3u)||(extra.dst&3u)||(extra.size&3u)||(extra.entry&3u) ||
                extra.src<0x08000000u || uint64_t(extra.src-0x08000000u)+extra.size>rom.bytes().size() ||
                extra.dst<0x03000000u || uint64_t(extra.dst)+extra.size>0x03008000ull ||
                extra.size<4u||extra.size>0x8000u || extra.entry<extra.dst || uint64_t(extra.entry)+4u>uint64_t(extra.dst)+extra.size){
                std::cerr<<"ERRO: mapeamento ARM extra fora de limites\n";return 7;}
            auto overlaps=[&](const Relocation& e){return extra.dst<uint64_t(e.dst)+e.size && e.dst<uint64_t(extra.dst)+extra.size;};
            if(!extra.overlay && (overlaps(relocation) || std::any_of(extraCopies.begin(),extraCopies.end(),overlaps))){
                std::cerr<<"ERRO: regioes ARM IWRAM sobrepostas\n";return 7;}
            extraCopies.push_back(extra);i+=5;
        }else if(opt=="--iwram-root" && hasRelocation && i+1<argc){
            uint32_t root=0;
            try{std::string t=argv[i+1];size_t n=0;const auto v=std::stoull(t,&n,0);
                if(n!=t.size()||v>0xFFFFFFFFull)throw std::out_of_range("root");
                root=uint32_t(v);
            }catch(const std::exception&){std::cerr<<"ERRO: root ARM invalido\n";return 7;}
            auto inside=[&](const Relocation& e){return root>=e.dst && uint64_t(root)+4u<=uint64_t(e.dst)+e.size;};
            if((root&3u)||(!inside(relocation)&&std::none_of(extraCopies.begin(),extraCopies.end(),inside))||observedArmRoots.size()>=128u){
                std::cerr<<"ERRO: root ARM IWRAM fora das regioes mapeadas\n";return 7;}
            observedArmRoots.push_back(root);i+=2;
        }else if(opt=="--iwram-irq-entry" && hasRelocation && !relocation.irqEntry && i+1<argc){
            try{std::string v=argv[i+1];size_t n=0;const auto raw=std::stoull(v,&n,0);
                if(n!=v.size()||raw>0xFFFFFFFFull)throw std::out_of_range("irq");
                relocation.irqEntry=uint32_t(raw);
            }catch(const std::exception&){std::cerr<<"ERRO: valor IRQ IWRAM invalido\n";return 7;}
            if((relocation.irqEntry&3u)||relocation.irqEntry<relocation.dst||
               uint64_t(relocation.irqEntry)+4u>uint64_t(relocation.dst)+relocation.size){
                std::cerr<<"ERRO: entrada IRQ fora da copia IWRAM\n";return 7;
            }
            i+=2;
        }else if(opt=="--hardware-log"&&hardwarePath.empty()&&i+1<argc){hardwarePath=argv[i+1];i+=2;}
        else if(opt=="--video-evidence"&&videoPath.empty()&&i+1<argc){videoPath=argv[i+1];i+=2;}
        else if(opt=="--frame-dir"&&frameDir.empty()&&i+1<argc){frameDir=argv[i+1];i+=2;}
        else if(i==3&&opt.rfind("--",0)!=0){tracePath=opt;++i;}
        else{std::cerr<<"ERRO: opcao invalida "<<opt<<"\n";return 7;}
    }
    const auto cfg = build_recursive_cfg(rom, entry, profiled);
    std::string report = build_report(cfg, entry);
    // ACQE has a Thumb->ARM handoff that constructs LR by reading pipelined PC:
    // 08039990 MOV r2,pc; 08039992 ADD r2,#5; 08039994 MOV lr,r2;
    // 08039996 BX r1. This is ROM evidence, not a claim of executed gameplay.
    if(rom.header().game_code=="ACQE" && rom.bytes().size()>0x39998u &&
       rom.read16(0x39990u)==0x467Au && rom.read16(0x39992u)==0x3205u &&
       rom.read16(0x39994u)==0x4696u && rom.read16(0x39996u)==0x4708u){
        const uint32_t expectedLink=0x08039990u+4u+5u;
        const uint32_t resumed=expectedLink&~1u;
        const bool present=std::any_of(cfg.blocks.begin(),cfg.blocks.end(),[&](const auto& b){
            return b.start==resumed && b.mode==GbaCodeMode::Thumb;});
        report+="\nACQE THUMB PC PIPELINE RETURN [ROM-VERIFIED]\n";
        report+="MOV r2,PC at 0x08039990: PC=0x08039994 (architectural PC+4)\n";
        report+="ADD r2,#5; MOV LR,r2; BX r1: expected LR="+hex8(expectedLink)+
                " and Thumb return="+hex8(resumed)+"\n";
        report+=std::string("Return root in profiled CFG: ")+(present?"YES":"NO")+"\n";
        report+="Does NOT infer correct PPU, gameplay, or later IWRAM code lifetimes.\n";
    }
    if(hasRelocation){
        // Audit the SAME runtime-address control-flow frontier as Builder,
        // separate from ROM block count and from gameplay code.
        std::vector<Relocation> mappings{relocation};
        mappings.insert(mappings.end(),extraCopies.begin(),extraCopies.end());
        for(const Relocation& region:mappings){
        // Each region retains its runtime address: no ROM CFG contamination.
        const auto& relocation=region;
        std::deque<uint32_t> pending{relocation.entry};if(relocation.irqEntry)pending.push_back(relocation.irqEntry);
        for(uint32_t root:observedArmRoots)if(root>=relocation.dst&&uint64_t(root)+4u<=uint64_t(relocation.dst)+relocation.size)pending.push_back(root);
        std::set<uint32_t> seen;
        size_t liftable=0,excluded=0;
        auto inside=[&](uint32_t a){return (a&3u)==0u&&a>=relocation.dst&&
            uint64_t(a)+4u<=uint64_t(relocation.dst)+relocation.size;};
        auto enqueue=[&](uint32_t a){if(inside(a)&&!seen.count(a))pending.push_back(a);};
        while(!pending.empty()&&seen.size()<8192u){
            const uint32_t addr=pending.front();pending.pop_front();
            if(!inside(addr)||!seen.insert(addr).second)continue;
            const uint32_t ins=rom.read32(size_t(relocation.src-0x08000000u)+addr-relocation.dst);
            if(!is_arm_instruction_liftable(ins)){++excluded;continue;}
            ++liftable;
            const unsigned cond=ins>>28;
            const bool branch=(ins&0x0E000000u)==0x0A000000u;
            const bool bx=(ins&0x0FFFFFF0u)==0x012FFF10u;
            const bool ldmPc=(ins&0x0E100000u)==0x08100000u&&(ins&0x8000u);
            const bool ldrPc=(ins&0x0C100000u)==0x04100000u&&((ins>>12)&15u)==15u;
            const bool dataPc=(ins&0x0C000000u)==0u&&((ins>>12)&15u)==15u&&
              ((ins>>21)&15u)!=8u&&((ins>>21)&15u)!=9u&&((ins>>21)&15u)!=10u&&((ins>>21)&15u)!=11u;
            if(branch){const int32_t raw=int32_t(ins&0xFFFFFFu);
                const int32_t sx=(raw&0x800000)?(raw|int32_t(0xFF000000u)):raw;
                enqueue(uint32_t(int64_t(addr)+8+int64_t(sx)*4));
                if(cond!=14u||(ins&0x01000000u))enqueue(addr+4u);
            }else if(bx||ldmPc||ldrPc||dataPc){if(cond!=14u)enqueue(addr+4u);}
            else enqueue(addr+4u);
        }
        std::ostringstream r;
        r<<"\nRELOCATED ARM IWRAM [address-aware, NOT added to ROM CFG]\n"
         <<"source_ROM="<<hex8(relocation.src)<<" RAM="<<hex8(relocation.dst)
         <<" length="<<relocation.size<<" entry="<<hex8(relocation.entry)
         <<" IRQ_entry="<<hex8(relocation.irqEntry)<<" overlay="<<(relocation.overlay?"YES":"NO")<<"\n"
         <<"Reachable lifted ARM instruction blocks="<<liftable
         <<" unsupported_or_unliftable="<<excluded<<"\n"
         <<"The emitted runtime must verify the live IWRAM instruction bytes.\n";
        r<<"Observed ARM entries="<<std::count_if(observedArmRoots.begin(),observedArmRoots.end(),[&](uint32_t root){return root>=relocation.dst&&uint64_t(root)+4u<=uint64_t(relocation.dst)+relocation.size;})<<"\n";
        report+=r.str();
        }
    }
    if(!frameDir.empty()){
        if(videoPath.empty()){
            std::cerr<<"ERRO: --frame-dir requires --video-evidence\n";return 8;
        }
        const auto frames=audit_gba_frame_manifest(rom.bytes(),frameDir,videoPath);
        if(!frames.valid){std::cerr<<frames.report<<"\n";return 8;}
        report+="\n"+frames.report;
    }
    if(!videoPath.empty()){
        const auto video=audit_video_evidence(rom.bytes(),videoPath);
        if(!video.valid){std::cerr<<"ERRO: video crossfeed: "<<video.report<<"\n";return 9;}
        std::cout<<video.report<<"\n";
    }
    if(!hardwarePath.empty()){
        const auto hw=audit_hardware_run(rom,hardwarePath);
        if(!hw.valid){std::cerr<<"ERRO: hardware crossfeed: "<<hw.explanation<<"\n";return 8;}
        report+=hw.explanation;
    }
    std::cout << report;
    if (argc >= 3) {
        std::ofstream f(argv[2]);
        if (!f) {
            std::cerr << "ERRO: nao foi possivel escrever " << argv[2] << "\n";
            return 5;
        }
        f << report;
    }
    if (!tracePath.empty()) {
        const auto observed=audit_dynamic_trace(rom,cfg,tracePath);
        std::cout<<"\n"<<observed.report;
        std::ofstream f(argv[2],std::ios::app);
        if(f)f<<"\n"<<observed.report;
        return observed.rejected?6:0;
    }
    return 0;
}
