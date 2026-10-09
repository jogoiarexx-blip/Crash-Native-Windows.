#include "cfg.hpp"
#include <algorithm>
#include <array>
#include <deque>
#include <iomanip>
#include <map>
#include <limits>
#include <set>
#include <sstream>
#include <tuple>

namespace {
constexpr uint32_t kRomBase = 0x08000000u;
constexpr size_t kMaxBlockInstructions = 4096;
constexpr size_t kMaxBlocks = 250000;

struct EdgeLess {
    bool operator()(const GbaCodeEdge& a, const GbaCodeEdge& b) const {
        return std::tie(a.address, a.mode) < std::tie(b.address, b.mode);
    }
};

uint32_t align_address(uint32_t address, GbaCodeMode mode) {
    return mode == GbaCodeMode::Thumb ? (address & ~1u) : (address & ~3u);
}

bool in_rom(const GbaRom& rom, uint32_t address, size_t bytes) {
    if (address < kRomBase) return false;
    const uint64_t off = uint64_t(address - kRomBase);
    return off + bytes <= rom.bytes().size();
}

int32_t sign_extend(uint32_t value, unsigned bits) {
    const uint32_t sign = 1u << (bits - 1u);
    const uint32_t mask = (1u << bits) - 1u;
    value &= mask;
    return static_cast<int32_t>((value ^ sign) - sign);
}

uint32_t ror32(uint32_t value, unsigned shift) {
    shift &= 31u;
    if (!shift) return value;
    return (value >> shift) | (value << (32u - shift));
}

void add_edge(std::vector<GbaCodeEdge>& edges, uint32_t address, GbaCodeMode mode, const GbaRom& rom) {
    address = align_address(address, mode);
    if (!in_rom(rom, address, mode == GbaCodeMode::Thumb ? 2u : 4u)) return;
    const GbaCodeEdge e{address, mode};
    if (std::find_if(edges.begin(), edges.end(), [&](const GbaCodeEdge& x){return x.address==e.address && x.mode==e.mode;}) == edges.end()) edges.push_back(e);
}

bool is_thumb_bl_pair(uint16_t h1, uint16_t h2) {
    return (h1 & 0xF800u) == 0xF000u && (h2 & 0xF800u) == 0xF800u;
}

bool is_thumb_bx(uint16_t h) {
    return (h & 0xFF87u) == 0x4700u;
}

bool is_thumb_pop_pc(uint16_t h) {
    return (h & 0xFF00u) == 0xBD00u;
}

bool is_thumb_high_reg(uint16_t h) {
    return (h & 0xFC00u) == 0x4400u;
}

bool is_thumb_supported_single(uint16_t h) {
    // Shift immediate, add/sub, immediate ALU.
    if ((h & 0xE000u) == 0x0000u) return true;
    if ((h & 0xE000u) == 0x2000u) return true;
    // ALU ops, high-register ops/BX.
    if ((h & 0xFC00u) == 0x4000u) return true;
    if ((h & 0xFC00u) == 0x4400u) return true;
    // PC-relative load.
    if ((h & 0xF800u) == 0x4800u) return true;
    // Register-offset and sign-extended loads/stores.
    if ((h & 0xF000u) == 0x5000u) return true;
    // Immediate load/store word/byte and halfword.
    if ((h & 0xE000u) == 0x6000u) return true;
    if ((h & 0xF000u) == 0x8000u) return true;
    // SP-relative load/store.
    if ((h & 0xF000u) == 0x9000u) return true;
    // ADD Rd, PC/SP, #imm.
    if ((h & 0xF000u) == 0xA000u) return true;
    // ADD/SUB SP immediate and PUSH/POP.
    if ((h & 0xFF00u) == 0xB000u) return true;
    if ((h & 0xF600u) == 0xB400u) return true;
    // STMIA/LDMIA.
    if ((h & 0xF000u) == 0xC000u) return true;
    // Conditional branch / SWI.
    if ((h & 0xF000u) == 0xD000u) return (h & 0x0F00u) != 0x0E00u;
    // Unconditional B.
    if ((h & 0xF800u) == 0xE000u) return true;
    return false;
}

struct DecodeContext {
    std::array<bool, 16> known{};
    std::array<uint32_t, 16> value{};
    void clear_reg(unsigned r) { if (r < known.size()) known[r] = false; }
    void set_reg(unsigned r, uint32_t v) { if (r < known.size()) { known[r] = true; value[r] = v; } }
};

void track_arm_constant(const GbaRom& rom, uint32_t address, uint32_t w, DecodeContext& ctx) {
    const uint32_t cond = w >> 28;
    if (cond != 0xEu) {
        // Conditional writes cannot be trusted for constant propagation.
        if ((w & 0x0C000000u) == 0x00000000u) {
            const uint32_t opcode = (w >> 21) & 0xFu;
            if (opcode < 8u || opcode > 11u) ctx.clear_reg((w >> 12) & 0xFu);
        } else if ((w & 0x0C100000u) == 0x04100000u) ctx.clear_reg((w >> 12) & 0xFu);
        return;
    }

    // LDR Rd, [PC, #+/-imm12]
    if ((w & 0x0F7F0000u) == 0x051F0000u) {
        const unsigned rd = (w >> 12) & 0xFu;
        const uint32_t imm = w & 0xFFFu;
        const uint32_t pc = address + 8u;
        const uint32_t lit = (w & (1u << 23)) ? pc + imm : pc - imm;
        if ((w & (1u << 22)) == 0 && in_rom(rom, lit, 4)) ctx.set_reg(rd, rom.read32(lit - kRomBase));
        else ctx.clear_reg(rd);
        return;
    }

    // MOV/MVN immediate.
    if ((w & 0x0E000000u) == 0x02000000u && ((w >> 25) & 1u)) {
        const unsigned opcode = (w >> 21) & 0xFu;
        const unsigned rd = (w >> 12) & 0xFu;
        const uint32_t imm = ror32(w & 0xFFu, ((w >> 8) & 0xFu) * 2u);
        if (opcode == 13u) ctx.set_reg(rd, imm);
        else if (opcode == 15u) ctx.set_reg(rd, ~imm);
        else if (opcode == 4u || opcode == 2u || opcode == 12u || opcode == 14u || opcode == 0u) {
            const unsigned rn = (w >> 16) & 0xFu;
            uint32_t base = 0;
            bool ok = false;
            if (rn == 15u) { base = address + 8u; ok = true; }
            else if (ctx.known[rn]) { base = ctx.value[rn]; ok = true; }
            if (!ok) { ctx.clear_reg(rd); return; }
            if (opcode == 4u) ctx.set_reg(rd, base + imm);
            else if (opcode == 2u) ctx.set_reg(rd, base - imm);
            else if (opcode == 12u) ctx.set_reg(rd, base | imm);
            else if (opcode == 14u) ctx.set_reg(rd, base & ~imm);
            else ctx.set_reg(rd, base & imm);
        } else if (opcode < 8u || opcode > 11u) ctx.clear_reg(rd);
        return;
    }

    // Conservatively invalidate destination of ordinary data-processing/load instructions.
    if ((w & 0x0C000000u) == 0x00000000u) {
        const uint32_t opcode = (w >> 21) & 0xFu;
        if (opcode < 8u || opcode > 11u) ctx.clear_reg((w >> 12) & 0xFu);
    } else if ((w & 0x0C100000u) == 0x04100000u) {
        ctx.clear_reg((w >> 12) & 0xFu);
    }
}

void track_thumb_constant(const GbaRom& rom, uint32_t address, uint16_t h, DecodeContext& ctx) {
    // LDR Rd, [PC,#imm]
    if ((h & 0xF800u) == 0x4800u) {
        const unsigned rd = (h >> 8) & 7u;
        const uint32_t lit = ((address + 4u) & ~3u) + uint32_t(h & 0xFFu) * 4u;
        if (in_rom(rom, lit, 4)) ctx.set_reg(rd, rom.read32(lit - kRomBase)); else ctx.clear_reg(rd);
        return;
    }
    // MOV Rd,#imm
    if ((h & 0xF800u) == 0x2000u) { ctx.set_reg((h >> 8) & 7u, h & 0xFFu); return; }
    // ADD/SUB Rd,#imm8
    if ((h & 0xF800u) == 0x3000u || (h & 0xF800u) == 0x3800u) {
        const unsigned rd = (h >> 8) & 7u;
        if (ctx.known[rd]) ctx.value[rd] = (h & 0xF800u) == 0x3000u ? ctx.value[rd] + (h & 0xFFu) : ctx.value[rd] - (h & 0xFFu);
        return;
    }
    // ADD Rd,PC/SP,#imm8*4
    if ((h & 0xF000u) == 0xA000u) {
        const unsigned rd = (h >> 8) & 7u;
        const uint32_t base = (h & 0x0800u) ? (ctx.known[13] ? ctx.value[13] : 0u) : ((address + 4u) & ~3u);
        if ((h & 0x0800u) && !ctx.known[13]) ctx.clear_reg(rd); else ctx.set_reg(rd, base + uint32_t(h & 0xFFu) * 4u);
        return;
    }
    // High-register MOV.
    if (is_thumb_high_reg(h) && ((h >> 8) & 3u) == 2u) {
        const unsigned rd = (h & 7u) | ((h >> 4) & 8u);
        const unsigned rs = (h >> 3) & 15u;
        if (ctx.known[rs]) ctx.set_reg(rd, ctx.value[rs]); else ctx.clear_reg(rd);
        return;
    }
    // Shift-immediate constant propagation.
    if ((h & 0xF800u) == 0x0000u || (h & 0xF800u) == 0x0800u || (h & 0xF800u) == 0x1000u) {
        const unsigned rd = h & 7u, rs = (h >> 3) & 7u, amount = (h >> 6) & 0x1Fu;
        if (!ctx.known[rs]) { ctx.clear_reg(rd); return; }
        uint32_t v = ctx.value[rs];
        if ((h & 0xF800u) == 0x0000u) ctx.set_reg(rd, amount ? v << amount : v);
        else if ((h & 0xF800u) == 0x0800u) ctx.set_reg(rd, amount ? v >> amount : 0u);
        else ctx.set_reg(rd, amount ? uint32_t(int32_t(v) >> amount) : (v & 0x80000000u ? 0xFFFFFFFFu : 0u));
        return;
    }
    // ADD/SUB register/immediate-3.
    if ((h & 0xF800u) == 0x1800u) {
        const bool immediate = (h & 0x0400u) != 0, sub = (h & 0x0200u) != 0;
        const unsigned op = (h >> 6) & 7u, rs = (h >> 3) & 7u, rd = h & 7u;
        if (!ctx.known[rs] || (!immediate && !ctx.known[op])) { ctx.clear_reg(rd); return; }
        const uint32_t rhs = immediate ? op : ctx.value[op];
        ctx.set_reg(rd, sub ? ctx.value[rs] - rhs : ctx.value[rs] + rhs);
        return;
    }
    // High-register ADD/MOV.
    if (is_thumb_high_reg(h)) {
        const unsigned op = (h >> 8) & 3u;
        const unsigned rd = (h & 7u) | ((h >> 4) & 8u), rs = (h >> 3) & 15u;
        if (op == 0u && rd != 15u) { if (ctx.known[rd] && ctx.known[rs]) ctx.set_reg(rd, ctx.value[rd] + ctx.value[rs]); else ctx.clear_reg(rd); return; }
        if (op == 2u && rd != 15u) { if (ctx.known[rs]) ctx.set_reg(rd, ctx.value[rs]); else ctx.clear_reg(rd); return; }
    }
    // Static ROM pointer dereferences. These recover direct targets loaded from
    // ROM-resident function tables while staying conservative for RAM/vtables.
    if ((h & 0xFE00u) == 0x5800u) {
        const unsigned ro = (h >> 6) & 7u, rb = (h >> 3) & 7u, rd = h & 7u;
        if (ctx.known[rb] && ctx.known[ro]) { const uint32_t a = ctx.value[rb] + ctx.value[ro]; if (in_rom(rom,a,4)) ctx.set_reg(rd,rom.read32(a-kRomBase)); else ctx.clear_reg(rd); } else ctx.clear_reg(rd);
        return;
    }
    if ((h & 0xF800u) == 0x6800u) {
        const unsigned imm = ((h >> 6) & 0x1Fu) * 4u, rb = (h >> 3) & 7u, rd = h & 7u;
        if (ctx.known[rb]) { const uint32_t a = ctx.value[rb] + imm; if (in_rom(rom,a,4)) ctx.set_reg(rd,rom.read32(a-kRomBase)); else ctx.clear_reg(rd); } else ctx.clear_reg(rd);
        return;
    }

    // Stack, bulk-memory operations and SWI invalidate any register that
    // cannot be proven constant. Without this, constants leak across POP/LDM
    // and cause false positive targets on an indirect call.
    if ((h & 0xFE00u) == 0xBC00u) {
        for (unsigned r = 0; r < 8; ++r) if (h & (1u << r)) ctx.clear_reg(r);
        ctx.clear_reg(13);
        return;
    }
    if ((h & 0xFE00u) == 0xB400u || (h & 0xFF00u) == 0xB000u) {
        ctx.clear_reg(13);
        return;
    }
    if ((h & 0xF000u) == 0xC000u) {
        const unsigned base = (h >> 8) & 7u;
        if (h & 0x0800u) for (unsigned r = 0; r < 8; ++r) if (h & (1u << r)) ctx.clear_reg(r);
        ctx.clear_reg(base);
        return;
    }
    if ((h & 0xFF00u) == 0xDF00u) {
        for (unsigned r : {0u, 1u, 2u, 3u, 12u, 14u}) ctx.clear_reg(r);
        return;
    }
    // Invalidate obvious low-register destinations.
    if ((h & 0xE000u) == 0x0000u) ctx.clear_reg(h & 7u);
    else if ((h & 0xE000u) == 0x2000u && (h & 0xF800u) != 0x2800u) ctx.clear_reg((h >> 8) & 7u);
    else if ((h & 0xFC00u) == 0x4000u) ctx.clear_reg(h & 7u);
    else if ((h & 0xF800u) == 0x6800u || (h & 0xF800u) == 0x7800u || (h & 0xF800u) == 0x8800u || (h & 0xF000u) == 0x9000u) ctx.clear_reg(h & 7u);
}

int thumb_call_veneer_register(const GbaRom& rom, uint32_t target) {
    target &= ~1u;
    if (!in_rom(rom, target, 2)) return -1;
    const uint16_t h = rom.read16(target - kRomBase);
    if (!is_thumb_bx(h)) return -1;
    const unsigned rm = (h >> 3) & 0xFu;
    return rm <= 5u ? static_cast<int>(rm) : -1;
}

bool thumb_pop_contains_reg(uint16_t h, unsigned reg) {
    if ((h & 0xFE00u) != 0xBC00u || reg >= 8u) return false;
    return (h & (1u << reg)) != 0;
}

bool resolve_thumb_switch_table(const GbaRom& rom, const std::vector<GbaInstructionRecord>& ins, std::vector<GbaCodeEdge>& out) {
    if (ins.size() < 5) return false;
    const uint16_t a = static_cast<uint16_t>(ins[ins.size()-5].raw);
    const uint16_t b = static_cast<uint16_t>(ins[ins.size()-4].raw);
    const uint16_t c = static_cast<uint16_t>(ins[ins.size()-3].raw);
    const uint16_t d = static_cast<uint16_t>(ins[ins.size()-2].raw);
    const uint16_t e = static_cast<uint16_t>(ins[ins.size()-1].raw);

    // Canonical agbcc switch veneer:
    //   LSL index,index,#2
    //   LDR base,[PC,#literal]
    //   ADD addr,index,base
    //   LDR target,[addr,#0]
    //   MOV PC,target
    if ((a & 0xF800u) != 0x0000u || ((a >> 6) & 0x1Fu) != 2u) return false;
    const unsigned indexDst = a & 7u;
    const unsigned indexSrc = (a >> 3) & 7u;
    if (indexDst != indexSrc) return false;
    if ((b & 0xF800u) != 0x4800u) return false;
    const unsigned baseReg = (b >> 8) & 7u;
    if ((c & 0xFE00u) != 0x1800u) return false;
    const unsigned addRn = (c >> 6) & 7u;
    const unsigned addRs = (c >> 3) & 7u;
    const unsigned addrReg = c & 7u;
    if (!((addRn == indexDst && addRs == baseReg) || (addRn == baseReg && addRs == indexDst))) return false;
    if ((d & 0xF800u) != 0x6800u || ((d >> 6) & 0x1Fu) != 0u) return false;
    const unsigned loadBase = (d >> 3) & 7u;
    const unsigned targetReg = d & 7u;
    if (loadBase != addrReg) return false;
    if ((e & 0xFC00u) != 0x4400u || ((e >> 8) & 3u) != 2u) return false;
    const unsigned pcDst = (e & 7u) | ((e >> 4) & 8u);
    const unsigned pcSrc = (e >> 3) & 15u;
    if (pcDst != 15u || pcSrc != targetReg) return false;

    const uint32_t ldrAddr = ins[ins.size()-4].address;
    const uint32_t literal = ((ldrAddr + 4u) & ~3u) + uint32_t(b & 0xFFu) * 4u;
    if (!in_rom(rom, literal, 4)) return false;
    const uint32_t table = rom.read32(literal - kRomBase);
    if (!in_rom(rom, table, 4)) return false;

    std::vector<GbaCodeEdge> candidates;
    constexpr size_t kMaxSwitchEntries = 256;
    for (size_t i = 0; i < kMaxSwitchEntries; ++i) {
        const uint32_t slot = table + static_cast<uint32_t>(i * 4u);
        if (!in_rom(rom, slot, 4)) break;
        uint32_t target = rom.read32(slot - kRomBase);
        // MOV PC in Thumb remains in Thumb state; gcc tables therefore carry
        // aligned code addresses rather than BX-style bit-0 tags.
        target &= ~1u;
        if (!in_rom(rom, target, 2) || (target & 1u) != 0u) break;
        candidates.push_back({target, GbaCodeMode::Thumb});
    }
    if (candidates.size() < 2) return false;
    for (const auto& edge : candidates) add_edge(out, edge.address, edge.mode, rom);
    return !out.empty();
}

GbaBasicBlock decode_block(const GbaRom& rom, GbaCodeEdge root, const std::set<GbaCodeEdge, EdgeLess>* boundaries) {
    GbaBasicBlock block;
    block.start = align_address(root.address, root.mode);
    block.end = block.start;
    block.mode = root.mode;
    DecodeContext ctx;
    uint32_t address = block.start;

    for (size_t count = 0; count < kMaxBlockInstructions; ++count) {
        if (boundaries && address != block.start && boundaries->count(GbaCodeEdge{address, block.mode})) {
            add_edge(block.successors, address, block.mode, rom);
            block.end = address;
            block.terminator = GbaTerminator::Boundary;
            return block;
        }

        if (block.mode == GbaCodeMode::Thumb) {
            if (!in_rom(rom, address, 2)) { block.terminator=GbaTerminator::Invalid; block.end=address; return block; }
            const uint16_t h = rom.read16(address - kRomBase);
            const uint32_t next = address + 2u;
            block.instructions.push_back({address, h, 2});

            if ((h & 0xF800u) == 0xF000u && in_rom(rom, address + 2u, 2)) {
                const uint16_t h2 = rom.read16(address + 2u - kRomBase);
                if (is_thumb_bl_pair(h, h2)) {
                    block.instructions.push_back({address + 2u, h2, 2});
                    const int32_t high = sign_extend(h & 0x7FFu, 11) * 4096;
                    const int32_t low = int32_t(h2 & 0x7FFu) * 2;
                    const uint32_t target = static_cast<uint32_t>(int64_t(address) + 4 + high + low);
                    add_edge(block.calls, target, GbaCodeMode::Thumb, rom);
                    const int veneerReg = thumb_call_veneer_register(rom, target);
                    if (veneerReg >= 0 && ctx.known[static_cast<unsigned>(veneerReg)]) {
                        const uint32_t dyn = ctx.value[static_cast<unsigned>(veneerReg)];
                        const GbaCodeMode dynMode = (dyn & 1u) ? GbaCodeMode::Thumb : GbaCodeMode::Arm;
                        add_edge(block.resolved_dynamic_calls, dyn, dynMode, rom);
                    }
                    add_edge(block.successors, address + 4u, GbaCodeMode::Thumb, rom);
                    block.end = address + 4u;
                    block.terminator = GbaTerminator::DirectCall;
                    return block;
                }
            }

            if ((h & 0xF800u) == 0xE000u) {
                const uint32_t target = static_cast<uint32_t>(int64_t(address) + 4 + int64_t(sign_extend(h & 0x7FFu, 11)) * 2);
                add_edge(block.successors, target, GbaCodeMode::Thumb, rom);
                block.end = next;
                block.terminator = GbaTerminator::DirectBranch;
                return block;
            }
            if ((h & 0xF000u) == 0xD000u && (h & 0x0F00u) != 0x0E00u && (h & 0x0F00u) != 0x0F00u) {
                const uint32_t target = static_cast<uint32_t>(int64_t(address) + 4 + int64_t(sign_extend(h & 0xFFu, 8)) * 2);
                add_edge(block.successors, target, GbaCodeMode::Thumb, rom);
                add_edge(block.successors, next, GbaCodeMode::Thumb, rom);
                block.end = next;
                block.terminator = GbaTerminator::ConditionalBranch;
                return block;
            }
            if (is_thumb_bx(h)) {
                const unsigned rm = (h >> 3) & 0xFu;
                const bool poppedReturn = block.instructions.size() >= 2 &&
                    thumb_pop_contains_reg(static_cast<uint16_t>(block.instructions[block.instructions.size()-2].raw), rm);
                if (rm == 14u || poppedReturn) {
                    block.terminator = GbaTerminator::Return;
                    block.compiler_epilogue_return = poppedReturn;
                } else if (ctx.known[rm]) {
                    const uint32_t target = ctx.value[rm];
                    const GbaCodeMode mode = (target & 1u) ? GbaCodeMode::Thumb : GbaCodeMode::Arm;
                    add_edge(block.successors, target, mode, rom);
                    block.indirect_resolved = !block.successors.empty();
                    block.terminator = block.indirect_resolved ? GbaTerminator::ResolvedIndirect : GbaTerminator::External;
                } else {
                    block.terminator = GbaTerminator::Indirect;
                    block.indirect_call_veneer = block.instructions.size() == 1 && rm <= 5u;
                }
                block.end = next;
                return block;
            }
            if (is_thumb_pop_pc(h)) {
                block.end = next;
                block.terminator = GbaTerminator::Return;
                return block;
            }
            if (is_thumb_high_reg(h)) {
                const unsigned op = (h >> 8) & 3u;
                const unsigned rd = (h & 7u) | ((h >> 4) & 8u);
                const unsigned rs = (h >> 3) & 15u;
                if ((op == 0u || op == 2u) && rd == 15u) {
                    if (op == 2u && rs == 14u) block.terminator = GbaTerminator::Return;
                    else if (ctx.known[rs]) {
                        const uint32_t target = ctx.value[rs];
                        const GbaCodeMode mode = (target & 1u) ? GbaCodeMode::Thumb : GbaCodeMode::Arm;
                        add_edge(block.successors, target, mode, rom);
                        block.indirect_resolved = !block.successors.empty();
                        block.terminator = block.indirect_resolved ? GbaTerminator::ResolvedIndirect : GbaTerminator::External;
                    } else if (op == 2u && resolve_thumb_switch_table(rom, block.instructions, block.successors)) {
                        block.indirect_resolved = true;
                        block.switch_table_resolved = true;
                        block.switch_table_entries = block.successors.size();
                        block.terminator = GbaTerminator::ResolvedIndirect;
                    } else block.terminator = GbaTerminator::Indirect;
                    block.end = next;
                    return block;
                }
            }

            track_thumb_constant(rom, address, h, ctx);
            address = next;
        } else {
            if (!in_rom(rom, address, 4)) { block.terminator=GbaTerminator::Invalid; block.end=address; return block; }
            const uint32_t w = rom.read32(address - kRomBase);
            const uint32_t next = address + 4u;
            const uint32_t cond = w >> 28;
            block.instructions.push_back({address, w, 4});

            if ((w & 0x0E000000u) == 0x0A000000u && cond != 0xFu) {
                const uint32_t target = static_cast<uint32_t>(int64_t(address) + 8 + int64_t(sign_extend(w & 0x00FFFFFFu, 24)) * 4);
                if (w & 0x01000000u) {
                    add_edge(block.calls, target, GbaCodeMode::Arm, rom);
                    add_edge(block.successors, next, GbaCodeMode::Arm, rom);
                    block.terminator = GbaTerminator::DirectCall;
                } else if (cond == 0xEu) {
                    add_edge(block.successors, target, GbaCodeMode::Arm, rom);
                    block.terminator = in_rom(rom,target,4) ? GbaTerminator::DirectBranch : GbaTerminator::External;
                } else {
                    add_edge(block.successors, target, GbaCodeMode::Arm, rom);
                    add_edge(block.successors, next, GbaCodeMode::Arm, rom);
                    block.terminator = GbaTerminator::ConditionalBranch;
                }
                block.end = next;
                return block;
            }
            if ((w & 0x0FFFFFF0u) == 0x012FFF10u) {
                const unsigned rm = w & 0xFu;
                if (rm == 14u) block.terminator = GbaTerminator::Return;
                else if (ctx.known[rm]) {
                    const uint32_t target = ctx.value[rm];
                    const GbaCodeMode mode = (target & 1u) ? GbaCodeMode::Thumb : GbaCodeMode::Arm;
                    add_edge(block.successors, target, mode, rom);
                    block.indirect_resolved = !block.successors.empty();
                    block.terminator = block.indirect_resolved ? GbaTerminator::ResolvedIndirect : GbaTerminator::External;
                } else block.terminator = GbaTerminator::Indirect;
                block.end = next;
                return block;
            }
            // LDM with PC is an indirect return/jump.
            if ((w & 0x0E100000u) == 0x08100000u && (w & (1u << 15))) {
                block.end = next;
                block.terminator = ((w >> 16) & 0xFu) == 13u ? GbaTerminator::Return : GbaTerminator::Indirect;
                return block;
            }
            // LDR PC, [...]
            if ((w & 0x0C100000u) == 0x04100000u && ((w >> 12) & 0xFu) == 15u) {
                block.end = next;
                block.terminator = GbaTerminator::Indirect;
                return block;
            }
            // Data processing writes PC. MOV PC,LR is the standard ARM return.
            if ((w & 0x0C000000u) == 0 && ((w >> 12) & 0xFu) == 15u) {
                const unsigned opcode = (w >> 21) & 0xFu;
                if (opcode < 8u || opcode > 11u) {
                    if (opcode == 13u && (w & (1u << 25)) == 0 && (w & 0xFu) == 14u) block.terminator = GbaTerminator::Return;
                    else block.terminator = GbaTerminator::Indirect;
                    block.end = next;
                    return block;
                }
            }

            track_arm_constant(rom, address, w, ctx);
            address = next;
        }
    }
    block.end = address;
    block.terminator = GbaTerminator::Limit;
    return block;
}


// Inter-basic-block must-constant analysis. A value is known only if every
// reached predecessor agrees. Direct calls destroy caller-saved registers;
// function entrypoints start completely unknown and are NEVER specialised by
// their call sites (which avoids unsound interprocedural guesses).
void simulate_context(const GbaRom& rom, const GbaBasicBlock& block,
                      DecodeContext initial, DecodeContext& exit,
                      DecodeContext* before_call = nullptr) {
    for (size_t i = 0; i < block.instructions.size(); ++i) {
        const auto& ins = block.instructions[i];
        if (block.mode == GbaCodeMode::Thumb) {
            const auto h = static_cast<uint16_t>(ins.raw);
            if (i + 1 < block.instructions.size() &&
                is_thumb_bl_pair(h, static_cast<uint16_t>(block.instructions[i+1].raw))) {
                if (before_call) *before_call = initial;
                for (unsigned r : {0u, 1u, 2u, 3u, 12u, 14u}) initial.clear_reg(r);
                ++i;
            } else {
                track_thumb_constant(rom, ins.address, h, initial);
            }
        } else {
            const uint32_t w = ins.raw;
            if ((w & 0x0F000000u) == 0x0B000000u) {
                if (before_call) *before_call = initial;
                for (unsigned r : {0u, 1u, 2u, 3u, 12u, 14u}) initial.clear_reg(r);
            } else track_arm_constant(rom, ins.address, w, initial);
        }
    }
    exit = initial;
}

bool join_known(DecodeContext& prior, const DecodeContext& incoming) {
    bool changed = false;
    for (size_t r=0; r<prior.known.size(); ++r) {
        if (prior.known[r] && (!incoming.known[r] || prior.value[r] != incoming.value[r])) {
            prior.known[r] = false;
            changed = true;
        }
    }
    return changed;
}

std::map<GbaCodeEdge, DecodeContext, EdgeLess> must_constant_contexts(
    const GbaRom& rom, const std::vector<GbaBasicBlock>& blocks,
    const std::set<GbaCodeEdge,EdgeLess>& functions) {
    std::map<GbaCodeEdge, const GbaBasicBlock*, EdgeLess> lookup;
    for (const auto& b : blocks) lookup[{b.start,b.mode}] = &b;
    std::map<GbaCodeEdge, DecodeContext, EdgeLess> input;
    std::deque<GbaCodeEdge> pending;
    std::set<GbaCodeEdge, EdgeLess> queued;
    for (const auto& e : functions) if (lookup.count(e)) {
        input[e] = DecodeContext{};
        pending.push_back(e); queued.insert(e);
    }
    size_t budget=0;
    while (!pending.empty() && ++budget <= blocks.size()*128u) {
        const auto cur = pending.front(); pending.pop_front(); queued.erase(cur);
        const auto found=lookup.find(cur);
        if (found==lookup.end()) continue;
        const auto& b=*found->second;
        DecodeContext output;
        simulate_context(rom,b,input.at(cur),output);
        // A resolved switch has an unknown dynamic index, and its target
        // may re-enter at multiple positions: never propagate registers.
        if (b.switch_table_resolved) output=DecodeContext{};
        for (const auto& target : b.successors) {
            if (!lookup.count(target)) continue;
            bool changed = false;
            auto old=input.find(target);
            if (old==input.end()) { input[target]=output; changed=true; }
            else changed=join_known(old->second,output);
            if (changed && !queued.count(target)) {pending.push_back(target);queued.insert(target);}
        }
    }
    return input;
}

// Only the exact veneer BL target and a must-constant argument can become a
// confirmed dynamic target. RAM-backed object vtables remain unresolved.
void resolve_must_constant_calls(const GbaRom& rom, std::vector<GbaBasicBlock>& blocks,
                                 const std::set<GbaCodeEdge,EdgeLess>& functions) {
    const auto contexts=must_constant_contexts(rom,blocks,functions);
    for (auto& b : blocks) {
        if (b.terminator != GbaTerminator::DirectCall || b.calls.empty()) continue;
        const auto it=contexts.find({b.start,b.mode});
        if (it==contexts.end()) continue;
        DecodeContext output, before;
        simulate_context(rom,b,it->second,output,&before);
        for (const auto& e : b.calls) {
            const int reg=thumb_call_veneer_register(rom,e.address);
            if (reg<0 || !before.known[static_cast<unsigned>(reg)]) continue;
            const uint32_t value=before.value[static_cast<unsigned>(reg)];
            const auto mode=(value&1u)?GbaCodeMode::Thumb:GbaCodeMode::Arm;
            if (!in_rom(rom, align_address(value,mode),mode==GbaCodeMode::Thumb?2u:4u)) continue;
            add_edge(b.resolved_dynamic_calls,value,mode,rom);
        }
    }
}

// GBA-era C++ vtables use two zero header words followed by pairs
// {this adjustment, Thumb function pointer}. Require THREE method records
// and a confirmed PC-relative LDR of this exact ROM address. This avoids
// indiscriminately treating every odd 0x08xxxxxx data word as executable.
// These are table-rooted code references, not resolved dynamic callsites.
std::map<uint32_t, GbaRootedFunctionTable> find_rooted_tables(
    const GbaRom& rom, const std::vector<GbaBasicBlock>& blocks) {
    std::map<uint32_t, GbaRootedFunctionTable> found;
    auto method_at=[&](uint32_t addr, GbaCodeEdge& edge) {
        if (!in_rom(rom,addr,8)) return false;
        const uint32_t w0=rom.read32(addr-kRomBase);
        const uint32_t ptr=rom.read32(addr-kRomBase+4u);
        if (w0!=0u || (ptr&1u)==0u || !in_rom(rom,ptr&~1u,2u)) return false;
        // Functions in these vtables must begin with an actual legal Thumb
        // instruction; the full liftability check happens downstream.
        const auto h=rom.read16((ptr&~1u)-kRomBase);
        if (!is_thumb_supported_single(h) && (h&0xF800u)!=0xF000u) return false;
        edge={ptr&~1u,GbaCodeMode::Thumb};
        return true;
    };
    for (const auto& block:blocks) {
        if (block.mode!=GbaCodeMode::Thumb) continue;
        for (const auto& ins:block.instructions) {
            const uint16_t h=static_cast<uint16_t>(ins.raw);
            if ((h&0xF800u)!=0x4800u) continue;
            const uint32_t literal=((ins.address+4u)&~3u)+uint32_t(h&0xFFu)*4u;
            if (!in_rom(rom,literal,4)) continue;
            const uint32_t value=rom.read32(literal-kRomBase);
            for (uint32_t shift:{0u,4u,8u}) {
                if (value<kRomBase+shift) continue;
                const uint32_t start=value-shift;
                if ((start&3u)!=0u || !in_rom(rom,start,32)) continue;
                if (found.count(start)) break;
                if (rom.read32(start-kRomBase)!=0u || rom.read32(start-kRomBase+4u)!=0u) continue;
                GbaCodeEdge a,b,c;
                if (!method_at(start+8u,a)||!method_at(start+16u,b)||!method_at(start+24u,c)) continue;
                GbaRootedFunctionTable t;
                t.address=start; t.literal_reference=ins.address;
                for (uint32_t i=0;i<128u;++i) {
                    GbaCodeEdge e;
                    if (!method_at(start+8u+i*8u,e)) break;
                    ++t.slots;
                    if (std::find_if(t.methods.begin(),t.methods.end(),
                        [&](const GbaCodeEdge& prev){return prev.address==e.address && prev.mode==e.mode;})==t.methods.end()) t.methods.push_back(e);
                }
                found.emplace(start,std::move(t));
                break;
            }
        }
    }
    return found;
}

// This recognizer is deliberately local to one straight-line basic block.
// It does NOT propagate an object identity from a constructor to a caller.
// All method relationships derived here are explicitly candidates.
void gather_virtual_evidence(const GbaRom& rom, GbaCfgAnalysis& cfg) {
    enum class Role : uint8_t { Unknown, TableAddress, ObjectField, MethodFromField };
    struct Origin {
        Role role = Role::Unknown;
        uint32_t table = 0;
        uint16_t object_offset = 0;
        uint16_t slot_offset = 0;
    };
    std::map<uint32_t, std::set<uint16_t>> installations;
    std::map<uint32_t, const GbaRootedFunctionTable*> tables;
    for (const auto& t : cfg.rooted_function_tables) tables.emplace(t.address, &t);

    // First pass: provenance-confirmed stores of ROM-table pointers.
    auto visit = [&](const GbaBasicBlock& b, bool collect_stores) {
        if (b.mode != GbaCodeMode::Thumb) return;
        std::array<Origin, 8> origins{};
        std::array<Origin, 8> callOrigins{};
        for (size_t i=0; i<b.instructions.size(); ++i) {
            const auto& ins=b.instructions[i];
            const uint16_t h=static_cast<uint16_t>(ins.raw);
            if ((h&0xF800u)==0xF000u && i+1<b.instructions.size() &&
                is_thumb_bl_pair(h,static_cast<uint16_t>(b.instructions[i+1].raw))) {
                if (i+2u==b.instructions.size()) callOrigins=origins;
                for (unsigned r=0;r<4;++r) origins[r]={};
                ++i;
                continue;
            }
            if ((h&0xF800u)==0x4800u) {
                const unsigned rd=(h>>8)&7u;
                const uint32_t addr=((ins.address+4u)&~3u)+uint32_t(h&255u)*4u;
                origins[rd]={};
                if (in_rom(rom,addr,4u)) {
                    const uint32_t pointer=rom.read32(addr-kRomBase);
                    if (tables.count(pointer)) origins[rd]={Role::TableAddress,pointer,0,0};
                    else if (pointer>=8u && tables.count(pointer-8u))
                        origins[rd]={Role::TableAddress,pointer-8u,0,0};
                }
                continue;
            }
            if ((h&0xFC00u)==0x4400u && ((h>>8)&3u)==2u) {
                const unsigned rd=(h&7u)|((h>>4)&8u),rs=(h>>3)&15u;
                if (rd<8u) origins[rd]=rs<8u?origins[rs]:Origin{};
                continue;
            }
            // STR word [Rb, #imm5*4]. A method table pointer is stored into
            // the object. This is constructor evidence, not a call proof.
            if ((h&0xF800u)==0x6000u) {
                const unsigned src=h&7u,base=(h>>3)&7u;
                const unsigned off=((h>>6)&31u)*4u;
                if (collect_stores && origins[src].role==Role::TableAddress) {
                    const uint32_t table=origins[src].table;
                    cfg.vtable_installs.push_back({ins.address,table,static_cast<uint16_t>(off),static_cast<uint8_t>(base)});
                    installations[table].insert(static_cast<uint16_t>(off));
                }
                continue;
            }
            // LDR word [Rb, #imm5*4]. The first load establishes that
            // the value came from an object's field, NOT that it is a vptr.
            if ((h&0xF800u)==0x6800u) {
                const unsigned rd=h&7u,base=(h>>3)&7u;
                const unsigned off=((h>>6)&31u)*4u;
                const Origin prior=origins[base];
                if (prior.role==Role::ObjectField && off<=124u && off%8u==4u)
                    origins[rd]={Role::MethodFromField,0,prior.object_offset,static_cast<uint16_t>(off)};
                else if (prior.role==Role::TableAddress) origins[rd]={};
                else origins[rd]={Role::ObjectField,0,static_cast<uint16_t>(off),0};
                continue;
            }
            if ((h&0xF800u)==0x2000u || (h&0xF800u)==0x3000u || (h&0xF800u)==0x3800u)
                origins[(h>>8)&7u]={};
            else if ((h&0xE000u)==0x0000u || (h&0xFC00u)==0x4000u || (h&0xF800u)==0x1800u)
                origins[h&7u]={};
            else if ((h&0xF000u)==0xC000u || (h&0xFE00u)==0xBC00u)
                origins={};
        }
        if (collect_stores || b.terminator!=GbaTerminator::DirectCall || b.calls.empty()) return;
        for (const auto& call:b.calls) {
            const int reg=thumb_call_veneer_register(rom,call.address);
            if (reg<0 || reg>=8 || callOrigins[static_cast<unsigned>(reg)].role!=Role::MethodFromField) continue;
            const Origin method=callOrigins[static_cast<unsigned>(reg)];
            GbaVirtualCallPattern p;
            p.callsite=b.instructions.size()>=2?b.instructions[b.instructions.size()-2].address:b.start;
            p.veneer=call.address;
            p.call_register=static_cast<uint8_t>(reg);
            p.object_offset=method.object_offset;
            p.slot_offset=method.slot_offset;
            for (const auto& t:tables) {
                if (!installations[t.first].count(method.object_offset)) continue;
                if (size_t(method.slot_offset) >= t.second->slots*8u) continue;
                const uint32_t functionWord=t.first+8u+method.slot_offset;
                if (!in_rom(rom,functionWord,4u)) continue;
                const uint32_t target=rom.read32(functionWord-kRomBase);
                if (!(target&1u) || !in_rom(rom,target&~1u,2)) continue;
                p.candidate_tables.push_back(t.first);
                const GbaCodeEdge e{target&~1u,GbaCodeMode::Thumb};
                if (std::find_if(p.candidate_methods.begin(),p.candidate_methods.end(),
                    [&](const GbaCodeEdge& prior){return prior.address==e.address && prior.mode==e.mode;})==p.candidate_methods.end())
                    p.candidate_methods.push_back(e);
            }
            cfg.virtual_call_patterns.push_back(std::move(p));
        }
    };
    for (const auto& b:cfg.blocks) visit(b,true);
    for (const auto& b:cfg.blocks) visit(b,false);
    std::sort(cfg.vtable_installs.begin(),cfg.vtable_installs.end(),
        [](const GbaVtableInstall& a,const GbaVtableInstall& b){return a.instruction<b.instruction;});
    std::sort(cfg.virtual_call_patterns.begin(),cfg.virtual_call_patterns.end(),
        [](const GbaVirtualCallPattern& a,const GbaVirtualCallPattern& b){return a.callsite<b.callsite;});
}


// Function-scoped must-provenance across control-flow edges. At a merge we
// retain a register's origin only if ALL incoming paths agree. Calls kill
// volatile r0-r3; no state is passed into a different function entry.
// This remains pattern discovery, NOT proof of the runtime vptr value.
void gather_interblock_virtual_evidence(const GbaRom& rom, GbaCfgAnalysis& cfg) {
    enum class Kind : uint8_t { Unknown, Table, ObjectField, Method };
    struct Origin {
        Kind kind=Kind::Unknown;
        uint32_t table=0;
        uint16_t field=0, slot=0;
        bool inherited=false;
        bool operator==(const Origin& b)const {return kind==b.kind && (kind==Kind::Unknown ||
            (table==b.table && field==b.field && slot==b.slot));}
    };
    using State=std::array<Origin,8>;
    const std::set<uint32_t> functionAddrs=[&]{std::set<uint32_t> a;for(const auto&e:cfg.function_entries)if(e.mode==GbaCodeMode::Thumb)a.insert(e.address);return a;}();
    std::map<uint32_t,const GbaRootedFunctionTable*> tables;
    for(const auto&t:cfg.rooted_function_tables)tables[t.address]=&t;
    std::map<uint32_t,std::set<uint16_t>> installs;
    for(const auto&i:cfg.vtable_installs) installs[i.table_address].insert(i.object_offset);
    std::map<uint32_t,size_t> blockByStart;
    for(size_t i=0;i<cfg.blocks.size();++i)if(cfg.blocks[i].mode==GbaCodeMode::Thumb)blockByStart[cfg.blocks[i].start]=i;
    // Include Thumb roots reached from ARM BX: they need not be BL entries.
    std::set<uint32_t> thumbPredecessors;
    for(const auto& b:cfg.blocks)if(b.mode==GbaCodeMode::Thumb)
        for(const auto& e:b.successors)if(e.mode==GbaCodeMode::Thumb)thumbPredecessors.insert(e.address);
    std::set<uint32_t> entryRoots=functionAddrs;
    for(const auto& b:cfg.blocks)if(b.mode==GbaCodeMode::Thumb && !thumbPredecessors.count(b.start))entryRoots.insert(b.start);
    std::vector<State> entry(cfg.blocks.size());
    std::vector<bool> reached(cfg.blocks.size());
    std::deque<size_t> todo;
    std::vector<bool> queued(cfg.blocks.size());
    for(auto a:entryRoots){auto it=blockByStart.find(a);if(it!=blockByStart.end()){
        reached[it->second]=true; todo.push_back(it->second);queued[it->second]=true;
    }}
    // A BL records the method provenance immediately BEFORE clobbering
    // volatile caller-saved registers (same convention as gather_virtual_evidence).
    auto transfer=[&](const GbaBasicBlock& b,State st,bool collect) {
        for(size_t i=0;i<b.instructions.size();++i){
            const auto& ins=b.instructions[i];const uint16_t h=static_cast<uint16_t>(ins.raw);
            if((h&0xF800u)==0xF000u && i+1<b.instructions.size() &&
                is_thumb_bl_pair(h,static_cast<uint16_t>(b.instructions[i+1].raw))){
                if(collect && b.terminator==GbaTerminator::DirectCall && i+2==b.instructions.size()){
                    for(const auto& e:b.calls){
                        const int reg=thumb_call_veneer_register(rom,e.address);
                        if(reg<0 || reg>=8)continue;
                        const auto& origin=st[static_cast<unsigned>(reg)];
                        if(origin.kind!=Kind::Method || !origin.inherited)continue;
                        const uint32_t callsite=ins.address;
                        if(std::any_of(cfg.virtual_call_patterns.begin(),cfg.virtual_call_patterns.end(),
                            [&](const GbaVirtualCallPattern& x){return x.callsite==callsite && x.veneer==e.address;}))continue;
                        GbaVirtualCallPattern v;
                        v.callsite=callsite;v.veneer=e.address;v.call_register=static_cast<uint8_t>(reg);
                        v.object_offset=origin.field;v.slot_offset=origin.slot;v.crosses_blocks=true;
                        for(const auto& t:tables){
                            if(!installs[t.first].count(v.object_offset) ||
                               v.slot_offset>=t.second->slots*8u)continue;
                            const uint32_t off=t.first+8u+v.slot_offset;
                            if(!in_rom(rom,off,4))continue;
                            const uint32_t target=rom.read32(off-kRomBase);
                            if(!(target&1u)||!in_rom(rom,target&~1u,2))continue;
                            v.candidate_tables.push_back(t.first);
                            const GbaCodeEdge method{target&~1u,GbaCodeMode::Thumb};
                            if(std::none_of(v.candidate_methods.begin(),v.candidate_methods.end(),
                                [&](const GbaCodeEdge& x){return x.address==method.address;}))v.candidate_methods.push_back(method);
                        }
                        cfg.virtual_call_patterns.push_back(std::move(v));
                    }
                }
                for(unsigned r=0;r<4;++r)st[r]={};
                ++i;continue;
            }
            if((h&0xF800u)==0x4800u){
                const unsigned rd=(h>>8)&7u;
                const uint32_t lit=((ins.address+4u)&~3u)+uint32_t(h&255u)*4u;
                st[rd]={};
                if(in_rom(rom,lit,4)){
                    const uint32_t v=rom.read32(lit-kRomBase);
                    if(tables.count(v))st[rd]={Kind::Table,v,0,0,false};
                    else if(v>=8u && tables.count(v-8u))st[rd]={Kind::Table,v-8u,0,0,false};
                }
                continue;
            }
            if((h&0xFC00u)==0x4400u && ((h>>8)&3u)==2u){
                const unsigned rd=(h&7u)|((h>>4)&8u),rs=(h>>3)&15u;
                if(rd<8)st[rd]=rs<8?st[rs]:Origin{};
                continue;
            }
            if((h&0xF800u)==0x6800u){
                const unsigned rd=h&7u,rb=(h>>3)&7u,off=((h>>6)&31u)*4u;
                Origin prev=st[rb];
                if(prev.kind==Kind::ObjectField && off<=124u && off%8u==4u)
                    st[rd]={Kind::Method,0,prev.field,static_cast<uint16_t>(off),prev.inherited};
                else if(prev.kind==Kind::Table)st[rd]={};
                else st[rd]={Kind::ObjectField,0,static_cast<uint16_t>(off),0,false};
                continue;
            }
            if((h&0xF800u)==0x2000u || (h&0xF800u)==0x3000u || (h&0xF800u)==0x3800u)st[(h>>8)&7u]={};
            else if((h&0xE000u)==0x0000u || (h&0xFC00u)==0x4000u || (h&0xF800u)==0x1800u)st[h&7u]={};
            else if((h&0xF000u)==0xC000u || (h&0xFE00u)==0xBC00u)st={};
            else if((h&0xFE00u)==0x5C00u || (h&0xF000u)==0x8000u ||
                (h&0xF800u)==0x7800u || (h&0xF800u)==0x8800u ||
                (h&0xF800u)==0x9800u)st[h&7u]={};
        }
        return st;
    };
    size_t budget=0;
    while(!todo.empty() && ++budget<350000){
        const size_t bi=todo.front();todo.pop_front();queued[bi]=false;
        const auto& b=cfg.blocks[bi];
        const State out=transfer(b,entry[bi],false);
        for(const auto& edge:b.successors){
            if(edge.mode!=GbaCodeMode::Thumb || functionAddrs.count(edge.address))continue;
            auto it=blockByStart.find(edge.address);if(it==blockByStart.end())continue;
            const size_t target=it->second;
            State proposed=out;
            for(auto& origin:proposed)if(origin.kind!=Kind::Unknown)origin.inherited=true;
            bool changed=false;
            if(!reached[target]){entry[target]=proposed;reached[target]=true;changed=true;}
            else for(size_t r=0;r<8;++r){
                Origin merged=entry[target][r]==proposed[r]?entry[target][r]:Origin{};
                if(merged.kind!=Kind::Unknown)merged.inherited=entry[target][r].inherited || proposed[r].inherited;
                if(merged.kind!=entry[target][r].kind || merged.table!=entry[target][r].table ||
                   merged.field!=entry[target][r].field || merged.slot!=entry[target][r].slot ||
                   merged.inherited!=entry[target][r].inherited){entry[target][r]=merged;changed=true;}
            }
            if(changed && !queued[target]){todo.push_back(target);queued[target]=true;}
        }
    }
    if(budget>=350000)return; // never claim evidence from unfinished fixed point
    for(size_t i=0;i<cfg.blocks.size();++i)if(reached[i] && cfg.blocks[i].mode==GbaCodeMode::Thumb)
        (void)transfer(cfg.blocks[i],entry[i],true);
    std::sort(cfg.virtual_call_patterns.begin(),cfg.virtual_call_patterns.end(),
        [](const GbaVirtualCallPattern&a,const GbaVirtualCallPattern&b){return a.callsite<b.callsite;});
    cfg.crossblock_virtual_patterns=std::count_if(cfg.virtual_call_patterns.begin(),cfg.virtual_call_patterns.end(),
        [](const GbaVirtualCallPattern&p){return p.crosses_blocks;});

    // Interprocedural callgraph adjacency: place each witnessed vptr store
    // inside reachable function bodies, then show their direct callers.
    // Not a must-alias proof and never an extra native root.
    std::map<uint32_t,std::vector<const GbaVtableInstall*>> installsAt;
    for(const auto& v:cfg.vtable_installs)installsAt[v.instruction].push_back(&v);
    std::map<uint32_t,std::vector<const GbaVtableInstall*>> inFunction;
    for(const auto& fe:cfg.function_entries){
        if(fe.mode!=GbaCodeMode::Thumb)continue;
        std::deque<uint32_t> work{fe.address};std::set<uint32_t> seen;
        while(!work.empty() && seen.size()<50000){
            const uint32_t addr=work.front();work.pop_front();
            if(addr!=fe.address && functionAddrs.count(addr))continue;
            if(!seen.insert(addr).second)continue;
            auto it=blockByStart.find(addr);if(it==blockByStart.end())continue;
            const auto& b=cfg.blocks[it->second];
            for(const auto& ins:b.instructions){
                auto sit=installsAt.find(ins.address);if(sit!=installsAt.end())
                    inFunction[fe.address].insert(inFunction[fe.address].end(),sit->second.begin(),sit->second.end());
            }
            for(const auto& e:b.successors)if(e.mode==GbaCodeMode::Thumb)work.push_back(e.address);
        }
    }
    std::set<std::tuple<uint32_t,uint32_t,uint32_t>> seenLinks;
    for(const auto& b:cfg.blocks){
        if(b.mode!=GbaCodeMode::Thumb || b.calls.empty() || b.instructions.size()<2)continue;
        const uint32_t callsite=b.instructions[b.instructions.size()-2].address;
        for(const auto& e:b.calls){
            if(e.mode!=GbaCodeMode::Thumb)continue;
            const auto it=inFunction.find(e.address);if(it==inFunction.end())continue;
            for(const auto* store:it->second){
                if(!seenLinks.emplace(callsite,e.address,store->instruction).second)continue;
                cfg.constructor_callers.push_back({callsite,e.address,store->instruction,
                    store->table_address,store->object_offset});
            }
        }
    }
    std::sort(cfg.constructor_callers.begin(),cfg.constructor_callers.end(),
        [](const GbaConstructorCaller&a,const GbaConstructorCaller&b){
            return std::tie(a.callsite,a.callee_entry,a.constructor_store)<
                std::tie(b.callsite,b.callee_entry,b.constructor_store);});
}

bool arm_word_liftable(uint32_t w) {
    const uint32_t cond=w>>28;
    if(cond==0xFu)return false;
    if((w&0x0E000000u)==0x0A000000u)return true; // B/BL
    if((w&0x0FFFFFF0u)==0x012FFF10u)return true; // BX
    if((w&0x0F000000u)==0x0F000000u)return true; // SWI
    if((w&0x0FB0FFF0u)==0x0120F000u)return true; // MSR CPSR_fields,Rm
    if((w&0x0E000000u)==0x08000000u)return (w&(1u<<22))==0; // ARM LDM/STM without user-bank/SPSR
    if((w&0x0C000000u)==0x04000000u)return (w&(1u<<25))==0 || (w&(1u<<4))==0; // immediate or register-offset LDR/STR
    if((w&0x0C000000u)==0x00000000u){
        if((w&0x0FC000F0u)==0x00000090u)return true; // MUL/MLA 32-bit
        if((w&0x0E000090u)==0x00000090u){ // ARM halfword/signed transfer
            const unsigned sh=(w>>5)&3u,load=(w>>20)&1u;
            if(sh==0u||(!load&&sh!=1u))return false;
            return ((w>>12)&15u)!=15u; // PC target requires explicit flow
        }
        const unsigned opcode=(w>>21)&15u;
        if(opcode>=8u&&opcode<=11u)return true; // test/compare
        if(((w>>12)&15u)==15u && opcode>=8u && opcode<=11u)return false; // tests never write PC
        if((w&(1u<<25))!=0)return true; // immediate operand2
        return (w&(1u<<4))==0; // register with immediate shift
    }
    return false;
}

bool block_arm_liftable(const GbaBasicBlock& b){if(b.mode!=GbaCodeMode::Arm||b.instructions.empty())return false;for(const auto&i:b.instructions)if(!arm_word_liftable(i.raw))return false;return true;}

bool block_thumb_liftable(const GbaBasicBlock& b) {
    if (b.mode != GbaCodeMode::Thumb || b.instructions.empty()) return false;
    for (size_t i = 0; i < b.instructions.size(); ++i) {
        const auto& ins = b.instructions[i];
        const uint16_t h = static_cast<uint16_t>(ins.raw);
        const bool has_next = i + 1 < b.instructions.size();
        const uint16_t next = has_next ? static_cast<uint16_t>(b.instructions[i+1].raw) : 0;
        if (!is_thumb_instruction_liftable(h, next, has_next)) return false;
        if ((h & 0xF800u) == 0xF000u && has_next && is_thumb_bl_pair(h,next)) ++i;
    }
    return true;
}
}

bool is_arm_instruction_liftable(uint32_t instruction) { return arm_word_liftable(instruction); }

bool is_thumb_instruction_liftable(uint16_t instruction, uint16_t next_halfword, bool has_next) {
    if ((instruction & 0xF800u) == 0xF000u) return has_next && is_thumb_bl_pair(instruction, next_halfword);
    return is_thumb_supported_single(instruction);
}

GbaCfgAnalysis build_recursive_cfg(const GbaRom& rom, uint32_t entry_target,
                                    const std::vector<GbaCodeEdge>& observed_roots) {
    GbaCfgAnalysis result;
    if (!in_rom(rom, entry_target, 4)) return result;

    const GbaCodeEdge entry{align_address(entry_target,GbaCodeMode::Arm), GbaCodeMode::Arm};
    std::deque<GbaCodeEdge> queue;
    std::set<GbaCodeEdge,EdgeLess> starts;
    std::set<GbaCodeEdge,EdgeLess> functions;
    queue.push_back(entry);
    starts.insert(entry);
    functions.insert(entry);
    // Profile roots are accepted only with valid instruction alignment and
    // coordinates inside the supplied ROM. They stay separate from calls.
    for(const auto& root:observed_roots){
        const size_t step=root.mode==GbaCodeMode::Thumb?2u:4u;
        if((root.address&(uint32_t(step)-1u))!=0 || !in_rom(rom,root.address,step))continue;
        if(starts.insert(root).second){queue.push_back(root);result.profiled_roots.push_back(root);}
    }

    // Iterate the conservative must-constant analysis. New confirmed
    // veneer targets become entrypoints and can expose additional code.
    // The hard boundaries and function entries are re-created each round.
    std::set<GbaCodeEdge, EdgeLess> visited;
    std::map<uint32_t, GbaRootedFunctionTable> table_evidence;
    for (unsigned round=0; round<16u; ++round) {
        while (!queue.empty() && starts.size() < kMaxBlocks) {
            const GbaCodeEdge cur=queue.front(); queue.pop_front();
            if (!visited.insert(cur).second) continue;
            const GbaBasicBlock block=decode_block(rom,cur,nullptr);
            auto enqueue=[&](const GbaCodeEdge& e) {
                if (starts.insert(e).second) queue.push_back(e);
            };
            for (const auto& e : block.successors) enqueue(e);
            for (const auto& e : block.calls) { functions.insert(e); enqueue(e); }
            for (const auto& e : block.resolved_dynamic_calls) { functions.insert(e); enqueue(e); }
        }
        result.blocks.clear(); result.blocks.reserve(starts.size());
        for (const auto& start : starts) result.blocks.push_back(decode_block(rom,start,&starts));
        resolve_must_constant_calls(rom,result.blocks,functions);
        size_t added=0;
        auto roots=find_rooted_tables(rom,result.blocks);
        for (const auto& item:roots) {
            if (table_evidence.emplace(item.first,item.second).second) {
                for (const auto& e:item.second.methods) {
                    if (functions.insert(e).second) ++added;
                    if (starts.insert(e).second) {queue.push_back(e); ++added;}
                }
            }
        }
        for (const auto& b:result.blocks) for (const auto& e:b.resolved_dynamic_calls) {
            if (functions.insert(e).second) ++added;
            if (starts.insert(e).second) { queue.push_back(e); ++added; }
        }
        if (added==0) break;
    }
    // Re-evaluate with final split boundaries, including the last expansion.
    result.blocks.clear(); result.blocks.reserve(starts.size());
    for (const auto& start : starts) result.blocks.push_back(decode_block(rom,start,&starts));
    resolve_must_constant_calls(rom,result.blocks,functions);
    // Expose provenance. If an iteration limit was hit, retain exactly
    // those table roots used to seed discovered blocks.
    std::set<GbaCodeEdge,EdgeLess> rootedFunctions;
    for (const auto& entry:table_evidence) {
        result.rooted_function_tables.push_back(entry.second);
        result.rooted_table_slots += entry.second.slots;
        for (const auto& method:entry.second.methods) rootedFunctions.insert(method);
    }
    result.rooted_table_distinct_functions=rootedFunctions.size();
    result.function_entries.assign(functions.begin(), functions.end());

    std::set<GbaCodeEdge, EdgeLess> dynamicTargets;
    for (const auto& b : result.blocks) {
        if (!b.resolved_dynamic_calls.empty()) ++result.resolved_dynamic_call_sites;
        for (const auto& e : b.resolved_dynamic_calls) dynamicTargets.insert(e);
    }
    result.resolved_dynamic_call_targets = dynamicTargets.size();
    gather_virtual_evidence(rom,result);
    gather_interblock_virtual_evidence(rom,result);

    std::map<uint32_t, GbaIndirectVeneerStat> veneerStats;
    for (const auto& b : result.blocks) {
        if (!b.indirect_call_veneer || b.instructions.empty()) continue;
        const uint16_t h = static_cast<uint16_t>(b.instructions.back().raw);
        veneerStats.emplace(b.start, GbaIndirectVeneerStat{b.start, static_cast<uint8_t>((h >> 3) & 0xFu), 0});
    }
    for (const auto& b : result.blocks) {
        for (const auto& call : b.calls) {
            auto it = veneerStats.find(call.address);
            if (it != veneerStats.end()) { ++it->second.callers; ++result.indirect_call_sites; }
        }
    }
    for (const auto& kv : veneerStats) result.indirect_veneer_stats.push_back(kv.second);

    for (const auto& b : result.blocks) {
        if (b.mode == GbaCodeMode::Thumb) ++result.thumb_blocks; else ++result.arm_blocks;
        result.decoded_instructions += b.instructions.size();
        switch (b.terminator) {
            case GbaTerminator::DirectCall: ++result.direct_calls; break;
            case GbaTerminator::DirectBranch: ++result.direct_branches; break;
            case GbaTerminator::ConditionalBranch: ++result.conditional_branches; break;
            case GbaTerminator::Return: ++result.returns; break;
            case GbaTerminator::ResolvedIndirect: ++result.resolved_indirects; break;
            case GbaTerminator::Indirect: ++result.unresolved_indirects; break;
            case GbaTerminator::External: ++result.external_exits; break;
            case GbaTerminator::Limit: ++result.limit_blocks; break;
            default: break;
        }
        if (b.compiler_epilogue_return) ++result.compiler_epilogue_returns;
        if (b.switch_table_resolved) { ++result.resolved_switch_tables; result.resolved_switch_targets += b.switch_table_entries; }
        if (b.indirect_call_veneer) ++result.indirect_call_veneers;
        if (block_arm_liftable(b)) ++result.arm_liftable_blocks;
        if (block_thumb_liftable(b)) {
            ++result.thumb_liftable_blocks;
            result.thumb_liftable_instructions += b.instructions.size();
        }
    }
    return result;
}

std::string terminator_name(GbaTerminator t) {
    switch (t) {
        case GbaTerminator::Boundary: return "boundary";
        case GbaTerminator::DirectBranch: return "branch";
        case GbaTerminator::ConditionalBranch: return "branch-cond";
        case GbaTerminator::DirectCall: return "call";
        case GbaTerminator::Return: return "return";
        case GbaTerminator::ResolvedIndirect: return "indirect-resolved";
        case GbaTerminator::Indirect: return "indirect";
        case GbaTerminator::External: return "external";
        case GbaTerminator::Limit: return "limit";
        default: return "invalid";
    }
}

std::string cfg_to_text(const GbaCfgAnalysis& cfg) {
    std::ostringstream o;
    o << "Recursive CFG\n-------------\n";
    o << "Confirmed blocks: " << cfg.blocks.size() << " (ARM " << cfg.arm_blocks << ", Thumb " << cfg.thumb_blocks << ")\n";
    o << "Confirmed function entries: " << cfg.function_entries.size() << "\n";
    o << "Execution-observed roots (NOT static call proof): " << cfg.profiled_roots.size() << "\n";
    o << "Decoded instructions: " << cfg.decoded_instructions << "\n";
    o << "Direct calls: " << cfg.direct_calls << "\nDirect branches: " << cfg.direct_branches << "\nConditional branches: " << cfg.conditional_branches << "\n";
    o << "Returns: " << cfg.returns << " (compiler pop/BX epilogues " << cfg.compiler_epilogue_returns << ")\n";
    o << "Resolved indirects: " << cfg.resolved_indirects << " (switch tables " << cfg.resolved_switch_tables << ", targets " << cfg.resolved_switch_targets << ")\n";
    o << "Unresolved indirect endpoints: " << cfg.unresolved_indirects << " (generic call-via veneers " << cfg.indirect_call_veneers << ")\n";
    o << "Dynamic callsites through call-via veneers: " << cfg.indirect_call_sites << "\n";
    o << "Statically recovered call-via sites: " << cfg.resolved_dynamic_call_sites << " -> " << cfg.resolved_dynamic_call_targets << " targets\n";
    o << "Reachable-code-rooted ROM method tables: " << cfg.rooted_function_tables.size()
      << " (slots " << cfg.rooted_table_slots << ", distinct methods " << cfg.rooted_table_distinct_functions
      << ") [separate from callsite resolution]\n";
    o << "Confirmed ROM-vtable constructor stores: " << cfg.vtable_installs.size() << "\n";
    o << "Virtual callsite load patterns (advisory ONLY): " << cfg.virtual_call_patterns.size()
      << " (cross-block must-provenance " << cfg.crossblock_virtual_patterns << ")\n";
    o << "Direct constructor-caller relationships (advisory ONLY): " << cfg.constructor_callers.size() << "\n";
    if (!cfg.indirect_veneer_stats.empty()) {
        o << "Call-via veneer usage:";
        for (const auto& v : cfg.indirect_veneer_stats) o << " 0x" << std::hex << std::uppercase << v.address << std::dec << "(r" << unsigned(v.register_index) << ":" << v.callers << ")";
        o << "\n";
    }
    o << "ARM blocks structurally liftable: " << cfg.arm_liftable_blocks << "/" << cfg.arm_blocks << "\n";
    o << "Thumb blocks structurally liftable: " << cfg.thumb_liftable_blocks << "/" << cfg.thumb_blocks << "\n";
    o << "Thumb instructions in liftable blocks: " << cfg.thumb_liftable_instructions << "\n";
    if (cfg.limit_blocks) o << "WARNING: block decode limit reached: " << cfg.limit_blocks << "\n";
    return o.str();
}
