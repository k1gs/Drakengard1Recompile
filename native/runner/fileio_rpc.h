#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace drakengard {
struct FileIoHost {
    virtual ~FileIoHost() = default;
    virtual int32_t fileOpen(const std::string& path, uint32_t flags) = 0;
    virtual int32_t fileClose(int32_t descriptor) = 0;
    virtual int32_t fileRead(int32_t descriptor, uint32_t address, uint32_t bytes) = 0;
    virtual int32_t fileSeek(int32_t descriptor, int32_t offset, uint32_t whence) = 0;
    virtual bool validEe(uint32_t address, size_t bytes) = 0;
    virtual void writeEe(uint32_t address, const void* data, size_t bytes) = 0;
};
// Read-only FileIO RPC subset: open, close, read, lseek. Unsupported commands have no result.
std::optional<int32_t> fileIoRpc(uint32_t function, std::span<const uint8_t> arguments, FileIoHost& host);
}
