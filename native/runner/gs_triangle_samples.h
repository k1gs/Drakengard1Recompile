#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <ostream>

namespace drakengard {
struct GsRawVertex {
    uint64_t sequence{}, encoding{}, lo{}, hi{};
};
struct GsRawVertexWindow {
    std::array<GsRawVertex, 4> entries{};
    uint64_t sequence{};
    void add(uint64_t encoding, uint64_t lo, uint64_t hi) {
        entries[sequence % entries.size()] = {sequence + 1, encoding, lo, hi};
        ++sequence;
    }
};
// Frontend decode and the CPU backend Submit/Draw execute synchronously.
// These are recent same-thread packets, not an asserted mapping of every fan vertex.
inline thread_local GsRawVertexWindow gsRawVertexWindow;
struct GsTriangleSample {
    unsigned category{};
    uint64_t draw{};
    std::array<uint64_t, 12> key{};
    std::array<uint32_t, 2> offset{};
    std::array<unsigned, 3> flags{}; // IIP, FGE, CTXT
    uint32_t fogRgb{};
    // X/Y float bits, Z double bits, S/T/Q float bits, UV, RGBA, FOG.
    std::array<std::array<uint64_t, 9>, 3> vertices{};
    GsRawVertexWindow raw;
};
struct GsTriangleSamples {
    // Two examples per independent degenerate/outside/nonfinite category per report.
    std::array<GsTriangleSample, 16> samples{};
    std::array<unsigned, 8> counts{};
    size_t size{};
    template<class Batch> void observe(const Batch& batch, const std::array<uint64_t, 12>& key,
                                      uint64_t draw, const GsRawVertexWindow& raw) {
        const auto type = unsigned(batch.state.prim.type);
        if (type < 3 || type > 5 || batch.vertexCount != 3) return;
        const auto& ctx = batch.state.context;
        std::array<float, 3> x{}, y{};
        unsigned category = 0;
        for (size_t i = 0; i < 3; ++i) {
            x[i] = batch.vertices[i].x - float(ctx.xyoffset.ofx >> 4);
            y[i] = batch.vertices[i].y - float(ctx.xyoffset.ofy >> 4);
            if (!std::isfinite(x[i]) || !std::isfinite(y[i])) category |= 4u;
        }
        if (!category) {
            const float area = (y[1]-y[2])*(x[0]-x[2]) + (x[2]-x[1])*(y[0]-y[2]);
            if (std::fabs(area) < 0.001f) category |= 1u;
            if (*std::max_element(x.begin(), x.end()) < ctx.scissor.x0 ||
                *std::min_element(x.begin(), x.end()) > ctx.scissor.x1 ||
                *std::max_element(y.begin(), y.end()) < ctx.scissor.y0 ||
                *std::min_element(y.begin(), y.end()) > ctx.scissor.y1) category |= 2u;
        }
        if (counts[category] >= 2) return;
        ++counts[category];
        auto& sample = samples[size++];
        sample.category = category; sample.draw = draw; sample.key = key;
        sample.offset = {ctx.xyoffset.ofx, ctx.xyoffset.ofy};
        sample.flags = {unsigned(batch.state.prim.iip), unsigned(batch.state.prim.fge), unsigned(batch.state.prim.ctxt)};
        sample.fogRgb = uint32_t(batch.state.fogR) | (uint32_t(batch.state.fogG)<<8) | (uint32_t(batch.state.fogB)<<16);
        for (size_t i = 0; i < 3; ++i) {
            const auto& v = batch.vertices[i];
            sample.vertices[i] = {std::bit_cast<uint32_t>(v.x), std::bit_cast<uint32_t>(v.y),
                std::bit_cast<uint64_t>(double(v.z)), std::bit_cast<uint32_t>(v.s),
                std::bit_cast<uint32_t>(v.t), std::bit_cast<uint32_t>(v.q),
                uint64_t(v.u) | (uint64_t(v.v)<<16),
                uint64_t(v.r) | (uint64_t(v.g)<<8) | (uint64_t(v.b)<<16) | (uint64_t(v.a)<<24), v.fog};
        }
        sample.raw = raw;
    }
    template<class Array> static void writeArray(std::ostream& out, const Array& values) {
        out << '[';
        for (size_t i = 0; i < values.size(); ++i) { if (i) out << ','; out << values[i]; }
        out << ']';
    }
    void writeJson(std::ostream& out) const {
        out << '[';
        for (size_t i = 0; i < size; ++i) {
            const auto& s = samples[i];
            if (i) out << ',';
            out << "{\"class\":" << s.category << ",\"draw\":" << s.draw << ",\"key\":";
            writeArray(out, s.key); out << ",\"xyoffset\":"; writeArray(out, s.offset);
            out << ",\"iip_fge_ctxt\":"; writeArray(out, s.flags);
            out << ",\"fog_rgb\":" << s.fogRgb << ",\"vertices_bits\":[";
            for (size_t j = 0; j < 3; ++j) { if (j) out << ','; writeArray(out, s.vertices[j]); }
            out << "],\"recent_raw_xyz\":[";
            const auto count = std::min<uint64_t>(s.raw.sequence, s.raw.entries.size());
            for (uint64_t j = 0; j < count; ++j) {
                if (j) out << ',';
                const auto& r = s.raw.entries[(s.raw.sequence-count+j)%s.raw.entries.size()];
                writeArray(out, std::array<uint64_t,4>{r.sequence,r.encoding,r.lo,r.hi});
            }
            out << "]}";
        }
        out << ']';
    }
    void clear() { size = 0; counts = {}; }
};
}
