#include "iop_boot.h"
#include "verify_input.h"
#include "bootstrap_session.h"
#include <fstream>
#include <filesystem>
#include <iterator>

namespace drakengard {
namespace {
const std::string& checkedIso(const std::string& path) {
    if (!verifyEuropeIso(path.c_str())) throw std::runtime_error("IOP disc SHA-256 refused");
    return path;
}
std::vector<uint8_t> checked(const std::string& path, const std::string& hash, size_t limit) {
    if (!verifyFileSha256(path.c_str(), hash.c_str())) throw std::runtime_error("IOP bootstrap input SHA-256 refused");
    const auto size = std::filesystem::file_size(path);
    if (size > limit) throw std::runtime_error("IOP bootstrap input size refused");
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open verified IOP input read-only");
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
    if (input.bad() || bytes.size() != size) throw std::runtime_error("IOP bootstrap input read failed");
    return bytes;
}
}
struct IopBoot::Impl {
    IsoMedia media;
    std::unique_ptr<BootstrapSession> session;
    explicit Impl(const IopBootInputs& inputs) : media(checkedIso(inputs.iso)) {
        auto bundle = checked(inputs.bundle, inputs.bundleHash, 512 * 1024);
        auto seed = checked(inputs.hardware, inputs.hardwareHash, 28);
        auto rom = checked(inputs.rom, inputs.romHash, 0x400000);
        auto mecha = checked(inputs.mecha, inputs.mechaHash, 4);
        if (mecha.size() != 4) throw std::runtime_error("Mecha profile must contain four bytes");
        session = std::make_unique<BootstrapSession>(bundle, seed, std::move(rom), media,
            std::array<uint8_t, 4>{mecha[0], mecha[1], mecha[2], mecha[3]});
        std::cerr << "[cooperative-iop] all input hashes verified; one modeled cycle/instruction; no PAL timing claim\n";
    }
};
IopBoot::IopBoot(const IopBootInputs& inputs) : impl(std::make_unique<Impl>(inputs)) {}
IopBoot::~IopBoot() = default;
void IopBoot::advance(uint32_t instructions) { impl->session->advance(instructions); }
uint32_t IopBoot::readEe(uint32_t address) const { return impl->session->sif.readEe(address); }
void IopBoot::writeEe(uint32_t address, uint32_t value) { impl->session->sif.writeEe(address, value); }
}
