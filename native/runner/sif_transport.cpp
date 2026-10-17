#include "sif_transport.h"
#include "sif_channel.h"
#include "fileio_rpc.h"
#include "cdvd_rpc.h"
#include "iop_bridge.h"
#include "runtime/ps2_memory.h"
#include "ps2_iop_transport.h"
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <cstdio>
#include <limits>

namespace drakengard {
namespace {
struct Copy { uint32_t source{}, size{}; };
enum class Provider { Iop, FileIo, Cdvd };
struct Binding { uint32_t sid{}; Provider provider{}; SifServer server; };

class RuntimeBackend final : public SifBackend, public FileIoHost, public CdvdHost {
public:
    explicit RuntimeBackend(PS2Runtime* value) : runtime(value) {}
    PS2Runtime* runtime;
    uint8_t* ram{};
    SifChannel channel;
    bool interrupt{};
    unsigned logs{};
    bool rebooted{};
    std::unordered_map<uint32_t, Copy> copies;
    std::unordered_map<uint32_t, Binding> bindings;
    std::unique_ptr<IsoMedia> media;
    std::unique_ptr<CdvdRpc> drive;

    void rebootIop(std::string_view arguments, uint32_t mode) override {
        std::cerr << "[iop-reboot-request] mode=" << mode << " bytes=" << arguments.size()
                  << " arguments=";
        for (unsigned char byte : arguments) {
            if (byte >= 0x20 && byte <= 0x7e) std::cerr << char(byte);
            else std::cerr << "<" << unsigned(byte) << ">";
        }
        std::cerr << '\n';
        activateCooperativeIop(arguments, mode);
        rebooted = true; channel.reset(); copies.clear(); bindings.clear(); interrupt = false;
    }

    int32_t fileOpen(const std::string& path, uint32_t flags) override {
        const auto& paths = PS2Runtime::getIoPaths();
        const int32_t descriptor = runtime->vfs().open(path, flags,
            {paths.hostRoot, paths.cdRoot, paths.mcRoot}, runtime->romDevice());
        std::cerr << "[fileio-open] path=" << path << " fd=" << descriptor << '\n';
        return descriptor;
    }
    int32_t fileClose(int32_t descriptor) override { return runtime->vfs().close(descriptor); }
    int32_t fileRead(int32_t descriptor, uint32_t address, uint32_t bytes) override {
        if (!validEe(address, bytes)) throw std::runtime_error("Invalid FileIO read target");
        const int64_t result = runtime->vfs().read(descriptor, ram + (address & 0x1fffffffu), bytes);
        if (result > int64_t(bytes)) throw std::runtime_error("FileIO read exceeded request");
        std::cerr << "[fileio-read] fd=" << descriptor << " bytes=" << bytes << " result=" << result << '\n';
        return static_cast<int32_t>(result);
    }
    int32_t fileSeek(int32_t descriptor, int32_t offset, uint32_t whence) override {
        if (whence > 2) throw std::runtime_error("Invalid FileIO seek origin");
        const int origins[] = {SEEK_SET, SEEK_CUR, SEEK_END};
        const int64_t result = runtime->vfs().seek(descriptor, offset, origins[whence]);
        if (result > std::numeric_limits<int32_t>::max())
            throw std::runtime_error("Unsupported FileIO position above signed 32-bit range");
        return static_cast<int32_t>(result);
    }

    bool validEe(uint32_t address, size_t size) override {
        const uint32_t physical = address & 0x1fffffffu;
        const bool direct = address < PS2_RAM_SIZE ||
            (address >= 0x20000000u && address < 0x40000000u) ||
            (address >= 0x80000000u && address < 0xc0000000u);
        return ram && direct && physical < PS2_RAM_SIZE && size <= PS2_RAM_SIZE - physical &&
               size <= uint64_t(UINT32_MAX) + 1 - address;
    }
    bool readEe(uint32_t address, void* data, size_t size) override {
        if (!validEe(address, size)) return false;
        if (size) std::memcpy(data, ram + (address & 0x1fffffffu), size);
        return true;
    }
    bool readIop(uint32_t address, void* data, size_t size) override {
        return size == 0 || runtime->readIopMemory(address, data, size);
    }
    void writeEe(uint32_t address, const void* data, size_t size) override {
        if (!validEe(address, size)) throw std::runtime_error("Invalid EE SIF write range");
        if (size) std::memcpy(ram + (address & 0x1fffffffu), data, size);
    }
    void replyEe(uint32_t address, const void* data, size_t size, bool raiseInterrupt) override {
        writeEe(address, data, size);
        interrupt |= raiseInterrupt;
        if (logs++ < 64)
            std::cerr << "[sif-reply] destination=0x" << std::hex << address
                      << " bytes=0x" << size << " irq=" << raiseInterrupt << std::dec << '\n';
    }
    SifServer bind(uint32_t sid) override {
        auto previous = bindings.find(sid);
        if (previous != bindings.end()) return previous->second.server;
        const bool nativeRoute = PS2IopTransport::canBindRpc(runtime, sid);
        Provider provider = nativeRoute ? Provider::Iop : sid == 0x80000001u ? Provider::FileIo : Provider::Cdvd;
        bool available = nativeRoute || sid == 0x80000001u;
        if (!available && CdvdRpc::supports(sid) && !PS2Runtime::getIoPaths().cdImage.empty()) {
            if (!media) {
                media = std::make_unique<IsoMedia>(PS2Runtime::getIoPaths().cdImage);
                drive = std::make_unique<CdvdRpc>(*media);
                std::cerr << "[cdvd-media] actual sectors=" << media->sectors()
                          << " files=" << media->fileCount() << '\n';
            }
            available = true;
        }
        if (logs++ < 64)
            std::cerr << "[sif-bind] sid=0x" << std::hex << sid
                      << " available=" << available << std::dec << '\n';
        if (!available) return {};
        constexpr uint32_t capacity = 0x10000;
        const uint32_t record = runtime->allocateIopMemory(80, 16);
        if (!record) throw std::runtime_error("Cannot allocate IOP RPC server record");
        const uint32_t buffer = runtime->allocateIopMemory(capacity, 64);
        if (!buffer) {
            runtime->freeIopMemory(record);
            throw std::runtime_error("Cannot allocate IOP RPC receive buffer");
        }
        // Opaque HLE server handle backed by real, separate IOP RAM.
        uint32_t words[20]{};
        words[0] = sid; words[2] = buffer;
        if (!runtime->writeIopMemory(record, words, sizeof(words)))
            throw std::runtime_error("Cannot initialize IOP RPC server record");
        Binding binding{sid, provider, {record, buffer, capacity}};
        bindings.emplace(sid, binding);
        return binding.server;
    }
    bool call(const SifServer& server, uint32_t client, uint32_t function,
              uint32_t sendSize, uint32_t receive, uint32_t receiveSize) override {
        const Binding* binding = nullptr;
        for (const auto& item : bindings)
            if (item.second.server.address == server.address) binding = &item.second;
        if (!binding) throw std::runtime_error("Unknown native RPC binding");
        uint32_t source = 0;
        std::vector<uint8_t> snapshot(sendSize);
        if (sendSize) {
            auto copy = copies.find(server.buffer & 0x1fffffffu);
            if (copy == copies.end() || copy->second.size < sendSize ||
                !validEe(copy->second.source, sendSize))
                throw std::runtime_error("RPC payload lacks a verified EE-to-IOP transfer");
            source = copy->second.source;
            if (!readIop(server.buffer, snapshot.data(), sendSize) ||
                std::memcmp(snapshot.data(), ram + (source & 0x1fffffffu), sendSize) != 0)
                throw std::runtime_error("RPC copied payload differs from its EE source");
            copies.erase(copy);
        }
        if (binding->provider == Provider::FileIo) {
            if (logs++ < 64)
                std::cerr << "[fileio-rpc] function=0x" << std::hex << function
                          << " send=0x" << sendSize << " receive=0x" << receiveSize << std::dec << '\n';
            if (receiveSize < 4 || receiveSize > 16)
                throw std::runtime_error("Unsupported FileIO RPC result size");
            const auto result = fileIoRpc(function, snapshot, *this);
            if (!result) throw std::runtime_error("Unsupported native FileIO RPC function");
            std::vector<uint8_t> response(receiveSize, 0);
            for (unsigned byte = 0; byte < 4; ++byte) response[byte] = uint8_t(uint32_t(*result) >> (byte * 8));
            writeEe(receive, response.data(), response.size());
            if (function == 0xff)
                std::cerr << "[fileio-rpc] newer callback-buffer extension rejected; no version advertised\n";
            return true;
        }
        if (binding->provider == Provider::Cdvd) {
            if (!drive || receiveSize > 64) throw std::runtime_error("Unsupported CDVD RPC reply shape");
            std::vector<uint8_t> response(receiveSize, 0);
            const auto previousReads = drive->readCount();
            const auto previousSectors = drive->sectorsRead();
            const bool handled = drive->call(binding->sid, function, snapshot, response, *this);
            if (logs++ < 64)
                std::cerr << "[cdvd-rpc] sid=0x" << std::hex << binding->sid
                          << " function=0x" << function << " send=0x" << sendSize
                          << " receive=0x" << receiveSize << " handled=" << handled << std::dec << '\n';
            if (!handled) return false;
            if (receiveSize) writeEe(receive, response.data(), response.size());
            if (drive->readCount() != previousReads && drive->readCount() <= 512)
                std::cerr << "[cdvd-read] actual lsn=" << drive->lastLsn()
                          << " sectors=" << drive->sectorsRead() - previousSectors << '\n';
            return true;
        }
        ps2x::iop::RpcRequest request{};
        request.clientAddress = client; request.serverAddress = server.address;
        request.serverBuffer = server.buffer; request.sid = binding->sid;
        request.function = function;
        request.send = {source, sendSize}; request.receive = {receive, receiveSize};
        const auto result = PS2IopTransport::handleRpc(runtime, ram, nullptr, request);
        if (logs++ < 64)
            std::cerr << "[sif-call] sid=0x" << std::hex << binding->sid
                      << " function=0x" << function << " handled=" << result.handled << std::dec << '\n';
        if (!result.handled || result.guestFunction != 0) return false;
        if (receiveSize && result.resultAddress != receive) {
            if (!result.resultAddress || !validEe(result.resultAddress, receiveSize))
                throw std::runtime_error("RPC service did not provide a valid result range");
            std::memmove(ram + (receive & 0x1fffffffu), ram + (result.resultAddress & 0x1fffffffu), receiveSize);
        }
        return true;
    }
};

std::recursive_mutex channelsMutex;
std::unordered_map<PS2Runtime*, std::shared_ptr<RuntimeBackend>> channels;
}

void processSifTransfer(PS2Runtime* runtime, uint8_t* ram, const ps2x::iop::SifTransfer& transfer) {
    if (!runtime || transfer.kind != ps2x::iop::SifTransferKind::SetDma ||
        transfer.phase != ps2x::iop::SifTransferPhase::AfterCopy) return;
    std::lock_guard lock(channelsMutex);
    auto state = channels[runtime];
    if (!state) {
        state = std::make_shared<RuntimeBackend>(runtime);
        channels[runtime] = state;
    }
    state->ram = ram;
    if (state->rebooted) throw std::runtime_error("Post-reset EE-to-IOP DMA requires strict IOP transfer routing");
    for (const auto& binding : state->bindings)
        if ((binding.second.server.buffer & 0x1fffffffu) == (transfer.destinationAddress & 0x1fffffffu) &&
            transfer.size <= binding.second.server.capacity)
            state->copies[transfer.destinationAddress & 0x1fffffffu] = {transfer.sourceAddress, transfer.size};
    state->channel.receive(transfer.destinationAddress, transfer.size, *state);
}

bool takeSifReplyInterrupt(PS2Runtime* runtime) {
    std::lock_guard lock(channelsMutex);
    auto state = channels.find(runtime);
    if (state == channels.end()) return false;
    const bool pending = state->second->interrupt;
    state->second->interrupt = false;
    return pending;
}

void resetSifChannel(PS2Runtime* runtime) {
    std::lock_guard lock(channelsMutex);
    channels.erase(runtime);
}

void stopSifReceive(PS2Runtime* runtime) {
    if (!runtime) throw std::runtime_error("SIF0 stop without runtime");
    std::lock_guard lock(channelsMutex);
    auto& state = channels[runtime];
    if (!state) state = std::make_shared<RuntimeBackend>(runtime);
    if (state->interrupt) throw std::runtime_error("SIF0 stop with a pending reply interrupt");
    state->channel.stopReceive(*state);
    // PS2SDK identifies SIF0 CHCR at 0x1000c000 and STR as bit 8.
    runtime->memory().write32(0x1000c000u, runtime->memory().read32(0x1000c000u) & ~0x100u);
    std::cerr << "[sif0-stop] receive disabled; no pending reply\n";
}

void startSifReceive(PS2Runtime* runtime) {
    if (!runtime) throw std::runtime_error("SIF0 start without runtime");
    std::lock_guard lock(channelsMutex);
    auto& state = channels[runtime];
    if (!state) state = std::make_shared<RuntimeBackend>(runtime);
    state->channel.startReceive();
    // Packet transport models reception synchronously; no guessed DMA tag is installed.
    runtime->memory().write32(0x1000c000u, runtime->memory().read32(0x1000c000u) | 0x100u);
}
}
