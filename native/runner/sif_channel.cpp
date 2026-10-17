#include "sif_channel.h"
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace drakengard {
namespace {
constexpr uint32_t ChangeAddress = 0x80000000u, SetSreg = 0x80000001u, Init = 0x80000002u;
constexpr uint32_t End = 0x80000008u, Bind = 0x80000009u, Call = 0x8000000au, Other = 0x8000000cu;
using Packet = std::array<uint8_t, 128>;
uint32_t word(const Packet& packet, size_t index) {
    const size_t n = index * 4;
    return uint32_t(packet[n]) | uint32_t(packet[n + 1]) << 8 |
           uint32_t(packet[n + 2]) << 16 | uint32_t(packet[n + 3]) << 24;
}
void put(Packet& packet, size_t index, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) packet[index * 4 + i] = uint8_t(value >> (i * 8));
}
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
Packet completion(const Packet& request, uint32_t command) {
    Packet result{};
    put(result, 0, 64); put(result, 2, End);
    for (size_t i = 4; i <= 7; ++i) put(result, i, word(request, i));
    put(result, 8, command);
    return result;
}
}

void SifChannel::reset() {
    m_receive = 0; m_ready = false; m_sregs.fill(0); m_servers.clear();
}

void SifChannel::emit(const Packet& packet, uint32_t size, uint32_t destination,
                      bool commandBuffer, SifBackend& backend) {
    require(backend.validEe(destination, size), "Invalid EE SIF reply range");
    if (commandBuffer) {
        Packet previous{};
        require(backend.readEe(destination, previous.data(), 4), "Cannot read EE SIF receive header");
        require((word(previous, 0) & 0xffu) == 0, "Unconsumed EE SIF receive packet");
    }
    backend.replyEe(destination, packet.data(), size, commandBuffer);
}

void SifChannel::receive(uint32_t destination, uint32_t size, SifBackend& backend) {
    // The pinned HLE bootstrap advertises IOP command receive address zero.
    // Other DMA destinations carry ordinary data and are not interpreted as commands.
    if ((destination & 0x1fffffffu) != 0) return;
    require(size >= 16 && size <= 128, "Unsupported SIF command DMA size");
    Packet packet{};
    require(backend.readIop(destination, packet.data(), size), "Cannot read copied IOP SIF packet");
    const uint32_t packetSize = word(packet, 0) & 0xffu;
    const uint32_t dataSize = word(packet, 0) >> 8;
    const uint32_t command = word(packet, 2), option = word(packet, 3);
    require(packetSize >= 16 && packetSize <= 112 && packetSize <= size && packetSize % 4 == 0,
            "Invalid SIF packet header size");

    if (command == Init || command == ChangeAddress) {
        require(dataSize == 0 && word(packet, 1) == 0, "Unexpected SIF initialization payload");
        if (command == ChangeAddress || option == 0) {
            require(packetSize == 20, "Invalid SIF receive-address packet");
            const uint32_t address = word(packet, 4);
            require(address != 0 && (address & 15u) == 0 && backend.validEe(address, 128),
                    "Invalid negotiated EE SIF receive buffer");
            m_receive = address;
            return;
        }
        require(option == 1 && packetSize == 16 && m_receive != 0, "Invalid SIF RPC initialization order");
        // This endpoint implements bind/call/other-data against actual backend routes.
        // Guest state is updated by its SET_SREG interrupt handler, never directly.
        Packet reply{};
        put(reply, 0, 24); put(reply, 2, SetSreg); put(reply, 4, 0); put(reply, 5, 1);
        emit(reply, 24, m_receive, true, backend);
        m_ready = true;
        return;
    }
    if (command == SetSreg) {
        require(m_receive != 0 && packetSize == 24 && dataSize == 0 && word(packet, 4) < m_sregs.size(),
                "Invalid incoming SIF software-register packet");
        m_sregs[word(packet, 4)] = word(packet, 5);
        return;
    }
    require(m_ready, "RPC command before SIF channel initialization");
    require(command == Bind || command == Call || command == Other, "Unsupported native SIF command");
    const uint32_t minimumSize = command == Bind ? 36 : command == Call ? 56 : 44;
    require(packetSize >= minimumSize && backend.validEe(word(packet, 5), 64), "Invalid RPC packet address or size");
    const uint32_t client = word(packet, 7);
    require(client != 0 && backend.validEe(client, command == Other ? 28 : 40), "Invalid RPC client range");
    Packet reply = completion(packet, command);

    if (command == Bind) {
        require(dataSize == 0 && word(packet, 1) == 0, "Unexpected RPC bind payload");
        const SifServer server = backend.bind(word(packet, 8));
        if (server.address) {
            require(server.buffer != 0 && server.capacity != 0, "Invalid backend RPC server");
            m_servers[server.address] = server;
        }
        put(reply, 9, server.address); put(reply, 10, server.buffer);
    } else if (command == Call) {
        auto entry = m_servers.find(word(packet, 13));
        require(entry != m_servers.end(), "RPC call to unbound server");
        const SifServer& server = entry->second;
        const uint32_t sendSize = word(packet, 9), receiveSize = word(packet, 11), receive = word(packet, 10);
        require(sendSize <= server.capacity && sendSize == dataSize &&
                (sendSize == 0 || word(packet, 1) == server.buffer), "Invalid RPC call payload");
        require(word(packet, 12) <= 1 && receiveSize <= 0xffffffu &&
                (receiveSize == 0 || backend.validEe(receive, receiveSize)), "Invalid RPC receive range or reply mode");
        require(backend.call(server, client, word(packet, 8), sendSize, receive, receiveSize),
                "RPC backend did not handle native call");
        if (word(packet, 12) == 0) {
            put(reply, 4, 0); put(reply, 6, 0);
            emit(reply, 64, word(packet, 5), false, backend);
            return;
        }
        put(reply, 0, 64 | (receiveSize << 8)); put(reply, 1, receiveSize ? receive : 0);
    } else {
        const uint32_t source = word(packet, 8), target = word(packet, 9), bytes = word(packet, 10);
        require(dataSize == 0 && bytes <= 0xffffffu && (bytes == 0 || backend.validEe(target, bytes)),
                "Invalid RPC other-data transfer");
        std::vector<uint8_t> data(bytes);
        require(backend.readIop(source, data.data(), bytes), "Invalid IOP other-data source");
        if (bytes) backend.writeEe(target, data.data(), bytes);
        put(reply, 0, 64 | (bytes << 8)); put(reply, 1, bytes ? target : 0);
    }
    emit(reply, 64, m_receive, true, backend);
}
}
