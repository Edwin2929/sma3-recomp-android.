#include "frame_interpolation.h"
#include "host_platform.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>
int main() {
    sma3::FrameBlend b;
    const uint8_t a[] = {0,100,200,20,120,220}, c[] = {20,120,220,40,140,240};
    assert(!b.prepare(a,6,10)); assert(b.prepare(c,6,11));
    assert(b.middle()[0]==10 && b.middle()[5]==230);
    assert(!b.prepare(a,6,11)); assert(!b.prepare(c,6,50));
    const uint8_t white[] = {255,255,255,255,255,255};
    assert(!b.prepare(white,6,51)); assert(b.prepare(white,6,52));
    for(int i=0;i<6;++i) assert(b.middle()[i]==255);
    b.reset(); assert(!b.prepare(a,6,53)); assert(!b.prepare(a,3,54));
    using Clock=std::chrono::steady_clock;
    gbarecomp::FramePacer p; auto start=Clock::now(); int middle=0;
    for(int i=0;i<60;++i) { if(p.wait_for_midpoint()) ++middle; p.wait_for_next_frame(); }
    double seconds=std::chrono::duration<double>(Clock::now()-start).count();
    assert(seconds>0.99 && seconds<1.5); assert(middle>40);
    p.reset(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); assert(p.wait_for_midpoint());
    p.wait_for_next_frame();
    p.reset(); std::this_thread::sleep_for(std::chrono::milliseconds(14)); assert(!p.wait_for_midpoint());
    p.set_uncapped(true); assert(!p.wait_for_midpoint());
    std::printf("PASS: blend/cuts/reset/resize; 60 guest periods in %.4fs; %d midpoint slots\n",seconds,middle);
}
