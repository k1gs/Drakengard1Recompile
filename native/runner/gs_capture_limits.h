#pragma once
#include <charconv>
#include <stdexcept>
#include <string_view>

// Covers a bounded 300-second run at one capture per 60 UI observations.
inline constexpr unsigned MaxGsPeriodicCaptures = 320;

inline unsigned parseGsCaptureLimit(std::string_view text, unsigned minimum, unsigned maximum) {
    unsigned value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || value < minimum || value > maximum)
        throw std::runtime_error("GS capture option outside supported bounds");
    return value;
}
