#include "cdvd_device.h"
#include "dvd_frame.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace drakengard {
namespace {
void deviceCheck(bool condition, const char* reason) { if (!condition) throw std::runtime_error(reason); }
uint32_t deviceWord(const std::vector<uint8_t>& bytes, size_t at) {
    deviceCheck(at <= bytes.size() && bytes.size() - at >= 4, "Short CDVD command word");
    return uint32_t(bytes[at]) | uint32_t(bytes[at + 1]) << 8 |
        uint32_t(bytes[at + 2]) << 16 | uint32_t(bytes[at + 3]) << 24;
}
[[noreturn]] void unknownDevice(const char* kind, uint8_t command, std::span<const uint8_t> parameters = {}) {
    std::ostringstream message;
    message << "Unsupported CDVD " << kind << " 0x" << std::hex << unsigned(command) << " parameters=";
    for (uint8_t byte : parameters) message << unsigned(byte) << ',';
    throw std::runtime_error(message.str());
}
}

uint8_t CdvdDevice::readRegister(uint8_t index) {
    switch (index) {
    case 0x04: return m_ncommand;
    case 0x05: return pendingBytes() ? 0x80 : 0x40;
    case 0x06: return std::exchange(m_error, uint8_t(0));
    case 0x07: return 0;
    case 0x08: return m_interrupt;
    case 0x0a: return m_status;
    case 0x0b: return m_traySticky;
    case 0x0f: return 0x14; // Explicit virtual PS2 DVD media type.
    case 0x13: return m_spinning ? uint8_t(((m_spindle & 0x3f) ? (m_spindle & 0x3f) : 3) + 0xf) : 0;
    case 0x16: return m_scommand;
    case 0x17: return m_responsePosition < m_response.size() ? 0 : 0x40;
    case 0x18:
        // Pinned functional controller returns zero without consuming anything
        // when SDATAOUT is empty. This is not a successful S-command response.
        if (m_responsePosition == m_response.size()) return 0;
        return m_response[m_responsePosition++];
    default: unknownDevice("register read", index);
    }
}

void CdvdDevice::writeRegister(uint8_t index, uint8_t value) {
    switch (index) {
    case 0x04: ncommand(value); return;
    case 0x05:
        deviceCheck(m_nparameters.size() < 16, "CDVD N-command parameter FIFO full");
        m_nparameters.push_back(value); return;
    case 0x06:
        // Pinned functional controller retains all eight HOWTO bits without
        // changing transfer state. Reads at this offset still consume ERROR.
        m_howTo = value; return;
    case 0x08: m_interrupt &= ~value; return;
    case 0x16: scommand(value); return;
    case 0x17:
        deviceCheck(m_sparameters.size() < 16, "CDVD S-command parameter FIFO full");
        m_sparameters.push_back(value); return;
    default: unknownDevice("register write", index);
    }
}

void CdvdDevice::complete() {
    deviceCheck(m_interrupt == 0, "Unacknowledged CDVD command completion");
    m_interrupt = 1; m_edge = true;
}

void CdvdDevice::ncommand(uint8_t command) {
    deviceCheck(!pendingBytes(), "CDVD N-command while actual read bytes remain pending");
    deviceCheck(m_interrupt == 0, "CDVD N-command before prior IRQ acknowledgement");
    const auto& args = m_nparameters;
    if (command <= 4) {
        deviceCheck(args.empty(), "Unexpected CDVD N-command parameters");
        if (command == 1) {
            deviceCheck(m_responsePosition == m_response.size(), "Reset with unconsumed CDVD response unsupported");
            m_response.clear(); m_responsePosition = 0; m_sparameters.clear();
        }
        if (command == 1 || command == 3) m_status = 0;
        else if (command == 2 || command == 4) m_status = 0x0a;
        if (command == 1 || command == 3) m_spinning = false;
        else if (command == 2) m_spinning = true;
        if (command == 2) m_sector = 0;
        complete();
    } else if (command == 5) {
        deviceCheck(args.size() == 4, "Invalid CDVD logical-seek parameters");
        const auto sector = deviceWord(args, 0);
        deviceCheck(sector < m_image.sectors(), "CDVD seek exceeds actual image");
        m_sector = sector; m_status = 0x0a; m_spinning = true; complete();
    } else if (command == 9) {
        deviceCheck(args.size() == 1 && args[0] == 0 && m_image.singleVolumeImage(),
            "Only reference single-volume DVD TOC command zero supported");
        std::array<uint8_t, 2048> toc;
        referenceDvdToc(m_image.sectors(), toc);
        m_payload.assign(toc.begin(), toc.end()); m_position = 0; m_toc = true; m_status = 6;
        std::cerr << "[cdvd-reference-toc] sectors=" << m_image.sectors() << " bytes=" << m_payload.size() << '\n';
    } else if (command == 6 || command == 8) {
        deviceCheck(args.size() == 11 && args[10] == 0, "Only actual 2048-byte CDVD user-sector reads supported");
        const uint32_t sector = deviceWord(args, 0), count = deviceWord(args, 4);
        deviceCheck(count && uint64_t(count) * 2048 <= 32 * 1024 * 1024, "CDVD read count exceeds supported bound");
        deviceCheck((args[9] & 0x7f) <= 3, "Unsupported virtual DVD spindle profile");
        const bool dvd = command == 8;
        if (dvd) {
            deviceCheck(m_image.singleVolumeImage(), "Reference DVD frames require a complete single-volume ISO");
            deviceCheck((args[9] & 0x7f) <= 3 && uint64_t(sector) + count <= 0xfd0000u,
                "Unsupported reference DVD spindle or frame range");
        }
        std::vector<uint8_t> bytes(size_t(count) * 2048);
        deviceCheck(m_image.readSectors(sector, count, bytes), "CDVD read exceeds actual image; no zero padding");
        if (dvd) {
            std::vector<uint8_t> frames(size_t(count) * 2064);
            for (uint32_t n = 0; n < count; ++n)
                referenceDvdFrame(sector + n, std::span(bytes).subspan(size_t(n) * 2048, 2048),
                    std::span(frames).subspan(size_t(n) * 2064, 2064));
            bytes = std::move(frames);
        }
        m_sectorSize = dvd ? 2064 : 2048;
        m_spindle = (args[9] & 0x3f) ? args[9] : uint8_t((args[9] & 0x80) | 3);
        m_spinning = true;
        m_payload = std::move(bytes); m_position = 0; m_sector = sector; m_status = 6;
        m_actualSectors += count;
        std::cerr << (dvd ? "[cdvd-reference-dvd-read] lsn=" : "[cdvd-user-read] lsn=") << sector << " sectors=" << count
                  << " bytes=" << m_payload.size() << '\n';
    } else {
        // Physical raw DVD, key/TOC/auth commands have no generic ISO fallback.
        unknownDevice("N-command", command, args);
    }
    m_ncommand = command; m_nparameters.clear();
}

void CdvdDevice::scommand(uint8_t command) {
    deviceCheck(m_responsePosition == m_response.size(), "CDVD S-command with an unconsumed reply");
    if (command == 3 && m_sparameters.size() == 1 && m_sparameters[0] == 0 && m_mecha) {
        // Caller-supplied reference device property; no hardcoded SCE/physical revision.
        m_response.assign(m_mecha->begin(), m_mecha->end()); m_responsePosition = 0;
    } else if (command == 5 && m_sparameters.empty()) {
        // Acknowledge tray detection by latching the current closed-tray bit.
        // Only the fixed, closed ISO drive is modeled; this does not open/close
        // a tray, complete pending sectors or acknowledge a command IRQ.
        deviceCheck(!(m_status & 1u), "CDVD tray detection requires the closed virtual-drive profile");
        m_traySticky = m_status & 1u;
        m_response = {0}; m_responsePosition = 0;
    } else if (command == 8 && m_sparameters.empty() && m_clock) {
        const auto date = m_clock();
        deviceCheck(date.year >= 2000 && date.year <= 2099 && date.month >= 1 && date.month <= 12 &&
            date.hour >= 0 && date.hour <= 23 && date.minute >= 0 && date.minute <= 59 &&
            date.second >= 0 && date.second <= 59, "CDVD clock calendar outside supported range");
        constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        const bool leap = date.year % 400 == 0 || (date.year % 4 == 0 && date.year % 100 != 0);
        deviceCheck(date.day >= 1 && date.day <= days[date.month - 1] + (date.month == 2 && leap),
            "Invalid CDVD clock calendar day");
        const auto bcd = [](int value) { return uint8_t((value / 10) * 16 + value % 10); };
        m_response = {0, bcd(date.second), bcd(date.minute), bcd(date.hour), 0,
            bcd(date.day), bcd(date.month), bcd(date.year - 2000)};
        m_responsePosition = 0;
    } else if (command == 0x1b && m_sparameters.empty()) {
        // This virtual drive has no external power-button event source. In its
        // quiescent state cancel-readiness is idempotent (zero inputs, one status
        // byte in pinned CDVD/PS2SDK). An actual power event needs its own model;
        // do not silently consume that IRQ if one is introduced later.
        deviceCheck(!(m_interrupt & 4u), "CDVD power-event cancellation requires an event model");
        m_response = {0}; m_responsePosition = 0;
    } else unknownDevice("S-command", command, m_sparameters);
    m_scommand = command; m_sparameters.clear();
}

size_t CdvdDevice::drainUserSectors(std::span<uint8_t> destination) {
    deviceCheck(m_sectorSize == 2048, "User-sector DMA cannot consume framed DVD data");
    return drainDmaSectors(destination);
}

size_t CdvdDevice::drainDmaSectors(std::span<uint8_t> destination) {
    deviceCheck(!m_toc, "DVD TOC requires its separate transfer contract");
    deviceCheck(!destination.empty() && destination.size() % m_sectorSize == 0 && destination.size() <= pendingBytes(),
                "CDVD DMA must consume actual complete pending sectors");
    std::copy_n(m_payload.begin() + m_position, destination.size(), destination.begin());
    m_position += destination.size(); m_sector += static_cast<uint32_t>(destination.size() / m_sectorSize);
    if (!pendingBytes()) {
        m_payload.clear(); m_position = 0; m_status = 0x0a; complete();
    }
    return destination.size();
}

size_t CdvdDevice::drainToc(std::span<uint8_t> destination) {
    deviceCheck(m_toc && destination.size() == 2048 && pendingBytes() == 2048,
        "DVD TOC DMA requires its complete actual reference structure");
    std::copy(m_payload.begin(), m_payload.end(), destination.begin());
    m_payload.clear(); m_position = 0; m_toc = false; m_status = 0x0a; complete();
    return destination.size();
}

bool CdvdDevice::takeInterruptEdge() { return std::exchange(m_edge, false); }
}
