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
    last_wall = wall; last_cpu = cpu;
}

__attribute__((destructor)) static void report(void) {
    if (count < 2 || last_wall <= first_wall) return;
    fprintf(stderr, "[sdl-presentation-timing] {\"scope\":\"host_software_sdl_not_android_fps\","
        "\"width\":%d,\"height\":%d,\"completed_present_calls\":%" PRIu64 ","
        "\"measurement_intervals\":%" PRIu64 ",\"wall_seconds\":%.9f,"
        "\"presentation_rate\":%.6f,\"thread_cpu_ms\":%.6f,\"swap_cpu_ms\":%.6f}\n",
        surface_width, surface_height, count, count - 1,
        (last_wall - first_wall) / 1e9, (count - 1) * 1e9 / (last_wall - first_wall),
        (last_cpu - first_cpu) / 1e6, swap_cpu / 1e6);
}
