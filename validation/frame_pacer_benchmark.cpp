// Real native FramePacer timer smoke benchmark; not display/GPU throughput.
#include "host_platform.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>

static double thread_ms() {
    timespec t{}; clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}
int main(int argc, char** argv) {
    const bool doubled = argc > 1 && std::atoi(argv[1]);
    constexpr unsigned frames = 90;
    gbarecomp::FramePacer pacer;
    using Clock = std::chrono::steady_clock;
    std::vector<double> intervals;
    unsigned middles = 0;
    double cpu_start = thread_ms();
    auto start = Clock::now(), previous = start;
    for (unsigned i = 0; i < frames; ++i) {
        if (doubled && pacer.wait_for_midpoint()) ++middles;
        pacer.wait_for_next_frame();
        auto now = Clock::now();
        intervals.push_back(std::chrono::duration<double,std::milli>(now - previous).count());
        previous = now;
    }
    double seconds = std::chrono::duration<double>(previous - start).count();
    double cpu_ms = thread_ms() - cpu_start;
    std::sort(intervals.begin(), intervals.end());
    std::printf("{\"target\":%d,\"guest_frames\":%u,\"midpoints\":%u,\"elapsed_s\":%.6f,\"thread_cpu_ms\":%.6f,\"guest_rate\":%.6f,\"interval_p50_ms\":%.6f,\"interval_p95_ms\":%.6f}\n",
        doubled ? 120 : 60, frames, middles, seconds, cpu_ms, frames/seconds,
        intervals[frames/2], intervals[frames*95/100]);
}
