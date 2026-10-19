#include "firmware_functions.h"
#include "iop_bridge.h"
#include "verify_input.h"
#include "strict_device_error.h"
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
    const auto initialControl = runtime.memory().read32(0x1000e000u);
    if (initialControl != 0 && initialControl != FirmwareInitialDmaControl)
        throw drakengard::StrictDeviceError("EE DMA control conflicts with the reviewed passive profile");
    // The component harness needs initialized passive hardware configuration.
    // Master/suspend configuration comes from a hash-verified reference; no
    // channel, tag, ready flag, IRQ or in-flight transfer state is imported.
    runtime.memory().write32(0x1000e000u, FirmwareInitialDmaControl);
    std::cerr << "[firmware-passive-ee] reviewed reference DMAE/suspend configuration; no channel/IRQ state imported\n";
    drakengard::bindFirmwareEeRam({ram, PS2_RAM_SIZE}, FirmwareInitialDmaEnable);
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
bool invokeFirmwareSifSyscall(uint8_t* ram, R5900Context* ctx, PS2Runtime* runtime, uint32_t number) {
    if (!drakengard::strictIopActive()) return false;
    if (!drakengard::firmwareGameEntryActive())
        throw drakengard::StrictDeviceError("Original firmware SIF DMA requires firmware-entry mode");
    if (!ram || !ctx || !runtime || (number != 0x76 && number != 0x77))
        throw drakengard::StrictDeviceError("Unsupported strict firmware SIF syscall context");
    // Verified BIOS table base 0x80014e80; adjacent slots are independently
    // checked against the source ROM during preparation. No guessed aliases.
    const uint32_t address = number == 0x77 ? 0x800067a0 : 0x80006958;
    uint32_t actual;
    std::memcpy(&actual, ram + 0x14e80 + number * 4, sizeof(actual));
    if (actual != address) throw drakengard::StrictDeviceError("Firmware SIF syscall table changed");
    R5900Context call = *ctx;
    call.pc = address;
    // Component-call harness, not a complete EE syscall exception/stack model.
    call.r[29] = _mm_set_epi64x(0, 0x8007f000u);
    call.r[31] = _mm_set_epi64x(0, 0xfffffffcu);
    std::cerr << "[firmware-sif-syscall] number=0x" << std::hex << number
              << " entry=0x" << address << " a0=0x" << getRegU32(ctx, 4)
              << " a1=0x" << getRegU32(ctx, 5) << std::dec << '\n';
    for (unsigned calls = 0; call.pc != 0xfffffffcu; ++calls) {
        if (calls >= 10000) throw drakengard::StrictDeviceError("Firmware SIF syscall dispatch bound exhausted");
        auto function = lookupFirmwareFunction(call.pc);
        if (!function) throw drakengard::StrictDeviceError("SIF syscall left verified translated firmware");
        try { function(ram, &call, runtime); }
        catch (const drakengard::StrictDeviceError&) { throw; }
        catch (const std::exception& error) { throw drakengard::StrictDeviceError(error.what()); }
    }
    ctx->r[2] = call.r[2]; // The original guest function supplies the DMA ID/status.
    std::cerr << "[firmware-sif-return] number=0x" << std::hex << number
              << " value=0x" << getRegU32(ctx, 2) << std::dec << '\n';
    drakengard::advanceFirmwareIop();
    return true;
}
#else
PS2Runtime::RecompiledFunction lookupFirmwareFunction(uint32_t) { return nullptr; }
void verifyFirmwareInstructions(const uint8_t*, uint32_t) { throw std::runtime_error("Firmware lift is not prepared"); }
void runFirmwareInitialization(PS2Runtime&, const char*) { throw std::runtime_error("Prepare the verified private firmware lift first"); }
bool invokeFirmwareSifSyscall(uint8_t*, R5900Context*, PS2Runtime*, uint32_t) {
    if (drakengard::strictIopActive()) throw drakengard::StrictDeviceError("Prepare the verified firmware DMA lift first");
    return false;
}
#endif
