#pragma once

#include "ps2_runtime.h"
#include "ee_write_watch.h"
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace drakengard {
inline uint32_t eeDiagnosticReg(const R5900Context& context,unsigned index) { return getRegU32(&context,index); }
inline bool eeDiagnosticInvalidPc(uint32_t pc) { return pc!=0 && (pc<0x100u || (pc&3u)!=0); }
inline void diagnoseEeTransfer(const char* phase,uint32_t sourceEntry,uint32_t entryPc,
                               uint32_t entryRa,uint32_t entrySp,uint8_t* rdram,
                               const R5900Context& context) {
    if (!eeDiagnosticInvalidPc(context.pc)) return;
    static std::atomic<unsigned> reports{};
    if (reports.fetch_add(1,std::memory_order_relaxed)>=16) return;
    dumpEeWatchedWrites();
    std::cerr << "[ee-transfer-invalid] phase=" << phase << " source=0x" << std::hex << sourceEntry
              << " entryPc=0x" << entryPc << " entryRa=0x" << entryRa << " entrySp=0x" << entrySp
              << " currentPc=0x" << context.pc << " currentRa=0x" << eeDiagnosticReg(context,31)
              << " currentSp=0x" << eeDiagnosticReg(context,29);
    for (unsigned r=0;r<32;++r) std::cerr << " gpr" << r << "=0x" << eeDiagnosticReg(context,r);
    std::cerr << std::dec << '\n';
    const uint32_t stack=eeDiagnosticReg(context,29)&0x1fffffffu;
    if (rdram && stack<=32u*1024u*1024u-0x100u) {
        std::cerr << "[ee-transfer-invalid-stack] address=0x" << std::hex << stack << " bytes=256\n";
        for (unsigned offset=0;offset<0x100;offset+=4) {
            uint32_t value{}; std::memcpy(&value,rdram+stack+offset,sizeof(value));
            std::cerr << "  +0x" << offset << "=0x" << value << (((offset&0x1c)==0x1c)?'\n':' ');
        }
        std::cerr << std::dec;
    }
    const char* output=std::getenv("DRAKENGARD_EE_FAILURE_OUTPUT");
    static std::atomic<bool> dumped{};
    if (!rdram || !output || dumped.exchange(true,std::memory_order_acq_rel)) return;
    const std::filesystem::path path(output);
    if (std::filesystem::exists(path)) return;
    std::ofstream file(path,std::ios::binary);
    if (!file) return;
    file.write(reinterpret_cast<const char*>(rdram),32u*1024u*1024u);
    if (file) std::cerr << "[ee-transfer-invalid-dump] path=" << path.string() << " bytes=33554432\n";
}
}
