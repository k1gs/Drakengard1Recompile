#include "iso_media.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace drakengard {
namespace {
constexpr uint32_t SectorSize = 2048;
uint32_t both32(std::span<const uint8_t> data, size_t offset) {
    if (offset + 8 > data.size()) throw std::runtime_error("Short ISO both-endian field");
    uint32_t little = 0, big = 0;
    for (unsigned n = 0; n < 4; ++n) {
        little |= uint32_t(data[offset + n]) << (8 * n);
        big = (big << 8) | data[offset + 4 + n];
    }
    if (little != big) throw std::runtime_error("Inconsistent ISO both-endian field");
    return little;
}
IsoFile record(std::span<const uint8_t> data) {
    if (data.size() < 34 || data[0] < 34 || data[0] > data.size() ||
        size_t(33 + data[32]) > data[0]) throw std::runtime_error("Invalid ISO directory record");
    if (data[1] || data[26] || data[27] || (data[25] & 0x80))
        throw std::runtime_error("Unsupported ISO extended/interleaved/multi-extent record");
    IsoFile file{both32(data, 2), both32(data, 10)};
    std::copy_n(data.begin() + 33, std::min<size_t>(data[32], file.name.size()), file.name.begin());
    std::copy_n(data.begin() + 18, 7, file.date.begin());
    return file;
}
std::optional<std::string> normalized(std::string path) {
    for (char& byte : path) {
        const auto ch = static_cast<unsigned char>(byte);
        if (ch >= 0x80 || ch == 0) return std::nullopt;
        if (byte == '\\') byte = '/';
        else if (byte >= 'a' && byte <= 'z') byte = char(byte - 'a' + 'A');
    }
    if (path.starts_with("CDROM0:")) path.erase(0, 7);
    else if (path.starts_with("CDROM:")) path.erase(0, 6);
    else if (path.find(':') != std::string::npos) return std::nullopt;
    while (!path.empty() && path.front() == '/') path.erase(0, 1);
    const auto version = path.rfind(';');
    if (version != std::string::npos) {
        if (path.substr(version) != ";1") return std::nullopt;
        path.resize(version);
    }
    if (path.empty()) return std::nullopt;
    size_t start = 0;
    while (start < path.size()) {
        auto end = path.find('/', start);
        if (end == std::string::npos) end = path.size();
        const auto component = path.substr(start, end - start);
        if (component.empty() || component == "." || component == "..") return std::nullopt;
        start = end + 1;
    }
    return path;
}
}

IsoMedia::IsoMedia(const std::filesystem::path& path) : m_file(path, std::ios::binary) {
    if (!m_file) throw std::runtime_error("Cannot open read-only ISO media");
    m_file.seekg(0, std::ios::end);
    const auto end = m_file.tellg();
    if (end < 0) throw std::runtime_error("Cannot measure ISO media");
    m_size = static_cast<uint64_t>(end);
    if (m_size % SectorSize || m_size < 18 * SectorSize)
        throw std::runtime_error("Invalid 2048-byte-sector ISO image size");
    std::array<uint8_t, SectorSize> descriptor{};
    bool primary = false, terminator = false;
    IsoFile root{};
    for (uint32_t sector = 16; sector < 64 && uint64_t(sector + 1) * SectorSize <= m_size; ++sector) {
        readBytes(uint64_t(sector) * SectorSize, descriptor);
        if (std::memcmp(descriptor.data() + 1, "CD001", 5) != 0 || descriptor[6] != 1)
            throw std::runtime_error("Invalid ISO volume descriptor signature/version");
        if (descriptor[0] == 255) { terminator = true; break; }
        if (descriptor[0] != 1) continue;
        if (primary) throw std::runtime_error("Repeated primary ISO descriptors are unsupported");
        if (descriptor[128] != 0 || descriptor[129] != 8 || descriptor[130] != 8 || descriptor[131] != 0)
            throw std::runtime_error("Unsupported ISO logical block size");
        m_sectors = both32(descriptor, 80);
        if (m_sectors < 18 || uint64_t(m_sectors) * SectorSize > m_size)
            throw std::runtime_error("ISO declared volume exceeds image");
        root = record(std::span(descriptor).subspan(156, descriptor[156]));
        if ((descriptor[156 + 25] & 2) == 0) throw std::runtime_error("ISO root is not a directory");
        primary = true;
    }
    if (!primary || !terminator) throw std::runtime_error("Incomplete ISO volume descriptor set");
    directory(root, "", 0);
}

void IsoMedia::readBytes(uint64_t offset, std::span<uint8_t> data) {
    if (offset > m_size || data.size() > m_size - offset)
        throw std::runtime_error("ISO extent exceeds file");
    m_file.clear();
    m_file.seekg(static_cast<std::streamoff>(offset));
    m_file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (static_cast<size_t>(m_file.gcount()) != data.size()) throw std::runtime_error("Short ISO read");
}

void IsoMedia::directory(IsoFile entry, const std::string& prefix, unsigned depth) {
    if (depth > 16 || entry.size > 16 * 1024 * 1024 || entry.size == 0 ||
        !m_directories.insert(entry.lsn).second)
        throw std::runtime_error("Unsupported ISO directory size/depth/cycle");
    if (uint64_t(entry.lsn) * SectorSize + entry.size > uint64_t(m_sectors) * SectorSize)
        throw std::runtime_error("ISO directory extent exceeds volume");
    std::vector<uint8_t> data(entry.size);
    readBytes(uint64_t(entry.lsn) * SectorSize, data);
    for (size_t offset = 0; offset < data.size();) {
        const uint8_t length = data[offset];
        if (!length) { offset = ((offset / SectorSize) + 1) * SectorSize; continue; }
        if (length > data.size() - offset || length > SectorSize - (offset % SectorSize))
            throw std::runtime_error("ISO directory record crosses a sector");
        const auto view = std::span<const uint8_t>(data).subspan(offset, length);
        const IsoFile file = record(view);
        offset += length;
        if (view[32] == 1 && (view[33] == 0 || view[33] == 1)) continue;
        if (uint64_t(file.lsn) * SectorSize + file.size > uint64_t(m_sectors) * SectorSize)
            throw std::runtime_error("ISO file extent exceeds volume");
        const std::string name(reinterpret_cast<const char*>(view.data() + 33), view[32]);
        const auto key = normalized(prefix + name);
        if (!key) throw std::runtime_error("Unsupported ISO identifier");
        if (view[25] & 2) directory(file, *key + '/', depth + 1);
        else {
            if (m_files.size() >= 100000 || !m_files.emplace(*key, file).second)
                throw std::runtime_error("Duplicate or excessive ISO file entries");
        }
    }
}

bool IsoMedia::readSectors(uint32_t lsn, uint32_t count, std::span<uint8_t> destination) {
    const uint64_t bytes = uint64_t(count) * SectorSize;
    if (bytes != destination.size() || uint64_t(lsn) + count > m_sectors) return false;
    readBytes(uint64_t(lsn) * SectorSize, destination);
    return true;
}

std::optional<IsoFile> IsoMedia::find(const std::string& path) const {
    const auto key = normalized(path);
    if (!key) return std::nullopt;
    const auto found = m_files.find(*key);
    return found == m_files.end() ? std::nullopt : std::optional<IsoFile>(found->second);
}
}
