#pragma once
#include "touch_layout.h"
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <vector>
namespace sma3 {
inline std::atomic<bool> interpolation_enabled{false};
// RGB temporal blending, not optical flow or additional guest simulation.
class FrameBlend {
public:
    void reset() { previous_.clear(); }
    // Keep consecutive guest history without calculating an image that will
    // not be presented. A skipped midpoint must not make the next blend use
    // an older guest frame or a stale interpolation buffer.
    void capture(const uint8_t* current, std::size_t size, uint64_t frame) {
        if (size == 0) previous_.clear();
        else previous_.assign(current, current + size);
        frame_ = frame;
    }
    bool prepare(const uint8_t* current, std::size_t size, uint64_t frame) {
        bool blend = previous_.size() == size && frame == frame_ + 1 && size != 0;
        if (blend) {
            uint64_t difference = 0;
            for (std::size_t i = 0; i < size; ++i)
                difference += std::abs(int(current[i]) - int(previous_[i]));
            blend = difference <= 48 * uint64_t(size); // reject large scene cuts
        }
        if (blend) {
            middle_.resize(size);
            for (std::size_t i = 0; i < size; ++i)
                middle_[i] = uint8_t((unsigned(previous_[i]) + current[i] + 1) / 2);
        }
        capture(current, size, frame);
        return blend;
    }
    const uint8_t* middle() const { return middle_.data(); }
private:
    std::vector<uint8_t> previous_, middle_;
    uint64_t frame_ = 0;
};
}
