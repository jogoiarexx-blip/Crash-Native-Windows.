#pragma once
#include "gba_rom.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class GbaCodeMode : uint8_t { Arm = 0, Thumb = 1 };
enum class GbaTerminator : uint8_t {
    Boundary,
    DirectBranch,
    ConditionalBranch,
    DirectCall,
    Return,
    ResolvedIndirect,
    Indirect,
    External,
    Limit,
    Invalid
};

struct GbaCodeEdge {
    uint32_t address = 0;
    GbaCodeMode mode = GbaCodeMode::Arm;
};

struct GbaInstructionRecord {
    uint32_t address = 0;
    uint32_t raw = 0;
    uint8_t size = 0;
};

struct GbaBasicBlock {
    uint32_t start = 0;
    uint32_t end = 0; // exclusive
    GbaCodeMode mode = GbaCodeMode::Arm;
    GbaTerminator terminator = GbaTerminator::Invalid;
    bool indirect_resolved = false;
    bool compiler_epilogue_return = false;
    bool switch_table_resolved = false;
    bool indirect_call_veneer = false;
    size_t switch_table_entries = 0;
    std::vector<GbaInstructionRecord> instructions;
    std::vector<GbaCodeEdge> successors;
    std::vector<GbaCodeEdge> calls;
    std::vector<GbaCodeEdge> resolved_dynamic_calls;
};

struct GbaIndirectVeneerStat {
    uint32_t address = 0;
    uint8_t register_index = 0;
    size_t callers = 0;
};

// Evidence rooted in a PC-relative literal load from reached code. These
// are concrete ROM-resident function-pointer records, but are NOT proof of
// which virtual call site uses a particular method.
struct GbaRootedFunctionTable {
    uint32_t address = 0;
    uint32_t literal_reference = 0;
    size_t slots = 0;
    std::vector<GbaCodeEdge> methods;
};

// A constructor-store is an observed ROM vtable pointer written to an
// object field. It does not establish which runtime instance is used later.
struct GbaVtableInstall {
    uint32_t instruction = 0;
    uint32_t table_address = 0;
    uint16_t object_offset = 0;
    uint8_t base_register = 0;
};

// A candidate virtual call comes from two consecutive provenance-backed
// loads: object+field -> possible vptr, vptr+slot -> call-via register.
// Candidate table/method associations are advisory, NEVER CFG roots.
struct GbaVirtualCallPattern {
    uint32_t callsite = 0;
    uint32_t veneer = 0;
    uint16_t object_offset = 0;
    uint16_t slot_offset = 0;
    uint8_t call_register = 0;
    std::vector<uint32_t> candidate_tables;
    std::vector<GbaCodeEdge> candidate_methods;
    bool crosses_blocks = false; // origin survived a control-flow edge
};

// Direct caller -> function containing a constructor vptr store.
// An observed call does NOT prove which object instance receives the store.
struct GbaConstructorCaller {
    uint32_t callsite = 0;
    uint32_t callee_entry = 0;
    uint32_t constructor_store = 0;
    uint32_t table_address = 0;
    uint16_t object_offset = 0;
};

struct GbaCfgAnalysis {
    std::vector<GbaBasicBlock> blocks;
    std::vector<GbaCodeEdge> function_entries;
    // Execution-observed roots: not statically proven call targets.
    std::vector<GbaCodeEdge> profiled_roots;
    std::vector<GbaIndirectVeneerStat> indirect_veneer_stats;
    std::vector<GbaRootedFunctionTable> rooted_function_tables;
    std::vector<GbaVtableInstall> vtable_installs;
    std::vector<GbaVirtualCallPattern> virtual_call_patterns;
    std::vector<GbaConstructorCaller> constructor_callers;
    size_t crossblock_virtual_patterns = 0;
    size_t arm_blocks = 0;
    size_t thumb_blocks = 0;
    size_t decoded_instructions = 0;
    size_t direct_calls = 0;
    size_t direct_branches = 0;
    size_t conditional_branches = 0;
    size_t returns = 0;
    size_t resolved_indirects = 0;
    size_t unresolved_indirects = 0;
    size_t compiler_epilogue_returns = 0;
    size_t resolved_switch_tables = 0;
    size_t resolved_switch_targets = 0;
    size_t indirect_call_veneers = 0;
    size_t indirect_call_sites = 0;
    size_t resolved_dynamic_call_sites = 0;
    size_t resolved_dynamic_call_targets = 0;
    size_t rooted_table_slots = 0;
    size_t rooted_table_distinct_functions = 0;
    size_t external_exits = 0;
    size_t limit_blocks = 0;
    size_t arm_liftable_blocks = 0;
    size_t thumb_liftable_blocks = 0;
    size_t thumb_liftable_instructions = 0;
};

GbaCfgAnalysis build_recursive_cfg(const GbaRom& rom, uint32_t entry_target,
                                    const std::vector<GbaCodeEdge>& observed_roots = {});
bool is_arm_instruction_liftable(uint32_t instruction);
bool is_thumb_instruction_liftable(uint16_t instruction, uint16_t next_halfword = 0, bool has_next = false);
std::string cfg_to_text(const GbaCfgAnalysis& cfg);
std::string terminator_name(GbaTerminator t);
