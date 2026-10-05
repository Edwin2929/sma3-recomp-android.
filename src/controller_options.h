#pragma once
#include <atomic>
namespace sma3 {
inline std::atomic<int> video_quality{0};
inline std::atomic<bool> stretch_screen{false};
inline std::atomic<bool> touch_visible{true};
// SDL GameController button numbers, indexed by GBA KEYINPUT bit.
inline std::atomic<int> controller_buttons[10] = {0,1,4,6,14,13,11,12,10,9};
inline std::atomic<bool> menu_open{false};
}
