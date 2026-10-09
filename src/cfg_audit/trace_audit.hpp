#pragma once
#include "cfg.hpp"
#include "gba_rom.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct GbaTraceEntry {
    uint32_t callsite=0, veneer=0, target=0;
    std::string category;
    uint32_t receiver_register=0xFFFFFFFFu,receiver=0,object_field=0,vtable=0,method_slot=0,evidence=0,write_serial=0;
};
struct GbaTraceAudit {
    std::vector<GbaTraceEntry> entries;
    size_t accepted=0, known=0, candidate=0, external=0, rejected=0;
    size_t distinct_callsites=0, polymorphic_callsites=0;
    size_t consistent_snapshots=0, ambiguous_snapshots=0, rejected_snapshots=0;
    size_t tracked_snapshots=0, memory_writes=0, lifecycle_rejected=0;
    std::string report;
};
uint64_t gba_trace_fingerprint(const GbaRom& rom);
std::string gba_trace_header(const GbaRom& rom);
GbaTraceAudit audit_dynamic_trace(const GbaRom& rom,const GbaCfgAnalysis& cfg,const std::filesystem::path& filename);

// Strict ROM-bound missing-block frontier, no implicit static proof.
bool read_native_frontier(const GbaRom& rom, const std::filesystem::path& file,
                          std::vector<GbaCodeEdge>& roots, std::string& error);
