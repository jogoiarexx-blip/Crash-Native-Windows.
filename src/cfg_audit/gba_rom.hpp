#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct GbaHeader {
    std::string title;
    std::string game_code;
    std::string maker_code;
    uint8_t fixed_value = 0;
    uint8_t software_version = 0;
    uint8_t complement_check = 0;
    uint8_t calculated_complement = 0;
    bool logo_signature_ok = false;
    bool checksum_ok = false;
};

class GbaRom {
public:
    bool load(const std::filesystem::path& path, std::string& error);
    const std::vector<uint8_t>& bytes() const { return data_; }
    const GbaHeader& header() const { return header_; }
    const std::filesystem::path& path() const { return path_; }
    uint32_t read32(size_t off) const;
    uint16_t read16(size_t off) const;

private:
    std::filesystem::path path_;
    std::vector<uint8_t> data_;
    GbaHeader header_;
    void parse_header();
};
