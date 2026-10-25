// Interleaved CPU-time benchmark; requires explicit historical header macros.
// GCC noipa keeps dynamic color matrices from being specialized to test constants.
#define sma3 sma3_before
#include SMA3_BASELINE_FILTER_HEADER
#include SMA3_BASELINE_COLOR_HEADER
#undef sma3
#include "image_filters.h"
#include "mod_color_profile.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <ctime>
#include <vector>

static volatile unsigned sink=0;
__attribute__((noinline,noipa)) static bool old_scale(const std::uint8_t* source,std::size_t bytes,
        int width,int factor,std::uint8_t* output,std::size_t capacity) {
    return sma3_before::scale_pixels_rgb24(source,bytes,width,160,factor,output,capacity);
}
__attribute__((noinline,noipa)) static bool new_scale(const std::uint8_t* source,std::size_t bytes,
        int width,int factor,std::uint8_t* output,std::size_t capacity) {
    return sma3::scale_pixels_rgb24(source,bytes,width,160,factor,output,capacity);
}
__attribute__((noinline,noipa)) static void old_color(const sma3_before::mods::ColorProfile& profile,
        const std::uint8_t* source,std::uint8_t* output,std::size_t pixels) { profile.map(source,output,pixels); }
__attribute__((noinline,noipa)) static void new_color(const sma3::mods::ColorProfile& profile,
        const std::uint8_t* source,std::uint8_t* output,std::size_t pixels) { profile.map(source,output,pixels); }
static double thread_ms() {
    timespec t{}; clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);
    return t.tv_sec*1000.0+t.tv_nsec/1000000.0;
}
template<class F> static double measure(F fn,int repetitions) {
    const double start=thread_ms();
    for(int i=0;i<repetitions;++i) fn();
    return (thread_ms()-start)/repetitions;
}
template<class A,class B> static void compare(const char* type,int width,int mode,const char* pattern,A before,B after,int reps) {
    std::array<double,9> old{},fresh{};
    for(int i=0;i<3;++i) { before(); after(); }
    for(int i=0;i<9;++i) {
        if(i%2) { fresh[i]=measure(after,reps);old[i]=measure(before,reps); }
        else { old[i]=measure(before,reps);fresh[i]=measure(after,reps); }
    }
    std::sort(old.begin(),old.end()); std::sort(fresh.begin(),fresh.end());
    std::printf("{\"kind\":\"%s\",\"width\":%d,\"mode\":%d,\"pattern\":\"%s\",\"before_ms\":%.6f,\"after_ms\":%.6f,\"ratio\":%.4f}\n",type,width,mode,pattern,old[4],fresh[4],fresh[4]/old[4]);
}

int main() {
    for(int width:{240,356}) for(int pattern=0;pattern<3;++pattern) {
        std::vector<std::uint8_t> source(std::size_t(width)*160*3),a(source.size()*9),b(a.size());
        unsigned seed=0x189713;
        for(int y=0;y<160;++y) for(int x=0;x<width;++x) {
            seed^=seed<<13; seed^=seed>>17;seed^=seed<<5;
            const unsigned value=pattern==0?0x47ab23:pattern==1?(((x/8)^(y/5))%3==0?0x305191:0xaf2733):((seed%4)*0x412315);
            const auto at=(std::size_t(y)*width+x)*3;
            source[at]=value>>16;source[at+1]=value>>8;source[at+2]=value;
        }
        const char* name=pattern==0?"flat":pattern==1?"pixel_art":"palette_noise";
        for(int factor:{2,3}) {
            const auto old=[&]{if(!old_scale(source.data(),source.size(),width,factor,a.data(),a.size())) std::abort();sink=a[source.size()];};
            const auto now=[&]{if(!new_scale(source.data(),source.size(),width,factor,b.data(),b.size())) std::abort();sink=b[source.size()];};
            old();now();
            if(!std::equal(a.begin(),a.begin()+source.size()*factor*factor,b.begin())) return 2;
            compare("scale",width,factor,name,old,now,60);
        }
        for(int matrix=0;matrix<3;++matrix) {
            sma3_before::mods::ColorProfile old;
            sma3::mods::ColorProfile current;
            old.matrix=matrix==0?std::array<int,9>{108,0,0,0,100,0,0,0,88}:matrix==1?std::array<int,9>{100,0,0,0,120,0,0,0,-50}:std::array<int,9>{103,-14,11,13,84,3,-1,18,91};
            current.matrix=old.matrix;
            const auto before=[&]{old_color(old,source.data(),a.data(),source.size()/3);sink=a[source.size()-1];};
            const auto after=[&]{new_color(current,source.data(),b.data(),source.size()/3);sink=b[source.size()-1];};
            before();after();
            if(!std::equal(a.begin(),a.begin()+source.size(),b.begin())) return 3;
            compare("color",width,matrix,name,before,after,120);
        }
    }
}
