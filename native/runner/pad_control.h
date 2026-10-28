#pragma once

#include <charconv>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace drakengard {

// Strict, host-side pad command consumer.  The command is deliberately a
// value protocol; it never talks to a window, keyboard, or guest RAM.
struct PadControlSample {
    uint16_t buttons{0xffff};
    std::array<uint8_t,4> axes{127,127,127,127}; // RX,RY,LX,LY
};
class PadControl {
    uint64_t lastSequence{};
    uint16_t lastButtons{0xffff};
    uint64_t lastHold{};
    std::array<uint8_t,4> lastAxes{127,127,127,127};
    uint64_t deadline{};
    uint64_t lastTime{};
    bool haveSequence{};
    bool haveTime{};

    static uint64_t decimal(std::string_view value, const char* what) {
        if (value.empty()) throw std::runtime_error(what);
        uint64_t result{};
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result, 10);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
            throw std::runtime_error(what);
        return result;
    }

    static uint16_t hex4(std::string_view value) {
        if (value.size() != 4) throw std::runtime_error("invalid pad button mask");
        uint16_t result{};
        for (const char digit : value) {
            unsigned nibble{};
            if (digit >= '0' && digit <= '9') nibble = unsigned(digit - '0');
            else if (digit >= 'a' && digit <= 'f') nibble = unsigned(digit - 'a' + 10);
            else if (digit >= 'A' && digit <= 'F') nibble = unsigned(digit - 'A' + 10);
            else throw std::runtime_error("invalid pad button mask");
            result = uint16_t((result << 4) | nibble);
        }
        return result;
    }

public:
    static constexpr std::size_t MaxCommandBytes = 128;
    // Accepts exactly: D1PAD1 <uint64 decimal> <four hex digits> <hold ms>\n.
    // D1PAD2 adds eight hex digits RXRYLXLY after hold, separated by a space.
    // Buttons are active-low. A duplicate sequence never extends its deadline.
    PadControlSample sampleInput(std::string_view text, uint64_t monotonicMilliseconds) {
        if (text.size() > MaxCommandBytes || text.size() < 8 || text.back() != '\n')
            throw std::runtime_error("invalid pad command framing");
        if (haveTime && monotonicMilliseconds < lastTime)
            throw std::runtime_error("pad clock moved backwards");
        text.remove_suffix(1);
        const bool analog=text.substr(0,7)=="D1PAD2 ";
        if (!analog && text.substr(0, 7) != "D1PAD1 ") throw std::runtime_error("invalid pad command prefix");
        text.remove_prefix(7);

        const auto first = text.find(' ');
        if (first == std::string_view::npos) throw std::runtime_error("invalid pad command fields");
        const uint64_t sequence = decimal(text.substr(0, first), "invalid pad sequence");
        text.remove_prefix(first + 1);
        const auto second = text.find(' ');
        if (second == std::string_view::npos) throw std::runtime_error("invalid pad command fields");
        const uint16_t buttons = hex4(text.substr(0, second));
        text.remove_prefix(second+1);
        std::array<uint8_t,4> axes{127,127,127,127};
        auto holdText=text;
        if(analog) {
            const auto separator=text.find(' ');
            if(separator==std::string_view::npos)throw std::runtime_error("missing pad axes");
            holdText=text.substr(0,separator);
            const auto axisText=text.substr(separator+1);
            if(axisText.size()!=8)throw std::runtime_error("invalid pad axes");
            const auto right=hex4(axisText.substr(0,4)),left=hex4(axisText.substr(4,4));
            axes={uint8_t(right>>8),uint8_t(right),uint8_t(left>>8),uint8_t(left)};
        }
        const uint64_t hold = decimal(holdText, "invalid pad hold");
        if (hold > 10000) throw std::runtime_error("pad hold is out of range");
        if (!hold && (buttons != 0xffff || axes!=PadControlSample{}.axes)) throw std::runtime_error("zero hold must be neutral");

        if (haveSequence && sequence < lastSequence)
            throw std::runtime_error("stale pad sequence");
        if (haveSequence && sequence == lastSequence) {
            if (buttons != lastButtons || hold != lastHold || axes != lastAxes)
                throw std::runtime_error("pad sequence payload changed");
            lastTime = monotonicMilliseconds;
            haveTime = true;
            return monotonicMilliseconds < deadline ? PadControlSample{lastButtons,lastAxes} : PadControlSample{};
        }

        lastSequence = sequence;
        lastButtons = buttons;
        lastHold = hold;
        lastAxes = axes;
        deadline = hold > (std::numeric_limits<uint64_t>::max)() - monotonicMilliseconds
            ? (std::numeric_limits<uint64_t>::max)() : monotonicMilliseconds + hold;
        haveSequence = true;
        lastTime = monotonicMilliseconds;
        haveTime = true;
        return monotonicMilliseconds < deadline ? PadControlSample{buttons,axes} : PadControlSample{};
    }
    uint16_t sample(std::string_view text,uint64_t now) { return sampleInput(text,now).buttons; }
};

}
