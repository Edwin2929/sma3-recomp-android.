#include "host_platform.h"
#include <chrono>
#include <thread>
#include <cstdio>
#include <cassert>
int main() {
    using Clock = std::chrono::steady_clock;
    for (int work_ms : {0, 6, 10, 14}) {
        gbarecomp::FramePacer pacer;
        int middle = 0;
        auto start = Clock::now();
        for (int i = 0; i < 120; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(work_ms));
            if (pacer.wait_for_midpoint()) ++middle;
            pacer.wait_for_next_frame();
        }
        double elapsed = std::chrono::duration<double>(Clock::now()-start).count();
        std::printf("work=%dms guest=%.2f/s available-presentation-slots=%.2f/s midpoints=%d/120\n",work_ms,120/elapsed,(120+middle)/elapsed,middle);
        assert(elapsed > 1.95 && elapsed < 2.5);
        if (work_ms <= 10) assert(middle >= 110);
        if (work_ms == 14) assert(middle == 0);
    }
    puts("PASS: preserves guest speed, recovers moderate lateness, drops overloaded extra slots without catch-up bursts");
}
