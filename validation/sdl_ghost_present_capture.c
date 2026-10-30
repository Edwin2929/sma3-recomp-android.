#define _GNU_SOURCE
#include <SDL.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Private host diagnostic: read the finished SDL drawable, including host
 * overlays, immediately before the real Present. No guest/runtime writes.
 * SMA3_GHOST_PRESENT_TAP is overwritten with the latest selected frame;
 * SMA3_GHOST_PRESENT_FROM/THROUGH optionally bound the zero-based presents.
 * File: uint32 LE {present,width,height,RGBA-byte-count}, then RGBA32 pixels.
 * This readback intentionally perturbs timing and is not a benchmark.
 */
void SDL_RenderPresent(SDL_Renderer* renderer) {
    static void (*original)(SDL_Renderer*);
    static unsigned long present;
    if (!original) original = dlsym(RTLD_NEXT,"SDL_RenderPresent");
    const char* path = getenv("SMA3_GHOST_PRESENT_TAP");
    const char* first = getenv("SMA3_GHOST_PRESENT_FROM");
    const char* last = getenv("SMA3_GHOST_PRESENT_THROUGH");
    const unsigned long begin = first ? strtoul(first,NULL,10) : 0;
    const unsigned long end = last ? strtoul(last,NULL,10) : 1000000;
    int width=0,height=0;
    if (path && *path && present>=begin && present<=end &&
        SDL_GetRendererOutputSize(renderer,&width,&height)==0 && width>0 && height>0 &&
        width<=4096 && height<=4096) {
        const uint32_t bytes=(uint32_t)width*(uint32_t)height*4;
        void* pixels=malloc(bytes);
        if (pixels && SDL_RenderReadPixels(renderer,NULL,SDL_PIXELFORMAT_RGBA32,pixels,width*4)==0) {
            FILE* output=fopen(path,"wb");
            if (output) {
                const uint32_t header[4]={(uint32_t)present,(uint32_t)width,(uint32_t)height,bytes};
                fwrite(header,sizeof(header),1,output);fwrite(pixels,bytes,1,output);fclose(output);
            }
        }
        free(pixels);
    }
    ++present;
    original(renderer);
}
