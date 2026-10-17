#include "fileio_rpc.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace drakengard {
namespace {
uint32_t word(std::span<const uint8_t> data, size_t index) {
    const size_t offset = index * 4;
    if (data.size() < offset + 4) throw std::runtime_error("Short FileIO RPC arguments");
    return uint32_t(data[offset]) | uint32_t(data[offset + 1]) << 8 |
           uint32_t(data[offset + 2]) << 16 | uint32_t(data[offset + 3]) << 24;
}
}

std::optional<int32_t> fileIoRpc(uint32_t function, std::span<const uint8_t> arguments, FileIoHost& host) {
    if (function == 0xff) {
        // This Europe client probes a newer callback-buffer extension, then
        // selects it only when reply word 1 is version 2. Our backend implements
        // legacy transfers: reject the extension and register no callbacks.
        if (arguments.size() != 8) throw std::runtime_error("Unexpected FileIO extension probe shape");
        return -1;
    }
    if (function == 0) {
        const uint32_t flags = word(arguments, 0);
        if (flags != 1) throw std::runtime_error("Unsupported FileIO open mode; source mounts are read-only");
        const size_t end = std::min<size_t>(arguments.size(), 260);
        if (end <= 4) throw std::runtime_error("Missing FileIO open path");
        auto zero = std::find(arguments.begin() + 4, arguments.begin() + end, uint8_t(0));
        if (zero == arguments.begin() + end) throw std::runtime_error("Unterminated FileIO path");
        const std::string path(arguments.begin() + 4, zero);
        return host.fileOpen(path, flags);
    }
    if (function == 1) return host.fileClose(int32_t(word(arguments, 0)));
    if (function == 2) {
        const int32_t descriptor = int32_t(word(arguments, 0));
        const uint32_t address = word(arguments, 1), bytes = word(arguments, 2), completion = word(arguments, 3);
        if (bytes > 0x7fffffffu || (bytes && !host.validEe(address, bytes)) ||
            completion == 0 || !host.validEe(completion, 16))
            throw std::runtime_error("Invalid FileIO read or edge-completion range");
        const uint32_t physical = address & 0x1fffffffu, edge = completion & 0x1fffffffu;
        if (bytes && physical < uint64_t(edge) + 16 && edge < uint64_t(physical) + bytes)
            throw std::runtime_error("FileIO edge-completion metadata overlaps data");
        const int32_t result = host.fileRead(descriptor, address, bytes);
        // The host transfers every returned byte directly, including unaligned
        // edges. No pending edge fragments remain for the guest callback to copy.
        const std::array<uint8_t, 16> noEdges{};
        host.writeEe(completion, noEdges.data(), noEdges.size());
        return result;
    }
    if (function == 4) {
        const uint32_t whence = word(arguments, 2);
        if (whence > 2) throw std::runtime_error("Unsupported FileIO seek origin");
        return host.fileSeek(int32_t(word(arguments, 0)), int32_t(word(arguments, 1)), whence);
    }
    return std::nullopt;
}
}
