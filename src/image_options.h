#pragma once
#include <atomic>

namespace sma3 {
// One exclusive presentation mode crosses the Java/native boundary. Keeping
// the choice atomic prevents a transient combination of panorama and stretch.
enum class ImageMode : int { Original = 0, Panorama = 1, Stretch = 2 };

constexpr ImageMode validated_image_mode(int value) {
    return value == static_cast<int>(ImageMode::Panorama) ? ImageMode::Panorama
        : value == static_cast<int>(ImageMode::Stretch) ? ImageMode::Stretch
        : ImageMode::Original;
}

inline std::atomic<ImageMode> image_mode{ImageMode::Original};
}
