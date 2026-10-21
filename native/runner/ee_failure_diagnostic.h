#pragma once
#include "ps2_runtime.h"
#include <cstring>
#include <iostream>

// Called on the EE executor only, before a device exception unwinds its context.
// Raw guest state belongs in ignored runtime logs, never in release artifacts.
inline void diagnoseEeDeviceFailure(const R5900Context* context,const uint8_t* ram) {
    if (!context || !ram) return;
    std::cerr << "[ee-device-failure] pc=0x" << std::hex << context->pc;
    for (unsigned r=0;r<32;++r) std::cerr << " r" << std::dec << r << "=0x" << std::hex << getRegU32(context,r);
    std::cerr << std::dec << '\n';
    for (unsigned r:{4u,17u}) {
        const auto base=getRegU32(context,r)&0x1fffffff;
        if (base>PS2_RAM_SIZE-0x140 || (base&3)) continue;
        for (unsigned offset=0x100;offset<0x140;offset+=4) {
            uint32_t value; std::memcpy(&value,ram+base+offset,4);
            std::cerr << "[ee-device-operand] register=" << r << " address=0x" << std::hex
                      << base+offset << " value=0x" << value << std::dec << '\n';
        }
    }
}
