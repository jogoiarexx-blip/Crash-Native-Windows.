#pragma once
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct VideoEvidenceAudit {
    bool valid = false;
    size_t samples = 0;
    size_t vram_transitions = 0;
    size_t palette_transitions = 0;
    size_t oam_transitions = 0;
    size_t register_transitions = 0;
    uint64_t last_vram_writes=0, last_dma3=0;
    std::string report;
};

// Pure ROM-bound diagnostic. These samples are hardware-state fingerprints,
// NOT rendered frames or independently verified emulator screenshots.
inline VideoEvidenceAudit audit_video_evidence(const std::vector<uint8_t>& rom,
                                                const std::string& path) {
    VideoEvidenceAudit result;
    std::ifstream file(path);
    if (!file) { result.report = "REJECT: cannot open video evidence file"; return result; }
    auto reject=[&](const std::string& reason) {
        result.report="REJECT: "+reason; result.valid=false; return result;
    };
    auto fields=[](const std::string& line) {
        std::istringstream tokens(line);
        std::map<std::string,std::string> items;
        std::string word;
        while(tokens>>word){
            const auto pos=word.find('=');
            if(pos==0||pos==std::string::npos||pos+1==word.size()||
               !items.emplace(word.substr(0,pos),word.substr(pos+1)).second)
                throw std::runtime_error("invalid or duplicate key");
        }
        return items;
    };
    auto get=[](const std::map<std::string,std::string>& items,const char* key)->uint64_t {
        const auto it=items.find(key);
        if(it==items.end())throw std::runtime_error(std::string("missing ")+key);
        size_t pos=0;
        const uint64_t value=std::stoull(it->second,&pos,0);
        if(pos!=it->second.size()||it->second[0]=='-'||it->second[0]=='+')
            throw std::runtime_error(std::string("invalid ")+key);
        return value;
    };
    try {
        std::string line;
        if(!std::getline(file,line)||line.rfind("GBA_VIDEO_V1 ",0)!=0)
            return reject("unsupported or missing GBA_VIDEO_V1 header");
        const auto header=fields(line.substr(13));
        if(header.size()!=2u)return reject("unexpected header fields");
        uint64_t expected=14695981039346656037ull;
        for(uint8_t b:rom){expected^=b;expected*=1099511628211ull;}
        if(get(header,"rom_fnv")!=expected||get(header,"rom_bytes")!=rom.size())
            return reject("ROM fingerprint or byte count mismatch");
        uint64_t prevVram=0,prevPal=0,prevOam=0,prevRegs=0;
        uint64_t writeVram=0,writePal=0,writeOam=0,writeIo=0,dma=0;
        constexpr uint64_t frameCycles=280896u,vblankStart=197120u;
        while(std::getline(file,line)) {
            if(line.empty())return reject("empty sample row");
            if(result.samples>=2048u)return reject("too many samples");
            const auto row=fields(line);
            if(row.size()!=13u)return reject("unexpected sample fields");
            const uint64_t n=get(row,"frame"),cycles=get(row,"cycles"),lineNo=get(row,"vcount");
            if(n!=uint64_t(result.samples)+1u||
               cycles!=vblankStart+(n-1u)*frameCycles||lineNo!=160u)
                return reject("non-monotonic or impossible VBlank timestamp");
            if(get(row,"dispcnt")>0xFFFFu)return reject("invalid DISPCNT");
            const uint64_t r=get(row,"regs"),v=get(row,"vram"),p=get(row,"palette"),o=get(row,"oam");
            const uint64_t wv=get(row,"vram_changes"),wp=get(row,"palette_changes"),
                           wo=get(row,"oam_changes"),wi=get(row,"video_io_changes"),d=get(row,"dma3");
            if(wv<writeVram||wp<writePal||wo<writeOam||wi<writeIo||d<dma)
                return reject("video mutation counters went backwards");
            if(result.samples){
                if(v!=prevVram)++result.vram_transitions;
                if(p!=prevPal)++result.palette_transitions;
                if(o!=prevOam)++result.oam_transitions;
                if(r!=prevRegs)++result.register_transitions;
                if(v!=prevVram&&wv==writeVram)return reject("VRAM hash changed without a counted write");
                if(p!=prevPal&&wp==writePal)return reject("palette hash changed without a counted write");
                if(o!=prevOam&&wo==writeOam)return reject("OAM hash changed without a counted write");
            }
            prevVram=v;prevPal=p;prevOam=o;prevRegs=r;
            writeVram=wv;writePal=wp;writeOam=wo;writeIo=wi;dma=d;
            ++result.samples;
        }
        if(!file.eof()||!result.samples)return reject("truncated or empty evidence");
        result.valid=true;result.last_vram_writes=writeVram;result.last_dma3=dma;
        std::ostringstream report;
        report<<"GBA_VIDEO_V1 ROM-bound audit PASS: samples="<<result.samples
              <<" VRAM transitions="<<result.vram_transitions
              <<" palette="<<result.palette_transitions
              <<" OAM="<<result.oam_transitions
              <<" video-registers="<<result.register_transitions
              <<" VRAM changed bytes="<<writeVram
              <<" DMA3 transfers="<<dma
              <<"; state only, NOT rendered frames or gameplay proof.";
        result.report=report.str();
        return result;
    }catch(const std::exception& e){return reject(e.what());}
}
