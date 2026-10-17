#include "cdvd_device.h"
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
    case 0x0b: return 0; // The indexed virtual image has a closed, unchanged tray.
    case 0x0f: return 0x14; // Explicit virtual PS2 DVD media type.
    case 0x16: return m_scommand;
    case 0x17: return m_responsePosition < m_response.size() ? 0 : 0x40;
    case 0x18:
        deviceCheck(m_responsePosition < m_response.size(), "CDVD response FIFO is empty");
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
        if (command == 2) m_sector = 0;
        complete();
    } else if (command == 5) {
        deviceCheck(args.size() == 4, "Invalid CDVD logical-seek parameters");
        const auto sector = deviceWord(args, 0);
        deviceCheck(sector < m_image.sectors(), "CDVD seek exceeds actual image");
        m_sector = sector; m_status = 0x0a; complete();
    } else if (command == 6) {
        deviceCheck(args.size() == 11 && args[10] == 0, "Only actual 2048-byte CDVD user-sector reads supported");
        const uint32_t sector = deviceWord(args, 0), count = deviceWord(args, 4);
        deviceCheck(count && uint64_t(count) * 2048 <= 32 * 1024 * 1024, "CDVD read count exceeds supported bound");
        std::vector<uint8_t> bytes(size_t(count) * 2048);
        deviceCheck(m_image.readSectors(sector, count, bytes), "CDVD read exceeds actual image; no zero padding");
        m_payload = std::move(bytes); m_position = 0; m_sector = sector; m_status = 6;
        m_actualSectors += count;
    } else {
        // Raw 2064-byte DVD frames, key/TOC/auth commands are not synthesized from plain ISO data.
        unknownDevice("N-command", command, args);
    }
    m_ncommand = command; m_nparameters.clear();
}

void CdvdDevice::scommand(uint8_t command) {
    deviceCheck(m_responsePosition == m_response.size(), "CDVD S-command with an unconsumed reply");
    if (command == 3 && m_sparameters.size() == 1 && m_sparameters[0] == 0 && m_mecha) {
        // Caller-supplied reference device property; no hardcoded SCE/physical revision.
        m_response.assign(m_mecha->begin(), m_mecha->end()); m_responsePosition = 0;
    } else unknownDevice("S-command", command, m_sparameters);
    m_scommand = command; m_sparameters.clear();
}

size_t CdvdDevice::drainUserSectors(std::span<uint8_t> destination) {
    deviceCheck(!destination.empty() && destination.size() % 2048 == 0 && destination.size() <= pendingBytes(),
                "CDVD DMA must consume actual complete pending sectors");
    std::copy_n(m_payload.begin() + m_position, destination.size(), destination.begin());
    m_position += destination.size(); m_sector += static_cast<uint32_t>(destination.size() / 2048);
    if (!pendingBytes()) {
        m_payload.clear(); m_position = 0; m_status = 0x0a; complete();
    }
    return destination.size();
}

bool CdvdDevice::takeInterruptEdge() { return std::exchange(m_edge, false); }
}
