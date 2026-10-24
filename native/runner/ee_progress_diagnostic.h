#pragma once
#include "ps2_runtime.h"
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <fstream>

struct EeProgressDiagnostic {
    uint64_t next{0x10000000};
    bool saved{};
    void observe(PS2Runtime& runtime,const R5900Context* context,uint64_t cycle) {
        if (cycle<next) return;
        next=cycle+0x10000000;
        if (!saved && cycle>=0x30000000) {
            saved=true;
            if (const auto* path=std::getenv("DRAKENGARD_EE_RAM_OUTPUT")) {
                if (std::filesystem::exists(path)) throw std::runtime_error("EE RAM diagnostic output already exists");
                std::ofstream file(path,std::ios::binary);
                file.write(reinterpret_cast<const char*>(runtime.memory().getRDRAM()),PS2_RAM_SIZE);
                if (!file) throw std::runtime_error("Could not save diagnostic EE RAM");
                std::cerr << "[ee-ram-diagnostic] cycle=" << cycle << " path=" << path << '\n';
            }
        }
        std::cerr << "[ee-graphics-progress] cycle=" << cycle << " csr=0x" << std::hex
                  << runtime.memory().gs().csr.load() << " imr=0x" << runtime.memory().gs().imr;
        const auto& display=runtime.memory().gs();
        std::cerr << " pmode=0x" << display.pmode << " smode2=0x" << display.smode2
                  << " dispfb1=0x" << display.dispfb1 << " display1=0x" << display.display1
                  << " dispfb2=0x" << display.dispfb2 << " display2=0x" << display.display2;
        for (uint32_t base:{0x10009000u,0x1000a000u}) for (uint32_t off:{0u,0x10u,0x20u,0x30u})
            std::cerr << " reg" << base+off << "=0x" << runtime.memory().read32(base+off);
        if (context) {
            std::cerr << " pc=0x" << context->pc;
            for (unsigned r:{2u,3u,4u,5u,6u,7u,8u,9u,10u,17u,18u,19u,31u}) std::cerr << " r" << std::dec << r << "=0x" << std::hex << getRegU32(context,r);
            auto* ram=runtime.memory().getRDRAM();
            for (unsigned r:{4u,17u}) {
                const auto base=getRegU32(context,r)&0x1fffffff;
                if (!base || (base&3) || base>PS2_RAM_SIZE-0x140) continue;
                for (unsigned off:{0x120u,0x124u,0x128u}) {
                    uint32_t value; std::memcpy(&value,ram+base+off,4);
                    std::cerr << " mem" << base+off << "=0x" << value;
                }
            }
        }
        std::cerr << std::dec << '\n';
    }
};
