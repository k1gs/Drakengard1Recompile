#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <string>

// Wire order: Select,L3,R3,Start,Up,Right,Down,Left,L2,R2,L1,R1,Triangle,Circle,Cross,Square.
struct VirtualPadInput {
    bool connected{};
    uint16_t buttons{0xffff}; // Active low, sampled from the host input producer.
    std::array<uint8_t,4> axes{0x7f,0x7f,0x7f,0x7f}; // RX,RY,LX,LY.
    std::array<uint8_t,12> pressure{};
};

// Packet-level device, independent of SIO2 transport and host APIs. Protocol facts
// checked against PCSX2 v2.8.2 PadDualshock2/PadTypes; unknown commands fail closed.
class Dualshock2 {
    bool config{}, analog{}, locked{};
    uint8_t mode{0x41};
    std::array<uint8_t,6> motorMap{0xff,0xff,0xff,0xff,0xff,0xff};
    std::array<uint8_t,2> motors{};
public:
    std::vector<uint8_t> exchange(std::span<const uint8_t> tx,const VirtualPadInput& input) {
        if (tx.empty() || tx.size()>32 || tx[0]!=1)
            throw std::runtime_error("Unsupported DualShock packet framing: length="+std::to_string(tx.size())+
                                     " device="+std::to_string(tx.empty()?0:tx[0]));
        std::vector<uint8_t> rx(tx.size(),0);
        if (!input.connected) { std::fill(rx.begin(),rx.end(),0xff); return rx; }
        rx[0]=0xff;
        if (tx.size()==1) return rx;
        const auto cmd=tx[1];
        if (cmd!=0x42 && cmd!=0x43 && !config)
            throw std::runtime_error("DualShock configuration command outside configuration mode");
        rx[1]=config?0xf3:mode;
        if (tx.size()==2) return rx;
        rx[2]=0x5a;
        if (tx.size()==3 && (cmd==0x42 || cmd==0x43)) return rx;
        const auto put=[&](unsigned i,uint8_t v) { if (i<rx.size()) rx[i]=v; };
        const auto arg=[&](unsigned i) -> uint8_t {
            if (i>=tx.size()) throw std::runtime_error("Truncated DualShock configuration payload");
            return tx[i];
        };
        switch (cmd) {
        case 0x42:
            put(3,uint8_t(input.buttons)); put(4,uint8_t(input.buttons>>8));
            if (mode!=0x41) for (unsigned i=0;i<4;++i) put(5+i,input.axes[i]);
            if (mode==0x79) for (unsigned i=0;i<12;++i) put(9+i,input.pressure[i]);
            for (unsigned i=0;i<motorMap.size();++i) if (3+i<tx.size() && motorMap[i]<2)
                motors[motorMap[i]]=motorMap[i]==0 ? tx[3+i]&1 : tx[3+i];
            break;
        case 0x43:
            if (arg(3)>1) throw std::runtime_error("Unsupported DualShock config selector");
            config=tx[3]!=0; break;
        case 0x44:
            if (arg(3)>1 || (arg(4)!=0 && tx[4]!=3))
                throw std::runtime_error("Unsupported DualShock mode/lock selector");
            analog=tx[3]!=0; locked=tx[4]==3; mode=analog?0x73:0x41; break;
        case 0x45:
            put(3,3); put(4,2); put(5,analog); put(6,2); put(7,1); break;
        case 0x40: put(5,2); put(8,0x5a); break;
        case 0x41:
            if (mode!=0x41) { put(3,0xff); put(4,0xff); put(5,3); put(8,0x5a); }
            break;
        case 0x46:
            if (arg(3)>1) throw std::runtime_error("Unsupported DualShock actuator selector");
            put(5,1); put(6,tx[3]?1:2); put(7,tx[3]?1:0); put(8,tx[3]?0x14:0xa); break;
        case 0x47: put(5,2); put(7,1); break;
        case 0x4c:
            if (arg(3)>1) throw std::runtime_error("Unsupported DualShock mode-query selector");
            put(6,tx[3]?7:4); break;
        case 0x4d:
            for (unsigned i=0;i<motorMap.size();++i) if (3+i<tx.size()) {
                put(3+i,motorMap[i]); motorMap[i]=tx[3+i];
            }
            break;
        case 0x4f: {
            const uint32_t mask=arg(3)|(uint32_t(arg(4))<<8)|(uint32_t(arg(5))<<16);
            if (mask!=0 && mask!=0x3f && mask!=0x3ffff)
                throw std::runtime_error("Unsupported DualShock response mask");
            analog=mask!=0; mode=mask==0x3ffff?0x79:analog?0x73:0x41; put(8,0x5a); break;
        }
        default: throw std::runtime_error("Unsupported DualShock command "+std::to_string(cmd));
        }
        return rx;
    }
    uint8_t currentMode() const { return mode; }
    bool inConfig() const { return config; }
    const auto& motorRequests() const { return motors; } // No host rumble success implied.
};
