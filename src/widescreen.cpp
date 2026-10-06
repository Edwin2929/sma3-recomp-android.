#include "widescreen.h"
#include "widescreen_tiles.h"
#include "widescreen_objects.h"
#include <cstdio>
#include "runtime_bus_bridge.h"
#include "runtime_arm.h"
#include "gba_bus.h"
#include "gba_ppu.h"
#include <cstddef>

namespace sma3::wide {
namespace {
const gba::GbaBus* memory = nullptr;
const uint8_t* registers = nullptr;
unsigned image_rows[2]{};
bool gameplay = false;
ObjectPositions positions;
std::array<ObjectPositions,3> submissions;
unsigned submission_count=0;
uint64_t epoch=~uint64_t(0);

uint32_t read(uint32_t address, unsigned width) {
    if (!memory || (width != 1 && width != 2 && width != 4)) return 0;
    const uint8_t* bytes = nullptr;
    uint32_t offset = 0;
    std::size_t size = 0;
    if (address >= 0x02000000 && address < 0x02040000) {
        bytes=memory->ewram_ptr(); offset=address-0x02000000; size=0x40000;
    } else if (address >= 0x03000000 && address < 0x03008000) {
        bytes=memory->iwram_ptr(); offset=address-0x03000000; size=0x8000;
    } else if (address >= 0x07000000 && address < 0x07000400) {
        bytes=memory->oam_ptr(); offset=address-0x07000000; size=0x400;
    } else if (address >= 0x08000000 && address < 0x08400000) {
        bytes=memory->rom_ptr(); offset=address-0x08000000; size=memory->rom_size();
    }
    if (!bytes || offset + width > size) return 0;
    uint32_t value=0;
    for (unsigned i=0;i<width;++i) value |= uint32_t(bytes[offset+i]) << (8*i);
    return value;
}
uint16_t reg(unsigned offset) {
    return uint16_t(registers[offset] | (unsigned(registers[offset+1]) << 8));
}
void object_submission(uint32_t pc) {
    if (pc!=0x080004a0 || g_cpu.R[0]!=0x03005a00 || g_cpu.R[1]!=0x0201a800) return;
    // Observe completed staging immediately before compaction. Frame-start
    // staging can already belong to the next frame. Never alter guest state.
    memory=gbarecomp::active_bus();
    submissions[2]=submissions[1];
    submissions[1]=submissions[0];
    resolve_object_positions(read,submissions[0],false);
    if(submission_count<submissions.size()) ++submission_count;
}
bool object_matches(int index, uint16_t a0, uint16_t a1, uint16_t a2) {
    if (!gameplay || index<0 || index>=128) return false;
    const auto& p=positions[index];
    return p.valid && p.a0==a0 && p.a1==a1 && p.a2==a2;
}
int object_x(int index,uint16_t a0,uint16_t a1,uint16_t a2,int* output) {
    if (!output || !object_matches(index,a0,a1,a2)) return 0;
    *output=positions[index].x;
    return 1;
}
int object_clip(int index,uint16_t a0,uint16_t a1,uint16_t a2) {
    return object_matches(index,a0,a1,a2) ? 0 : 1;
}
int tile(int layer, int x, int y, uint16_t* output) {
    if (!gameplay || !registers || layer<0 || layer>3 || !output) return gba::kWsTilemapUnavailable;
    // Use live scroll, including its complete world coordinates. No game RAM,
    // CPU state, timing, open-bus or prefetch state is modified by this reader.
    const int wx=x + int(int16_t(reg(0x10+layer*4)));
    const int wy=y + int(int16_t(reg(0x12+layer*4)));
    const bool available=layer<2 ? foreground_tile(read,wx,wy,layer,output)
        : background_tile(read,wx,wy,layer,image_rows[layer-2],output);
    return available ? gba::kWsTilemapReplace : gba::kWsTilemapUnavailable;
}
}

void frame(const gbarecomp::ExtendedViewFrameInfo* info) {
    if (!info) return;
    if (epoch!=info->state_epoch) { submissions={}; submission_count=0; epoch=info->state_epoch; }
    if (!g_runtime_fn_entry_hook || g_runtime_fn_entry_hook==object_submission)
        g_runtime_fn_entry_hook=object_submission;
    memory=gbarecomp::active_bus();
    registers=info && info->io_size>=0x60 ? info->io : nullptr;
    gameplay=memory && registers && info->view_width<=356 &&
        read(0x03006d64,1)==0 && read(0x03006b05,1)==0x0d &&
        read(0x03007010,4)==0x0200000c && (reg(0)&7)==0;
    for (unsigned i=0;i<2;++i) {
        image_rows[i]=0;
        if (!gameplay) continue;
        const unsigned id=read(0x03004ba2+i*4,2);
        if (id>(i==0 ? 0x20u : 0x2fu)) continue;
        const unsigned pointer=read((i==0 ? 0x081675e4u : 0x0816766cu)+id*4,4);
        if (pointer<0x08000000 || pointer>=0x083ffffc) continue;
        const unsigned header=read(pointer,4), bytes=header>>8;
        if ((header&255)==0x10 && bytes>=64 && bytes<=8192 && bytes%64==0) image_rows[i]=bytes/64;
    }
    gba::g_ws_tilemap_provider=tile;
    gba::g_ws_authored_margin_layers=0;
    gba::g_ws_pillarbox=gameplay ? 0 : 1;
    positions={};
    unsigned matched=0;
    int submission_age=-1;
    if (gameplay && g_runtime_fn_entry_hook==object_submission) {
        for(unsigned age=0;age<submission_count;++age) {
            if(!matches_display(read,submissions[age])) continue;
            positions=submissions[age];
            submission_age=int(age);
            for(const auto& p:positions) if(p.valid) ++matched;
            break;
        }
    }
    gba::g_ws_obj_x_provider=nullptr;
    gba::g_ws_obj_attr_x_provider=object_x;
    gba::g_ws_obj_native_clip=0;
    gba::g_ws_obj_native_clip_provider=object_clip;
    if (info && (info->frame_count==7200 || info->frame_count==8000 ||
                 info->frame_count==9599 || info->frame_count==12000 ||
                 info->frame_count==16000 || info->frame_count==17999)) {
        std::fprintf(stderr,"[sma3:wide] frame=%llu matched_object_positions=%u submission_age=%d\n",
            static_cast<unsigned long long>(info->frame_count),matched,submission_age);
    }
}
}
