#include "gba_rom.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>

namespace {
std::string field_string(const std::vector<uint8_t>& d, size_t off, size_t len) {
    if (off + len > d.size()) return {};
    std::string s(reinterpret_cast<const char*>(d.data() + off), len);
    while (!s.empty() && (s.back() == '\0' || s.back() == ' ')) s.pop_back();
    return s;
}

constexpr uint8_t kNintendoLogoPrefix[] = {
    0x24,0xFF,0xAE,0x51,0x69,0x9A,0xA2,0x21,
    0x3D,0x84,0x82,0x0A,0x84,0xE4,0x09,0xAD
};
}

bool GbaRom::load(const std::filesystem::path& path, std::string& error) {
    path_ = path;
    std::ifstream f(path, std::ios::binary);
    if (!f) { error = "Nao foi possivel abrir a ROM."; return false; }
    data_ = std::vector<uint8_t>(std::istreambuf_iterator<char>(f), {});
    if (data_.size() < 0xC0) { error = "Arquivo pequeno demais para ser uma ROM GBA valida."; return false; }
    if (data_.size() > 32u * 1024u * 1024u) {
        error = "ROM maior que 32 MiB; isso foge do tamanho padrao do Game Boy Advance.";
        return false;
    }
    parse_header();
    return true;
}

void GbaRom::parse_header() {
    header_.title = field_string(data_, 0xA0, 12);
    header_.game_code = field_string(data_, 0xAC, 4);
    header_.maker_code = field_string(data_, 0xB0, 2);
    header_.fixed_value = data_[0xB2];
    header_.software_version = data_[0xBC];
    header_.complement_check = data_[0xBD];

    uint8_t chk = 0;
    for (size_t i = 0xA0; i <= 0xBC; ++i) chk = static_cast<uint8_t>(chk - data_[i]);
    chk = static_cast<uint8_t>(chk - 0x19);
    header_.calculated_complement = chk;
    header_.checksum_ok = (chk == header_.complement_check);

    header_.logo_signature_ok = true;
    for (size_t i = 0; i < sizeof(kNintendoLogoPrefix); ++i) {
        if (data_[0x04 + i] != kNintendoLogoPrefix[i]) {
            header_.logo_signature_ok = false;
            break;
        }
    }
}

uint32_t GbaRom::read32(size_t off) const {
    if (off + 4 > data_.size()) return 0;
    return static_cast<uint32_t>(data_[off]) |
           (static_cast<uint32_t>(data_[off+1]) << 8) |
           (static_cast<uint32_t>(data_[off+2]) << 16) |
           (static_cast<uint32_t>(data_[off+3]) << 24);
}

uint16_t GbaRom::read16(size_t off) const {
    if (off + 2 > data_.size()) return 0;
    return static_cast<uint16_t>(data_[off]) |
           (static_cast<uint16_t>(data_[off+1]) << 8);
}
