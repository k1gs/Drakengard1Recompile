#include "gs_production_metrics.h"
#include "capture/gs_capture_limits.h"
#include <iostream>
#include <limits>

struct GeometryFixture {
    struct Vertex { float x{}, y{}; };
    std::array<Vertex, 3> vertices{{{10, 10}, {20, 10}, {10, 20}}};
    unsigned vertexCount{3};
    struct State {
        struct Prim { unsigned type{3}; } prim;
        struct Context {
            struct Offset { unsigned ofx{}, ofy{}; } xyoffset;
            struct Scissor { unsigned x0{}, x1{640}, y0{}, y1{512}; } scissor;
        } context;
    } state;
};

int main() {
    const auto origin = drakengard::GsMetricClock::time_point{};
    drakengard::GsMetricGate gate(origin, 300);
    if (gate.active(origin + std::chrono::seconds(299)) ||
        !gate.active(origin + std::chrono::seconds(300)) ||
        !gate.active(origin + std::chrono::seconds(600))) return 10;
    if (parseGsCaptureLimit("0", 0, 600) != 0 || parseGsCaptureLimit("600", 0, 600) != 600) return 11;
    GeometryFixture batch;
    drakengard::GsGeometryCounters geometry;
    geometry.observe(batch);
    batch.state.context.xyoffset.ofx = 1600; // finite triangle wholly outside scissor
    geometry.observe(batch);
    batch.state.context.xyoffset.ofx = 0;
    batch.vertices[2] = batch.vertices[1];
    geometry.observe(batch);
    batch.vertices[0].x = std::numeric_limits<float>::quiet_NaN();
    geometry.observe(batch);
    batch.vertexCount = 2;
    geometry.observe(batch);
    batch.state.prim.type = 6; // sprite is not classified as scene geometry
    geometry.observe(batch);
    if (geometry.triangles != 5 || geometry.outsideScissor != 1 || geometry.degenerate != 1 ||
        geometry.nonfinite != 1 || geometry.malformed != 1) return 12;
    if (parseGsCaptureLimit("1", 1, MaxGsPeriodicCaptures) != 1 ||
        parseGsCaptureLimit("64", 1, MaxGsPeriodicCaptures) != 64 ||
        parseGsCaptureLimit("65", 1, MaxGsPeriodicCaptures) != 65 ||
        parseGsCaptureLimit("320", 1, MaxGsPeriodicCaptures) != 320 ||
        parseGsCaptureLimit("60", 60, 3600) != 60 || parseGsCaptureLimit("3600", 60, 3600) != 3600) return 8;
    for (const char* invalid : {"", "0", "321", "-1", "320x", " 320", "4294967296"}) {
        bool rejected = false;
        try { (void)parseGsCaptureLimit(invalid, 1, MaxGsPeriodicCaptures); }
        catch (const std::runtime_error&) { rejected = true; }
        if (!rejected) return 9;
    }
    drakengard::GsProductionCounters c;
    c.ui(10); c.ui(10); c.ui(13); c.ui(12); c.ui(14);
    if (c.uiCalls != 5 || c.observedTicks != 5 || c.tickRegressions != 1) return 1;
    drakengard::GsObservedRgb frames;
    std::vector<uint8_t> a{1, 2, 3, 255, 4, 5, 6, 128};
    frames.observe(c, a, 2, 1); frames.observe(c, a, 2, 1);
    a[3] = 0; frames.observe(c, a, 2, 1);
    if (c.presentations != 3 || c.firstImages != 1 || c.rgbChanges) return 2;
    a[6] ^= 1; frames.observe(c, a, 2, 1);
    if (c.rgbChanges != 1) return 3;
    frames.observe(c, a, 1, 2);
    if (c.rgbChanges != 2) return 4;
    frames.observe(c, a, UINT32_MAX, UINT32_MAX); frames.observe(c, {}, 0, 0);
    if (c.invalidImages != 2 || c.presentations != 5) return 5;
    for (unsigned i = 0; i != 65; ++i) {
        drakengard::GsDrawKey key; key.words[0] = uint64_t(i) << 8; c.draw(key, i);
    }
    drakengard::GsDrawKey first; c.draw(first, 3);
    if (c.binCount != 64 || c.overflowDraws != 1 || c.overflowWallNs != 64 ||
        c.draws != 66 || c.drawWallNs != 2083 || c.bins[0].draws != 2 ||
        c.primitiveDraws[0] != 66 || c.invalidPrimitiveDraws) return 6;
    c.clearBins();
    if (c.binCount || c.overflowDraws || c.draws != 66 || c.drawWallNs != 2083) return 7;
    std::cout << "Independent UI/tick/RGB/draw counters and bounded state overflow passed\n";
}
