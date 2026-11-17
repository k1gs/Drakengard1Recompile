#pragma once

#include "gs_texture_snapshot.h"
#include "runtime/gs/ps2_gs_memory.h"
#include "runtime/gs/ps2_gs_psmt4.h"
#include <algorithm>
#include <atomic>
#include <bitset>
#include <cmath>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace drakengard {

struct GsReplayScanout {
    enum class Kind { None, Direct, PreferredTexture, AlternateFrame, Composite };
    Kind kind = Kind::None;
    GSFrameReg source{};
    uint32_t originX{}, originY{}, width{}, readHeight{}, displayHeight{};
    uint64_t smode2{};
};

struct GsReplayFrame {
    struct ColorUpload {
        std::size_t beforeDraw;
        GSTransferCommand transfer;
        uint32_t firstPixel;
        std::vector<uint8_t> pixels;
        unsigned channels() const { return transfer.bitbltbuf.dpsm == GS_PSM_CT24 ? 3u : 4u; }
    };
    enum class Status { Complete, VramWrite, BudgetExceeded, Discontinuous };
    Status status = Status::Complete;
    uint64_t startTick{}, endTick{};
    std::vector<uint8_t> initialVram;
    std::vector<GsTextureSnapshot> draws;
    GsReplayScanout scanout;
    std::vector<uint8_t> cpuPixels;
    uint32_t cpuWidth{}, cpuHeight{}, cpuStride{};
    bool pairCpuPixels = true;
    static constexpr std::size_t PageCount = GSMem::MEMORY_SIZE / GSMem::GS_PAGE_SIZE;
    std::bitset<PageCount> uploadedPages;
    std::optional<GSTransferCommand> firstOtherUpload;
    std::vector<ColorUpload> colorUploads;
};

struct GsReplaySeed {
    uint32_t width{}, height{};
    std::vector<uint8_t> color;
    std::vector<uint8_t> depth; // Z16 in R/G, least significant byte first.
};

inline bool gsReplayWritesDepth(const GSContext& context) {
    return (context.test & (1u << 16)) && !context.zbuf.zmask;
}

inline unsigned gsReplayDepthTest(const GSContext& context) {
    return context.test & (1u << 16) ? unsigned((context.test >> 17) & 3) : 1u;
}

inline bool gsReplayUsesDepth(const GSContext& context) {
    const auto test = gsReplayDepthTest(context);
    return test >= 2 || (test == 1 && gsReplayWritesDepth(context));
}

struct GsReplaySurface {
    GSFrameReg frame;
    uint32_t height{};
    std::optional<GSZbufReg> depth;
    bool sampledFromCpu{};
    uint32_t width() const { return frame.fbw * 64u; }
    uint32_t colorEnd() const { return frame.fbp + frame.fbw * ((height + 31) / 32); }
    uint32_t depthEnd() const { return depth->zbp + frame.fbw * ((height + 63) / 64); }
};

struct GsReplayPass {
    std::size_t target;
    std::optional<std::size_t> textureSurface;
    int left{}, top{}, right{}, bottom{};
};

struct GsReplayPlan {
    std::vector<GsReplaySurface> surfaces;
    std::vector<GsReplayPass> passes;
    std::vector<std::optional<std::size_t>> uploadTargets;
};

inline std::vector<unsigned> gsReplayTexturePageAxis(const GSPrimitiveBatch& batch, unsigned axis, unsigned pageSize,
                                                     unsigned extent, int scale, int textureScale) {
    const auto& state = batch.state;
    const unsigned size = axis ? state.textureHeight : state.textureWidth;
    int low = 0, high = int(size) - 1;
    if (state.prim.type == GS_PRIM_SPRITE && state.prim.fst && batch.vertexCount == 2) {
        const auto& ctx = state.context;
        const auto& a = batch.vertices[0];
        const auto& b = batch.vertices[1];
        const float offset = (axis ? ctx.xyoffset.ofy : ctx.xyoffset.ofx) >> 4;
        const float first = (axis ? a.y : a.x) - offset;
        const float last = (axis ? b.y : b.x) - offset;
        const auto firstScissor = axis ? ctx.scissor.y0 : ctx.scissor.x0;
        const auto lastScissor = axis ? ctx.scissor.y1 : ctx.scissor.x1;
        const auto pixel = [scale](float position) { return int(std::ceil(position * scale - 0.5f)); };
        const int start = std::max(int(firstScissor) * scale, pixel(std::min(first, last)));
        const int end = std::min(int(std::min<unsigned>(lastScissor + 1, extent)) * scale,
                                 pixel(std::max(first, last)));
        if (start >= end || first == last) return {};
        const float aUv = (axis ? a.v : a.u) / 16.0f;
        const float bUv = (axis ? b.v : b.u) / 16.0f;
        const auto uv = [&](int x) {
            const float t = ((x + 0.5f) / scale - first) / (last - first);
            return aUv * (1.0f - t) + bUv * t;
        };
        const auto [uMin, uMax] = std::minmax({uv(start), uv(end - 1)});
        const auto tap = [&](float coordinate, bool lastTap) {
            const auto texel = std::floor(coordinate * textureScale - (state.linearFilter ? 0.5f : 0.0f));
            return int(std::floor((texel + (state.linearFilter && lastTap ? 1 : 0)) / textureScale));
        };
        low = tap(uMin, false);
        high = tap(uMax, true);
    }
    const auto clamp = state.context.clamp;
    const unsigned mode = (clamp >> (axis * 2)) & 3u;
    const int regionLow = (clamp >> (4 + axis * 20)) & 1023u;
    const int regionHigh = (clamp >> (14 + axis * 20)) & 1023u;
    std::vector<unsigned> pages;
    if (!size || mode == 3 || (mode == 2 && (regionLow > regionHigh || regionHigh >= int(size)))) return pages;
    std::vector<bool> seen((size + pageSize - 1) / pageSize);
    for (int coordinate = low; coordinate <= high; ++coordinate) {
        const unsigned mapped = mode == 0 ? unsigned(coordinate) & (size - 1) :
                                mode == 1 ? std::clamp(coordinate, 0, int(size) - 1) :
                                            std::clamp(coordinate, regionLow, regionHigh);
        const unsigned page = mapped / pageSize;
        if (seen[page]) continue;
        seen[page] = true;
        pages.push_back(page);
    }
    return pages;
}

inline GsReplayPlan planGsReplay(const GsReplayFrame& frame, int scale = 2) {
    const auto& scanout = frame.scanout;
    const auto& source = scanout.source;
    if (scale < 1 || scale > 4 || frame.status != GsReplayFrame::Status::Complete || frame.draws.empty() ||
        frame.initialVram.size() != GSMem::MEMORY_SIZE || scanout.kind != GsReplayScanout::Kind::Direct ||
        scanout.originX || scanout.originY || (scanout.smode2 & 3u) == 1u || source.psm != GS_PSM_CT32 ||
        !source.fbw || source.fbw > 63 || !scanout.width || scanout.width > source.fbw * 64u || !scanout.readHeight)
        throw std::invalid_argument("GS replay requires one direct CT32 scanout and complete input");

    constexpr uint32_t pages = GSMem::MEMORY_SIZE / GSMem::GS_PAGE_SIZE;
    GsReplayPlan plan;
    plan.surfaces.push_back({source, scanout.readHeight, {}});
    plan.passes.reserve(frame.draws.size());
    for (const auto& draw : frame.draws) {
        const auto& ctx = draw.batch.state.context;
        if (ctx.frame.fbp >= pages || !ctx.frame.fbw || ctx.frame.fbw > 63 || ctx.frame.psm != GS_PSM_CT32 ||
            ctx.scissor.x0 > ctx.scissor.x1 || ctx.scissor.y0 > ctx.scissor.y1 ||
            !draw.batch.vertexCount || draw.batch.vertexCount > draw.batch.vertices.size())
            throw std::invalid_argument("Unsupported GS replay color layout or primitive bounds");
        const auto found = std::find_if(plan.surfaces.begin(), plan.surfaces.end(), [&](const auto& s) {
            return s.frame.fbp == ctx.frame.fbp;
        });
        const auto index = std::size_t(found - plan.surfaces.begin());
        if (found == plan.surfaces.end()) plan.surfaces.push_back({ctx.frame, 1, {}});
        auto& surface = plan.surfaces[index];
        if (surface.frame.fbw != ctx.frame.fbw)
            throw std::invalid_argument("GS replay changes stride of a live color surface");
        float maxX = 0, maxY = 0, minX = 4096, minY = 4096;
        for (const auto& vertex : std::span(draw.batch.vertices).first(draw.batch.vertexCount)) {
            if (!(vertex.x >= 0 && vertex.x < 4096 && vertex.y >= 0 && vertex.y < 4096))
                throw std::invalid_argument("Invalid GS replay coordinates");
            maxX = std::max(maxX, vertex.x);
            maxY = std::max(maxY, vertex.y);
            minX = std::min(minX, vertex.x);
            minY = std::min(minY, vertex.y);
        }
        const bool sprite = draw.batch.state.prim.type == GS_PRIM_SPRITE;
        const auto end = [sprite](float low, float high, unsigned offset) {
            // Cover both CPU integer sprite endpoints and the scaled raster.
            const int rasterEnd = int(std::ceil(high));
            return (sprite ? std::max(int(low) + 1, rasterEnd) : rasterEnd + 1) - int(offset >> 4);
        };
        const int right = std::min(int(ctx.scissor.x1) + 1, end(minX, maxX, ctx.xyoffset.ofx));
        const int bottom = std::min(int(ctx.scissor.y1) + 1, end(minY, maxY, ctx.xyoffset.ofy));
        if (right > int(surface.width())) throw std::invalid_argument("GS replay raster crosses color stride");
        surface.height = std::max(surface.height, uint32_t(std::max(bottom, 0)));
        const int left = std::max(int(ctx.scissor.x0), int(std::floor(minX)) - int(ctx.xyoffset.ofx >> 4));
        const int top = std::max(int(ctx.scissor.y0), int(std::floor(minY)) - int(ctx.xyoffset.ofy >> 4));
        plan.passes.push_back({index, {}, left, top, right, bottom});
        if (!gsReplayUsesDepth(ctx)) continue;
        if ((ctx.zbuf.psm != GS_PSM_Z16 && ctx.zbuf.psm != GS_PSM_Z16S) ||
            (surface.depth && (ctx.zbuf.zbp != surface.depth->zbp || ctx.zbuf.psm != surface.depth->psm)))
            throw std::invalid_argument("GS replay draw addresses an unsupported depth surface");
        surface.depth = ctx.zbuf;
    }
    const auto overlaps = [](uint32_t first, uint32_t end, uint32_t other, uint32_t otherEnd) {
        return first < otherEnd && other < end;
    };
    for (auto& surface : plan.surfaces) {
        surface.height = (surface.height + 31) & ~31u;
        if (surface.colorEnd() > pages || (surface.depth && (surface.depth->zbp >= pages || surface.depthEnd() > pages)))
            throw std::invalid_argument("GS replay surface wraps VRAM");
    }
    const auto checkUploadOverlap = [&](unsigned base, unsigned stride, unsigned pageHeight,
                                        int left, int top, int right, int bottom, const char* usage,
                                        bool auxiliary = false) {
        if (left >= right || top >= bottom) return false;
        for (unsigned row = unsigned(top) / pageHeight; row <= unsigned(bottom - 1) / pageHeight; ++row) {
            for (unsigned column = unsigned(left) / 64; column <= unsigned(right - 1) / 64; ++column) {
                const unsigned page = base + row * stride + column;
                if (!frame.uploadedPages.test(page)) continue;
                if (auxiliary) return true;
                std::string detail;
                if (const auto& upload = frame.firstOtherUpload) {
                    const auto& dst = upload->bitbltbuf;
                    detail = " (first other upload: PSM=" + std::to_string(dst.dpsm) +
                             ", DBP=" + std::to_string(dst.dbp) + ", DBW=" + std::to_string(dst.dbw) + ")";
                }
                throw std::invalid_argument(std::string("GS image upload overlaps ") + usage + " page " +
                                            std::to_string(page) + " (base=" + std::to_string(base) + ")" + detail);
            }
        }
        return false;
    };
    checkUploadOverlap(source.fbp, source.fbw, 32, 0, 0, scanout.width, scanout.readHeight, "scanout");
    for (std::size_t i = 0; i < plan.passes.size(); ++i) {
        const auto& pass = plan.passes[i];
        auto& surface = plan.surfaces[pass.target];
        const auto& ctx = frame.draws[i].batch.state.context;
        if (!gsReplayDepthTest(ctx)) continue;
        surface.sampledFromCpu |= checkUploadOverlap(surface.frame.fbp, surface.frame.fbw, 32, pass.left, pass.top,
                                                    pass.right, pass.bottom, "color raster", pass.target != 0);
        if (gsReplayUsesDepth(ctx))
            surface.sampledFromCpu |= checkUploadOverlap(surface.depth->zbp, surface.frame.fbw, 64, pass.left, pass.top,
                                                        pass.right, pass.bottom, "depth raster", pass.target != 0);
    }
    for (std::size_t i = 0; i < plan.surfaces.size(); ++i) {
        const auto& a = plan.surfaces[i];
        for (std::size_t j = 0; j < plan.surfaces.size(); ++j) {
            const auto& b = plan.surfaces[j];
            if ((i < j && overlaps(a.frame.fbp, a.colorEnd(), b.frame.fbp, b.colorEnd())) ||
                (b.depth && overlaps(a.frame.fbp, a.colorEnd(), b.depth->zbp, b.depthEnd())) ||
                (i < j && a.depth && b.depth && overlaps(a.depth->zbp, a.depthEnd(), b.depth->zbp, b.depthEnd())))
                throw std::invalid_argument("GS replay surfaces overlap in VRAM");
        }
    }
    std::size_t previousUploadDraw = 0;
    for (const auto& upload : frame.colorUploads) {
        const auto& transfer = upload.transfer;
        const auto& dst = transfer.bitbltbuf;
        const unsigned stride = std::max<unsigned>(dst.dbw, 1);
        const unsigned rowWidth = transfer.trxreg.rrw;
        const auto count = upload.pixels.size() / upload.channels();
        if (!rowWidth || !count || upload.pixels.size() % upload.channels() || upload.beforeDraw > frame.draws.size() ||
            upload.beforeDraw < previousUploadDraw ||
            (dst.dpsm != GS_PSM_CT32 && dst.dpsm != GS_PSM_CT24) ||
            upload.firstPixel + count > uint64_t(rowWidth) * transfer.trxreg.rrh)
            throw std::invalid_argument("Invalid GS color upload payload");
        previousUploadDraw = upload.beforeDraw;
        std::optional<std::size_t> target;
        for (std::size_t j = 0; j < plan.surfaces.size(); ++j) {
            const auto& surface = plan.surfaces[j];
            if (surface.sampledFromCpu) continue;
            for (std::size_t pixel = upload.firstPixel, end = pixel + count; pixel < end;) {
                const unsigned x = transfer.trxpos.dsax + unsigned(pixel % rowWidth);
                const unsigned y = transfer.trxpos.dsay + unsigned(pixel / rowWidth);
                const auto length = std::min<std::size_t>(end - pixel, rowWidth - pixel % rowWidth);
                const unsigned first = dst.dbp / GSMem::BLOCKS_PER_PAGE + y / 32 * stride + x / 64;
                const unsigned last = first + (x % 64 + unsigned(length) - 1) / 64;
                for (unsigned p = first; p <= last + (dst.dbp % GSMem::BLOCKS_PER_PAGE != 0); ++p) {
                    const unsigned page = p % pages;
                    if (surface.depth && page >= surface.depth->zbp && page < surface.depthEnd())
                        throw std::invalid_argument("GS color upload overlaps depth page " + std::to_string(page));
                    if (page < surface.frame.fbp || page >= surface.colorEnd()) continue;
                    if (dst.dbp != surface.frame.fbp * GSMem::BLOCKS_PER_PAGE || stride != surface.frame.fbw ||
                        x + length > surface.width() || y >= surface.height || (target && *target != j))
                        throw std::invalid_argument("GS color upload uses an incompatible layout at page " + std::to_string(page) +
                            " (DBP=" + std::to_string(dst.dbp) + ", DBW=" + std::to_string(stride) + ")");
                    target = j;
                }
                pixel += length;
            }
        }
        if (target) {
            const auto& surface = plan.surfaces[*target];
            if (transfer.trxpos.dsax + rowWidth > surface.width() ||
                transfer.trxpos.dsay + (upload.firstPixel + count - 1) / rowWidth >= surface.height)
                throw std::invalid_argument("GS color upload extends outside its replay surface");
        }
        plan.uploadTargets.push_back(target);
    }
    std::vector<bool> written(plan.surfaces.size());
    std::array<int, pages> depthWriter;
    depthWriter.fill(-1);
    GSMem::TexturePageCache initialDepthCache;
    for (std::size_t i = 0; i < plan.passes.size(); ++i) {
        auto& pass = plan.passes[i];
        if (plan.surfaces[pass.target].sampledFromCpu) continue;
        const auto& snapshot = frame.draws[i];
        const auto& state = snapshot.batch.state;
        const auto& tex = state.context.tex0;
        if (state.prim.tme && snapshot.status == GsTextureSnapshot::Status::Decoded && gsReplayDepthTest(state.context)) {
            const bool direct = tex.psm == GS_PSM_CT32;
            const unsigned pageWidth = direct ? 64 : 128;
            const unsigned pageHeight = direct ? 32 : tex.psm == GS_PSM_T8 ? 64 : 128;
            const auto& target = plan.surfaces[pass.target];
            const auto visit = [&](int textureScale, const auto& inspect) {
                const auto rows = gsReplayTexturePageAxis(snapshot.batch, 1, pageHeight, target.height, scale, textureScale);
                const auto columns = gsReplayTexturePageAxis(snapshot.batch, 0, pageWidth, target.width(), scale, textureScale);
                for (unsigned row : rows)
                    for (unsigned column : columns)
                        inspect((tex.tbp0 / GSMem::BLOCKS_PER_PAGE + row * (tex.tbw * 64 / pageWidth) + column) % pages);
            };
            const auto resolveColor = [&](unsigned page) {
                if (frame.uploadedPages.test(page)) return;
                for (std::size_t j = 0; j < plan.surfaces.size(); ++j) {
                    if (!written[j]) continue;
                    const auto& surface = plan.surfaces[j];
                    if (page < surface.frame.fbp || page >= surface.colorEnd()) continue;
                    if (j == pass.target || !direct || tex.tbp0 % GSMem::BLOCKS_PER_PAGE || !snapshot.matchesVram ||
                        (pass.textureSurface && *pass.textureSurface != j))
                        throw std::invalid_argument("GS replay cannot sample this color surface alias");
                    pass.textureSurface = j;
                }
            };
            if (direct) visit(scale, resolveColor);
            if (!pass.textureSurface) visit(1, resolveColor);
            visit(pass.textureSurface ? scale : 1, [&](unsigned page) {
                if (pass.textureSurface && frame.uploadedPages.test(page)) {
                    const auto& producer = plan.surfaces[*pass.textureSurface];
                    if (page >= producer.frame.fbp && page < producer.colorEnd())
                        throw std::invalid_argument("GS texture mixes a live color surface with an uploaded page");
                }
                if (depthWriter[page] >= 0) {
                    // A display sprite may cross from its scaled CT32 producer
                    // into unrelated depth bytes. The sampler uses the captured
                    // CPU texel outside the producer's physical color pages.
                    if (direct && pass.textureSurface && state.prim.type == GS_PRIM_SPRITE) {
                        const auto& producer = plan.surfaces[*pass.textureSurface];
                        if (page < producer.frame.fbp || page >= producer.colorEnd()) return;
                    }
                    std::string evidence;
                    if (direct && tex.tbp0 % GSMem::BLOCKS_PER_PAGE == 0) {
                        const auto pixels = snapshot.pixels();
                        GSMem::TexturePageCache cache;
                        unsigned changed = 0;
                        unsigned words = 0;
                        for (unsigned y = 0; y < state.textureHeight; ++y) {
                            for (unsigned x = 0; x < state.textureWidth; ++x) {
                                if ((tex.tbp0 / GSMem::BLOCKS_PER_PAGE + y / 32 * tex.tbw + x / 64) % pages != page)
                                    continue;
                                const auto offset = (std::size_t(y) * state.textureWidth + x) * 4;
                                if (offset + 4 > pixels.size()) continue;
                                const auto value = GSMem::ReadTexture(cache, frame.initialVram.data(), tex.psm,
                                                                      tex.tbp0, tex.tbw, x, y);
                                for (unsigned channel = 0; channel < 4; ++channel)
                                    changed += pixels[offset + channel] != uint8_t(value >> (8 * channel));
                                ++words;
                            }
                        }
                        evidence = ", snapshot bytes changed=" + std::to_string(changed) + "/" + std::to_string(words * 4);
                    }
                    throw std::invalid_argument("GS replay draw " + std::to_string(i) + " samples written depth page " +
                                                    std::to_string(page) + " after draw " + std::to_string(depthWriter[page]) +
                                                    " (TBP=" + std::to_string(tex.tbp0) + ", PSM=" + std::to_string(tex.psm) +
                                                    ", linear=" + std::to_string(state.linearFilter) + ", UV=" +
                                                    std::to_string(snapshot.batch.vertices[0].u) + "," + std::to_string(snapshot.batch.vertices[0].v) +
                                                    ".." + std::to_string(snapshot.batch.vertices[1].u) + "," +
                                                    std::to_string(snapshot.batch.vertices[1].v) + evidence + ")");
                }
            });
        }
        if (gsReplayDepthTest(state.context)) {
            written[pass.target] = true;
            const auto test = state.context.test;
            const bool rejectsDepthOnAlphaFail = (test & 1u) && ((test >> 1) & 7u) == 0 && ((test >> 12) & 3u) != 2;
            if (gsReplayWritesDepth(state.context) && !rejectsDepthOnAlphaFail && pass.left < pass.right && pass.top < pass.bottom) {
                const auto& surface = plan.surfaces[pass.target];
                const auto vertices = std::span(snapshot.batch.vertices).first(snapshot.batch.vertexCount);
                const double z = vertices.back().z;
                const bool flat = z >= 0 && z <= 65535 && z == std::floor(z) &&
                    (state.prim.type == GS_PRIM_SPRITE || std::all_of(vertices.begin(), vertices.end(),
                        [z](const auto& vertex) { return vertex.z == z; }));
                for (int y = pass.top / 64; y <= (pass.bottom - 1) / 64; ++y) {
                    for (int x = pass.left / 64; x <= (pass.right - 1) / 64; ++x) {
                        auto& writer = depthWriter.at(surface.depth->zbp + unsigned(y) * surface.frame.fbw + unsigned(x));
                        // A constant write cannot change an untouched page already holding that value.
                        bool unchanged = flat && writer < 0;
                        for (int py = std::max(pass.top, y * 64); unchanged && py < std::min(pass.bottom, (y + 1) * 64); ++py) {
                            for (int px = std::max(pass.left, x * 64); px < std::min(pass.right, (x + 1) * 64); ++px) {
                                const auto initial = GSMem::ReadTexture(initialDepthCache, frame.initialVram.data(), surface.depth->psm,
                                    surface.depth->zbp * GSMem::BLOCKS_PER_PAGE, surface.frame.fbw, px, py);
                                if (initial != z && (((test >> 17) & 3u) < 2 || initial < z)) {
                                    unchanged = false;
                                    break;
                                }
                            }
                        }
                        if (!unchanged) writer = int(i);
                    }
                }
            }
        }
    }
    return plan;
}

inline GsReplaySeed makeGsReplaySeed(std::span<const uint8_t> vram, const GsReplaySurface& surface) {
    const auto& source = surface.frame;
    const auto& depth = surface.depth;
    GsReplaySeed seed{surface.width(), surface.height, {}, {}};
    const auto bytes = std::size_t(seed.width) * seed.height * 4;
    seed.color.resize(bytes);
    if (depth) seed.depth.resize(bytes);
    GSMem::TexturePageCache colorCache, depthCache;
    for (uint32_t y = 0; y < seed.height; ++y) {
        for (uint32_t x = 0; x < seed.width; ++x) {
            const auto index = (std::size_t(y) * seed.width + x) * 4;
            const auto color = GSMem::ReadTexture(colorCache, vram.data(), source.psm,
                                                 source.fbp * GSMem::BLOCKS_PER_PAGE, source.fbw, x, y);
            for (unsigned channel = 0; channel < 4; ++channel) seed.color[index + channel] = uint8_t(color >> (channel * 8));
            if (depth) {
                const auto z = GSMem::ReadTexture(depthCache, vram.data(), depth->psm,
                                                 depth->zbp * GSMem::BLOCKS_PER_PAGE, source.fbw, x, y);
                seed.depth[index] = uint8_t(z);
                seed.depth[index + 1] = uint8_t(z >> 8);
            }
        }
    }
    return seed;
}

// Present runs on the display thread and Submit on the producer thread.
// Both hooks hold the same CPU backend lock while cutting the draw interval.
class GsFrameReplayCapture {
public:
    bool arm(std::size_t drawBudgetBytes, bool continuous = false) {
        std::lock_guard lock(mutex);
        if (!drawBudgetBytes || armed || ready) return false;
        budget = drawBudgetBytes;
        rolling = continuous;
        armed.store(true, std::memory_order_release);
        return true;
    }

    bool active() const noexcept { return armed.load(std::memory_order_acquire); }

    template<class Snapshot>
    void draw(const void* backend, const GSPrimitiveBatch& batch, Snapshot snapshot,
              std::span<const uint8_t> vram = {}) {
        if (!active()) return;
        std::lock_guard lock(mutex);
        if (owner != backend) return;
        if (!sawDraw && leadingVramWrite) {
            if (vram.size() == frame.initialVram.size())
                frame.initialVram.assign(vram.begin(), vram.end());
            else
                reject(GsReplayFrame::Status::VramWrite);
            leadingVramWrite = false;
        }
        sawDraw = true;
        if (frame.status != GsReplayFrame::Status::Complete) return;
        if (remaining < sizeof(GsTextureSnapshot)) {
            reject(GsReplayFrame::Status::BudgetExceeded);
            return;
        }
        auto draw = snapshot();
        std::size_t bytes = sizeof(GsTextureSnapshot);
        if (draw.rgba) {
            const auto& state = batch.state;
            const uint64_t key = state.context.tex0.tbp0 | (uint64_t(state.textureWidth) << 16) |
                                 (uint64_t(state.textureHeight) << 32);
            const auto [first, last] = textures.equal_range(key);
            const auto same = std::find_if(first, last, [&](const auto& candidate) {
                return *candidate.second == *draw.rgba;
            });
            if (same != last) {
                draw.rgba = same->second;
            } else {
                bytes += draw.rgba->size();
                if (bytes <= remaining) textures.emplace(key, draw.rgba);
            }
        }
        if (bytes > remaining) {
            reject(GsReplayFrame::Status::BudgetExceeded);
            return;
        }
        remaining -= bytes;
        frame.draws.push_back(std::move(draw));
    }

    void vramWrite(const void* backend) {
        if (!active()) return;
        std::lock_guard lock(mutex);
        if (owner != backend) return;
        if (sawDraw) reject(GsReplayFrame::Status::VramWrite);
        else leadingVramWrite = true;
    }

    // The per-draw texture snapshots already include completed uploads. Mark
    // their physical destination pages to detect writes into GPU attachments.
    void imageUpload(const void* backend, const GSTransferCommand& transfer, uint32_t firstPixel,
                     std::span<const uint8_t> data) {
        if (!active()) return;
        std::lock_guard lock(mutex);
        if (owner != backend || frame.status != GsReplayFrame::Status::Complete) return;
        if (!sawDraw) {
            leadingVramWrite = true;
            return;
        }
        const auto& dst = transfer.bitbltbuf;
        const auto psm = static_cast<GSMem::PixelStorageMode>(dst.dpsm);
        if (psm == GSMem::C32 || psm == GSMem::C24) {
            const uint32_t total = uint32_t(transfer.trxreg.rrw) * transfer.trxreg.rrh;
            if (firstPixel >= total) return;
            const unsigned channels = psm == GSMem::C24 ? 3u : 4u;
            const auto bytes = std::min<std::size_t>(data.size() / channels, total - firstPixel) * channels;
            if (!bytes) return;
            if (bytes + sizeof(GsReplayFrame::ColorUpload) > remaining) {
                reject(GsReplayFrame::Status::BudgetExceeded);
                return;
            }
            remaining -= bytes + sizeof(GsReplayFrame::ColorUpload);
            frame.colorUploads.push_back({frame.draws.size(), transfer, firstPixel, {data.begin(), data.begin() + bytes}});
            return;
        }
        if (!frame.firstOtherUpload) frame.firstOtherUpload = transfer;
        unsigned width = 64, height = 32;
        switch (psm) {
        case GSMem::C32: case GSMem::C24: case GSMem::Z32: case GSMem::Z24:
        case GSMem::P8H: case GSMem::P4HL: case GSMem::P4HH: break;
        case GSMem::C16: case GSMem::C16S: case GSMem::Z16: case GSMem::Z16S: height = 64; break;
        case GSMem::P8: width = 128; height = 64; break;
        case GSMem::P4: width = 128; height = 128; break;
        default: reject(GsReplayFrame::Status::VramWrite); return;
        }
        if (!transfer.trxreg.rrw || !transfer.trxreg.rrh) return;
        if (psm == GSMem::P4) {
            const unsigned rowWidth = transfer.trxreg.rrw;
            const unsigned total = rowWidth * transfer.trxreg.rrh;
            if (firstPixel >= total) return;
            const auto count = std::min<std::size_t>(data.size() * 2, total - firstPixel);
            const unsigned stride = std::max<unsigned>(dst.dbw, 1) / 2;
            for (std::size_t pixel = firstPixel, end = pixel + count; pixel < end;) {
                const unsigned x = transfer.trxpos.dsax + unsigned(pixel % rowWidth);
                const unsigned y = transfer.trxpos.dsay + unsigned(pixel / rowWidth);
                const auto length = std::min({end - pixel, std::size_t(rowWidth - pixel % rowWidth),
                                              std::size_t(32 - x % 32)});
                const unsigned block = (dst.dbp % GSMem::BLOCKS_PER_PAGE) +
                                       GSPSMT4::blockTable4[(y / 16) % 8][(x / 32) % 4];
                const unsigned page = dst.dbp / GSMem::BLOCKS_PER_PAGE + y / 128 * stride + x / 128 +
                                      block / GSMem::BLOCKS_PER_PAGE;
                frame.uploadedPages.set(page % GsReplayFrame::PageCount);
                pixel += length;
            }
            return;
        }
        const unsigned startX = transfer.trxpos.dsax, startY = transfer.trxpos.dsay;
        const unsigned endX = startX + transfer.trxreg.rrw - 1, endY = startY + transfer.trxreg.rrh - 1;
        const unsigned stride = std::max<unsigned>(dst.dbw, 1u) * 64 / width;
        // Account for block-aligned bases that straddle two physical pages.
        for (unsigned y = startY / height; y <= endY / height; ++y) {
            for (unsigned x = startX / width; x <= endX / width; ++x) {
                const auto page = dst.dbp / GSMem::BLOCKS_PER_PAGE + y * stride + x;
                frame.uploadedPages.set(page % GsReplayFrame::PageCount);
                if (dst.dbp % GSMem::BLOCKS_PER_PAGE) frame.uploadedPages.set((page + 1) % GsReplayFrame::PageCount);
            }
        }
    }

    std::optional<GsReplayFrame> boundary(const void* backend, uint64_t tick, std::span<const uint8_t> vram,
                                          bool displayConfigured) {
        if (!active() || vram.empty()) return {};
        std::lock_guard lock(mutex);
        if (!armed || finishing) return {};
        if (owner && owner != backend) return {};
        if (owner && tick == frame.startTick) return {};
        if (!owner || !sawDraw) {
            beginInterval(backend, tick, vram, displayConfigured);
            return {};
        }
        if (tick < frame.startTick || tick - frame.startTick != 1)
            reject(GsReplayFrame::Status::Discontinuous);
        frame.endTick = tick;
        auto completed = std::move(frame);
        finishing = true;
        beginInterval(backend, tick, vram, rolling && displayConfigured);
        return completed;
    }

    void publish(GsReplayFrame finished) {
        std::lock_guard lock(mutex);
        if (!finishing) return;
        ready.emplace(std::move(finished));
        finishing = false;
        if (!rolling) armed.store(false, std::memory_order_release);
    }

    std::optional<GsReplayFrame> take() {
        std::lock_guard lock(mutex);
        return std::exchange(ready, std::nullopt);
    }

    void stop() {
        std::lock_guard lock(mutex);
        armed.store(false, std::memory_order_release);
        finishing = false;
        beginInterval(nullptr, 0, {}, false);
        ready.reset();
    }

private:
    void beginInterval(const void* backend, uint64_t tick, std::span<const uint8_t> vram,
                       bool displayConfigured) {
        frame = {};
        textures.clear();
        sawDraw = false;
        leadingVramWrite = false;
        owner = displayConfigured ? backend : nullptr;
        remaining = budget;
        if (!owner) return;
        frame.startTick = tick;
        frame.pairCpuPixels = !rolling;
        frame.initialVram.assign(vram.begin(), vram.end());
    }

    void reject(GsReplayFrame::Status reason) {
        if (frame.status != GsReplayFrame::Status::Complete) return;
        frame.status = reason;
        frame.draws.clear();
        frame.colorUploads.clear();
        frame.initialVram.clear();
        textures.clear();
    }

    std::mutex mutex;
    std::atomic<bool> armed = false;
    bool finishing = false;
    bool sawDraw = false;
    bool leadingVramWrite = false;
    bool rolling = false;
    const void* owner = nullptr;
    std::size_t remaining{}, budget{};
    GsReplayFrame frame;
    std::unordered_multimap<uint64_t, std::shared_ptr<const std::vector<uint8_t>>> textures;
    std::optional<GsReplayFrame> ready;
};

inline GsFrameReplayCapture gsFrameReplayCapture;

// Populated only by PresentFromLocalMemory on the presentation thread.
inline thread_local GsReplayScanout* gsReplayScanout = nullptr;

}
