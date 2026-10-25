#pragma once

#include "pad_control.h"
#include <array>
#include <cstddef>

namespace drakengard {

using PadControlFileBuffer = std::array<char, PadControl::MaxCommandBytes + 1>;

// Read one immutable command version while its pathname may be atomically replaced.
// Missing, unreadable and oversized files fail; no cached or neutral substitute.
std::size_t readPadControlFile(const char* path, PadControlFileBuffer& bytes);

} // namespace drakengard
