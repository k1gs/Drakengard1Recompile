#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <numbers>

// The SPU interpolation coefficients are generated analytically.  Keeping the
// generator here avoids embedding a copied upstream coefficient table while
// retaining the documented 16-bit normalization and phase ordering.
inline std::array<std::array<int16_t, 4>, 256> spuGaussianTable() {
    // Windowed sinc kernel and normalization are numeric SPU reference facts.
    // Evaluate the four tap distances directly for each fractional phase.
    const auto kernel=[](double distance) {
        const double angle=std::numbers::pi*distance/1023.0;
        const double window=0.42+0.50*std::cos(2*angle)+0.08*std::cos(4*angle);
        return std::sin(std::numbers::pi*distance/500.0)*window/distance;
    };
    double integral=0;
    for (int sample=511;sample>=0;--sample) integral+=kernel(sample+0.5);
    const double normalization=32640.0*128/integral;
    std::array<std::array<int16_t, 4>, 256> result{};
    for (unsigned phase = 0; phase < 256; ++phase) {
        const std::array<double,4> distances{256.5+phase,0.5+phase,255.5-phase,511.5-phase};
        std::array<double,4> taps{};
        for (unsigned tap=0;tap<4;++tap) taps[tap]=normalization*kernel(distances[tap]);
        const double correction=(std::accumulate(taps.begin(),taps.end(),0.0)-32640)/4;
        for (unsigned tap=0;tap<4;++tap)
            result[phase][tap]=int16_t(std::lround(taps[tap]-correction));
    }
    return result;
}
