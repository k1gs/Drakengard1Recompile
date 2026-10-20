#pragma once
#include "iso_media.h"
#include <array>
#include <optional>
#include <span>
#include <vector>
#include <functional>

namespace drakengard {
struct CdvdCalendar { int year, month, day, hour, minute, second; };
// Synchronous virtual DVD controller. Command acceptance is separate from actual
// payload consumption and completion IRQ; unsupported formats/commands fail closed.
class CdvdDevice {
public:
    explicit CdvdDevice(IsoMedia& image, std::optional<std::array<uint8_t, 4>> mecha = std::nullopt)
        : m_image(image), m_mecha(mecha) {}
    uint8_t readRegister(uint8_t index);
    void writeRegister(uint8_t index, uint8_t value);
    size_t drainUserSectors(std::span<uint8_t> destination);
    bool takeInterruptEdge();
    void setClockSource(std::function<CdvdCalendar()> source) { m_clock = std::move(source); }
    size_t pendingBytes() const { return m_payload.size() - m_position; }
    uint64_t actualSectorsRead() const { return m_actualSectors; }
private:
    void ncommand(uint8_t command);
    void scommand(uint8_t command);
    void complete();
    IsoMedia& m_image;
    std::optional<std::array<uint8_t, 4>> m_mecha;
    std::function<CdvdCalendar()> m_clock;
    uint8_t m_ncommand{}, m_scommand{}, m_error{}, m_interrupt{}, m_status{0x0a}, m_traySticky{};
    uint8_t m_howTo{};
    uint32_t m_sector{};
    bool m_edge{};
    uint64_t m_actualSectors{};
    std::vector<uint8_t> m_nparameters, m_sparameters, m_response, m_payload;
    size_t m_responsePosition{}, m_position{};
};
}
