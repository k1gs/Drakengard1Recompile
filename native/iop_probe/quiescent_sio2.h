#pragma once
#include <cstdint>
#include <array>
#include <stdexcept>
#include <deque>
#include <iostream>
#include "dualshock2.h"

// Reset/setup registers are always the reviewed quiescent subset. The explicit
// virtual-pad profile adds bounded PIO/DMA packets and START/IRQ acknowledgement.
// PortStat 0xF is a reset register, not proof of an attached pad. Memory cards
// remain unattached: negative serial responses cannot read or write saves.
class QuiescentSio2 {
    bool enabled{}, configured{};
    std::array<uint32_t,8> portControls{};
    std::array<uint32_t,16> commands{};
    bool virtualPad{}, interruptEdge{};
    uint32_t control{0x3bc}, status{}, istat{}, packetIndex{}, packetCount{};
    std::vector<uint8_t> tx;
    std::deque<uint8_t> rx;
    Dualshock2 pad;
    VirtualPadInput input;
    uint64_t totalPackets{};
    static bool command(uint32_t address) {
        return address>=0x1f808200 && address<=0x1f80823c && !(address&3);
    }
    static bool portControl(uint32_t address) {
        return address>=0x1f808240 && address<=0x1f80825c && !(address&3);
    }
public:
    void enableVirtualPad() {
        if (virtualPad) throw std::runtime_error("Virtual pad already enabled");
        virtualPad=true;
    }
    void publishInput(const VirtualPadInput& sample) { input=sample; }
    bool takeInterruptEdge() { const bool value=interruptEdge; interruptEdge=false; return value; }
    void fifoWrite(uint8_t value) {
        if (!virtualPad || !configured || packetIndex>=commands.size())
            throw std::runtime_error("SIO2 TX outside a configured command queue");
        const uint32_t descriptor=commands[packetIndex];
        const auto length=(descriptor>>8)&0x3ffu;
        if (!length || length>32 || ((descriptor>>18)&0x3ffu)!=length)
            throw std::runtime_error("Unsupported SIO2 descriptor length/response size");
        tx.push_back(value);
        if (tx.size()<length) return;
        const unsigned port=descriptor&1;
        if (tx[0]==0x81) {
            // Unattached serial endpoint: every clock reads the pulled-up data
            // line. This is a negative response, never a card operation or auth success.
            if (rx.size()+tx.size()>1024) throw std::runtime_error("SIO2 RX capacity exceeded");
            rx.insert(rx.end(),tx.size(),0xff); status=0x1d100;
            ++packetIndex; ++packetCount; tx.clear(); return;
        }
        auto sample=input; if (port) sample.connected=false;
        const auto reply=pad.exchange(tx,sample);
        if (rx.size()+reply.size()>1024) throw std::runtime_error("SIO2 RX capacity exceeded");
        rx.insert(rx.end(),reply.begin(),reply.end());
        ++packetCount;
        status=(status&0x100u)?((status&~0x100u)|0x200u):(status|0x100u);
        status|=0x1000u;
        if (!sample.connected) status|=port?0x2d000u:0x1d000u;
        if (++totalPackets<=64 || totalPackets%256==0)
            std::cerr << "[sio2-pad-packet] port=" << port << " command=0x" << std::hex
                      << unsigned(tx.size()>1?tx[1]:0) << " reply_id=0x" << unsigned(reply.size()>1?reply[1]:0)
                      << " buttons=0x" << sample.buttons << std::dec << " bytes=" << length << '\n';
        ++packetIndex; tx.clear();
    }
    uint8_t fifoRead() {
        if (rx.empty()) throw std::runtime_error("SIO2 RX underflow");
        const auto value=rx.front(); rx.pop_front(); return value;
    }
    size_t availableBytes() const { return rx.size(); }
    void dmaBlock(std::span<const uint8_t> bytes) {
        if (!tx.empty() || packetIndex>=commands.size()) throw std::runtime_error("SIO2 DMA mixed with partial PIO packet");
        const auto descriptor=commands[packetIndex];
        const auto length=(descriptor>>8)&0x3ffu;
        if (!length || length>bytes.size()) throw std::runtime_error("SIO2 DMA block shorter than descriptor");
        if (bytes[0]==0x81 && ((length==4 && bytes[1]==0x11) || (length==5 && bytes[1]==0xf3)) && bytes.size()>=6 &&
            ((descriptor>>18)&0x3ffu)==length) {
            // No card is attached. Pinned Sio2::Memcard + MemoryCardProtocol::Probe
            // emits two leading FF bytes and four absent Probe/AuthF3 FF bytes, then DMA padding.
            if (rx.size()+bytes.size()>4096) throw std::runtime_error("SIO2 DMA RX capacity exceeded");
            rx.insert(rx.end(),6,0xff); rx.insert(rx.end(),bytes.size()-6,0);
            status=0x1d100; ++packetIndex; ++packetCount;
            if (++totalPackets<=64 || totalPackets%256==0)
                std::cerr << "[sio2-absent-card-probe] port=" << (descriptor&1) << "; no storage attached\n";
            return;
        }
        if (bytes[0]!=1) {
            std::cerr << "[sio2-dma-unhandled-device] descriptor=0x" << std::hex << descriptor << " tx=";
            for (unsigned i=0;i<std::min<size_t>(8,bytes.size());++i) std::cerr << unsigned(bytes[i]) << ' ';
            std::cerr << std::dec << '\n';
            throw std::runtime_error("Unsupported SIO2 DMA device protocol");
        }
        for (unsigned i=0;i<length;++i) fifoWrite(bytes[i]);
        if (rx.size()+bytes.size()-length>4096) throw std::runtime_error("SIO2 DMA RX capacity exceeded");
        rx.insert(rx.end(),bytes.size()-length,0); // Real DMA-block padding, not pad payload.
    }
    void enable() {
        if (enabled) throw std::runtime_error("SIO2 initialization profile cannot be enabled twice");
        enabled = true;
    }
    bool allows(uint32_t address, uint32_t opcode, uint32_t value) const {
        if (!enabled) return false;
        if (virtualPad && configured) {
            if (address==0x1f808260 && opcode==0x28) return true;
            if (address==0x1f808264 && (opcode==0x20 || opcode==0x24)) return true;
            if ((address==0x1f80826c || address==0x1f808274 || address==0x1f808280) && opcode==0x23) return true;
            if (address==0x1f808280 && opcode==0x2b) return true;
            if (address==0x1f808268 && opcode==0x2b && value==0x3bd) return true;
        }
        if (configured && command(address) && (opcode==0x23 || opcode==0x2b)) return true;
        if (configured && portControl(address) && (opcode==0x23 || opcode==0x2b)) return true;
        if (configured && address==0x1f808270 && opcode==0x23) return true;
        return address == 0x1f808268 &&
            ((opcode == 0x2b && value == 0x3bc) || (opcode == 0x23 && configured));
    }
    uint32_t read(uint32_t address) const {
        if (!allows(address,0x23,0)) throw std::runtime_error("Unsupported quiescent SIO2 read");
        if (address==0x1f80826c) return status;
        if (address==0x1f808274) return 0; // Pad-only transactions, no memory-card FIFO flags.
        if (address==0x1f808280) return istat;
        if (command(address)) return commands[(address-0x1f808200)/4];
        if (portControl(address)) return portControls[(address-0x1f808240)/4];
        return address==0x1f808270 ? 0xfu : control;
    }
    template<class Memory> void afterWrite(const Memory& memory,uint32_t address=0x1f808268) {
        if (configured && command(address)) {
            if (virtualPad && address==0x1f808200) {
                if (!tx.empty() || !rx.empty()) throw std::runtime_error("SIO2 queue reset with pending FIFO data");
                packetIndex=0; packetCount=0; status=0;
            }
            commands[(address-0x1f808200)/4]=memory.read32(address);
            return;
        }
        if (virtualPad && address==0x1f808260) { fifoWrite(uint8_t(memory.read32(address))); return; }
        if (virtualPad && address==0x1f808280) { istat&=~memory.read32(address); return; }
        if (configured && portControl(address)) {
            portControls[(address-0x1f808240)/4]=memory.read32(address);
            return;
        }
        if (address!=0x1f808268) throw std::runtime_error("Unsupported quiescent SIO2 store");
        const auto value=memory.read32(0x1f808268);
        if (virtualPad && value==0x3bd) {
            if (!tx.empty() || !packetIndex) throw std::runtime_error("SIO2 START requires completed packets");
            if (!(istat&1)) interruptEdge=true;
            istat|=1; control=value; return;
        }
        if (!enabled || value != 0x3bc)
            throw std::runtime_error("SIO2 reset control requires its actual completed CPU store");
        configured = true;
        control=value;
    }
};
