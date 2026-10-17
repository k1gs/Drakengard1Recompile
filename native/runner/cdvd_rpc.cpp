#include "cdvd_rpc.h"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>

namespace drakengard {
namespace {
uint32_t cdWord(std::span<const uint8_t> bytes, size_t index) {
    const size_t n = index * 4;
    if (bytes.size() < n + 4) throw std::runtime_error("Short CDVD RPC arguments");
    return uint32_t(bytes[n]) | uint32_t(bytes[n + 1]) << 8 |
           uint32_t(bytes[n + 2]) << 16 | uint32_t(bytes[n + 3]) << 24;
}
void cdPut(std::span<uint8_t> bytes, size_t index, uint32_t value) {
    if (bytes.size() < index * 4 + 4) throw std::runtime_error("Short CDVD RPC result");
    for (unsigned n = 0; n < 4; ++n) bytes[index * 4 + n] = uint8_t(value >> (8 * n));
}
void cdCheck(bool value, const char* error) { if (!value) throw std::runtime_error(error); }
void cdResult(std::span<uint8_t> reply, uint32_t value) {
    cdCheck(reply.size() >= 4 && reply.size() <= 16, "Unsupported CDVD scalar result size");
    std::fill(reply.begin(), reply.end(), 0); cdPut(reply, 0, value);
}
}

bool CdvdRpc::supports(uint32_t sid) {
    return sid == 0x80000592u || sid == 0x80000593u || sid == 0x80000595u ||
           sid == 0x80000597u || sid == 0x8000059au || sid == 0x8000059cu;
}

bool CdvdRpc::call(uint32_t sid, uint32_t function, std::span<const uint8_t> arguments,
                   std::span<uint8_t> reply, CdvdHost& host) {
    if (!supports(sid)) return false;
    if (sid == 0x80000592u) {
        if (function != 0) return false;
        cdCheck(arguments.size() == 4, "Unsupported CDVD init request size");
        const uint32_t mode = cdWord(arguments, 0);
        cdCheck(mode == 0 || mode == 1 || mode == 5, "Unsupported CDVD init mode");
        cdResult(reply, 1); // Media is already opened, indexed and range validated.
        m_initialized = mode != 5; m_status = m_initialized ? 0x0a : 0; m_error = 0;
        // Version fields remain zero: this backend advertises no SCE module revision.
        return true;
    }
    cdCheck(m_initialized, "CDVD request before media initialization");
    if (sid == 0x8000059au || sid == 0x8000059cu) {
        if (function != 0) return false;
        cdCheck(arguments.size() == 4 && cdWord(arguments, 0) <= 1, "Unsupported CDVD ready mode");
        cdResult(reply, 2); // No operation is pending on this synchronous image drive.
        return true;
    }
    if (sid == 0x80000593u) {
        if (function == 3 || function == 4 || function == 12) {
            cdCheck(arguments.empty(), "Unexpected CDVD status arguments");
            cdResult(reply, function == 3 ? 0x14 : function == 4 ? m_error : m_status);
            return true;
        }
        if (function == 34) {
            cdCheck(arguments.size() == 4, "Invalid CDVD media-mode request");
            const uint32_t mode = cdWord(arguments, 0);
            cdCheck(mode == 1 || mode == 2, "Unsupported CDVD media mode");
            cdResult(reply, mode == 2 ? 1 : 0);
            m_error = mode == 2 ? 0 : 0x14; // Current input is the verified PS2 DVD.
            return true;
        }
        return false;
    }
    if (sid == 0x80000597u) {
        if (function != 0) return false;
        cdCheck(arguments.size() == 292, "Unsupported CDVD search structure revision");
        auto begin = arguments.begin() + 32;
        auto end = std::find(begin, begin + 256, uint8_t(0));
        cdCheck(end != begin + 256, "Unterminated CDVD search path");
        const auto file = m_media.find(std::string(begin, end));
        const uint32_t destination = cdWord(arguments, 72);
        cdCheck(destination != 0 && host.validEe(destination, 32), "Invalid CDVD search result range");
        cdResult(reply, file ? 1 : 0);
        if (file) {
            std::array<uint8_t, 32> data{};
            cdPut(data, 0, file->lsn); cdPut(data, 1, file->size);
            std::copy(file->name.begin(), file->name.end(), data.begin() + 8);
            std::copy(file->date.begin(), file->date.end(), data.begin() + 24);
            host.writeEe(destination, data.data(), data.size());
        }
        return true;
    }
    if (function == 14) {
        cdCheck(arguments.empty(), "Unexpected CDVD N-command ready arguments");
        cdResult(reply, 2); return true;
    }
    if (function == 1) {
        cdCheck(arguments.size() == 24 && reply.empty(), "Unsupported CDVD read packet shape");
        const uint32_t lsn = cdWord(arguments, 0), count = cdWord(arguments, 1), destination = cdWord(arguments, 2);
        const uint32_t mode = cdWord(arguments, 3), edges = cdWord(arguments, 4), progress = cdWord(arguments, 5);
        cdCheck(((mode >> 16) & 0xffu) == 0, "Only actual 2048-byte ISO user sectors are supported");
        const uint64_t bytes = uint64_t(count) * 2048;
        cdCheck(bytes <= 32 * 1024 * 1024 && (bytes == 0 || destination != 0) &&
                host.validEe(destination, static_cast<size_t>(bytes)) && edges != 0 && progress != 0 &&
                host.validEe(edges, 16) && host.validEe(progress, 4), "Invalid CDVD read target/completion range");
        const uint32_t dataAddress = destination & 0x1fffffffu;
        for (const auto metadata : {std::pair{edges & 0x1fffffffu, 16u}, std::pair{progress & 0x1fffffffu, 4u}})
            cdCheck(bytes == 0 || dataAddress >= uint64_t(metadata.first) + metadata.second ||
                    metadata.first >= uint64_t(dataAddress) + bytes, "CDVD read metadata overlaps data");
        std::vector<uint8_t> data(static_cast<size_t>(bytes));
        const bool read = m_media.readSectors(lsn, count, data);
        m_error = read ? 0 : 0x20; // SCECdErIPI: invalid LSN/range, never a padded successful read.
        if (read) {
            host.writeEe(destination, data.data(), data.size());
            ++m_reads; m_sectorsRead += count; m_lastLsn = lsn;
        }
        const std::array<uint8_t, 16> noEdges{};
        host.writeEe(edges, noEdges.data(), noEdges.size());
        std::array<uint8_t, 4> position{}; cdPut(position, 0, read ? static_cast<uint32_t>(bytes) : 0);
        host.writeEe(progress, position.data(), position.size());
        m_status = 0x0a;
        return true;
    }
    if (function == 5 || function == 6 || function == 7 || function == 8) {
        cdCheck(reply.empty(), "Unsupported CDVD control completion shape");
        if (function == 5) {
            cdCheck(arguments.size() == 4, "Invalid CDVD seek request");
            m_error = cdWord(arguments, 0) < m_media.sectors() ? 0 : 0x20;
        } else {
            cdCheck(arguments.empty(), "Unexpected CDVD control arguments"); m_error = 0;
        }
        m_status = function == 7 ? 0 : 0x0a;
        return true;
    }
    return false;
}
}
