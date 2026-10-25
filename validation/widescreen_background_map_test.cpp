// Real-PPU regression for the fully uploaded SMA3 BG3 map. Private fixtures
// contain BGF1, u32 authored rows, EWRAM[0x40000], VRAM[0x18000], IO[0x400].
// Original 240px rendering with translated scroll is the independent margin
// oracle. A synthetic unique palette exposes flips, tile indices and banks;
// no ROM, RAM, textures or states are embedded in this test source.
#include "gba_ppu.h"
#include "foreign_presentation_internal.h"
#include "../src/widescreen_tiles.h"
#include "../src/widescreen_objects.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
uint64_t checks=0;
void require(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
uint16_t u16(const uint8_t* p){return uint16_t(p[0]|unsigned(p[1])<<8);}
void put16(uint8_t* p,unsigned n){p[0]=uint8_t(n);p[1]=uint8_t(n>>8);}
struct Fixture {
 std::array<uint8_t,0x40000> ewram{};
 std::array<uint8_t,0x18000> vram{};
 std::array<uint8_t,0x400> io{},oam{},pal{};
 unsigned rows=32;
 Fixture(){
  for(unsigned i=0;i<128;++i)put16(oam.data()+i*8,0x200);
  for(unsigned i=0;i<256;++i)put16(pal.data()+i*2,(i&31)|((i>>5)<<5)|(((i*13)&31)<<10));
 }
};
const Fixture* active=nullptr;bool legacy=false;
int provider(int layer,int x,int y,uint16_t* output){
 if(!active||layer!=3)return gba::kWsTilemapUnavailable;
 if(!legacy && sma3::wide::complete_background_map(layer,u16(active->io.data()+0xe),active->rows))
  return gba::kWsTilemapKeepWrapped;
 auto read=[](uint32_t address,unsigned width){
  if(address<0x02000000||address+width>0x02040000)return uint32_t(0);
  uint32_t value=0;for(unsigned i=0;i<width;++i)value|=uint32_t(active->ewram[address-0x02000000+i])<<(8*i);return value;
 };
 return sma3::wide::background_tile(read,x+int(int16_t(u16(active->io.data()+0x1c))),
  y+int(int16_t(u16(active->io.data()+0x1e))),3,active->rows,output)
  ?gba::kWsTilemapReplace:gba::kWsTilemapUnavailable;
}
std::vector<uint8_t> render(const Fixture& f,bool wide,uint16_t dispcnt=0x0800){
 gba::GbaPpu ppu;if(wide)ppu.set_view_margins(58,58,0,0);
 std::vector<uint8_t> rgb(ppu.render_bytes());ppu.render(rgb.data(),dispcnt,f.io.data(),f.vram.data(),f.oam.data(),f.pal.data());return rgb;
}
int known_x(int index,uint16_t a0,uint16_t a1,uint16_t a2,int* x){
 if(index!=1||a0!=24||a1!=246||a2!=2||!x)return 0;
 *x=246;return 1;
}
int clip_unknown(int index,uint16_t a0,uint16_t a1,uint16_t a2){int x=0;return !known_x(index,a0,a1,a2,&x);}
void individual_object_fallback(){
 Fixture f;put16(f.io.data()+0xe,0xda00);
 for(unsigned a=0xd000;a<0xf000;a+=2)put16(f.vram.data()+a,1);
 std::fill_n(f.vram.data()+32,32,uint8_t(0x11));put16(f.pal.data()+2,0x001f);
 std::fill_n(f.vram.data()+0x10020,32,uint8_t(0x11));put16(f.pal.data()+0x202,0x7c00);
 std::fill_n(f.vram.data()+0x10040,32,uint8_t(0x22));put16(f.pal.data()+0x204,0x03e0);
 put16(f.oam.data(),24);put16(f.oam.data()+2,508);put16(f.oam.data()+4,1);
 put16(f.oam.data()+8,24);put16(f.oam.data()+10,246);put16(f.oam.data()+12,2);
 sma3::wide::ObjectPositions positions;
 positions[0]={-4,false,24,508,1};positions[1]={246,true,24,246,2};
 require(sma3::wide::window_positions_resolved(positions),"ordinary unknown object incorrectly closes scenery");
 active=&f;legacy=false;gba::g_ws_obj_attr_x_provider=known_x;gba::g_ws_obj_native_clip_provider=clip_unknown;
 const auto wide=render(f,true,0x1800),native=render(f,false,0x1800);
 for(unsigned y=0;y<160;++y)for(unsigned x=0;x<240;++x)for(unsigned c=0;c<3;++c)
  require(wide[(y*356+x+58)*3+c]==native[(y*240+x)*3+c],"unresolved object changed original viewport");
 for(unsigned y=24;y<32;++y){
  for(unsigned x=54;x<58;++x)require(wide[(y*356+x)*3]==255&&wide[(y*356+x)*3+2]==0,"unresolved object leaked into margin");
  for(unsigned x=58;x<62;++x)require(wide[(y*356+x)*3+2]==255,"unresolved object disappeared from native viewport");
  for(unsigned x=304;x<312;++x)require(wide[(y*356+x)*3+1]==255,"known margin object disappeared because another object is unknown");
 }
 gba::g_ws_obj_attr_x_provider=nullptr;gba::g_ws_obj_native_clip_provider=nullptr;
 std::cout<<"{\"case\":\"individual_object_fallback\",\"center_exact\":true,\"unknown_native_pixels_preserved\":true,\"unknown_margin_clipped\":true,\"known_margin_preserved\":true}\n";
}
sma3::wide::ObjectPositions window_positions;
int window_x(int index,uint16_t a0,uint16_t a1,uint16_t a2,int* x){
 if(index<0||index>=128||!x)return 0;const auto& p=window_positions[unsigned(index)];
 if(!p.valid||p.a0!=a0||p.a1!=a1||p.a2!=a2)return 0;*x=p.x;return 1;
}
void unresolved_window_counterexample(){
 Fixture f;put16(f.io.data()+0xe,0xda00);
 for(unsigned a=0xd000;a<0xf000;a+=2)put16(f.vram.data()+a,1);
 std::fill_n(f.vram.data()+32,32,uint8_t(0x11));put16(f.pal.data()+2,0x001f);
 std::fill_n(f.vram.data()+0x10020,32,uint8_t(0x11));
 put16(f.io.data()+0x4a,0x0008); // Outside: red BG3; inside: black backdrop.
 put16(f.oam.data(),0x0818);put16(f.oam.data()+2,488);put16(f.oam.data()+4,1);
 window_positions={};window_positions[0]={-24,false,0x0818,488,1};
 active=&f;legacy=false;gba::g_ws_obj_window_x_provider=window_x;
 gba::g_ws_pillarbox=0;const auto ungated=render(f,true,0x9800);
 require(ungated[(24*356+34)*3]==255,"counterexample did not expose uniform native control in unknown margin");
 require(!sma3::wide::window_positions_resolved(window_positions),"unknown offscreen window was trusted");
 gba::g_ws_pillarbox=1;const auto closed=render(f,true,0x9800),native=render(f,false,0x9800);
 for(unsigned y=0;y<160;++y)for(unsigned x=0;x<240;++x)for(unsigned c=0;c<3;++c)
  require(closed[(y*356+x+58)*3+c]==native[(y*240+x)*3+c],"window guard changed the native center");
 for(unsigned y=0;y<160;++y)for(unsigned x=0;x<58;++x)for(unsigned c=0;c<3;++c){
  require(closed[(y*356+x)*3+c]==0,"unknown window leaked left margin");
  require(closed[(y*356+x+298)*3+c]==0,"unknown window leaked right margin");
 }
 window_positions[0].valid=true;
 window_positions[1]={0,false,24,0,1}; // Ordinary unresolved OBJ does not close a proven stencil.
 require(sma3::wide::window_positions_resolved(window_positions),"ordinary sprite blocked known window");
 gba::g_ws_pillarbox=0;const auto known=render(f,true,0x9800);
 require(known[(24*356+34)*3]==0&&known[(24*356+33)*3]==255,"known stencil did not retain its exact margin boundary");
 window_positions[0].x=-23;require(!sma3::wide::window_positions_resolved(window_positions),"window with wrong low X bits was trusted");
 window_positions[0].x=32768;require(!sma3::wide::window_positions_resolved(window_positions),"unbounded window X was trusted");
 window_positions[0].valid=false;window_positions[0].a0|=0x200;
 require(sma3::wide::window_positions_resolved(window_positions),"disabled window blocked scenery");
 gba::g_ws_obj_window_x_provider=nullptr;gba::g_ws_pillarbox=0;
 std::cout<<"{\"case\":\"unknown_offscreen_window_with_uniform_native_row\",\"unsafe_uniform_extension_reproduced\":true,\"unknown_window_margins_closed\":true,\"known_window_preserved\":true,\"center_exact\":true}\n";
}
uint64_t run(const std::string& name,Fixture& f){
 require(sma3::wide::complete_background_map(3,u16(f.io.data()+0xe),f.rows),"fixture does not have authoritative complete BG3 layout");
 active=&f;legacy=false;auto wide=render(f,true),native=render(f,false);
 legacy=true;auto before=render(f,true);legacy=false;
 uint64_t old_differences=0,margin_pixels=0;
 for(unsigned y=0;y<160;++y)for(unsigned x=0;x<240;++x)for(unsigned c=0;c<3;++c)
  require(wide[(y*356+x+58)*3+c]==native[(y*240+x)*3+c],"center differs from original");
 for(int shift:{-58,58}){
  auto translated=f;put16(translated.io.data()+0x1c,unsigned(u16(f.io.data()+0x1c)+shift));
  auto expected=render(translated,false);
  for(unsigned y=0;y<160;++y)for(unsigned x=0;x<58;++x){
   unsigned wx=shift<0?x:298+x,nx=shift<0?x:182+x;bool old_bad=false;
   for(unsigned c=0;c<3;++c){
    auto want=expected[(y*240+nx)*3+c];
    require(wide[(y*356+wx)*3+c]==want,"margin differs from translated original");
    old_bad|=before[(y*356+wx)*3+c]!=want;
   }
   old_differences+=old_bad;++margin_pixels;
  }
 }
 std::cout<<"{\"fixture\":\""<<name<<"\",\"margin_pixels\":"<<margin_pixels<<",\"old_wrong_margin_pixels\":"<<old_differences<<",\"new_wrong_margin_pixels\":0,\"center_exact\":true}\n";
 return old_differences;
}
}
int main(int argc,char** argv){try{
 gba::foreign_presentation_internal::clear();gba::g_ws_tilemap_provider=provider;
 gba::g_ws_pillarbox=gba::g_ws_pillarbox_left=gba::g_ws_pillarbox_right=0;
 gba::g_ws_authored_margin_layers=0;gba::g_ws_bg_x_provider=nullptr;gba::g_ws_bg_xy_provider=nullptr;
 gba::g_ws_obj_x_provider=nullptr;gba::g_ws_obj_attr_x_provider=nullptr;
 gba::g_ws_obj_window_x_provider=nullptr;gba::g_ws_obj_native_clip_provider=nullptr;gba::g_ws_obj_margin_provider=nullptr;
 for(int layer=-1;layer<=4;++layer)for(unsigned control:{0x1a00u,0x5a00u,0x9a00u,0xda00u,0xfa06u,0xd900u,0xdb00u})for(unsigned rows:{0u,16u,32u,64u,128u})
  require(sma3::wide::complete_background_map(layer,uint16_t(control),rows)==(layer==3&&rows==32&&(control==0xda00||control==0xfa06)),"layout guard accepted partial or foreign map");
 individual_object_fallback();
 unresolved_window_counterexample();
 uint64_t old_wrong=0;unsigned count=0;
 for(int i=1;i<argc;++i){
  std::ifstream in(argv[i],std::ios::binary);require(bool(in),"cannot open private fixture");
  std::vector<uint8_t> b((std::istreambuf_iterator<char>(in)),{});
  require(b.size()==8+0x40000+0x18000+0x400&&!std::memcmp(b.data(),"BGF1",4),"invalid fixture");
  Fixture f;f.rows=unsigned(b[4])|(unsigned(b[5])<<8);require(f.rows>0&&f.rows<=128,"invalid rows");
  std::memcpy(f.ewram.data(),b.data()+8,f.ewram.size());
  std::memcpy(f.vram.data(),b.data()+8+f.ewram.size(),f.vram.size());
  std::memcpy(f.io.data(),b.data()+8+f.ewram.size()+f.vram.size(),f.io.size());
  std::string name=argv[i];name=name.substr(name.find_last_of('/')+1);
  old_wrong+=run(name,f);++count;
 }
 require(count>0,"no fixtures supplied");require(old_wrong>0,"fixtures do not reproduce the old background defect");
 std::cout<<"PASS "<<count<<" real-map fixtures, "<<checks<<" assertions; "<<old_wrong<<" old wrong margin pixels corrected\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<" after "<<checks<<" assertions\n";return 1;}}
