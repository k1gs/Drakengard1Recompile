#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace drakengard {
// Register-state inventory only; none of these values control rendering.
struct GsDrawKey {
    std::array<uint64_t, 8> words{};
    bool operator==(const GsDrawKey&) const = default;
};
struct GsDrawBin { GsDrawKey key; uint64_t draws{}, wallNs{}; };
struct GsProductionCounters {
    uint64_t uiCalls{}, observedTicks{}, tickRegressions{}, lastTick{};
    uint64_t presentations{}, firstImages{}, rgbChanges{}, invalidImages{};
    uint64_t draws{}, drawWallNs{}, clutLoads{};
    bool hasTick{};
    std::array<GsDrawBin, 64> bins{};
    size_t binCount{};
    uint64_t overflowDraws{}, overflowWallNs{};

    void ui(uint64_t tick) {
        ++uiCalls;
        if (hasTick) {
            if (tick >= lastTick) observedTicks += tick - lastTick;
            else ++tickRegressions;
        }
        lastTick = tick; hasTick = true;
    }
    void draw(const GsDrawKey& key, uint64_t wallNs) {
        ++draws; drawWallNs += wallNs;
        for (size_t i = 0; i < binCount; ++i)
            if (bins[i].key == key) { ++bins[i].draws; bins[i].wallNs += wallNs; return; }
        if (binCount < bins.size()) bins[binCount++] = {key, 1, wallNs};
        else { ++overflowDraws; overflowWallNs += wallNs; }
    }
    void clearBins() { bins = {}; binCount = 0; overflowDraws = overflowWallNs = 0; }
};

class GsObservedRgb {
    std::vector<uint8_t> previous;
    uint32_t previousWidth{}, previousHeight{};
public:
    // Counts changes in observed RGB, not application frame boundaries. Identical
    // redraws and frames missed between host observations are deliberately distinct.
    void observe(GsProductionCounters& counters, const std::vector<uint8_t>& rgba,
                 uint32_t width, uint32_t height) {
        if (!width || !height || width > rgba.size() / 4 ||
            height > rgba.size() / (size_t(width) * 4) || rgba.size() != size_t(width) * 4 * height) {
            ++counters.invalidImages; return;
        }
        ++counters.presentations;
        if (previous.empty()) ++counters.firstImages;
        else {
            bool equal = width == previousWidth && height == previousHeight;
            if (equal && rgba != previous)
                for (size_t i = 0; i < rgba.size(); i += 4)
                    if (rgba[i] != previous[i] || rgba[i + 1] != previous[i + 1] || rgba[i + 2] != previous[i + 2]) {
                        equal = false; break;
                    }
            if (!equal) ++counters.rgbChanges;
        }
        previous = rgba; previousWidth = width; previousHeight = height;
    }
};

using GsMetricClock = std::chrono::steady_clock;
inline const char* gsMetricPath() {
    static const char* path = std::getenv("DRAKENGARD_GS_METRICS_OUTPUT");
    return path && *path ? path : nullptr;
}
struct GsMetricStore {
    std::mutex mutex;
    GsProductionCounters counters;
    GsObservedRgb image;
    GsMetricClock::time_point origin = GsMetricClock::now(), lastReport = origin;
    unsigned reports{};
};
inline GsMetricStore& gsMetricStore() { static GsMetricStore store; return store; }
inline GsMetricClock::time_point beginGsDrawMetric() {
    return gsMetricPath() ? GsMetricClock::now() : GsMetricClock::time_point{};
}
template<class State> void completeGsDrawMetric(GsMetricClock::time_point start, const State& s) {
    if (!gsMetricPath()) return;
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(GsMetricClock::now() - start).count();
    const auto& t = s.context.tex0;
    GsDrawKey key{{
        uint64_t(s.prim.type) | (uint64_t(t.psm) << 8) | (uint64_t(t.cpsm) << 16) |
            (uint64_t(s.linearFilter) << 24) | (uint64_t(s.prim.fst) << 25) |
            (uint64_t(s.prim.tme) << 26) | (uint64_t(t.tfx) << 27) | (uint64_t(t.tcc) << 29),
        uint64_t(s.textureWidth) | (uint64_t(s.textureHeight) << 32),
        uint64_t(t.tbp0) | (uint64_t(t.tbw) << 32),
        uint64_t(t.cbp) | (uint64_t(t.csm) << 32) | (uint64_t(t.csa) << 40) | (uint64_t(t.cld) << 48),
        uint64_t(s.texa.ta0) | (uint64_t(s.texa.ta1) << 8) | (uint64_t(s.texa.aem) << 16),
        s.context.clamp,
        uint64_t(s.texclut.cbw) | (uint64_t(s.texclut.cou) << 8) | (uint64_t(s.texclut.cov) << 16),
        uint64_t(s.context.frame.psm) | (uint64_t(s.context.frame.fbp) << 8) | (uint64_t(s.prim.abe) << 40)}};
    auto& store = gsMetricStore(); std::lock_guard lock(store.mutex);
    store.counters.draw(key, uint64_t(elapsed));
}
inline void observeGsClutLoad() {
    if (!gsMetricPath()) return;
    auto& store = gsMetricStore(); std::lock_guard lock(store.mutex); ++store.counters.clutLoads;
}
inline void observeGsUiTick(uint64_t tick) {
    if (!gsMetricPath()) return;
    auto& store = gsMetricStore(); std::lock_guard lock(store.mutex); store.counters.ui(tick);
}
inline void observeGsRgb(const std::vector<uint8_t>& pixels, uint32_t width, uint32_t height) {
    if (!gsMetricPath()) return;
    auto& store = gsMetricStore(); std::lock_guard lock(store.mutex);
    store.image.observe(store.counters, pixels, width, height);
}
inline void pollGsProductionMetrics() {
    const char* path = gsMetricPath(); if (!path) return;
    auto& store = gsMetricStore(); const auto now = GsMetricClock::now();
    if (store.reports >= 64 || now - store.lastReport < std::chrono::seconds(5)) return;
    GsProductionCounters copy;
    { std::lock_guard lock(store.mutex); copy = store.counters; store.counters.clearBins(); }
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - store.origin).count();
    std::ofstream file(path, std::ios::app);
    if (!file) throw std::runtime_error("Cannot open private GS metrics output");
    file << "{\"elapsed_ns\":" << elapsed << ",\"ui_calls\":" << copy.uiCalls
         << ",\"observed_vblank_ticks\":" << copy.observedTicks << ",\"last_tick\":" << copy.lastTick
         << ",\"tick_regressions\":" << copy.tickRegressions << ",\"presentations\":" << copy.presentations
         << ",\"first_images\":" << copy.firstImages << ",\"rgb_changes\":" << copy.rgbChanges
         << ",\"invalid_images\":" << copy.invalidImages << ",\"draws\":" << copy.draws
         << ",\"draw_wall_ns\":" << copy.drawWallNs << ",\"clut_loads\":" << copy.clutLoads
         << ",\"overflow_draws\":" << copy.overflowDraws << ",\"overflow_wall_ns\":" << copy.overflowWallNs
         << ",\"states\":[";
    for (size_t i = 0; i < copy.binCount; ++i) {
        if (i) file << ',';
        file << "{\"key\":[";
        for (size_t j = 0; j < copy.bins[i].key.words.size(); ++j) {
            if (j) file << ','; file << copy.bins[i].key.words[j];
        }
        file << "],\"draws\":" << copy.bins[i].draws << ",\"wall_ns\":" << copy.bins[i].wallNs << '}';
    }
    file << "]}\n"; file.flush();
    if (!file) throw std::runtime_error("Cannot write private GS metrics output");
    ++store.reports; store.lastReport = now;
}
}
