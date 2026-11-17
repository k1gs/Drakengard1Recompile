#pragma once

#include "gs_opengl_triangles.h"
#include "gs_ppm.h"
#include "gs_replay_archive.h"
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

namespace drakengard {

// One replay candidate, exported beside the normal CPU frame in the run directory.
class GsFrameReplayExport {
public:
    GsFrameReplayExport() {
        const auto* requestedTick = std::getenv("DRAKENGARD_GS_REPLAY_TICK");
        const auto* frameOutput = std::getenv("DRAKENGARD_FRAME_OUTPUT");
        if (!requestedTick || !frameOutput || !*frameOutput) return;
        const std::string_view tick(requestedTick);
        const auto [end, error] = std::from_chars(tick.data(), tick.data() + tick.size(), minTick);
        if (error != std::errc{} || end != tick.data() + tick.size()) {
            std::cerr << "[gs-replay] invalid DRAKENGARD_GS_REPLAY_TICK\n";
            return;
        }
        if (const char* requestedScale = std::getenv("DRAKENGARD_GS_REPLAY_SCALE")) {
            const std::string_view value(requestedScale);
            if (value.size() != 1 || value.front() < '2' || value.front() > '4') {
                std::cerr << "[gs-replay] invalid DRAKENGARD_GS_REPLAY_SCALE\n";
                return;
            }
            rasterScale = value.front() - '0';
        }
        output = std::filesystem::path(frameOutput).parent_path() / ("gs-replay-" + std::to_string(rasterScale) + "x.png");
        requireVisible = std::getenv("DRAKENGARD_GS_REPLAY_VISIBLE") != nullptr;
        requireTrigger = std::getenv("DRAKENGARD_GS_REPLAY_WAIT_TRIGGER") != nullptr;
        compareInternal = std::getenv("DRAKENGARD_GS_REPLAY_COMPARE_INTERNAL") != nullptr;
        archive = std::getenv("DRAKENGARD_GS_REPLAY_ARCHIVE") != nullptr;
    }

    void arm(uint64_t currentTick) {
        if (requireVisible) return;
        request(currentTick);
    }

    void observe(uint64_t currentTick, std::span<const uint8_t> pixels) {
        if (!requireVisible || output.empty() || requested || currentTick < minTick) return;
        unsigned visible = 0;
        for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) {
            if (std::max({pixels[i], pixels[i + 1], pixels[i + 2]}) >= 32 && ++visible >= 4096) {
                request(currentTick);
                return;
            }
        }
    }

private:
    void request(uint64_t currentTick) {
        if (output.empty() || requested || currentTick < minTick) return;
        if (requireTrigger) {
            std::error_code error;
            if (!std::filesystem::is_regular_file(output.parent_path() / "gs-replay.trigger", error)) return;
        }
        const std::size_t budget = requireTrigger ? 256u * 1024 * 1024 : 64u * 1024 * 1024;
        requested = gsFrameReplayCapture.arm(budget);
        if (requested) std::cerr << "[gs-replay] armed at tick " << currentTick << '\n';
    }

public:
    void finish() {
        if (!requested || finished) return;
        auto frame = gsFrameReplayCapture.take();
        if (!frame) return;
        finished = true;
        const auto& scanout = frame->scanout;
        std::cerr << "[gs-replay] ticks=" << frame->startTick << ',' << frame->endTick
                  << " status=" << static_cast<int>(frame->status) << " scanout=" << static_cast<int>(scanout.kind)
                  << " source=" << scanout.source.fbp << " draws=" << frame->draws.size() << '\n';
        try {
            if (!frame->cpuPixels.empty()) {
                const auto cpuOutput = output.parent_path() / "gs-replay-cpu.ppm";
                if (std::filesystem::exists(cpuOutput)) throw std::runtime_error("GS replay CPU output already exists");
                if (frame->cpuWidth > frame->cpuStride ||
                    uint64_t(frame->cpuStride) * frame->cpuHeight * 4 > frame->cpuPixels.size())
                    throw std::runtime_error("Invalid GS replay CPU frame layout");
                std::vector<uint8_t> packed;
                packed.reserve(std::size_t(frame->cpuWidth) * frame->cpuHeight * 4);
                for (unsigned y = 0; y < frame->cpuHeight; ++y) {
                    const auto row = std::span(frame->cpuPixels).subspan(std::size_t(y) * frame->cpuStride * 4,
                                                                       std::size_t(frame->cpuWidth) * 4);
                    packed.insert(packed.end(), row.begin(), row.end());
                }
                std::ofstream file(cpuOutput, std::ios::binary);
                writeGsPpm(file, frame->cpuWidth, frame->cpuHeight, packed);
                file.close();
                if (!file) throw std::runtime_error("Could not export paired GS CPU frame");
                std::cerr << "[gs-replay] paired CPU tick=" << frame->endTick << " size=" << frame->cpuWidth
                          << 'x' << frame->cpuHeight << '\n';
            }
            if (frame->status != GsReplayFrame::Status::Complete) return;
            if (archive) {
                const auto path = output.parent_path() / "gs-replay.bin";
                if (std::filesystem::exists(path)) throw std::runtime_error("GS replay archive already exists");
                auto pending = path;
                pending += ".partial";
                if (std::filesystem::exists(pending)) throw std::runtime_error("Partial GS replay archive already exists");
                std::ofstream file(pending, std::ios::binary);
                writeGsReplayArchive(file, *frame);
                file.close();
                if (!file) throw std::runtime_error("Could not close GS replay archive");
                std::filesystem::rename(pending, path);
                std::cerr << "[gs-replay] retained commands for offline replay\n";
            }
            if (std::filesystem::exists(output)) throw std::runtime_error("GS replay output already exists");
            if (!scanout.width || !scanout.readHeight) throw std::runtime_error("GS replay has no scanout surface");
            struct Target {
                RenderTexture2D value;
                ~Target() { if (value.id) UnloadRenderTexture(value); }
            };
            const GsOpenGlTriangles renderer;
            const auto save = [&](int scale, const std::filesystem::path& path) {
                if (std::filesystem::exists(path)) throw std::runtime_error("GS replay output already exists");
                const Target target{LoadRenderTexture(int(scanout.width * scale), int(scanout.readHeight * scale))};
                renderer.renderFrame(target.value, scale, *frame);
                struct Readback {
                    Image value;
                    ~Readback() { if (value.data) UnloadImage(value); }
                } image{LoadImageFromTexture(target.value.texture)};
                if (!image.value.data) throw std::runtime_error("Could not read GS replay pixels");
                ImageFlipVertical(&image.value);
                if (!ExportImage(image.value, path.string().c_str())) throw std::runtime_error("Could not export GS replay PNG");
                std::cerr << "[gs-replay] wrote " << image.value.width << 'x' << image.value.height
                          << " internal GS raster to " << path.string() << '\n';
            };
            save(rasterScale, output);
            if (compareInternal) save(1, output.parent_path() / "gs-replay-1x.png");
            std::cerr << "[gs-replay] export complete\n";
        } catch (const std::exception& error) {
            std::cerr << "[gs-replay] skipped: " << error.what() << '\n';
        }
    }

private:
    std::filesystem::path output;
    uint64_t minTick{};
    int rasterScale = 2;
    bool requested{}, finished{}, requireVisible{}, requireTrigger{}, compareInternal{}, archive{};
};

}
