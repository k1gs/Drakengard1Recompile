#pragma once
#include "iso_media.h"
#include <cstddef>
#include <cstdint>
#include <span>

namespace drakengard {
struct CdvdHost {
    virtual ~CdvdHost() = default;
    virtual bool validEe(uint32_t address, size_t bytes) = 0;
    virtual void writeEe(uint32_t address, const void* data, size_t bytes) = 0;
};

class CdvdRpc {
public:
    explicit CdvdRpc(IsoMedia& media) : m_media(media) {}
    static bool supports(uint32_t sid);
    bool call(uint32_t sid, uint32_t function, std::span<const uint8_t> arguments,
              std::span<uint8_t> reply, CdvdHost& host);
    uint64_t readCount() const { return m_reads; }
    uint64_t sectorsRead() const { return m_sectorsRead; }
    uint32_t lastLsn() const { return m_lastLsn; }
private:
    IsoMedia& m_media;
    bool m_initialized{};
    uint32_t m_error{}, m_status{};
    uint64_t m_reads{}, m_sectorsRead{};
    uint32_t m_lastLsn{};
};
}
