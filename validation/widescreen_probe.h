#pragma once
#include "runtime.h"
#include "runtime_bus_bridge.h"
#include "gba_bus.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

inline void sma3_probe_frame(const gbarecomp::ExtendedViewFrameInfo* frame) {
    if (!frame) return;
    switch (frame->frame_count) {
        case 7200: case 8000: case 9599: case 12000: case 16000: case 17999: break;
        default: return;
    }
    const char* dir=std::getenv("SMA3_WIDE_PROBE_DIR");
    auto* bus=gbarecomp::active_bus();
    if (!dir || !bus) return;
    const std::string prefix=std::string(dir)+"/wide-"+std::to_string(frame->frame_count);
    for (auto region : {std::pair<unsigned,unsigned>{0x02000000,0x40000},
                       {0x03000000,0x8000},{0x06000000,0x18000},{0x07000000,0x400}}) {
        const auto name=prefix+"-"+std::to_string(region.first)+".bin";
        const uint8_t* data=region.first==0x02000000 ? bus->ewram_ptr()
            : region.first==0x03000000 ? bus->iwram_ptr()
            : region.first==0x06000000 ? bus->vram_ptr() : bus->oam_ptr();
        if (FILE* file=std::fopen(name.c_str(),"wb")) {
            std::fwrite(data,1,region.second,file);std::fclose(file);
        }
    }
    if (FILE* file=std::fopen((prefix+"-io.bin").c_str(),"wb")) {
        std::fwrite(frame->io,1,frame->io_size,file);std::fclose(file);
    }
}
