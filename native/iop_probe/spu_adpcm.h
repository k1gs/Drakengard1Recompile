#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

// Original SPU ADPCM block decoder.  The decoder owns only the two-sample
// predictor history; block traversal, FIFO buffering and endpoint handling
// remain voice responsibilities.
class SpuAdpcmDecoder {
public:
    using Block = std::span<const uint16_t, 8>;
    using Samples = std::array<int16_t, 28>;

    struct History {
        int32_t prev1{};
        int32_t prev2{};
    };

private:
    History history_{};

    static constexpr std::array<std::array<int32_t, 2>, 5> predictors{{
        {{0, 0}}, {{60, 0}}, {{115, -52}}, {{98, -55}}, {{122, -60}}
    }};

    static int32_t nibble(int value) {
        value &= 0xf;
        return value >= 8 ? value - 16 : value;
    }

    static int32_t residual(int32_t value, unsigned shift) {
        // The hardware path sign-extends the nibble into bits 31..28 and
        // arithmetic-shifts by (header low nibble + 16).
        return (value * 4096) >> shift;
    }

    static bool validHeader(uint16_t header) {
        return ((header >> 4) & 0xf) < predictors.size() &&
            ((header >> 8) & ~unsigned(7)) == 0;
    }

    static std::array<int16_t, 4> decodeWordWithHistory(uint16_t header,
                                                         uint16_t packed,
                                                         History& state) {
        const unsigned shift = header & 0xf;
        const auto factor = predictors[(header >> 4) & 0xf];
        std::array<int16_t, 4> output{};
        size_t out = 0;
        for (const uint8_t byte : {uint8_t(packed), uint8_t(packed >> 8)}) {
            const int32_t first = residual(nibble(byte), shift) +
                ((factor[0] * state.prev1 + factor[1] * state.prev2 + 32) >> 6);
            const int32_t pcm1 = std::clamp(first, -0x8000, 0x7fff);
            const int32_t second = residual(nibble(byte >> 4), shift) +
                ((factor[0] * pcm1 + factor[1] * state.prev1 + 32) >> 6);
            const int32_t pcm2 = std::clamp(second, -0x8000, 0x7fff);
            output[out++] = static_cast<int16_t>(pcm1);
            output[out++] = static_cast<int16_t>(pcm2);
            state.prev2 = pcm1;
            state.prev1 = pcm2;
        }
        return output;
    }

public:
    void reset() { history_ = {}; }
    History history() const { return history_; }

    // Decode one payload halfword. The caller supplies the block header again
    // for each fetch; history is committed only for a valid header.
    std::optional<std::array<int16_t, 4>> decodeWord(uint16_t header,
                                                      uint16_t payload) {
        if (!validHeader(header))
            return std::nullopt;
        History next = history_;
        const auto output = decodeWordWithHistory(header, payload, next);
        history_ = next;
        return output;
    }

    // Invalid predictor/flag combinations fail closed.  Predictor history is
    // committed only after the complete block has passed validation/decoding.
    std::optional<Samples> decode(Block block) {
        const uint16_t header = block[0];
        if (!validHeader(header))
            return std::nullopt;
        History next = history_;
        Samples output{};
        size_t out = 0;

        for (size_t word = 1; word < block.size(); ++word) {
            const auto samples = decodeWordWithHistory(header, block[word], next);
            for (const auto sample : samples)
                output[out++] = sample;
        }

        history_ = next;
        return output;
    }
};
