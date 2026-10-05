#pragma once
#include <SDL.h>
#include <algorithm>
namespace sma3 {
// GPU presentation only: guest framebuffer and emulated timing stay unchanged.
inline SDL_Rect video_destination(int w, int h, int sw, int sh, bool stretch) {
    if (w<=0 || h<=0 || sw<=0 || sh<=0) return {0,0,0,0};
    if (stretch) return {0,0,w,h};
    int dw=w, dh=static_cast<int>(static_cast<long long>(w)*sh/sw);
    if (dh>h) { dh=h; dw=static_cast<int>(static_cast<long long>(h)*sw/sh); }
    return {(w-dw)/2,(h-dh)/2,dw,dh};
}
struct VideoScaler {
    SDL_Texture* integer_texture=nullptr;
    SDL_Texture* output_texture=nullptr;
    int iw=0,ih=0,ow=0,oh=0;
    void reset() {
        if(integer_texture) SDL_DestroyTexture(integer_texture);
        if(output_texture) SDL_DestroyTexture(output_texture);
        integer_texture=nullptr; output_texture=nullptr; iw=ih=ow=oh=0;
    }
    static bool ensure(SDL_Renderer* renderer,SDL_Texture*& texture,int& oldw,int& oldh,int w,int h) {
        if(texture && oldw==w && oldh==h) return true;
        if(texture) SDL_DestroyTexture(texture);
        texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,w,h);
        oldw=oldh=0;
        if(!texture) return false;
        oldw=w;oldh=h;
        SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_NONE);
        SDL_SetTextureScaleMode(texture,SDL_ScaleModeLinear);
        return true;
    }
    bool draw(SDL_Renderer* renderer, SDL_Texture* source, int sw,int sh,
              const SDL_Rect& destination,int quality) {
        if(sw<=0 || sh<=0 || destination.w<=0 || destination.h<=0) return false;
        const int h=quality==720 ? 720 : quality==1080 ? 1080 : destination.h;
        const int w=quality==720 || quality==1080 ? h*16/9 : destination.w;
        const int factor=std::max(1,std::max((w+sw-1)/sw,(h+sh-1)/sh));
        // Integer nearest prescale keeps pixel interiors sharp; linear downscale
        // smooths only fractional pixel boundaries instead of blurring sprites.
        if(!ensure(renderer,integer_texture,iw,ih,sw*factor,sh*factor)) return false;
        if(quality && !ensure(renderer,output_texture,ow,oh,w,h)) return false;
        if(!quality && output_texture) { SDL_DestroyTexture(output_texture);output_texture=nullptr;ow=oh=0; }
        SDL_Texture* original_target=SDL_GetRenderTarget(renderer);
        SDL_SetTextureScaleMode(source,SDL_ScaleModeNearest);
        bool ok=SDL_SetRenderTarget(renderer,integer_texture)==0;
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
};
}
