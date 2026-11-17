#pragma once

#include "raylib.h"
#include "rlgl.h"
#include <algorithm>

namespace drakengard {

inline Rectangle fitGamePicture(float width, float height) {
    constexpr float aspect = 4.0f / 3.0f;
    const float pictureWidth = std::min(width, height * aspect);
    const float pictureHeight = pictureWidth / aspect;
    return {(width - pictureWidth) * 0.5f, (height - pictureHeight) * 0.5f,
            pictureWidth, pictureHeight};
}

inline void sizeGameWindow() {
    if (IsWindowState(FLAG_WINDOW_HIDDEN)) return;
    const int monitor = GetCurrentMonitor();
    const int width = GetMonitorWidth(monitor);
    const int height = GetMonitorHeight(monitor);
    if (width <= 0 || height <= 0) return;
    const auto picture = fitGamePicture(std::min(1280.0f, width * 0.85f), height * 0.85f);
    const auto monitorOrigin = GetMonitorPosition(monitor);
    SetWindowSize(static_cast<int>(picture.width), static_cast<int>(picture.height));
    SetWindowPosition(static_cast<int>(monitorOrigin.x + (width - picture.width) * 0.5f),
                      static_cast<int>(monitorOrigin.y + (height - picture.height) * 0.5f));
}

class HostFramePresenter {
public:
    explicit HostFramePresenter(Texture2D texture) : frame(texture) {
        SetTextureFilter(frame, TEXTURE_FILTER_BILINEAR);
        const Shader candidate = LoadShaderFromMemory(nullptr, UpscaleShader);
        if (candidate.id != 0 && candidate.id != rlGetShaderIdDefault()) {
            shader = candidate;
            activeSizeLocation = GetShaderLocation(shader, "activeSize");
            if (activeSizeLocation < 0) release();
        }
    }

    HostFramePresenter(const HostFramePresenter&) = delete;
    HostFramePresenter& operator=(const HostFramePresenter&) = delete;
    ~HostFramePresenter() { release(); }

    bool cubicAvailable() const { return shader.id != 0; }

    void release() {
        if (!shader.id) return;
        UnloadShader(shader);
        shader = {};
    }

    void draw(unsigned activeWidth, unsigned activeHeight, float width, float height) const {
        if (!frame.id || !activeWidth || !activeHeight || width <= 0 || height <= 0) return;
        const float sourceWidth = static_cast<float>(std::min(activeWidth, static_cast<unsigned>(frame.width)));
        const float sourceHeight = static_cast<float>(std::min(activeHeight, static_cast<unsigned>(frame.height)));
        const Rectangle source{0, 0, sourceWidth, sourceHeight};
        const Rectangle destination = fitGamePicture(width, height);
        const bool useCubic = cubicAvailable() && destination.width >= sourceWidth && destination.height >= sourceHeight;
        if (useCubic) {
            const float activeSize[] = {sourceWidth, sourceHeight};
            BeginShaderMode(shader);
            SetShaderValue(shader, activeSizeLocation, activeSize, SHADER_UNIFORM_VEC2);
        }
        DrawTexturePro(frame, source, destination, {}, 0, WHITE);
        if (useCubic) EndShaderMode();
    }

private:
    // Catmull–Rom with the two center taps combined by bilinear sampling.
    // Clamp to the active GS rectangle so texture padding cannot bleed in.
    static constexpr const char* UpscaleShader = R"GLSL(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 activeSize;

vec4 sampleFrame(vec2 position, vec2 imageSize) {
    return texture(texture0, clamp(position, vec2(0.5), activeSize - 0.5) / imageSize);
}

void main() {
    vec2 imageSize = vec2(textureSize(texture0, 0));
    vec2 position = fragTexCoord * imageSize;
    vec2 center = floor(position - 0.5) + 0.5;
    vec2 f = position - center;
    vec2 f2 = f * f;
    vec2 f3 = f2 * f;
    vec2 low = -0.5 * f + f2 - 0.5 * f3;
    vec2 left = 1.0 - 2.5 * f2 + 1.5 * f3;
    vec2 right = 0.5 * f + 2.0 * f2 - 1.5 * f3;
    vec2 high = -0.5 * f2 + 0.5 * f3;
    vec2 middle = left + right;
    vec2 x = vec2(center.x - 1.0, center.x + right.x / middle.x);
    vec2 y = vec2(center.y - 1.0, center.y + right.y / middle.y);
    vec2 last = center + 2.0;
    vec4 color =
        low.x * low.y       * sampleFrame(vec2(x.x, y.x), imageSize) +
        middle.x * low.y    * sampleFrame(vec2(x.y, y.x), imageSize) +
        high.x * low.y      * sampleFrame(vec2(last.x, y.x), imageSize) +
        low.x * middle.y    * sampleFrame(vec2(x.x, y.y), imageSize) +
        middle.x * middle.y * sampleFrame(vec2(x.y, y.y), imageSize) +
        high.x * middle.y   * sampleFrame(vec2(last.x, y.y), imageSize) +
        low.x * high.y      * sampleFrame(vec2(x.x, last.y), imageSize) +
        middle.x * high.y   * sampleFrame(vec2(x.y, last.y), imageSize) +
        high.x * high.y     * sampleFrame(last, imageSize);
    finalColor = clamp(color, 0.0, 1.0) * fragColor;
}
)GLSL";

    Texture2D frame;
    Shader shader{};
    int activeSizeLocation = -1;
};

}
