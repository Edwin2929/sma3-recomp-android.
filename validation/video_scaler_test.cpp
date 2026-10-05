#include "video_scaler.h"
#include <cstdio>
#include <cstdlib>
static void check(bool ok,const char* msg) { if(!ok) {std::fprintf(stderr,"FAIL %s: %s\n",msg,SDL_GetError());std::exit(1);} }
int main() {
 check(SDL_Init(0)==0,"SDL init");
 auto* screen=SDL_CreateRGBSurfaceWithFormat(0,2400,1080,32,SDL_PIXELFORMAT_RGBA32);
 auto* renderer=SDL_CreateSoftwareRenderer(screen);check(renderer,"renderer");
 auto* src=SDL_CreateRGBSurfaceWithFormat(0,240,160,32,SDL_PIXELFORMAT_RGBA32);
 SDL_Rect left{0,0,120,160},right{120,0,120,160};
 SDL_FillRect(src,&left,SDL_MapRGBA(src->format,255,0,0,255));
 SDL_FillRect(src,&right,SDL_MapRGBA(src->format,0,255,0,255));
 auto* texture=SDL_CreateTextureFromSurface(renderer,src);check(texture,"source");
 sma3::VideoScaler scaler;
 for(int height:{0,720,1080,720,0})for(bool stretch:{false,true}) {
  SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);
  auto dst=sma3::video_destination(2400,1080,240,160,stretch);
  check(dst.w==(stretch?2400:1620) && dst.h==1080 && dst.x==(stretch?0:390),"aspect and stretch");
  check(scaler.draw(renderer,texture,240,160,dst,height),"render pipeline");
  check(SDL_GetRenderTarget(renderer)==nullptr,"restored screen target");
  if(height) {
   int w,h;check(SDL_QueryTexture(scaler.output_texture,nullptr,nullptr,&w,&h)==0,"query HD buffer");
   check(w==height*16/9 && h==height,"real 720p/1080p buffer dimensions");
  } else check(scaler.output_texture==nullptr,"automatic releases HD buffer");
  SDL_RenderPresent(renderer);
  auto pixel=[&](int x,int y,int r,int g,int b) {
   Uint32 value=*(Uint32*)((Uint8*)screen->pixels+y*screen->pitch+x*4);Uint8 cr,cg,cb,ca;
   SDL_GetRGBA(value,screen->format,&cr,&cg,&cb,&ca);
   check(cr==r && cg==g && cb==b,"sharp interior pixel color");
  };
  pixel(dst.x+dst.w/4,540,255,0,0);pixel(dst.x+dst.w*3/4,540,0,255,0);
  if(!stretch)pixel(0,540,0,0,0);else pixel(0,540,255,0,0);
  std::printf("PASS quality=%d stretch=%d buffer=%dx%d viewport=%d,%d %dx%d\n",height,stretch,scaler.ow,scaler.oh,dst.x,dst.y,dst.w,dst.h);
 }
 scaler.reset();SDL_DestroyTexture(texture);SDL_FreeSurface(src);SDL_DestroyRenderer(renderer);SDL_FreeSurface(screen);SDL_Quit();
}
