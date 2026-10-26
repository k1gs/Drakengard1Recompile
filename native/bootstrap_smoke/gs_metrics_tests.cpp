#include "gs_production_metrics.h"
#include "capture/gs_capture_limits.h"
#include <iostream>
#include <limits>
#include <sstream>

struct GeometryFixture {
    struct Vertex { float x{}, y{}; double z{}; float s{}, t{}, q{1};
        uint16_t u{},v{}; uint8_t r{},g{},b{},a{},fog{}; };
    std::array<Vertex, 3> vertices{{{10, 10}, {20, 10}, {10, 20}}};
    unsigned vertexCount{3};
    struct State {
        struct Prim { unsigned type{3},iip{},fge{},ctxt{}; } prim;
        uint8_t fogR{},fogG{},fogB{};
        struct Context {
            struct Offset { unsigned ofx{}, ofy{}; } xyoffset;
            struct Scissor { unsigned x0{}, x1{640}, y0{}, y1{512}; } scissor;
        } context;
    } state;
};

int main() {
    std::array<uint8_t,64> original{},copied{};
    drakengard::beginGsDmaSource(true,0x3000,0x105);
    drakengard::markGsDmaTag(0x3000);
    drakengard::appendGsDmaSource(0,0x5000,32,false);
    drakengard::appendGsDmaSource(32,0x100,32,true);
    drakengard::queueGsDmaSource(original.data(),64);
    drakengard::relayGsDmaSource(original.data(),64,copied.data());
    const auto* trace=drakengard::findGsDmaSource(copied.data(),64);
    const auto address=drakengard::resolveGsDmaAddress(trace,40,8);
    if(!address.known || !address.scratch || address.address!=0x108 || address.tag!=0x3000 ||
       drakengard::resolveGsDmaAddress(trace,24,16).known || drakengard::findGsDmaSource(original.data(),64)) return 25;
    std::array<uint8_t,64> packet{};
    const auto setTag = [&](uint64_t tag) { std::memcpy(packet.data(), &tag, 8); };
    setTag(1ull | (2ull<<60)); // two packed registers
    if (drakengard::inspectGsGifPacket(packet.data(),48).status != 0 ||
        drakengard::inspectGsGifPacket(packet.data(),32).status != 2) return 19;
    setTag(3ull | (1ull<<58) | (1ull<<60)); // odd REGLIST count padded to a quadword
    if (drakengard::inspectGsGifPacket(packet.data(),48).status != 0 ||
        drakengard::inspectGsGifPacket(packet.data(),40).status != 2) return 20;
    setTag(1ull); // NREG=0 encodes sixteen registers
    if (drakengard::inspectGsGifPacket(packet.data(),64).expectedEnd != 272) return 21;
    setTag(2ull | (2ull<<58));
    if (drakengard::inspectGsGifPacket(packet.data(),48).status != 0 ||
        drakengard::inspectGsGifPacket(packet.data(),49).status != 1) return 22;
    std::array<uint8_t,257*16> manyTags{};
    if (drakengard::inspectGsGifPacket(manyTags.data(),manyTags.size()).status != 3) return 23;
    drakengard::gsGifPacketContext.id = 7; drakengard::gsGifPacketContext.path = 2;
    drakengard::GsRawVertexWindow raw;
    for (uint64_t i = 0; i < 6; ++i) raw.add(0x104,i,i+1);
    if (raw.sequence != 6 || raw.entries[0].sequence != 5 || raw.entries[3].sequence != 4 ||
        raw.entries[0].packetId != 7 || raw.entries[0].path != 2) return 13;
    drakengard::GsTriangleSamples samples;
    GeometryFixture sampleBatch;
    for (unsigned i = 0; i < 100; ++i) samples.observe(sampleBatch,{},i,raw);
    if (samples.size != 2 || samples.samples[0].category != 0) return 14;
    if (samples.samples[0].packet.id != 7) return 24;
    drakengard::gsGifPacketContext = {};
    sampleBatch.vertices[2] = sampleBatch.vertices[1];
    samples.observe(sampleBatch,{},101,raw);
    if (samples.size != 3 || samples.samples[2].category != 1) return 15;
    sampleBatch.vertices[0].x = std::numeric_limits<float>::quiet_NaN();
    samples.observe(sampleBatch,{},102,raw);
    if (samples.size != 4 || samples.samples[3].category != 4 ||
        samples.samples[3].vertices[0][0] != std::bit_cast<uint32_t>(sampleBatch.vertices[0].x)) return 16;
    std::ostringstream json; samples.writeJson(json);
    if (json.str().find("nan") != std::string::npos || json.str().find("recent_raw_xyz") == std::string::npos) return 17;
    samples.clear();
    samples.observe(GeometryFixture{}, {},103,raw);
    if (samples.size != 1) return 18;
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
