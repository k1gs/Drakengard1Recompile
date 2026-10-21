#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <functional>
#include "../iop_probe/dualshock2.h"

namespace drakengard {
struct IopBootInputs {
    std::string bundle, bundleHash, hardware, hardwareHash, rom, romHash, mecha, mechaHash, iso;
};
class IopBoot {
public:
    explicit IopBoot(const IopBootInputs& inputs);
    ~IopBoot();
    void advance(uint32_t instructions);
    void videoEvent(bool start);
    void enableVirtualPad();
    void publishPadInput(const VirtualPadInput& input);
    void enableSif1Receiver();
    void feedSif1(std::span<const uint8_t> quantum);
    void bindSif0(std::function<bool(std::span<const uint8_t>)> sink);
    void pumpSif0();
    void enableEntryDevices();
    void enableHostRtc();
    uint32_t readEe(uint32_t address) const;
    void writeEe(uint32_t address, uint32_t value);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
