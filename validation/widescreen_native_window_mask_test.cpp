/* Frozen native OBJ-window mask oracle. No game data is included in this file.

  First run prepare_window_mask_fixtures.py with private state/ROM inputs.
  Build with the patched engine (example from the repository root):
    c++ -std=c++20 -O1 -g -Wall -Wextra -fsanitize=address,undefined \
      -fno-omit-frame-pointer -fno-sanitize-recover=all \
      -I../gbarecomp/src/gba -I../gbarecomp/src/debug \
      -I../gbarecomp/src/runtime validation/widescreen_native_window_mask_test.cpp \
      ../gbarecomp/src/gba/gba_ppu.cpp -o /private/window-mask-test
  Run with ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 and
  UBSAN_OPTIONS=halt_on_error=1, passing all nine private native-mask-*.bin files.

  This compares actual frozen OAM/matrices/OBJ textures on a synthetic mask
  background. It does not compare a save state's live OAM to its latched PNG,
  or validate the complete native scene. Translated faithful rendering is the
  independent reference; ordinary objects are disabled only in host copies. */

#include "gba_ppu.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::array<int,128> xs;
std::array<uint8_t,1024> expected_oam;
uint64_t checks=0;
void require(bool ok,const char* s) {++checks;if(!ok)throw std::runtime_error(s);}
uint16_t u16(const uint8_t* p) {return uint16_t(p[0]|unsigned(p[1])<<8);}
void put16(uint8_t* p,uint16_t v) {p[0]=uint8_t(v);p[1]=uint8_t(v>>8);}
int provider(int i,uint16_t a0,uint16_t a1,uint16_t a2,int* output) {
 if(i<0||i>=128||!output||xs[i]==std::numeric_limits<int>::min())return 0;
 const auto* p=expected_oam.data()+i*8;
 if(u16(p)!=a0||u16(p+2)!=a1||u16(p+4)!=a2)return 0;
 *output=xs[i];return 1;
}
int tile(int,int,int,uint16_t* out){*out=1;return gba::kWsTilemapReplace;}
constexpr unsigned widths[3][4]={{8,16,32,64},{16,32,32,64},{8,8,16,32}};
std::vector<uint8_t> render(bool wide,const std::array<uint8_t,1024>& oam,
 const std::array<uint8_t,98304>& vram,const std::array<uint8_t,1024>& io,
 const std::array<uint8_t,1024>& pal,uint16_t dispcnt){
 gba::GbaPpu ppu;if(wide)ppu.set_view_margins(58,58,0,0);
 std::vector<uint8_t> rgb(ppu.render_bytes());ppu.render(rgb.data(),dispcnt,io.data(),vram.data(),oam.data(),pal.data());return rgb;
}
void run(const std::string& filename) {
 std::ifstream in(filename,std::ios::binary);
 if(!in)throw std::runtime_error("cannot open private fixture");
 std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),{});
 require(data.size()==34312,"invalid fixture size");require(!std::memcmp(data.data(),"OWM1",4),"bad fixture marker");
 std::array<uint8_t,1024> oam{},io{},pal{};std::array<uint8_t,98304> vram{};
 std::memcpy(oam.data(),data.data()+8,1024);std::memcpy(vram.data()+0x10000,data.data()+1032,32768);
 for(int i=0;i<128;++i){const uint8_t* p=data.data()+33800+i*4;const uint32_t n=uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;xs[i]=static_cast<int32_t>(n);}
 // The game texture/matrix bytes are untouched. All ordinary sprites are
 // disabled in this host-only copy so colors encode only the window mask.
 unsigned windows=0;
 for(int i=0;i<128;++i){auto* p=oam.data()+i*8;if(((u16(p)>>10)&3)!=2)put16(p,0x200);else {++windows;require(xs[i]!=std::numeric_limits<int>::min(),"unproven window");}}
 require(windows==11,"unexpected native windows");expected_oam=oam;
 // The fixture producer enforces this range. Recheck it before any coordinate
 // arithmetic so malformed external input cannot overflow the oracle shift.
 for(int x:xs)if(x!=std::numeric_limits<int>::min() && (x < -32768 || x > 32767))
  throw std::runtime_error("fixture full X outside signed 16-bit range");
 std::fill(vram.begin()+32,vram.begin()+64,0x11);
 for(unsigned off=0xf800;off<0x10000;off+=2)put16(vram.data()+off,1);
 put16(io.data()+8,0x1f00);put16(io.data()+0x4a,0x0100);put16(pal.data()+2,0x7fff);
 const uint16_t dispcnt=uint16_t(0x9100|(data[4]&0x40));
 gba::g_ws_tilemap_provider=tile;gba::g_ws_obj_window_x_provider=provider;
 gba::g_ws_authored_margin_layers=0;gba::g_ws_pillarbox=0;
 const auto wide=render(true,oam,vram,io,pal,dispcnt),center=render(false,oam,vram,io,pal,dispcnt);
 uint64_t left_on=0,right_on=0;
 for(unsigned y=0;y<160;++y)for(unsigned x=0;x<240;++x)for(unsigned ch=0;ch<3;++ch)
  require(center[(y*240+x)*3+ch]==wide[(y*356+x+58)*3+ch],"native center mismatch");
 for(int shift:{58,-58}) {
  auto moved=oam;
  for(unsigned i=0;i<128;++i){auto* p=moved.data()+i*8;const auto a0=u16(p),a1=u16(p+2);if(((a0>>10)&3)!=2)continue;
   const unsigned shape=a0>>14;require(shape<3,"bad shape");const int width=int(widths[shape][a1>>14])*((a0&0x300)==0x300?2:1);
   const int x=xs[i]+shift;
   // Discard objects outside the oracle viewport before encoding nine-bit X;
   // otherwise an offscreen full coordinate could alias into its native view.
   // Keep OAM padding intact because it carries the original affine matrices.
   if(x>=240||x+width<=0){put16(p,0x200);continue;}
   require(x>=-256&&x<256,"faithful oracle coordinate not representable");
   put16(p+2,uint16_t((a1&0xfe00)|(unsigned(x)&511)));
  }
  const auto reference=render(false,moved,vram,io,pal,dispcnt);
  for(unsigned y=0;y<160;++y)for(unsigned j=0;j<58;++j){
   const unsigned wx=shift==58?j:j+298,nx=shift==58?j:j+182;
   const auto v=wide[(y*356+wx)*3];require(v==0||v==255,"mask color is not binary");
   if(v)(shift==58?left_on:right_on)++;
   for(unsigned ch=0;ch<3;++ch)require(v==reference[(y*240+nx)*3+ch],"translated faithful mask mismatch");
  }
 }
 std::cout<<"{\"input\":\""<<filename.substr(filename.find_last_of('/')+1)<<"\",\"window_objects\":"<<windows<<",\"left_mask_pixels\":"<<left_on<<",\"right_mask_pixels\":"<<right_on<<",\"center_exact\":true,\"margins_match_translated_faithful\":true}"<<'\n';
}
}
int main(int argc,char** argv){try{require(argc>1,"missing fixtures");for(int i=1;i<argc;++i)run(argv[i]);std::cout<<"PASS "<<argc-1<<" native frozen states, "<<checks<<" assertions\n";}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<" after "<<checks<<" assertions\n";return 1;}}
