#define _GNU_SOURCE
#include <SDL.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Linux host diagnostics only. Capture the exact game RGB texture uploads;
 * call the original SDL functions and never write guest memory or registers.
 * Each little-endian record is four uint32_t values (index,width,height,bytes)
 * followed by width*height*3 RGB bytes. Captures contain private game images.
 */
static uint32_t upload_count;

void SDL_GetWindowSize(SDL_Window *window, int *width, int *height) {
    static void (*original)(SDL_Window*, int*, int*);
    if (!original) original = dlsym(RTLD_NEXT, "SDL_GetWindowSize");
    original(window, width, height);
    const char *sequence = getenv("SMA3_TAP_DRAWABLES");
    if (!sequence) return;
    while (*sequence) {
        unsigned at = 0, w = 0, h = 0;
        int consumed = 0;
        if (sscanf(sequence, "%u:%ux%u%n", &at, &w, &h, &consumed) != 3 || consumed <= 0)
            break;
        if (upload_count >= at && w > 0 && h > 0) {
            if (width) *width = (int)w;
            if (height) *height = (int)h;
        }
        sequence += consumed;
        if (*sequence != ',') break;
        ++sequence;
    }
}

int SDL_UpdateTexture(SDL_Texture *texture, const SDL_Rect *rect,
                      const void *pixels, int pitch) {
    static int (*original)(SDL_Texture*, const SDL_Rect*, const void*, int);
    static FILE *output;
    static int initialized;
    static uint32_t index;
    if (!original) original = dlsym(RTLD_NEXT, "SDL_UpdateTexture");
    if (!initialized) {
        initialized = 1;
        const char *path = getenv("SMA3_UPLOAD_TAP");
        if (path && *path) output = fopen(path, "wb");
    }
    uint32_t format = 0;
    int access = 0, width = 0, height = 0;
    if (output && !rect && SDL_QueryTexture(texture, &format, &access, &width, &height) == 0 &&
        format == SDL_PIXELFORMAT_RGB24 && access == SDL_TEXTUREACCESS_STREAMING &&
        height == 160 && width >= 240 && width <= 356 && pitch == width * 3) {
        const uint32_t header[4] = {index++, (uint32_t)width, (uint32_t)height,
                                   (uint32_t)(pitch * height)};
        fwrite(header, sizeof(header), 1, output);
        fwrite(pixels, (size_t)pitch * height, 1, output);
        fflush(output);
        ++upload_count;
    }
    return original(texture, rect, pixels, pitch);
}
