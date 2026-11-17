#pragma once

#include "runtime/gs/gs_types.h"
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace drakengard {

struct GsTextureSnapshot {
    enum class Status { Untextured, Decoded, Unsupported };
    GSPrimitiveBatch batch;
    Status status;
    std::shared_ptr<const std::vector<uint8_t>> rgba; // GS (0,0) first, then row-major RGBA bytes.
    bool matchesVram = false;

    std::span<const uint8_t> pixels() const noexcept {
        return rgba ? std::span<const uint8_t>(*rgba) : std::span<const uint8_t>{};
    }
};

// Opt in on the GS submission thread. The sink runs under the backend lock
// and may retain the snapshot, but must not call back into GS or do GL work.
class GsTextureSnapshotScope {
public:
    using Sink = void (*)(void*, GsTextureSnapshot&&);

    GsTextureSnapshotScope(Sink sink, void* context) noexcept
        : sink(sink), context(context), previous(std::exchange(active, this)) {}
    ~GsTextureSnapshotScope() { active = previous; }
    GsTextureSnapshotScope(const GsTextureSnapshotScope&) = delete;
    GsTextureSnapshotScope& operator=(const GsTextureSnapshotScope&) = delete;

    static inline thread_local const GsTextureSnapshotScope* active = nullptr;

    void submit(GsTextureSnapshot&& snapshot) const { sink(context, std::move(snapshot)); }

private:
    Sink sink;
    void* context;
    const GsTextureSnapshotScope* previous;
};

template<class TextureCache, class Sample>
GsTextureSnapshot snapshotGsTexture(const GSPrimitiveBatch& batch, TextureCache& cache, Sample sample) {
    using Status = GsTextureSnapshot::Status;
    GsTextureSnapshot result{batch, Status::Untextured, {}};
    if (!batch.state.prim.tme) return result;

    const auto& state = batch.state;
    const auto& tex = state.context.tex0;
    const bool indexed = tex.psm == GS_PSM_T4 || tex.psm == GS_PSM_T8;
    const bool paletteSupported = tex.cpsm == GS_PSM_CT32 || tex.cpsm == GS_PSM_CT24 ||
                                  tex.cpsm == GS_PSM_CT16 || tex.cpsm == GS_PSM_CT16S;
    const auto width = state.textureWidth;
    const auto height = state.textureHeight;
    if ((tex.psm != GS_PSM_CT32 && !(indexed && paletteSupported)) || ((state.context.tex1 >> 2) & 7) ||
        !std::has_single_bit(width) || !std::has_single_bit(height) || width > 1024 || height > 1024) {
        result.status = Status::Unsupported;
        return result;
    }

    auto point = state;
    point.prim.fst = true;
    point.linearFilter = false;
    point.context.clamp = 0;
    auto pixels = std::make_shared<std::vector<uint8_t>>(std::size_t(width) * height * 4);

    // Reuse the live sampler and loaded CLUT. Reading extra pages for the
    // snapshot must not evict the page that the next software draw observes.
    const struct RestoreCache {
        TextureCache& current;
        TextureCache saved;
        ~RestoreCache() { current = saved; }
    } restore{cache, cache};
    auto* rgba = pixels->data();
    result.matchesVram = tex.psm == GS_PSM_CT32;
    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            const auto [texel, current] = sample(point, uint16_t(x * 16), uint16_t(y * 16), result.matchesVram);
            result.matchesVram &= current;
            for (unsigned shift = 0; shift < 32; shift += 8) *rgba++ = uint8_t(texel >> shift);
        }
    }
    result.status = Status::Decoded;
    result.rgba = std::move(pixels);
    return result;
}

}
