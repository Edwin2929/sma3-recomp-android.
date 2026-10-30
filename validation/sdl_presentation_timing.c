#define _GNU_SOURCE
#include <SDL.h>
#include <dlfcn.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Linux desktop qualification only. Use LD_PRELOAD with a runner built with
 * SMA3_PRESENTATION_DIAGNOSTICS. This counts real SDL calls and measures host
 * time, not compositor output or Android GPU performance. No per-frame I/O.
 * A requested size creates a real SDL surface; it does not fake size queries.
 */
static uint64_t count, first_wall, last_wall, first_cpu, last_cpu, swap_cpu;
static int surface_width, surface_height;
/* Fixed 0.1 ms bins avoid allocations and per-frame file writes. Quantiles
 * are upper bounds; the last bin includes overflow and uses the observed max.
 * These are gaps between completed SDL calls, not physical display scanout. */
#define GAP_BUCKET_NS UINT64_C(100000)
#define GAP_BUCKET_COUNT 2048
static uint64_t gap_histogram[GAP_BUCKET_COUNT], max_gap;
static uint64_t gaps_over_25ms, gaps_over_50ms;

static double gap_quantile_ms_upper(unsigned percentile) {
    const uint64_t needed = ((count - 1) * percentile + 99) / 100;
    uint64_t cumulative = 0;
    for (unsigned i = 0; i < GAP_BUCKET_COUNT; ++i) {
        cumulative += gap_histogram[i];
        if (cumulative >= needed) {
            const uint64_t upper = i == GAP_BUCKET_COUNT - 1
                ? max_gap : (uint64_t)(i + 1) * GAP_BUCKET_NS;
            return upper / 1e6;
        }
    }
    return max_gap / 1e6;
}

static uint64_t nanoseconds(clockid_t clock) {
    struct timespec value;
    if (clock_gettime(clock, &value) != 0) abort();
    return (uint64_t)value.tv_sec * UINT64_C(1000000000) + value.tv_nsec;
}

SDL_Window *SDL_CreateWindow(const char *title, int x, int y, int w, int h, Uint32 flags) {
    static SDL_Window *(*original)(const char*, int, int, int, int, Uint32);
    if (!original) original = dlsym(RTLD_NEXT, "SDL_CreateWindow");
    const char *size = getenv("SMA3_TIMING_DRAWABLE");
    if (size) {
        int width = 0, height = 0, used = 0;
        if (sscanf(size, "%dx%d%n", &width, &height, &used) != 2 || size[used] ||
            width < 1 || height < 1 || width > 8192 || height > 8192) abort();
        w = width; h = height;
    }
    surface_width = w; surface_height = h;
    return original(title, x, y, w, h, flags);
}

void SDL_RenderPresent(SDL_Renderer *renderer) {
    static void (*original)(SDL_Renderer*);
    if (!original) original = dlsym(RTLD_NEXT, "SDL_RenderPresent");
    const uint64_t before = nanoseconds(CLOCK_THREAD_CPUTIME_ID);
    original(renderer);
    const uint64_t cpu = nanoseconds(CLOCK_THREAD_CPUTIME_ID);
    const uint64_t wall = nanoseconds(CLOCK_MONOTONIC);
    swap_cpu += cpu - before;
    if (count++ == 0) { first_wall = wall; first_cpu = cpu; }
    else {
        const uint64_t gap = wall - last_wall;
        uint64_t bucket = gap / GAP_BUCKET_NS;
        if (bucket >= GAP_BUCKET_COUNT) bucket = GAP_BUCKET_COUNT - 1;
        ++gap_histogram[bucket];
        if (gap > max_gap) max_gap = gap;
        gaps_over_25ms += gap > UINT64_C(25000000);
        gaps_over_50ms += gap > UINT64_C(50000000);
    }
    last_wall = wall; last_cpu = cpu;
}

__attribute__((destructor)) static void report(void) {
    if (count < 2 || last_wall <= first_wall) return;
    fprintf(stderr, "[sdl-presentation-timing] {\"scope\":\"host_software_sdl_not_android_fps\","
        "\"width\":%d,\"height\":%d,\"completed_present_calls\":%" PRIu64 ","
        "\"measurement_intervals\":%" PRIu64 ",\"wall_seconds\":%.9f,"
        "\"presentation_rate\":%.6f,\"thread_cpu_ms\":%.6f,\"swap_cpu_ms\":%.6f,"
        "\"gap_p50_ms_upper\":%.6f,\"gap_p95_ms_upper\":%.6f,"
        "\"gap_p99_ms_upper\":%.6f,\"gap_max_ms\":%.6f,"
        "\"gaps_over_25ms\":%" PRIu64 ",\"gaps_over_50ms\":%" PRIu64 "}\n",
        surface_width, surface_height, count, count - 1,
        (last_wall - first_wall) / 1e9, (count - 1) * 1e9 / (last_wall - first_wall),
        (last_cpu - first_cpu) / 1e6, swap_cpu / 1e6,
        gap_quantile_ms_upper(50), gap_quantile_ms_upper(95),
        gap_quantile_ms_upper(99), max_gap / 1e6, gaps_over_25ms, gaps_over_50ms);
}
