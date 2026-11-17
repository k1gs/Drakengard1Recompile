#pragma once

#include "gs_opengl_triangles.h"
#include "gs_scanout.h"
#include "host_frame_presenter.h"
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string_view>

namespace drakengard {

// Runs on the window thread. The CPU GS remains authoritative for guest VRAM
// and provides the same scanout whenever a frame cannot be replayed.
class GsLivePresenter {
public:
    GsLivePresenter() {
        const auto* requested = std::getenv("DRAKENGARD_INTERNAL_SCALE");
        if (!requested || !*requested || std::string_view(requested) == "1") return;
        const std::string_view value(requested);
        if (value.size() != 1 || value.front() < '2' || value.front() > '4') {
            std::cerr << "[gs-live] expected DRAKENGARD_INTERNAL_SCALE=1..4\n";
            return;
        }
        if (std::getenv("DRAKENGARD_GS_REPLAY_TICK")) {
            std::cerr << "[gs-live] replay export already owns GS capture\n";
            return;
        }
        scale = value.front() - '0';
    }

    GsLivePresenter(const GsLivePresenter&) = delete;
    GsLivePresenter& operator=(const GsLivePresenter&) = delete;
    ~GsLivePresenter() { release(); }

    void beforeLatch() {
        activeRows = 0;
        if (!scale || recording) return;
        recording = gsFrameReplayCapture.arm(64u * 1024 * 1024, true);
        if (recording) std::cerr << "[gs-live] armed internal scale " << scale << "x\n";
    }

    void afterLatch() {
        if (!recording) return;
        auto frame = gsFrameReplayCapture.take();
        if (!frame) return;
        if (frame->status != GsReplayFrame::Status::Complete) {
            if (!rejected++)
                std::cerr << "[gs-live] capture dropped, status=" << int(frame->status) << '\n';
            return;
        }
        const auto start = std::chrono::steady_clock::now();
        try {
            const auto& scanout = frame->scanout;
            if (scanout.kind != GsReplayScanout::Kind::Direct ||
                !scanout.width || scanout.width > 640 || !scanout.readHeight || scanout.readHeight > 512)
                throw std::invalid_argument("Unsupported GS scanout");
            const auto width = int(scanout.width) * scale;
            const auto height = int(scanout.readHeight) * scale;
            const auto rows = int(scanout.displayHeight) * scale / int(hostScanoutRowStep(scanout.smode2));
            if (rows <= 0 || rows > height)
                throw std::invalid_argument("Invalid active GS scanout rows");
            if (!renderer) renderer = std::make_unique<GsOpenGlTriangles>();
            if (target.texture.width != width || target.texture.height != height) {
                if (target.id) UnloadRenderTexture(target);
                target = LoadRenderTexture(width, height);
                if (!target.id) {
                    target = {};
                    throw std::runtime_error("Could not allocate GS live output");
                }
                SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
            }
            renderer->renderFrame(target, scale, *frame, false);
            activeRows = rows;
            tick = frame->endTick;
            drawCount = frame->draws.size();
            submitMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        } catch (const std::exception& error) {
            if (error.what() != lastError) {
                lastError = error.what();
                std::cerr << "[gs-live] CPU fallback at tick " << frame->endTick << ": " << lastError << '\n';
            }
            ++rejected;
        }
    }

    bool draw(float width, float height) {
        if (!activeRows || width <= 0 || height <= 0) return false;
        const auto destination = fitGamePicture(width, height);
        const Rectangle source{0, float(target.texture.height - activeRows), float(target.texture.width), -float(activeRows)};
        // The GS alpha byte is internal state. The selected scanout is opaque.
        const bool blend = glIsEnabled(GL_BLEND);
        rlDrawRenderBatchActive();
        rlDisableColorBlend();
        DrawTexturePro(target.texture, source, destination, {}, 0, WHITE);
        rlDrawRenderBatchActive();
        if (blend) rlEnableColorBlend();
        if (tick != lastPresentedTick) {
            lastPresentedTick = tick;
            if (++presented == 1 || presented % 120 == 0)
                std::cerr << "[gs-live] presented tick=" << tick << " raster=" << target.texture.width
                          << 'x' << target.texture.height << " activeRows=" << activeRows
                          << " commands=" << drawCount << " submitMs=" << submitMs
                          << " frames=" << presented << " rejected=" << rejected << '\n';
        }
        return true;
    }

    void release() {
        if (recording) gsFrameReplayCapture.stop();
        recording = false;
        activeRows = 0;
        renderer.reset();
        if (target.id) UnloadRenderTexture(target);
        target = {};
    }

private:
    std::unique_ptr<GsOpenGlTriangles> renderer;
    RenderTexture2D target{};
    std::string lastError;
    int scale{}, activeRows{};
    bool recording{};
    uint64_t tick{}, lastPresentedTick{}, presented{}, rejected{};
    std::size_t drawCount{};
    double submitMs{};
};

}
