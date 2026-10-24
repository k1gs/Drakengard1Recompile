#pragma once

#include <charconv>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace drakengard {

// Strict, host-side pad command consumer.  The command is deliberately a
// value protocol; it never talks to a window, keyboard, or guest RAM.
class PadControl {
    uint64_t lastSequence{};
    uint16_t lastButtons{0xffff};
    uint64_t lastHold{};
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
    // Accepts exactly: D1PAD1 <uint64 decimal> <four hex digits> <hold ms>\n.
    // Buttons are active-low. A duplicate sequence never extends its deadline.
    uint16_t sample(std::string_view text, uint64_t monotonicMilliseconds) {
        if (text.size() > 128 || text.size() < 8 || text.back() != '\n')
            throw std::runtime_error("invalid pad command framing");
        if (haveTime && monotonicMilliseconds < lastTime)
            throw std::runtime_error("pad clock moved backwards");
        text.remove_suffix(1);
        if (text.substr(0, 7) != "D1PAD1 ") throw std::runtime_error("invalid pad command prefix");
        text.remove_prefix(7);

        const auto first = text.find(' ');
        if (first == std::string_view::npos) throw std::runtime_error("invalid pad command fields");
        const uint64_t sequence = decimal(text.substr(0, first), "invalid pad sequence");
        text.remove_prefix(first + 1);
        const auto second = text.find(' ');
        if (second == std::string_view::npos) throw std::runtime_error("invalid pad command fields");
        const uint16_t buttons = hex4(text.substr(0, second));
        const uint64_t hold = decimal(text.substr(second + 1), "invalid pad hold");
        if (hold > 10000) throw std::runtime_error("pad hold is out of range");
        if (!hold && buttons != 0xffff) throw std::runtime_error("zero hold must be neutral");

        if (haveSequence && sequence < lastSequence)
            throw std::runtime_error("stale pad sequence");
        if (haveSequence && sequence == lastSequence) {
            if (buttons != lastButtons || hold != lastHold)
                throw std::runtime_error("pad sequence payload changed");
            lastTime = monotonicMilliseconds;
            haveTime = true;
            return monotonicMilliseconds < deadline ? lastButtons : uint16_t(0xffff);
        }

        lastSequence = sequence;
        lastButtons = buttons;
        lastHold = hold;
        deadline = hold > (std::numeric_limits<uint64_t>::max)() - monotonicMilliseconds
            ? (std::numeric_limits<uint64_t>::max)() : monotonicMilliseconds + hold;
        haveSequence = true;
        lastTime = monotonicMilliseconds;
        haveTime = true;
        return monotonicMilliseconds < deadline ? buttons : uint16_t(0xffff);
    }
};

}
