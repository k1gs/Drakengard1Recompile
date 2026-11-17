#pragma once

#include "raylib.h"
#include "rlgl.h"
#include "external/glad.h"
#include "gs_frame_replay.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <iostream>
#include <span>
#include <stdexcept>

namespace drakengard {

// One CT32 color target. Texture handles must reference a decoded RGBA snapshot
// of the guest texture at submission, with GS (0,0) in the first uploaded row.
struct GsOpenGlTriangle {
    GSPrimitiveBatch batch;
    Texture2D texture{};
    Texture2D colorSource{};
    uint32_t colorSourcePage{};
};

class GsOpenGlTriangles {
public:
    GsOpenGlTriangles() {
        const Shader compiled = LoadShaderFromMemory(nullptr, FragmentShader);
        if (!compiled.id || compiled.id == rlGetShaderIdDefault())
            throw std::runtime_error("GS triangle shader did not link");
        shader = compiled;
        positionsLocation = GetShaderLocation(shader, "positions[0]");
        colorsLocation = GetShaderLocation(shader, "colors[0]");
        textureCoordinatesLocation = GetShaderLocation(shader, "coordinates[0]");
        depthsLocation = GetShaderLocation(shader, "depths");
        outputLocation = GetShaderLocation(shader, "outputGrid");
        modesLocation = GetShaderLocation(shader, "modes");
        spriteLocation = GetShaderLocation(shader, "sprite");
        wrapLocation = GetShaderLocation(shader, "wrapMode");
        samplerLocation = GetShaderLocation(shader, "gsTexture");
        blendLocation = GetShaderLocation(shader, "blendState");
        alphaTestLocation = GetShaderLocation(shader, "alphaTestState");
        destinationLocation = GetShaderLocation(shader, "gsDestination");
        seedModeLocation = GetShaderLocation(shader, "seedMode");
        uploadOriginLocation = GetShaderLocation(shader, "uploadOrigin");
        colorSourceLocation = GetShaderLocation(shader, "gsColorSource");
        colorSourceLayoutLocation = GetShaderLocation(shader, "colorSourceLayout");
        fogFactorsLocation = GetShaderLocation(shader, "fogFactors");
        fogColorLocation = GetShaderLocation(shader, "fogColor");
    }

    GsOpenGlTriangles(const GsOpenGlTriangles&) = delete;
    GsOpenGlTriangles& operator=(const GsOpenGlTriangles&) = delete;
    ~GsOpenGlTriangles() { UnloadShader(shader); }

    void renderFrame(RenderTexture2D target, int scale, const GsReplayFrame& frame, bool diagnostics = true) const {
        const auto plan = planGsReplay(frame, scale);
        if (scale < 1 || scale > 4 || !target.id || target.texture.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 ||
            target.texture.width != int(frame.scanout.width * scale) || target.texture.height != int(frame.scanout.readHeight * scale))
            throw std::invalid_argument("GS replay output does not match scanout");
        struct Targets {
            std::vector<RenderTexture2D> items;
            ~Targets() { for (const auto& item : items) if (item.id) UnloadRenderTexture(item); }
        } targets;
        targets.items.reserve(plan.surfaces.size());
        std::size_t gpuTriangles = 0;
        for (const auto& surface : plan.surfaces) {
            if (surface.sampledFromCpu) {
                targets.items.emplace_back();
                continue;
            }
            const auto item = LoadRenderTexture(int(surface.width() * scale), int(surface.height * scale));
            targets.items.push_back(item);
            if (!item.id) throw std::runtime_error("Could not allocate GS replay surface");
        }

        UploadedDraws uploaded;
        uploaded.draws.reserve(frame.draws.size());
        for (std::size_t i = 0; i < frame.draws.size(); ++i) {
            if (plan.surfaces[plan.passes[i].target].sampledFromCpu) uploaded.draws.emplace_back();
            else {
                uploaded.upload(frame.draws[i]);
                const auto type = frame.draws[i].batch.state.prim.type;
                if (diagnostics)
                    gpuTriangles += type == GS_PRIM_TRIANGLE || type == GS_PRIM_TRISTRIP || type == GS_PRIM_TRIFAN;
            }
        }
        if (diagnostics) {
            for (const auto& surface : plan.surfaces)
                if (surface.sampledFromCpu)
                    std::cerr << "[gs-replay] auxiliary FBP " << surface.frame.fbp << " uses captured CPU texture values\n";
            std::cerr << "[gs-replay] GPU triangle commands=" << gpuTriangles << '\n';
        }
        std::vector<bool> initialized(plan.surfaces.size());
        std::size_t nextUpload = 0;
        for (std::size_t begin = 0;;) {
            while (nextUpload < frame.colorUploads.size() && frame.colorUploads[nextUpload].beforeDraw == begin) {
                if (const auto index = plan.uploadTargets[nextUpload]) {
                    if (!initialized[*index]) {
                        const auto seed = makeGsReplaySeed(frame.initialVram, plan.surfaces[*index]);
                        render(targets.items[*index], scale, {}, BLANK, &seed);
                        initialized[*index] = true;
                    }
                    applyColorUpload(targets.items[*index], scale, frame.colorUploads[nextUpload]);
                }
                ++nextUpload;
            }
            if (begin == plan.passes.size()) break;
            const auto index = plan.passes[begin].target;
            if (plan.surfaces[index].sampledFromCpu) {
                ++begin;
                continue;
            }
            std::size_t end = begin;
            do {
                if (const auto producer = plan.passes[end].textureSurface) {
                    uploaded.draws[end].colorSource = targets.items[*producer].texture;
                    uploaded.draws[end].colorSourcePage = plan.surfaces[*producer].frame.fbp;
                }
                ++end;
            } while (end < plan.passes.size() && plan.passes[end].target == index &&
                     (nextUpload == frame.colorUploads.size() || frame.colorUploads[nextUpload].beforeDraw != end));
            std::optional<GsReplaySeed> seed;
            if (!initialized[index]) seed = makeGsReplaySeed(frame.initialVram, plan.surfaces[index]);
            render(targets.items[index], scale, std::span(uploaded.draws).subspan(begin, end - begin),
                   BLANK, seed ? &*seed : nullptr, initialized[index]);
            initialized[index] = true;
            begin = end;
        }
        if (!initialized.front()) {
            const auto seed = makeGsReplaySeed(frame.initialVram, plan.surfaces.front());
            render(targets.items.front(), scale, {}, BLANK, &seed);
        }

        // Copy the visible rectangle only after all passes succeeded. GL's
        // framebuffer origin is bottom-left and the GS scanout starts at top-left.
        const auto& scanout = targets.items.front();
        const int width = target.texture.width, height = target.texture.height;
        glBindFramebuffer(GL_READ_FRAMEBUFFER, scanout.id);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target.id);
        glBlitFramebuffer(0, scanout.texture.height - height, width, scanout.texture.height,
                          0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // CPU-owned snapshots cross to the GL thread before any texture upload.
    void renderSnapshots(RenderTexture2D target, int scale, std::span<const GsTextureSnapshot> snapshots,
                         Color clear, const GsReplaySeed* seed = nullptr) const {
        const UploadedDraws uploaded(snapshots);
        render(target, scale, uploaded.draws, clear, seed);
    }

private:
    void applyColorUpload(RenderTexture2D target, int scale, const GsReplayFrame::ColorUpload& upload) const {
        const auto& transfer = upload.transfer;
        const unsigned rowWidth = transfer.trxreg.rrw;
        const auto channels = upload.channels();
        const auto count = upload.pixels.size() / channels;
        if (channels == 3) {
            // CT24 shares CT32's RGB layout and leaves the high byte intact.
            const bool blend = glIsEnabled(GL_BLEND), depth = glIsEnabled(GL_DEPTH_TEST);
            std::array<GLboolean, 4> mask{};
            glGetBooleanv(GL_COLOR_WRITEMASK, mask.data());
            BeginTextureMode(target);
            rlDisableColorBlend();
            rlDisableDepthTest();
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
            for (std::size_t offset = 0; offset < count;) {
                const auto pixel = upload.firstPixel + offset;
                const auto length = std::min(count - offset, rowWidth - pixel % rowWidth);
                const int x = int(transfer.trxpos.dsax + pixel % rowWidth);
                const int y = int(transfer.trxpos.dsay + pixel / rowWidth);
                const auto id = rlLoadTexture(upload.pixels.data() + offset * 3, int(length), 1,
                                              PIXELFORMAT_UNCOMPRESSED_R8G8B8, 1);
                if (!id) {
                    EndTextureMode();
                    glColorMask(mask[0], mask[1], mask[2], mask[3]);
                    if (blend) rlEnableColorBlend();
                    if (depth) rlEnableDepthTest();
                    throw std::runtime_error("Could not upload GS CT24 row");
                }
                const Texture2D row{id, int(length), 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8};
                const int mode = 3;
                const float grid[] = {float(target.texture.height), float(scale)};
                const int origin[] = {x, y};
                BeginShaderMode(shader);
                SetShaderValue(shader, seedModeLocation, &mode, SHADER_UNIFORM_INT);
                SetShaderValue(shader, outputLocation, grid, SHADER_UNIFORM_VEC2);
                SetShaderValue(shader, uploadOriginLocation, origin, SHADER_UNIFORM_IVEC2);
                SetShaderValueTexture(shader, samplerLocation, row);
                DrawRectangle(x * scale, y * scale, int(length) * scale, scale, WHITE);
                EndShaderMode();
                UnloadTexture(row);
                offset += length;
            }
            EndTextureMode();
            glColorMask(mask[0], mask[1], mask[2], mask[3]);
            if (blend) rlEnableColorBlend();
            if (depth) rlEnableDepthTest();
            return;
        }
        std::vector<uint8_t> row;
        row.reserve(std::size_t(rowWidth) * scale * 4);
        rlActiveTextureSlot(0);
        glBindTexture(GL_TEXTURE_2D, target.texture.id);
        for (std::size_t offset = 0; offset < count;) {
            const auto pixel = upload.firstPixel + offset;
            const auto length = std::min(count - offset, rowWidth - pixel % rowWidth);
            const int x = int(transfer.trxpos.dsax + pixel % rowWidth) * scale;
            const int y = target.texture.height - int(transfer.trxpos.dsay + pixel / rowWidth + 1) * scale;
            row.clear();
            for (std::size_t i = offset; i < offset + length; ++i) {
                const auto first = upload.pixels.begin() + i * 4;
                for (int repeat = 0; repeat < scale; ++repeat) row.insert(row.end(), first, first + 4);
            }
            for (int repeat = 0; repeat < scale; ++repeat)
                glTexSubImage2D(GL_TEXTURE_2D, 0, x, y + repeat, int(length) * scale, 1,
                                GL_RGBA, GL_UNSIGNED_BYTE, row.data());
            offset += length;
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    struct UploadedDraws {
        std::vector<GsOpenGlTriangle> draws;
        std::unordered_map<const std::vector<uint8_t>*, Texture2D> textures;
        ~UploadedDraws() { release(); }
        UploadedDraws() = default;
        void release() {
            for (const auto& entry : textures) if (entry.second.id) UnloadTexture(entry.second);
        }
        explicit UploadedDraws(std::span<const GsTextureSnapshot> snapshots) {
            try {
                draws.reserve(snapshots.size());
                for (const auto& snapshot : snapshots) upload(snapshot);
            } catch (...) {
                release();
                throw;
            }
        }
        void upload(const GsTextureSnapshot& snapshot) {
            using Status = GsTextureSnapshot::Status;
            const auto& state = snapshot.batch.state;
            const auto width = state.textureWidth;
            const auto height = state.textureHeight;
            const auto pixels = snapshot.pixels();
            if (state.prim.tme) {
                if (snapshot.status != Status::Decoded || !width || !height || width > 1024 || height > 1024 ||
                    pixels.size() != std::size_t(width) * height * 4)
                    throw std::invalid_argument("Missing or invalid decoded GS texture snapshot");
            } else if (snapshot.status != Status::Untextured || !pixels.empty()) {
                throw std::invalid_argument("Unexpected GS texture snapshot on an untextured draw");
            }
            auto& draw = draws.emplace_back(GsOpenGlTriangle{snapshot.batch, {}});
            if (state.prim.tme) {
                auto& texture = textures[snapshot.rgba.get()];
                if (!texture.id) {
                    const auto id = rlLoadTexture(pixels.data(), width, height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
                    if (!id) throw std::runtime_error("Could not upload decoded GS texture snapshot");
                    texture = {id, width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
                }
                draw.texture = texture;
            }
        }
    };

public:
    // Call on the GL thread outside a drawing pass. Validate the whole batch
    // before touching the output so an unsupported draw cannot disappear.
    // The pass owns one depth surface and defaults to guest depth zero.
    void render(RenderTexture2D target, int scale, std::span<const GsOpenGlTriangle> draws, Color clear,
                const GsReplaySeed* seed = nullptr, bool resume = false) const {
        if (!target.id || target.texture.width <= 0 || target.texture.height <= 0 ||
            target.texture.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 ||
            scale < 1 || scale > 4 || target.texture.width % scale || target.texture.height % scale)
            throw std::invalid_argument("Invalid GS raster target or scale");
        const GSZbufReg* depth = nullptr;
        bool needsDestination = false;
        for (const auto& draw : draws) {
            validate(draw, target, draws.front().batch.state.context.frame);
            if (draw.colorSource.id == target.texture.id)
                throw std::invalid_argument("GS replay samples the active color attachment");
            needsDestination |= readsDestination(draw.batch.state);
            const auto& context = draw.batch.state.context;
            if (!usesDepth(context)) continue;
            if (depth && (context.zbuf.zbp != depth->zbp || context.zbuf.psm != depth->psm))
                throw std::invalid_argument("GS triangles require a single depth surface");
            depth = &context.zbuf;
        }

        struct SeedTextures {
            Texture2D color{}, depth{};
            ~SeedTextures() {
                if (color.id) UnloadTexture(color);
                if (depth.id) UnloadTexture(depth);
            }
        } initial;
        const bool seedDepth = seed && !seed->depth.empty();
        if (seed) {
            const auto width = target.texture.width / scale;
            const auto height = target.texture.height / scale;
            const auto bytes = std::size_t(width) * height * 4;
            if (seed->width != width || seed->height != height || seed->color.size() != bytes ||
                (seedDepth && seed->depth.size() != bytes) || (depth && !seedDepth) || resume)
                throw std::invalid_argument("GS replay seed dimensions or depth do not match the target");
            const auto upload = [width, height](const std::vector<uint8_t>& pixels) {
                const auto id = rlLoadTexture(pixels.data(), width, height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
                if (!id) throw std::runtime_error("Could not upload initial GS surface");
                return Texture2D{id, width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
            };
            initial.color = upload(seed->color);
            if (seedDepth) initial.depth = upload(seed->depth);
        }

        BeginTextureMode(target);
        if (depth || seedDepth) {
            GLint bits = 0;
            if (target.depth.id)
                glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                                       GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &bits);
            if (bits < 16) {
                EndTextureMode();
                throw std::invalid_argument("GS target requires a depth attachment of at least 16 bits");
            }
        }
        Texture2D destination{};
        if (needsDestination) {
            destination = {rlLoadTexture(nullptr, target.texture.width, target.texture.height,
                                         PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1),
                           target.texture.width, target.texture.height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
            GLint allocatedWidth = 0;
            glBindTexture(GL_TEXTURE_2D, destination.id);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &allocatedWidth);
            glBindTexture(GL_TEXTURE_2D, 0);
            if (!destination.id || allocatedWidth != destination.width) {
                rlUnloadTexture(destination.id);
                EndTextureMode();
                throw std::runtime_error("Could not allocate GS blend destination snapshot");
            }
        }
        const bool blendEnabled = glIsEnabled(GL_BLEND);
        const bool depthEnabled = glIsEnabled(GL_DEPTH_TEST);
        GLboolean oldDepthMask;
        std::array<GLboolean, 4> oldColorMask{};
        GLint oldDepthFunc;
        GLdouble oldDepthClear;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &oldDepthMask);
        glGetBooleanv(GL_COLOR_WRITEMASK, oldColorMask.data());
        glGetIntegerv(GL_DEPTH_FUNC, &oldDepthFunc);
        glGetDoublev(GL_DEPTH_CLEAR_VALUE, &oldDepthClear);
        glClearDepth(1.0);
        glDepthMask(GL_TRUE);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        if (!resume) ClearBackground(clear);
        rlDisableColorBlend();
        if (depth || seedDepth) rlEnableDepthTest();
        else rlDisableDepthTest();
        if (seed) {
            const float grid[] = {float(target.texture.height), float(scale)};
            const int seedMode = seedDepth ? 2 : 1;
            glDepthFunc(GL_ALWAYS);
            BeginShaderMode(shader);
            SetShaderValue(shader, seedModeLocation, &seedMode, SHADER_UNIFORM_INT);
            SetShaderValue(shader, outputLocation, grid, SHADER_UNIFORM_VEC2);
            SetShaderValueTexture(shader, samplerLocation, initial.color);
            if (seedDepth) SetShaderValueTexture(shader, destinationLocation, initial.depth);
            DrawRectangle(0, 0, target.texture.width, target.texture.height, WHITE);
            EndShaderMode();
        }
        constexpr std::array<GLenum, 4> comparisons{GL_NEVER, GL_ALWAYS, GL_LEQUAL, GL_LESS};
        for (const auto& draw : draws) {
            const auto& context = draw.batch.state.context;
            const auto test = depthTest(context);
            if (test == 0) continue;
            glDepthFunc(comparisons[test]);
            drawPrimitive(draw, target.texture.width, target.texture.height, scale, destination);
        }
        EndTextureMode();
        if (destination.id) UnloadTexture(destination);
        glDepthFunc(static_cast<GLenum>(oldDepthFunc));
        glDepthMask(oldDepthMask);
        glColorMask(oldColorMask[0], oldColorMask[1], oldColorMask[2], oldColorMask[3]);
        glClearDepth(oldDepthClear);
        if (depthEnabled) rlEnableDepthTest();
        else rlDisableDepthTest();
        if (blendEnabled) rlEnableColorBlend();
    }

private:
    static bool readsDestination(const GSDrawState& state) {
        return state.prim.abe || (state.context.test & (1u << 14));
    }

    static unsigned depthTest(const GSContext& context) { return gsReplayDepthTest(context); }

    static bool usesDepth(const GSContext& context) {
        return gsReplayUsesDepth(context);
    }

    static void validate(const GsOpenGlTriangle& draw, RenderTexture2D target, const GSFrameReg& frame) {
        const auto& state = draw.batch.state;
        const auto& context = state.context;
        const auto type = state.prim.type;
        const bool sprite = type == GS_PRIM_SPRITE;
        constexpr uint64_t testBits = (1u << 19) - 1;
        if (draw.batch.vertexCount != (sprite ? 2 : 3) ||
            (!sprite && type != GS_PRIM_TRIANGLE && type != GS_PRIM_TRISTRIP && type != GS_PRIM_TRIFAN) ||
            context.frame.psm != GS_PSM_CT32 || context.frame.fbp != frame.fbp || context.frame.fbw != frame.fbw ||
            context.frame.fbmsk || context.fba || (context.test & ~testBits) ||
            state.prim.aa1 || state.scanmsk || state.dthe)
            throw std::invalid_argument("Unsupported GS primitive or framebuffer state: FBP=" + std::to_string(context.frame.fbp) +
                " primitive=" + std::to_string(type) + " vertices=" + std::to_string(draw.batch.vertexCount) +
                " FBMSK=" + std::to_string(context.frame.fbmsk) + " TEST=" + std::to_string(context.test) +
                " FBA=" + std::to_string(context.fba) + " FGE=" + std::to_string(state.prim.fge) +
                " AA1=" + std::to_string(state.prim.aa1) + " SCANMSK=" + std::to_string(state.scanmsk) +
                " DTHE=" + std::to_string(state.dthe) + " ZMSK=" + std::to_string(context.zbuf.zmask));
        const uint64_t alpha = context.alpha;
        if (state.prim.abe && ((alpha & ~0xff000000ffull) || (alpha & 3) == 3 || ((alpha >> 2) & 3) == 3 ||
                               ((alpha >> 4) & 3) == 3 || ((alpha >> 6) & 3) == 3 || state.colclamp > 1))
            throw std::invalid_argument("Unsupported GS blend selectors or COLCLAMP");
        if (usesDepth(context) && ((context.zbuf.psm != GS_PSM_Z16 && context.zbuf.psm != GS_PSM_Z16S) ||
                                   context.frame.fbp == context.zbuf.zbp))
            throw std::invalid_argument("Unsupported GS depth format or overlapping surface base");
        if (state.prim.tme) {
            const auto format = context.tex0.psm;
            if ((format != GS_PSM_CT32 && format != GS_PSM_T8 && format != GS_PSM_T4) || context.tex0.tfx > 1 ||
                ((context.tex1 >> 2) & 7) || !std::has_single_bit(state.textureWidth) ||
                !std::has_single_bit(state.textureHeight) || state.textureWidth > 1024 || state.textureHeight > 1024 ||
                !draw.texture.id || draw.texture.id == target.texture.id || draw.texture.width != state.textureWidth ||
                draw.texture.height != state.textureHeight || draw.texture.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8)
                throw std::invalid_argument("Unsupported GS texture snapshot or feedback");
            for (unsigned axis = 0; axis < 2; ++axis) {
                const auto mode = (context.clamp >> (2 * axis)) & 3;
                const auto low = (context.clamp >> (4 + 20 * axis)) & 1023;
                const auto high = (context.clamp >> (14 + 20 * axis)) & 1023;
                const auto size = axis == 0 ? state.textureWidth : state.textureHeight;
                if (mode == 3 || (mode == 2 && (low > high || high >= size)))
                    throw std::invalid_argument("GS texture addressing outside the decoded snapshot");
            }
        }
        for (const auto& vertex : std::span(draw.batch.vertices).first(draw.batch.vertexCount)) {
            if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || vertex.x < 0 || vertex.y < 0 ||
                vertex.x >= 4096 || vertex.y >= 4096)
                throw std::invalid_argument("Invalid GS triangle coordinates");
            if (usesDepth(context) && (!(vertex.z >= 0 && vertex.z <= 65535) || vertex.z != std::floor(vertex.z)))
                throw std::invalid_argument("GS triangle depth outside supported 16-bit integer range");
            if (state.prim.tme && !state.prim.fst &&
                (!(vertex.q > 1e-8f && vertex.q <= 64) ||
                 !(std::abs(vertex.s) <= 4096.0f * vertex.q) || !(std::abs(vertex.t) <= 4096.0f * vertex.q)))
                throw std::invalid_argument("GS perspective texture coordinates outside supported range: " +
                    std::to_string(vertex.s) + "," + std::to_string(vertex.t) + "," + std::to_string(vertex.q));
        }
    }

    void drawPrimitive(const GsOpenGlTriangle& draw, int width, int height, int scale, Texture2D destination) const {
        const auto& state = draw.batch.state;
        const auto& context = state.context;
        const auto& scissor = context.scissor;
        std::array<Vector2, 3> positions{};
        std::array<Vector4, 3> colors{};
        std::array<Vector3, 3> coordinates{};
        std::array<float, 3> depths{};
        std::array<float, 3> fogFactors{};
        const bool interpolateDepth = usesDepth(context);
        const int sprite = state.prim.type == GS_PRIM_SPRITE;
        for (size_t index = 0; index < positions.size(); ++index) {
            const auto& vertex = draw.batch.vertices[sprite && index == 2 ? 1 : index];
            positions[index] = {vertex.x - (context.xyoffset.ofx >> 4), vertex.y - (context.xyoffset.ofy >> 4)};
            colors[index] = {float(vertex.r), float(vertex.g), float(vertex.b), float(vertex.a)};
            coordinates[index] = state.prim.fst ? Vector3{float(vertex.u), float(vertex.v), 1.0f}
                                              : Vector3{vertex.s, vertex.t, vertex.q};
            if (interpolateDepth) depths[index] = float(vertex.z);
            fogFactors[index] = float(vertex.fog);
        }
        const auto [a, b, c] = positions;
        const int left = std::max(int(scissor.x0), int(std::floor(std::min({a.x, b.x, c.x}))));
        const int right = std::min({int(scissor.x1) + 1, width / scale, int(std::ceil(std::max({a.x, b.x, c.x}))) + 1});
        const int top = std::max(int(scissor.y0), int(std::floor(std::min({a.y, b.y, c.y}))));
        const int bottom = std::min({int(scissor.y1) + 1, height / scale, int(std::ceil(std::max({a.y, b.y, c.y}))) + 1});
        if (left >= right || top >= bottom) return;

        const bool readDestination = readsDestination(state);
        if (readDestination) {
            // Copy the affected rectangle after the preceding draw has flushed.
            // Sampling the live framebuffer attachment is undefined in OpenGL.
            rlActiveTextureSlot(0);
            glBindTexture(GL_TEXTURE_2D, destination.id);
            const int bottomY = height - bottom * scale;
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, left * scale, bottomY, left * scale, bottomY,
                                (right - left) * scale, (bottom - top) * scale);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        const int modes[] = {state.prim.iip, state.prim.tme, state.prim.fst,
                             context.tex0.tfx | ((context.tex0.tcc != 0) << 1) | (int(state.linearFilter) << 2) |
                                 (int(state.prim.fge) << 3)};
        const int fogColor[] = {state.fogR, state.fogG, state.fogB};
        const int blend[] = {state.prim.abe, int(context.alpha & 255), int((context.alpha >> 32) & 255),
                             int(state.colclamp & 1) | (int(state.pabe) << 1)};
        const int wrap[] = {int(context.clamp & 3), int((context.clamp >> 2) & 3),
                            int((context.clamp >> 4) & 0xfffff), int((context.clamp >> 24) & 0xfffff)};
        const int sourceLayout[] = {int(context.tex0.tbp0 / GSMem::BLOCKS_PER_PAGE), context.tex0.tbw,
                                    int(draw.colorSourcePage), draw.colorSource.id ? scale : 0};
        const float output[] = {float(height), float(scale)};
        const bool alphaTestEnabled = context.test & 1;
        const int alphaComparison = alphaTestEnabled ? int((context.test >> 1) & 7) : 1;
        const int alphaReference = int((context.test >> 4) & 255);
        const int destinationAlpha = context.test & (1u << 14) ? int((context.test >> 15) & 1) : -1;
        const auto submit = [&](bool failedAlpha, bool rgb, bool alpha, bool depth) {
            const int test[] = {alphaComparison, alphaReference, failedAlpha, destinationAlpha};
            glColorMask(rgb, rgb, rgb, alpha);
            glDepthMask(depth);
            BeginShaderMode(shader);
            const int seedMode = 0;
            SetShaderValue(shader, seedModeLocation, &seedMode, SHADER_UNIFORM_INT);
            SetShaderValueV(shader, positionsLocation, positions.data(), SHADER_UNIFORM_VEC2, 3);
            SetShaderValueV(shader, colorsLocation, colors.data(), SHADER_UNIFORM_VEC4, 3);
            SetShaderValueV(shader, textureCoordinatesLocation, coordinates.data(), SHADER_UNIFORM_VEC3, 3);
            SetShaderValue(shader, depthsLocation, depths.data(), SHADER_UNIFORM_VEC3);
            SetShaderValue(shader, fogFactorsLocation, fogFactors.data(), SHADER_UNIFORM_VEC3);
            SetShaderValue(shader, fogColorLocation, fogColor, SHADER_UNIFORM_IVEC3);
            SetShaderValue(shader, outputLocation, output, SHADER_UNIFORM_VEC2);
            SetShaderValue(shader, modesLocation, modes, SHADER_UNIFORM_IVEC4);
            SetShaderValue(shader, spriteLocation, &sprite, SHADER_UNIFORM_INT);
            SetShaderValue(shader, wrapLocation, wrap, SHADER_UNIFORM_IVEC4);
            SetShaderValue(shader, colorSourceLayoutLocation, sourceLayout, SHADER_UNIFORM_IVEC4);
            SetShaderValue(shader, blendLocation, blend, SHADER_UNIFORM_IVEC4);
            SetShaderValue(shader, alphaTestLocation, test, SHADER_UNIFORM_IVEC4);
            if (state.prim.tme) SetShaderValueTexture(shader, samplerLocation, draw.texture);
            if (draw.colorSource.id) SetShaderValueTexture(shader, colorSourceLocation, draw.colorSource);
            if (readDestination) SetShaderValueTexture(shader, destinationLocation, destination);
            DrawRectangle(left * scale, top * scale, (right - left) * scale, (bottom - top) * scale, WHITE);
            EndShaderMode();
        };
        const bool writeDepth = gsReplayWritesDepth(context);
        if (alphaComparison != 0) submit(false, true, true, writeDepth);
        if (alphaComparison == 1) return;
        switch ((context.test >> 12) & 3) {
        case 1: submit(true, true, true, false); break; // FB_ONLY
        case 2: if (writeDepth) submit(true, false, false, true); break; // ZB_ONLY
        case 3: submit(true, true, false, false); break; // RGB_ONLY
        }
    }

    static constexpr const char* FragmentShader = R"GLSL(#version 330
out vec4 finalColor;
uniform vec2 positions[3];
uniform vec4 colors[3];
uniform vec3 coordinates[3];
uniform vec3 depths;
uniform vec3 fogFactors;
uniform ivec3 fogColor;
uniform vec2 outputGrid;
uniform ivec4 modes;
uniform int sprite;
uniform ivec4 wrapMode;
uniform sampler2D gsTexture;
uniform ivec4 blendState;
uniform ivec4 alphaTestState;
uniform sampler2D gsDestination;
uniform sampler2D gsColorSource;
uniform ivec4 colorSourceLayout; // TBP page, TBW, producer FBP, producer scale.
uniform int seedMode;
uniform ivec2 uploadOrigin;

int wrapTexel(int coord, int size, int mode, int region) {
    if (mode == 0) return coord & (size - 1);
    if (mode == 2) return clamp(coord, region & 1023, region >> 10);
    return clamp(coord, 0, size - 1);
}

ivec4 readTexel(ivec2 address, ivec2 size) {
    int scale = max(colorSourceLayout.w, 1);
    ivec2 guest = ivec2(floor(vec2(address) / float(scale)));
    ivec2 wrapped = ivec2(wrapTexel(guest.x, size.x, wrapMode.x, wrapMode.z),
                          wrapTexel(guest.y, size.y, wrapMode.y, wrapMode.w));
    if (colorSourceLayout.w != 0) {
        int page = (colorSourceLayout.x + (wrapped.y / 32) * colorSourceLayout.y + wrapped.x / 64) & 511;
        int relative = page - colorSourceLayout.z;
        ivec2 producerSize = textureSize(gsColorSource, 0);
        int stride = producerSize.x / (64 * scale);
        if (relative >= 0 && relative < stride * (producerSize.y / (32 * scale))) {
            ivec2 producer = ivec2((relative % stride) * 64 + wrapped.x % 64,
                                  (relative / stride) * 32 + wrapped.y % 32);
            ivec2 pixel = producer * scale + (address - guest * scale);
            pixel.y = producerSize.y - 1 - pixel.y;
            return ivec4(round(texelFetch(gsColorSource, pixel, 0) * 255.0));
        }
    }
    return ivec4(round(texelFetch(gsTexture, wrapped, 0) * 255.0));
}

ivec4 lerpTexels(ivec4 first, ivec4 last, int weight) {
    return first + (((last - first) * weight) >> 4);
}

ivec4 sampleTexture(vec2 position, ivec2 size) {
    if (colorSourceLayout.w != 0) position *= float(colorSourceLayout.w);
    if ((modes.w & 4) == 0)
        return readTexel(ivec2(colorSourceLayout.w != 0 ? floor(position) : position), size);
    vec2 centered = position - 0.5;
    ivec2 origin = ivec2(floor(centered));
    ivec2 weight = ivec2(fract(centered) * 16.0);
    ivec4 top = lerpTexels(readTexel(origin, size), readTexel(origin + ivec2(1, 0), size), weight.x);
    ivec4 bottom = lerpTexels(readTexel(origin + ivec2(0, 1), size), readTexel(origin + ivec2(1), size), weight.x);
    return lerpTexels(top, bottom, weight.y);
}

ivec3 selectColor(int selector, ivec3 source, ivec3 destination) {
    if (selector == 0) return source;
    if (selector == 1) return destination;
    return ivec3(0);
}

bool passesAlphaTest(int alpha, int reference, int comparison) {
    switch (comparison) {
        case 0: return false;
        case 1: return true;
        case 2: return alpha < reference;
        case 3: return alpha <= reference;
        case 4: return alpha == reference;
        case 5: return alpha >= reference;
        case 6: return alpha > reference;
        case 7: return alpha != reference;
    }
    return false;
}

void main() {
    vec2 point = vec2(gl_FragCoord.x, outputGrid.x - gl_FragCoord.y) / outputGrid.y;
    if (seedMode != 0) {
        ivec2 texel = ivec2(point);
        if (seedMode == 3) texel -= uploadOrigin;
        finalColor = texelFetch(gsTexture, texel, 0);
        gl_FragDepth = 1.0;
        if (seedMode == 2) {
            ivec2 bytes = ivec2(round(texelFetch(gsDestination, texel, 0).rg * 255.0));
            gl_FragDepth = (65535.0 - float(bytes.x | (bytes.y << 8))) / 65535.0;
        }
        return;
    }
    vec3 weights;
    vec2 spriteUV;
    if (sprite != 0) {
        vec2 first = positions[0];
        vec2 last = positions[1];
        if (any(lessThan(point, min(first, last))) || any(greaterThanEqual(point, max(first, last)))) discard;
        spriteUV = (point - first) / (last - first);
        weights = vec3(0, 0, 1);
    } else {
        vec2 a = positions[0] - positions[2];
        vec2 b = positions[1] - positions[2];
        vec2 p = point - positions[2];
        float denominator = b.y*a.x - b.x*a.y;
        if (abs(denominator) < 0.001) discard;
        float w0 = (b.y*p.x - b.x*p.y) / denominator;
        float w1 = (a.x*p.y - a.y*p.x) / denominator;
        weights = vec3(w0, w1, 1.0 - w0 - w1);
        if (any(lessThan(weights, vec3(-0.0001)))) discard;
    }
    float z = depths.z + dot(depths.xy - depths.zz, weights.xy);
    gl_FragDepth = (65535.0 - floor(clamp(z, 0.0, 65535.0))) / 65535.0;
    vec4 shade = modes.x != 0 ? colors[0]*weights.x + colors[1]*weights.y + colors[2]*weights.z : colors[2];
    ivec4 rgba = ivec4(clamp(shade, 0.0, 255.0));
    if (modes.y != 0) {
        ivec2 size = textureSize(gsTexture, 0);
        vec3 uvq = coordinates[0]*weights.x + coordinates[1]*weights.y + coordinates[2]*weights.z;
        vec2 texelPosition = modes.z != 0 ? vec2(ivec2(uvq.xy) & 65535) / 16.0
            : uvq.xy * (1.0 / (abs(uvq.z) > 1e-8 ? uvq.z : 1.0)) * vec2(size);
        if (sprite != 0) {
            vec2 first = coordinates[0].xy;
            vec2 last = coordinates[1].xy;
            texelPosition = modes.z != 0 ? mix(first, last, spriteUV) / 16.0
                : mix(first / coordinates[0].z, last / coordinates[1].z, spriteUV) * vec2(size);
        }
        ivec4 texel = sampleTexture(texelPosition, size);
        bool decal = (modes.w & 1) != 0;
        bool useAlpha = (modes.w & 2) != 0;
        rgba.rgb = decal ? texel.rgb : min(ivec3(255), (texel.rgb * rgba.rgb) >> 7);
        if (useAlpha) rgba.a = decal ? texel.a : min(255, (texel.a * rgba.a) >> 7);
    }
    if ((modes.w & 8) != 0) {
        int fog = int(clamp(dot(weights, fogFactors), 0.0, 255.0));
        rgba.rgb = ((rgba.rgb * fog) >> 8) + ((fogColor * (255 - fog)) >> 8);
    }
    if (passesAlphaTest(rgba.a, alphaTestState.y, alphaTestState.x) == (alphaTestState.z != 0)) discard;
    bool blend = blendState.x != 0 && ((blendState.w & 2) == 0 || (rgba.a & 128) != 0);
    ivec4 dst;
    if (blend || alphaTestState.w >= 0)
        dst = ivec4(round(texelFetch(gsDestination, ivec2(gl_FragCoord.xy), 0) * 255.0));
    if (alphaTestState.w >= 0 && (dst.a >> 7) != alphaTestState.w) discard;
    if (blend) {
        int selectors = blendState.y;
        ivec3 a = selectColor(selectors & 3, rgba.rgb, dst.rgb);
        ivec3 b = selectColor((selectors >> 2) & 3, rgba.rgb, dst.rgb);
        int factor = (selectors >> 4) & 3;
        int c = factor == 0 ? rgba.a : (factor == 1 ? dst.a : blendState.z);
        ivec3 d = selectColor((selectors >> 6) & 3, rgba.rgb, dst.rgb);
        ivec3 result = (((a - b) * c) >> 7) + d;
        rgba.rgb = (blendState.w & 1) != 0 ? clamp(result, 0, 255) : result & 255;
    }
    finalColor = vec4(rgba) / 255.0;
}
)GLSL";

    Shader shader{};
    int positionsLocation{}, colorsLocation{}, textureCoordinatesLocation{}, depthsLocation{}, outputLocation{};
    int modesLocation{}, spriteLocation{}, wrapLocation{}, samplerLocation{}, blendLocation{}, alphaTestLocation{};
    int destinationLocation{}, seedModeLocation{}, uploadOriginLocation{}, colorSourceLocation{}, colorSourceLayoutLocation{};
    int fogFactorsLocation{}, fogColorLocation{};
};

}
