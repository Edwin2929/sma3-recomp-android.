#pragma once
#include "runtime.h"
namespace sma3::wide {
// Presentation only. Disabled mode removes its observer and cached providers.
void frame(const gbarecomp::ExtendedViewFrameInfo* info, bool enabled = true);
// Conservative per-scene authorization for the adaptive Android viewport.
bool presentation_ready();
}
