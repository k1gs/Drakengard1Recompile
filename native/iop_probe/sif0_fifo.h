#pragma once
#include <array>
#include <span>
#include <cstdint>
#include <algorithm>
#include <stdexcept>

// Hardware-sized SIF FIFO, expressed in complete quadwords. Only accepted data
// advances the producer; a failed consumer leaves the front quantum intact.
class Sif0Fifo {
    std::array<std::array<uint8_t, 16>, 32> data{};
    unsigned head{}, count{};
public:
    unsigned size() const { return count; }
    bool push(std::span<const uint8_t> quantum) {
        if (quantum.size() != 16) throw std::runtime_error("SIF0 FIFO requires a complete quantum");
        if (count == data.size()) return false;
        std::copy(quantum.begin(), quantum.end(), data[(head + count) % data.size()].begin());
        ++count; return true;
    }
    template<class Consumer> unsigned drain(Consumer consumer) {
        unsigned consumed = 0;
        while (count && consumer(std::span<const uint8_t>(data[head]))) {
            head = (head + 1) % data.size(); --count; ++consumed;
        }
        return consumed;
    }
};
