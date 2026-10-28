#include "presentation_pacing.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

static unsigned checks;
static void require(bool value, const char* label) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL %s\n", label); std::exit(1); }
}
static bool near(double a, double b) { return std::abs(a - b) < 1e-6; }

int main() {
    sma3::PresentationRateCounter counter;
    constexpr std::uint64_t frequency = 1000000000;
    constexpr std::uint64_t step = 10000000;
    require(counter.record(0, frequency, 0, true, false, 1000000, 200000) < 0,
            "first sample does not invent a completed interval");
    double fps = -1;
    for (unsigned i = 1; i <= 50; ++i)
        fps = counter.record(i * step, frequency, 0, true, i % 2,
                             i % 2 ? 2000000 : 1000000, i % 2 ? 400000 : 200000);
    require(near(fps, 100), "measured count differs honestly from requested120");
    require(near(counter.base_rate(), 50) && near(counter.extra_rate(), 50),
            "base and extra belong to the same measurement interval");
    require(near(counter.base_rate() + counter.extra_rate(), fps),
            "displayed total is the sum of completed presentation kinds");
    require(near(counter.preparation_ms(), 1.5) && near(counter.swap_ms(), .3),
            "preparation and swap means exclude intentional pacing waits");

    // Resume and switch to60 after a long pause. No stale extra/timing samples.
    const auto resumed = 20 * frequency;
    require(counter.record(resumed, frequency, 1, false, false, 90000000, 90000000) < 0,
            "resume starts a fresh interval");
    for (unsigned i = 1; i <= 25; ++i)
        fps = counter.record(resumed + i * 2 * step, frequency, 1, false,
                             false, 500000, 100000);
    require(near(fps, 50) && near(counter.base_rate(), 50) && near(counter.extra_rate(), 0),
            "60 mode contains only actual base images");
    require(near(counter.preparation_ms(), .5) && near(counter.swap_ms(), .1),
            "no pre-resume costs leak into the new measurement");
    std::printf("PASS presentation metrics checks=%u\n", checks);
}
