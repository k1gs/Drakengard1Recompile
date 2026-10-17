#include "firmware_functions.h"
#include "iop_bridge.h"
#include "verify_input.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#ifdef DRAKENGARD_HAVE_FIRMWARE
#include <firmware_recompiled_functions.h>
namespace {
struct FirmwareEntry { uint32_t address; PS2Runtime::RecompiledFunction function; };
struct FirmwareCode { uint32_t address; const uint8_t* bytes; size_t size; };
#include <firmware_bindings.h>
void dispatchFirmware(uint8_t* ram, R5900Context* ctx, PS2Runtime* runtime) {
    verifyFirmwareInstructions(ram, ctx->pc);
    auto last = std::end(firmwareEntries);
    auto entry = std::lower_bound(std::begin(firmwareEntries), last, ctx->pc,
        [](const FirmwareEntry& item, uint32_t pc) { return item.address < pc; });
    if (entry == last || entry->address != ctx->pc) throw std::runtime_error("Unknown firmware entry");
    entry->function(ram, ctx, runtime);
}
}
PS2Runtime::RecompiledFunction lookupFirmwareFunction(uint32_t address) {
    auto last = std::end(firmwareEntries);
    auto entry = std::lower_bound(std::begin(firmwareEntries), last, address,
        [](const FirmwareEntry& item, uint32_t pc) { return item.address < pc; });
    return entry != last && entry->address == address ? &dispatchFirmware : nullptr;
}
void verifyFirmwareInstructions(const uint8_t* ram, uint32_t address) {
    if (!ram || address < 0x80000000 || address >= 0x80000000u + FirmwareSize)
        throw std::runtime_error("Firmware dispatch outside verified kernel");
    for (const auto& range : firmwareCode)
        if (std::memcmp(ram + (range.address & 0x1fffffffu), range.bytes, range.size))
            throw std::runtime_error("Firmware instruction bytes changed");
}
void runFirmwareInitialization(PS2Runtime& runtime, const char* path) {
    if (!verifyFileSha256(path, "ee7ae4cc152588b7da1dab17494a321ceafafaf799769baea4e8a28afa5044b1"))
        throw std::runtime_error("EE firmware requires the exact reviewed BIOS SHA-256");
    std::ifstream input(path, std::ios::binary);
    input.seekg(FirmwareRomOffset);
    auto* ram = runtime.memory().getRDRAM();
    input.read(reinterpret_cast<char*>(ram), FirmwareSize);
    if (!input || input.gcount() != FirmwareSize) throw std::runtime_error("EE firmware read failed");
    verifyFirmwareInstructions(ram, 0x80006240);
    drakengard::beginFirmwareExperiment();
    R5900Context context;
    context.pc = 0x80006240;
    // Harness call stack and return boundary, not a claimed BIOS reset state.
    context.r[29] = _mm_set_epi64x(0, 0x8007f000u);
    context.r[31] = _mm_set_epi64x(0, 0xfffffffcu);
    std::cerr << "[ee-firmware-init] execute original 0x80006240; harness stack=0x8007f000; no game entry yet\n";
    unsigned calls = 0;
    while (context.pc != 0xfffffffcu) {
        if (++calls > 10000) throw std::runtime_error("Firmware dispatch budget exhausted");
        auto function = lookupFirmwareFunction(context.pc);
        if (!function) {
            std::cerr << "[ee-firmware-blocker] pc=0x" << std::hex << context.pc
                      << " ra=0x" << getRegU32(&context, 31) << std::dec << '\n';
            throw std::runtime_error("Firmware left reviewed translated code");
        }
        function(ram, &context, &runtime);
    }
    std::cerr << "[ee-firmware-init] actual guest return; not a complete boot/menu acceptance\n";
    drakengard::finishFirmwareExperiment();
}
#else
PS2Runtime::RecompiledFunction lookupFirmwareFunction(uint32_t) { return nullptr; }
void verifyFirmwareInstructions(const uint8_t*, uint32_t) { throw std::runtime_error("Firmware lift is not prepared"); }
void runFirmwareInitialization(PS2Runtime&, const char*) { throw std::runtime_error("Prepare the verified private firmware lift first"); }
#endif
