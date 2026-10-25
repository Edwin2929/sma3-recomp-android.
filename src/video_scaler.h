#pragma once
#include <SDL.h>
#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <new>
#include "image_filters.h"
namespace sma3 {
// Presentation only: guest framebuffer and emulated timing stay unchanged.
inline SDL_Rect video_destination(int w, int h, int sw, int sh, bool stretch) {
    if (w<=0 || h<=0 || sw<=0 || sh<=0) return {0,0,0,0};
    if (stretch) return {0,0,w,h};
    // Compare the exact aspect ratios before rounding to raster pixels. A
    // rounded height could pick the wrong axis (241x160 for a 240x160 source).
    // Wide products also keep extreme input sizes out of an overflowing int.
    int dw=w, dh=h;
    if (static_cast<long long>(w)*sh > static_cast<long long>(h)*sw)
        dw=static_cast<int>(static_cast<long long>(h)*sw/sh);
    else
        dh=static_cast<int>(static_cast<long long>(w)*sh/sw);
    return {(w-dw)/2,(h-dh)/2,dw,dh};
}
struct VideoScaler {
    SDL_Texture* integer_texture=nullptr;
    SDL_Texture* output_texture=nullptr;
    SDL_Texture* filtered_texture=nullptr;
    int iw=0,ih=0,ow=0,oh=0;
    int fw=0,fh=0;
    std::unique_ptr<std::uint8_t[]> filtered_pixels;
    std::size_t filtered_capacity=0;
    void reset() {
        if(integer_texture) SDL_DestroyTexture(integer_texture);
        if(output_texture) SDL_DestroyTexture(output_texture);
        if(filtered_texture) SDL_DestroyTexture(filtered_texture);
        integer_texture=nullptr; output_texture=nullptr; filtered_texture=nullptr;
        iw=ih=ow=oh=fw=fh=0;
        filtered_pixels.reset(); filtered_capacity=0;
    }
    static bool valid_texture_size(SDL_Renderer* renderer, int w, int h) {
        if (!renderer || w<=0 || h<=0 || std::uint64_t(w)*h>16u*1024u*1024u)
            return false;
        SDL_RendererInfo info{};
        return SDL_GetRendererInfo(renderer,&info)==0 &&
            (info.max_texture_width<=0 || w<=info.max_texture_width) &&
            (info.max_texture_height<=0 || h<=info.max_texture_height);
    }
    static bool ensure(SDL_Renderer* renderer,SDL_Texture*& texture,int& oldw,int& oldh,int w,int h) {
        if(texture && oldw==w && oldh==h) return true;
        if(!valid_texture_size(renderer,w,h)) return false;
        auto* replacement=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,w,h);
        if(!replacement) return false;
        if(SDL_SetTextureBlendMode(replacement,SDL_BLENDMODE_NONE)!=0 ||
           SDL_SetTextureScaleMode(replacement,SDL_ScaleModeLinear)!=0) {
            SDL_DestroyTexture(replacement); return false;
        }
        if(texture) SDL_DestroyTexture(texture);
        texture=replacement;
        oldw=w;oldh=h;
        return true;
    }

    bool prepare_pixels(SDL_Renderer* renderer, const std::uint8_t* rgb,
                        int sw, int sh, int factor) {
        int w=0,h=0;
        std::size_t bytes=0;
        if(!rgb || !filtered_image_size(sw,sh,factor,w,h,bytes) ||
           !valid_texture_size(renderer,w,h)) return false;
        if(bytes>filtered_capacity) {
            std::unique_ptr<std::uint8_t[]> replacement(new(std::nothrow) std::uint8_t[bytes]);
            if(!replacement) return false;
            filtered_pixels=std::move(replacement); filtered_capacity=bytes;
        }
        if(!filtered_texture || fw!=w || fh!=h) {
            auto* replacement=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGB24,
                                                 SDL_TEXTUREACCESS_STREAMING,w,h);
            if(!replacement) return false;
            if(SDL_SetTextureBlendMode(replacement,SDL_BLENDMODE_NONE)!=0) {
                SDL_DestroyTexture(replacement); return false;
            }
            if(filtered_texture) SDL_DestroyTexture(filtered_texture);
            filtered_texture=replacement; fw=w; fh=h;
        }
        // Always regenerate from this present's input, including a blended
        // midpoint. Cached textures never supply pixels from an earlier frame.
        return scale_pixels_rgb24(rgb,std::size_t(sw)*sh*3u,sw,sh,factor,
                                  filtered_pixels.get(),filtered_capacity) &&
            SDL_UpdateTexture(filtered_texture,nullptr,filtered_pixels.get(),w*3)==0;
    }

    bool draw_balanced(SDL_Renderer* renderer, SDL_Texture* source, int sw,int sh,
                       const SDL_Rect& destination,int quality) {
        const int h=quality==720 ? 720 : quality==1080 ? 1080 : destination.h;
        const int w=quality==720 || quality==1080 ? h*16/9 : destination.w;
        const std::int64_t factor=std::max<std::int64_t>(1,
            std::max((std::int64_t(w)+sw-1)/sw,(std::int64_t(h)+sh-1)/sh));
        if(factor*sw>std::numeric_limits<int>::max() ||
           factor*sh>std::numeric_limits<int>::max()) return false;
        // Integer nearest prescale keeps pixel interiors sharp; linear downscale
        // smooths only fractional pixel boundaries instead of blurring sprites.
        if(!ensure(renderer,integer_texture,iw,ih,int(sw*factor),int(sh*factor))) return false;
        if(quality && !ensure(renderer,output_texture,ow,oh,w,h)) return false;
        SDL_Texture* original_target=SDL_GetRenderTarget(renderer);
        bool ok=SDL_SetTextureScaleMode(source,SDL_ScaleModeNearest)==0 &&
            SDL_SetTextureScaleMode(integer_texture,SDL_ScaleModeLinear)==0 &&
            (!quality || SDL_SetTextureScaleMode(output_texture,SDL_ScaleModeLinear)==0) &&
            SDL_SetRenderTarget(renderer,integer_texture)==0;
        if(ok) ok=SDL_RenderCopy(renderer,source,nullptr,nullptr)==0;
        SDL_Texture* final_texture=integer_texture;
        if(ok && quality) {
            ok=SDL_SetRenderTarget(renderer,output_texture)==0;
            if(ok) ok=SDL_RenderCopy(renderer,integer_texture,nullptr,nullptr)==0;
            final_texture=output_texture;
        }
        const bool restored=SDL_SetRenderTarget(renderer,original_target)==0;
        return ok && restored && SDL_RenderCopy(renderer,final_texture,nullptr,&destination)==0;
    }

    bool draw_sampled(SDL_Renderer* renderer, SDL_Texture* source,
                      const SDL_Rect& destination,int quality,SDL_ScaleMode mode) {
        if(SDL_SetTextureScaleMode(source,mode)!=0) return false;
        if(!quality) return SDL_RenderCopy(renderer,source,nullptr,&destination)==0;
        if(!ensure(renderer,output_texture,ow,oh,quality*16/9,quality) ||
           SDL_SetTextureScaleMode(output_texture,mode)!=0) return false;
        auto* original_target=SDL_GetRenderTarget(renderer);
        bool ok=SDL_SetRenderTarget(renderer,output_texture)==0;
        if(ok) ok=SDL_RenderCopy(renderer,source,nullptr,nullptr)==0;
        const bool restored=SDL_SetRenderTarget(renderer,original_target)==0;
        return ok && restored && SDL_RenderCopy(renderer,output_texture,nullptr,&destination)==0;
    }

    // Stable scanlines follow source rows; no clock, alternating phase, or
    // previous-frame history is used. Suppress them below 3x vertical scale,
    // where the thin pattern cannot be represented without strong aliasing.
    static bool draw_scanlines(SDL_Renderer* renderer,const SDL_Rect& destination,int source_height) {
        if(!renderer || source_height<=0 || destination.w<=0 || destination.h<=0 ||
           std::int64_t(destination.y)+destination.h>std::numeric_limits<int>::max()) return false;
        if(std::int64_t(destination.h)<std::int64_t(source_height)*3) return true;
        Uint8 r=0,g=0,b=0,a=0;
        SDL_BlendMode blend=SDL_BLENDMODE_NONE;
        if(SDL_GetRenderDrawColor(renderer,&r,&g,&b,&a)!=0 ||
           SDL_GetRenderDrawBlendMode(renderer,&blend)!=0) return false;
        bool ok=SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND)==0 &&
            SDL_SetRenderDrawColor(renderer,0,0,0,40)==0;
        std::array<SDL_Rect,128> stripes{};
        int count=0;
        for(int row=0;ok && row<source_height;++row) {
            const int row_end=int((std::int64_t(row+1)*destination.h)/source_height);
            const int stripe_start=int(((std::int64_t(row)*4+3)*destination.h)/(std::int64_t(source_height)*4));
            if(row_end>stripe_start)
                stripes[count++]={destination.x,destination.y+stripe_start,destination.w,row_end-stripe_start};
            if(count==int(stripes.size()) || row==source_height-1) {
                if(count) ok=SDL_RenderFillRects(renderer,stripes.data(),count)==0;
                count=0;
            }
        }
        const bool restored_color=SDL_SetRenderDrawColor(renderer,r,g,b,a)==0;
        const bool restored_blend=SDL_SetRenderDrawBlendMode(renderer,blend)==0;
        return ok && restored_color && restored_blend;
    }

    // Two stationary opaque bars mask the top/bottom of the already scaled
    // game image. They cover 10% each, rounded down equally, without moving,
    // stretching, or resampling the visible center. Host controls draw later.
    // Below ten output pixels tall there is no resolvable bar to draw.
    static bool draw_cinematic_bars(SDL_Renderer* renderer,const SDL_Rect& destination) {
        if(!renderer || destination.w<=0 || destination.h<=0 ||
           std::int64_t(destination.x)+destination.w>std::numeric_limits<int>::max() ||
           std::int64_t(destination.y)+destination.h>std::numeric_limits<int>::max()) return false;
        const int bar_height=destination.h/10;
        if(bar_height==0) return true;
        Uint8 r=0,g=0,b=0,a=0;
        SDL_BlendMode blend=SDL_BLENDMODE_NONE;
        if(SDL_GetRenderDrawColor(renderer,&r,&g,&b,&a)!=0 ||
           SDL_GetRenderDrawBlendMode(renderer,&blend)!=0) return false;
        const std::array<SDL_Rect,2> bars={{{destination.x,destination.y,
                                            destination.w,bar_height},
                                           {destination.x,destination.y+destination.h-bar_height,
                                            destination.w,bar_height}}};
        const bool ok=SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE)==0 &&
            SDL_SetRenderDrawColor(renderer,0,0,0,255)==0 &&
            SDL_RenderFillRects(renderer,bars.data(),int(bars.size()))==0;
        const bool restored_color=SDL_SetRenderDrawColor(renderer,r,g,b,a)==0;
        const bool restored_blend=SDL_SetRenderDrawBlendMode(renderer,blend)==0;
        return ok && restored_color && restored_blend;
    }

    bool draw(SDL_Renderer* renderer, SDL_Texture* source, int sw,int sh,
              const SDL_Rect& destination,int quality,
              ImageFilter filter=ImageFilter::Balanced,const std::uint8_t* rgb888=nullptr) {
        if(!renderer || !source || sw<=0 || sh<=0 || sw>4096 || sh>4096 ||
           destination.w<=0 || destination.h<=0 ||
           std::int64_t(destination.x)+destination.w>std::numeric_limits<int>::max() ||
           std::int64_t(destination.y)+destination.h>std::numeric_limits<int>::max()) return false;
        quality=quality==720 || quality==1080 ? quality : 0;
        filter=validated_image_filter(static_cast<int>(filter));
        if(!quality && output_texture) {
            SDL_DestroyTexture(output_texture); output_texture=nullptr; ow=oh=0;
        }
        if(filter==ImageFilter::Nearest || filter==ImageFilter::Bilinear)
            return draw_sampled(renderer,source,destination,quality,
                filter==ImageFilter::Nearest ? SDL_ScaleModeNearest : SDL_ScaleModeLinear);
        const int native_height=sh;
        if(filter==ImageFilter::Scale2x || filter==ImageFilter::Scale3x) {
            const int factor=filter==ImageFilter::Scale2x ? 2 : 3;
            if(!prepare_pixels(renderer,rgb888,sw,sh,factor)) return false;
            source=filtered_texture; sw=fw; sh=fh;
        }
        return draw_balanced(renderer,source,sw,sh,destination,quality) &&
            (filter!=ImageFilter::Crt || draw_scanlines(renderer,destination,native_height)) &&
            (filter!=ImageFilter::Cinematic || draw_cinematic_bars(renderer,destination));
    }
};
}
