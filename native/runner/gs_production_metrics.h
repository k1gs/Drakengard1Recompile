#pragma once
#include "capture/gs_capture_limits.h"
#include "gs_triangle_samples.h"
#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <vector>

namespace drakengard {
// Register-state inventory only; none of these values control rendering.
struct GsDrawKey {
    std::array<uint64_t, 12> words{};
    bool operator==(const GsDrawKey&) const = default;
};
struct GsDrawBin { GsDrawKey key; uint64_t draws{}, wallNs{}; };
struct GsGeometryCounters {
    uint64_t triangles{}, malformed{}, nonfinite{}, degenerate{}, outsideScissor{};
    template<class Batch> void observe(const Batch& batch) {
        const auto type = uint32_t(batch.state.prim.type);
        // GS PRIM types 3, 4 and 5 are triangle, strip and fan.
        if (type < 3 || type > 5) return;
        ++triangles;
        if (batch.vertexCount != 3) { ++malformed; return; }
        std::array<float, 3> x{}, y{};
        const auto& ctx = batch.state.context;
        for (size_t i = 0; i < 3; ++i) {
            x[i] = batch.vertices[i].x - float(ctx.xyoffset.ofx >> 4);
            y[i] = batch.vertices[i].y - float(ctx.xyoffset.ofy >> 4);
            if (!std::isfinite(x[i]) || !std::isfinite(y[i])) { ++nonfinite; return; }
        }
        const float area = (y[1] - y[2]) * (x[0] - x[2]) + (x[2] - x[1]) * (y[0] - y[2]);
        if (std::fabs(area) < 0.001f) ++degenerate;
        if (*std::max_element(x.begin(), x.end()) < ctx.scissor.x0 ||
            *std::min_element(x.begin(), x.end()) > ctx.scissor.x1 ||
            *std::max_element(y.begin(), y.end()) < ctx.scissor.y0 ||
            *std::min_element(y.begin(), y.end()) > ctx.scissor.y1) ++outsideScissor;
    }
};
struct GsProductionCounters {
    uint64_t uiCalls{}, observedTicks{}, tickRegressions{}, lastTick{};
    uint64_t presentations{}, firstImages{}, rgbChanges{}, invalidImages{};
    uint64_t draws{}, drawWallNs{}, clutLoads{};
    std::array<uint64_t, 8> primitiveDraws{};
    uint64_t invalidPrimitiveDraws{};
    GsGeometryCounters geometry;
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
        const auto primitive = key.words[0] & 0xffu;
        if (primitive < primitiveDraws.size()) ++primitiveDraws[primitive];
        else ++invalidPrimitiveDraws;
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
struct GsMetricGate {
    GsMetricClock::time_point readyAt;
    GsMetricGate(GsMetricClock::time_point origin, unsigned delaySeconds)
        : readyAt(origin + std::chrono::seconds(delaySeconds)) {}
    bool active(GsMetricClock::time_point now) const { return now >= readyAt; }
};
inline unsigned gsMetricStartDelay() {
    static const unsigned seconds = [] {
        const char* text = std::getenv("DRAKENGARD_GS_METRICS_START_SECONDS");
        return text ? parseGsCaptureLimit(text, 0, 600) : 0u;
    }();
    return seconds;
}
inline const char* gsMetricPath() {
    static const char* path = std::getenv("DRAKENGARD_GS_METRICS_OUTPUT");
    if (!path || !*path) return nullptr;
    static const GsMetricGate gate(GsMetricClock::now(), gsMetricStartDelay());
    return gate.active(GsMetricClock::now()) ? path : nullptr;
}
inline void observeGsRawVertex(uint64_t encoding, uint64_t lo, uint64_t hi) {
    if (gsMetricPath()) gsRawVertexWindow.add(encoding, lo, hi);
}
struct GsMetricStore {
    std::mutex mutex;
    GsProductionCounters counters;
    GsObservedRgb image;
    GsTriangleSamples triangles;
    GsGifPacketCounters gif;
    GsMetricClock::time_point origin = GsMetricClock::now(), lastReport = origin;
    unsigned reports{};
};
inline GsMetricStore& gsMetricStore() { static GsMetricStore store; return store; }
inline std::atomic<uint64_t> gsGifPacketSequence{};
inline thread_local const uint8_t* gsCurrentGifData{};
inline thread_local uint32_t gsCurrentGifBytes{};
class GsDmaCpuScope {
    std::optional<GsDmaCpuContext> previous;
public:
    GsDmaCpuScope(bool gifWrite,uint32_t pc,uint32_t ra,uint32_t sp,const uint8_t* ram,uint32_t size) {
        if (!gifWrite || !gsMetricPath()) return;
        previous=gsDmaCpuContext;
        gsDmaCpuContext={}; gsDmaCpuContext.known=true;
        gsDmaCpuContext.pc=pc; gsDmaCpuContext.ra=ra; gsDmaCpuContext.sp=sp;
        if (ram && uint64_t(sp)+sizeof(gsDmaCpuContext.stack)<=size) {
            std::memcpy(gsDmaCpuContext.stack.data(),ram+sp,sizeof(gsDmaCpuContext.stack));
            gsDmaCpuContext.stackKnown=true;
        }
    }
    ~GsDmaCpuScope() { if(previous)gsDmaCpuContext=*previous; }
};
class GsGifPacketScope {
    std::optional<GsGifPacketSummary> previous;
    const GsDmaSource* previousSource{};
    const uint8_t* previousData{};
    uint32_t previousBytes{},previousOffset{};
    GsDmaSource* source{};
public:
    GsGifPacketScope(unsigned path, const uint8_t* data, uint32_t bytes) {
        if (!gsMetricPath()) return;
        previous = gsGifPacketContext;
        previousSource=gsCurrentDmaSource; previousData=gsCurrentGifData;
        previousBytes=gsCurrentGifBytes; previousOffset=gsCurrentGifDataOffset;
        source=path==3?findGsDmaSource(data,bytes):nullptr;
        gsCurrentDmaSource=source; gsCurrentGifData=data; gsCurrentGifBytes=bytes;
        gsCurrentGifDataOffset=0;
        auto packet = inspectGsGifPacket(data, bytes);
        packet.id = gsGifPacketSequence.fetch_add(1, std::memory_order_relaxed) + 1;
        packet.path = path;
        if(source) {
            packet.dmaKnown=true; packet.cpuKnown=source->cpu.known; packet.spanOverflow=source->overflow;
            packet.dmaTadr=source->tadr; packet.dmaChcr=source->chcr;
            packet.dmaPc=source->cpu.pc; packet.dmaRa=source->cpu.ra; packet.dmaSp=source->cpu.sp;
        }
        gsGifPacketContext = packet;
        auto& store = gsMetricStore(); std::lock_guard lock(store.mutex);
        store.gif.observe(packet);
    }
    ~GsGifPacketScope() { if (previous) {
        gsGifPacketContext=*previous; gsCurrentDmaSource=previousSource;
        gsCurrentGifData=previousData; gsCurrentGifBytes=previousBytes; gsCurrentGifDataOffset=previousOffset;
        if(source)source->active=false;
    } }
    GsGifPacketScope(const GsGifPacketScope&) = delete;
    GsGifPacketScope& operator=(const GsGifPacketScope&) = delete;
};
template<class Batch> void captureGsZeroPacket(const Batch& batch) {
    static const char* directory=std::getenv("DRAKENGARD_GIF_SOURCE_OUTPUT");
    static unsigned saved{}; static uint64_t lastPacket{};
    if (!directory || !*directory || saved>=2 || gsGifPacketContext.path!=3 ||
        gsGifPacketContext.id==lastPacket || !gsCurrentDmaSource || !gsCurrentGifData || gsCurrentGifBytes>1024u*1024u ||
        batch.vertexCount!=3 || unsigned(batch.state.prim.type)<3 || unsigned(batch.state.prim.type)>5) return;
    for(const auto& v:batch.vertices) if(v.x!=0 || v.y!=0 || v.z!=0) return;
    const auto base=std::filesystem::path(directory)/("packet-"+std::to_string(gsGifPacketContext.id));
    const auto binary=base.string()+".bin", metadata=base.string()+".source.json";
    if(std::filesystem::exists(binary)||std::filesystem::exists(metadata))
        throw std::runtime_error("GIF source capture already exists");
    std::ofstream data(binary,std::ios::binary); data.write(reinterpret_cast<const char*>(gsCurrentGifData),gsCurrentGifBytes);
    std::ofstream sourceFile(metadata); sourceFile<<"{\"packet\":";
    writeGsGifPacketJson(sourceFile,gsGifPacketContext); sourceFile<<",\"dma\":";
    writeGsDmaSourceJson(sourceFile,gsCurrentDmaSource); sourceFile<<'}';
    data.flush(); sourceFile.flush();
    if(!data || !sourceFile)throw std::runtime_error("Cannot write bounded GIF source capture");
    ++saved; lastPacket=gsGifPacketContext.id;
}
inline GsMetricClock::time_point beginGsDrawMetric() {
    return gsMetricPath() ? GsMetricClock::now() : GsMetricClock::time_point{};
}
template<class Batch> void completeGsDrawMetric(GsMetricClock::time_point start, const Batch& batch) {
    if (start == GsMetricClock::time_point{}) return;
    const auto& s = batch.state;
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
        uint64_t(s.context.frame.psm) | (uint64_t(s.context.frame.fbp) << 8) | (uint64_t(s.prim.abe) << 40),
        s.context.test,
        uint64_t(s.context.zbuf.zbp) | (uint64_t(s.context.zbuf.psm) << 32) | (uint64_t(s.context.zbuf.zmask) << 40),
        s.context.frame.fbmsk,
        uint64_t(s.context.scissor.x0) | (uint64_t(s.context.scissor.x1) << 16) |
            (uint64_t(s.context.scissor.y0) << 32) | (uint64_t(s.context.scissor.y1) << 48)}};
    auto& store = gsMetricStore(); std::lock_guard lock(store.mutex);
    store.counters.draw(key, uint64_t(elapsed));
    store.counters.geometry.observe(batch);
    store.triangles.observe(batch, key.words, store.counters.draws, gsRawVertexWindow);
    captureGsZeroPacket(batch);
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
    GsTriangleSamples samples;
    GsGifPacketCounters gif;
    { std::lock_guard lock(store.mutex); copy = store.counters; store.counters.clearBins();
      samples = store.triangles; store.triangles.clear(); gif = store.gif; }
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - store.origin).count();
    std::ofstream file(path, std::ios::app);
    if (!file) throw std::runtime_error("Cannot open private GS metrics output");
    file << "{\"elapsed_ns\":" << elapsed << ",\"start_delay_seconds\":" << gsMetricStartDelay()
         << ",\"ui_calls\":" << copy.uiCalls
         << ",\"observed_vblank_ticks\":" << copy.observedTicks << ",\"last_tick\":" << copy.lastTick
         << ",\"tick_regressions\":" << copy.tickRegressions << ",\"presentations\":" << copy.presentations
         << ",\"first_images\":" << copy.firstImages << ",\"rgb_changes\":" << copy.rgbChanges
         << ",\"invalid_images\":" << copy.invalidImages << ",\"draws\":" << copy.draws
         << ",\"draw_wall_ns\":" << copy.drawWallNs << ",\"clut_loads\":" << copy.clutLoads
         << ",\"overflow_draws\":" << copy.overflowDraws << ",\"overflow_wall_ns\":" << copy.overflowWallNs
         << ",\"primitive_draws\":[";
    for (size_t i = 0; i < copy.primitiveDraws.size(); ++i) {
        if (i) file << ',';
        file << copy.primitiveDraws[i];
    }
    file << "],\"invalid_primitive_draws\":" << copy.invalidPrimitiveDraws
         << ",\"triangles\":" << copy.geometry.triangles
         << ",\"malformed_triangles\":" << copy.geometry.malformed
         << ",\"nonfinite_xy_triangles\":" << copy.geometry.nonfinite
         << ",\"degenerate_triangles\":" << copy.geometry.degenerate
         << ",\"outside_scissor_triangles\":" << copy.geometry.outsideScissor
         << ",\"states\":[";
    for (size_t i = 0; i < copy.binCount; ++i) {
        if (i) file << ',';
        file << "{\"key\":[";
        for (size_t j = 0; j < copy.bins[i].key.words.size(); ++j) {
            if (j) file << ','; file << copy.bins[i].key.words[j];
        }
        file << "],\"draws\":" << copy.bins[i].draws << ",\"wall_ns\":" << copy.bins[i].wallNs << '}';
    }
    file << "],\"triangle_samples\":";
    samples.writeJson(file);
    file << ",\"gif_path_packets\":"; GsTriangleSamples::writeArray(file, gif.packets);
    file << ",\"gif_path_bytes\":"; GsTriangleSamples::writeArray(file, gif.bytes);
    file << ",\"gif_framing_status\":"; GsTriangleSamples::writeArray(file, gif.framing);
    file << ",\"gif_unknown_paths\":" << gif.unknownPaths;
    file << "}\n"; file.flush();
    if (!file) throw std::runtime_error("Cannot write private GS metrics output");
    ++store.reports; store.lastReport = now;
}
}
