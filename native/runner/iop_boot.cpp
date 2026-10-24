#include "iop_boot.h"
#include "verify_input.h"
#include "bootstrap_session.h"
#include "strict_device_error.h"
#include <fstream>
#include <filesystem>
#include <iterator>
#include <ctime>
#include <charconv>
#include <cstdlib>
#include <string_view>

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
    bool reviewedRom{};
    std::filesystem::path romPath;
    explicit Impl(const IopBootInputs& inputs) : media(checkedIso(inputs.iso)) {
        auto bundle = checked(inputs.bundle, inputs.bundleHash, 512 * 1024);
        auto seed = checked(inputs.hardware, inputs.hardwareHash, 28);
        auto rom = checked(inputs.rom, inputs.romHash, 0x400000);
        reviewedRom = inputs.romHash == "ee7ae4cc152588b7da1dab17494a321ceafafaf799769baea4e8a28afa5044b1";
        romPath = inputs.rom;
        auto mecha = checked(inputs.mecha, inputs.mechaHash, 4);
        if (mecha.size() != 4) throw std::runtime_error("Mecha profile must contain four bytes");
        session = std::make_unique<BootstrapSession>(bundle, seed, std::move(rom), media,
            std::array<uint8_t, 4>{mecha[0], mecha[1], mecha[2], mecha[3]});
        std::cerr << "[cooperative-iop] all input hashes verified; one modeled cycle/instruction; no PAL timing claim\n";
    }
};
IopBoot::IopBoot(const IopBootInputs& inputs) : impl(std::make_unique<Impl>(inputs)) {}
IopBoot::~IopBoot() = default;
void IopBoot::videoEvent(bool start) { impl->session->videoEvent(start); }
void IopBoot::enableVirtualPad() { impl->session->enableVirtualPad(); }
void IopBoot::publishPadInput(const VirtualPadInput& input) { impl->session->publishPadInput(input); }
void IopBoot::enableEntryDevices() {
    if (!impl->reviewedRom) throw StrictDeviceError("IOPBOOT execution requires the exact reviewed BIOS SHA-256");
    // PCSX2 tries both companion names. Refuse even empty/unreadable entries:
    // this runner supports only the positively reviewed absent-ROM1 profile.
    auto replaced = impl->romPath; replaced.replace_extension(".rom1");
    for (const auto& candidate : {std::filesystem::path(impl->romPath.string() + ".rom1"), replaced}) {
        std::error_code error;
        const auto status = std::filesystem::symlink_status(candidate, error);
        if ((error && error != std::errc::no_such_file_or_directory) ||
            (status.type() != std::filesystem::file_type::not_found))
            throw StrictDeviceError("Reference absent-ROM1 profile requires both ROM1 companions to be absent");
    }
    uint32_t runtimeLimit = IopExecutionBudget::RuntimeLimit;
    if (const char* value = std::getenv("DRAKENGARD_IOP_RUNTIME_BUDGET")) {
        uint32_t millions{};
        const std::string_view text(value);
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), millions);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
            (millions != 256 && millions != 512 && millions != 1024))
            throw StrictDeviceError("Invalid DRAKENGARD_IOP_RUNTIME_BUDGET preset");
        runtimeLimit = millions * 1000000u;
    }
    impl->session->configureRuntimeBudget(runtimeLimit);
    std::cerr << "[iop-runtime-budget] instructions=" << runtimeLimit << "; fixed, never renewed by device polls\n";
    impl->session->enableEntryDevices();
    std::cerr << "[cooperative-rom1] verified absent companion profile; 4 MiB zero-backed read-only aperture\n";
}
void IopBoot::bindSif0(std::function<bool(std::span<const uint8_t>)> sink) {
    impl->session->bindSif0(std::move(sink));
}
void IopBoot::pumpSif0() {
    try { impl->session->pumpSif0(); }
    catch (const std::exception& error) { throw StrictDeviceError(error.what()); }
}
void IopBoot::enableSif1Receiver() { impl->session->enableSif1Receiver(); }
void IopBoot::feedSif1(std::span<const uint8_t> quantum) {
    try { impl->session->feedSif1(quantum); }
    catch (const std::exception& error) { throw StrictDeviceError(error.what()); }
}
void IopBoot::enableHostRtc() {
    impl->session->setClockSource([] {
        const auto now = std::time(nullptr);
        if (now == std::time_t(-1)) throw StrictDeviceError("Host RTC source unavailable");
        const std::time_t shifted = now + 9 * 60 * 60; // CDVD internal calendar is GMT+9, per pinned reference.
        std::tm date{};
#ifdef _WIN32
        if (gmtime_s(&date, &shifted)) throw StrictDeviceError("Host RTC conversion failed");
#else
        if (!gmtime_r(&shifted, &date)) throw StrictDeviceError("Host RTC conversion failed");
#endif
        return CdvdCalendar{date.tm_year + 1900, date.tm_mon + 1, date.tm_mday,
            date.tm_hour, date.tm_min, date.tm_sec};
    });
    std::cerr << "[cdvd-rtc] read-only host wall clock, CDVD GMT+9 calendar; no physical RTC/NVM claim\n";
}
void IopBoot::advance(uint32_t instructions) {
    try { impl->session->advance(instructions); }
    catch (const std::exception& error) { throw StrictDeviceError(error.what()); }
}
uint32_t IopBoot::readEe(uint32_t address) const {
    try { return impl->session->sif.readEe(address); }
    catch (const std::exception& error) { throw StrictDeviceError(error.what()); }
}
void IopBoot::writeEe(uint32_t address, uint32_t value) {
    try { impl->session->sif.writeEe(address, value); }
    catch (const std::exception& error) { throw StrictDeviceError(error.what()); }
}
}
