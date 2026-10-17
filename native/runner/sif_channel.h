#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <string_view>

namespace drakengard {
struct SifServer {
    uint32_t address{}, buffer{}, capacity{};
};

// Directional memory and real RPC services supplied by the host adapter.
struct SifBackend {
    virtual ~SifBackend() = default;
    virtual bool readIop(uint32_t address, void* data, size_t size) = 0;
    virtual bool readEe(uint32_t address, void* data, size_t size) = 0;
    virtual bool validEe(uint32_t address, size_t size) = 0;
    virtual void writeEe(uint32_t address, const void* data, size_t size) = 0;
    virtual void replyEe(uint32_t address, const void* data, size_t size, bool interrupt) = 0;
    virtual SifServer bind(uint32_t sid) = 0;
    virtual void rebootIop(std::string_view arguments, uint32_t mode) = 0;
    virtual bool call(const SifServer& server, uint32_t client, uint32_t function,
                      uint32_t sendSize, uint32_t receive, uint32_t receiveSize) = 0;
};

class SifChannel {
public:
    // Called only after an actual EE -> IOP DMA copy has completed.
    void receive(uint32_t destination, uint32_t size, SifBackend& backend);
    void reset();
    void stopReceive(SifBackend& backend);
    void startReceive() { m_receiveEnabled = true; }
    bool ready() const { return m_ready; }
    uint32_t receiveAddress() const { return m_receive; }

private:
    void emit(const std::array<uint8_t, 128>& packet, uint32_t size,
              uint32_t destination, bool commandBuffer, SifBackend& backend);
    uint32_t m_receive{};
    bool m_ready{};
    bool m_receiveEnabled{true};
    std::array<uint32_t, 32> m_sregs{};
    std::unordered_map<uint32_t, SifServer> m_servers;
};
}
