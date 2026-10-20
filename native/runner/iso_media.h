#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace drakengard {
struct IsoFile {
    uint32_t lsn{}, size{};
    std::array<uint8_t, 16> name{};
    std::array<uint8_t, 8> date{};
};

// Read-only ISO9660 primary-volume subset. Every LSN comes from actual records.
class IsoMedia {
public:
    explicit IsoMedia(const std::filesystem::path& path);
    bool readSectors(uint32_t lsn, uint32_t count, std::span<uint8_t> destination);
    std::optional<IsoFile> find(const std::string& path) const;
    uint32_t sectors() const { return m_sectors; }
    bool singleVolumeImage() const { return uint64_t(m_sectors) * 2048 == m_size; }
    size_t fileCount() const { return m_files.size(); }
private:
    void readBytes(uint64_t offset, std::span<uint8_t> data);
    void directory(IsoFile entry, const std::string& prefix, unsigned depth);
    std::ifstream m_file;
    uint64_t m_size{};
    uint32_t m_sectors{};
    std::unordered_map<std::string, IsoFile> m_files;
    std::unordered_set<uint32_t> m_directories;
};
}
