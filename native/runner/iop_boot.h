#pragma once
#include <cstdint>
#include <memory>
#include <string>

namespace drakengard {
struct IopBootInputs {
    std::string bundle, bundleHash, hardware, hardwareHash, rom, romHash, mecha, mechaHash, iso;
};
class IopBoot {
public:
    explicit IopBoot(const IopBootInputs& inputs);
    ~IopBoot();
    void advance(uint32_t instructions);
    void enableSif1Receiver();
    void enableHostRtc();
    uint32_t readEe(uint32_t address) const;
    void writeEe(uint32_t address, uint32_t value);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
