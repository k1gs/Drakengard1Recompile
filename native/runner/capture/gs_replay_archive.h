#pragma once

#include "gs_frame_replay.h"
#include <fstream>
#include <type_traits>

namespace drakengard {

inline constexpr std::array<uint32_t, 4> gsReplayArchiveHeader{
    0x31525347u, sizeof(GSPrimitiveBatch), sizeof(GSTransferCommand), sizeof(GsReplayScanout)
};

// Private capture for replay with the same native ABI. Pixel blocks are stored
// once, including when many consecutive draws share a texture snapshot.
inline void writeGsReplayArchive(std::ostream& file, const GsReplayFrame& frame) {
    const auto write = [&file]<class T>(const T& value) {
        static_assert(std::is_trivially_copyable_v<T>);
        file.write(reinterpret_cast<const char*>(&value), sizeof(value));
    };
    const auto bytes = [&](std::span<const uint8_t> pixels) {
        write(uint32_t(pixels.size()));
        if (!pixels.empty()) file.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
    };
    write(gsReplayArchiveHeader);
    write(frame.startTick);
    write(frame.endTick);
    write(frame.scanout);
    bytes(frame.initialVram);
    for (std::size_t page = 0; page < GsReplayFrame::PageCount; ++page) write(uint8_t(frame.uploadedPages[page]));

    write(uint32_t(frame.draws.size()));
    std::unordered_map<const std::vector<uint8_t>*, uint32_t> textures;
    for (const auto& draw : frame.draws) {
        write(draw.batch);
        write(uint8_t(draw.status));
        write(uint8_t(draw.matchesVram));
        if (draw.rgba) {
            const auto [entry, inserted] = textures.try_emplace(draw.rgba.get(), uint32_t(textures.size()) + 1u);
            write(entry->second);
            if (inserted) bytes(draw.pixels());
        } else {
            write(uint32_t(0));
        }
    }
    write(uint32_t(frame.colorUploads.size()));
    for (const auto& upload : frame.colorUploads) {
        write(uint32_t(upload.beforeDraw));
        write(upload.transfer);
        write(upload.firstPixel);
        bytes(upload.pixels);
    }
    if (!file) throw std::runtime_error("Could not write GS replay archive");
}

}
