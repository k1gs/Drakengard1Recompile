#include "ps2_runtime.h"
#include "verify_input.h"
#include "iop_bridge.h"
#include "firmware_functions.h"
#include "guest_frame_capture.h"
#include "host_pad_input.h"
#include "runtime/ee_scheduler.h"
#include <exception>
#include <iostream>
#include <memory>

int main(int argc, char** argv) {
    if ((argc != 2 && argc != 3 && argc != 11 && argc != 12) ||
        (argc == 12 && std::string(argv[11]) != "--ee-firmware-init" &&
                       std::string(argv[11]) != "--ee-firmware-entry")) {
        std::cerr << "Pass the locally verified boot ELF; use scripts/run_native.py\n";
        return 2;
    }
    try {
        if (!verifyEuropeElf(argv[1])) {
            std::cerr << "Native input refused: cannot hash file or wrong Europe ELF SHA-256\n";
            return 2;
        }
        std::cout << "Verified Europe ELF SHA-256" << std::endl;
        if (argc >= 3) {
            if (!verifyEuropeIso(argv[2])) {
                std::cerr << "Native input refused: cannot hash file or wrong Europe ISO SHA-256\n";
                return 2;
            }
            std::cout << "Verified Europe ISO SHA-256" << std::endl;
            PS2Runtime::IoPaths paths;
            paths.cdImage = argv[2];
            PS2Runtime::setIoPaths(paths);
        }
        if (argc >= 11) {
            drakengard::prepareCooperativeIop({argv[3], argv[4], argv[5], argv[6],
                argv[7], argv[8], argv[9], argv[10], argv[2]});
        }
        auto runtime = std::make_unique<PS2Runtime>();
        struct Diagnostics { unsigned frames{}; GuestFrameCapture capture; } diagnostics;
        runtime->setDebugUiCallbacks(
            [](PS2Runtime&, void*) {},
            [](PS2Runtime& rt, void* data) {
                auto& state = *static_cast<Diagnostics*>(data);
                pollVirtualPadInput();
                state.capture.observe(rt.gs());
                auto& ticks = state.frames;
                if (++ticks % 120 != 0) return;
                const auto snapshot = rt.eeScheduler().snapshot();
                std::cerr << "[diagnostic] frame=" << ticks << " sequence=" << snapshot.sequence
                          << " cycle=" << snapshot.eeCycle << " running=" << snapshot.runningThreadId;
                for (const auto& thread : snapshot.threads) {
                    std::cerr << " tid=" << thread.id << " pc=0x" << std::hex << thread.pc
                              << " ra=0x" << thread.ra << " sp=0x" << thread.sp << std::dec
                              << " state=" << static_cast<int>(thread.status)
                              << " wait=" << static_cast<int>(thread.waitReason);
                }
                std::cerr << std::endl;
            }, nullptr, &diagnostics);
        runtime->setMissingFunctionPolicy(PS2Runtime::MissingFunctionPolicy::Stop);
        if (!runtime->initialize("Drakengard Europe - diagnostic runtime")) return 3;
        pollVirtualPadInput();
        diagnostics.capture.enable(runtime->gs());
        if (!runtime->loadELF(argv[1])) return 4;
        if (argc == 12) {
            runFirmwareInitialization(*runtime, argv[7]);
            if (std::string(argv[11]) == "--ee-firmware-init") return 5;
            drakengard::enterGameAfterFirmware();
            std::cerr << "[firmware-game-entry] start loaded ELF after guest BOOTEND; experimental, not full BIOS boot\n";
        }
        try { runtime->run(); }
        catch (...) {
            try { diagnostics.capture.saveFinal(runtime->gs()); }
            catch (const std::exception& error) { std::cerr << "Final frame diagnostic failed: " << error.what() << '\n'; }
            throw;
        }
        diagnostics.capture.saveFinal(runtime->gs());
        std::cerr << "Diagnostic run ended; this is not a playable-port acceptance result\n";
        return 5;
    } catch (const std::exception& error) {
        std::cerr << "Native blocker: " << error.what() << '\n';
        return 6;
    }
}
