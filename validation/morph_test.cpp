#include "runtime_arm.h"
#include "runtime_bus_bridge.h"
#include "gba_bus.h"
#include <vector>
#include <fstream>
#include <iterator>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <memory>
int sma3_morph_dispatch(uint32_t,int);
static void require(bool ok,const char* why) { if(!ok) { std::fprintf(stderr,"FAIL: %s pc=%08X\n",why,g_cpu.R[15]);std::exit(1); } }
struct Result { ArmCpuState cpu; std::vector<uint8_t> ram,ewram; };
Result run(const std::vector<uint8_t>& rom,uint32_t base,bool native,int rows,bool interrupt) {
 auto bus=std::make_unique<gba::GbaBus>();bus->set_rom(rom.data(),rom.size());
 gbarecomp::set_active_bus(bus.get());gbarecomp::set_active_ppu(nullptr);
 std::memcpy(bus->iwram_ptr()+base-0x03000000,rom.data()+0x40CE0,0x4B4);
 bus->write32(0x03006394,base|1);bus->write32(0x03007240,0x0300220C);
 // Valid one-pixel morph work packet. Zero rows exercises early return.
 bus->write16(0x0300220C+0x2A5C,rows);
 bus->write16(0x030069F4+0x0E,0);bus->write16(0x030069F4+0x16,0);
 bus->write16(0x0300220C+0x29D2+0x14,0x100);
 bus->write16(0x0300220C+0x29D2+0x16,0x100);
 for(unsigned i=0;i<0x4000;++i)bus->ewram_ptr()[i]=static_cast<uint8_t>(i*13+7);
 // Row source: x-left/right and y-left/right are unsigned byte coordinates.
 for(unsigned i=0;i<16;++i) {bus->ewram_ptr()[0x8000+i*4]=2;bus->ewram_ptr()[0x8001+i*4]=3;bus->ewram_ptr()[0x8002+i*4]=4;bus->ewram_ptr()[0x8003+i*4]=5;}
 g_cpu={};g_cpu.cpsr=0xFfu;g_cpu.R[13]=0x03007E00;g_cpu.R[14]=0x0203FF01;g_cpu.R[15]=base;
 g_cpu.R[0]=0x02002000;g_cpu.R[1]=0x02003000;g_cpu.R[2]=0x02008000;g_cpu.R[3]=0x02008000;
 g_runtime_cycles=0;g_runtime_resume_pc=0;g_runtime_break_pc=interrupt ? base+0x1C : 0x0203FF00;
 g_runtime_ram_dispatch_hook=[](uint32_t pc,int thumb) { if(pc==0x0203FF00) {g_cpu.R[15]=pc;return 1;}return sma3_morph_dispatch(pc,thumb);};
 unsigned steps=0;
 while(g_cpu.R[15]!=0x0203FF00 && steps++<100000) {
  if(native) require(sma3_morph_dispatch(g_cpu.R[15],1)==1,"native dispatch");
  else runtime_force_interp_step();
  if(g_cpu.R[15]==base+0x1C) g_runtime_break_pc=0x0203FF00;
 }
 require(g_cpu.R[15]==0x0203FF00,"returned before instruction limit");
 if(rows>0) {
  bool rendered=false;for(unsigned i=0x11400;i<0x15400;++i) rendered |= bus->ewram_ptr()[i]!=0;
  require(rendered,"nonempty morph packet rendered pixels");
  require(bus->read16(0x0300220C+0x2A5C)==0,"all morph rows consumed");
 }
 Result result{g_cpu,{bus->iwram_ptr(),bus->iwram_ptr()+0x8000},{bus->ewram_ptr(),bus->ewram_ptr()+0x40000}};
 // Negative coverage: changed bytes and invalid pointers must never dispatch.
 if(native) {
  bus->iwram_ptr()[base-0x03000000]^=1;require(!sma3_morph_dispatch(base,1),"reject modified code");
  require(!sma3_morph_dispatch(base,0),"reject ARM mode");
 }
 gbarecomp::set_active_bus(nullptr);return result;
}
int main(int argc,char** argv) {
 require(argc==2,"ROM argument");std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};require(rom.size()==0x400000,"ROM size");
 for(uint32_t base:{0x03004D50u,0x03005400u,0x03005000u})for(int rows:{0,1,4}) for(bool interrupt:{false,true}) {
  auto a=run(rom,base,true,rows,interrupt),b=run(rom,base,false,rows,interrupt);
  require(!std::memcmp(a.cpu.R,b.cpu.R,sizeof(a.cpu.R)),"registers equal interpreter");
  require(a.cpu.cpsr==b.cpu.cpsr,"CPSR equal interpreter");
  require(a.ram==b.ram && a.ewram==b.ewram,"memory equal interpreter");
  std::printf("PASS base=%08X rows=%d resume=%d native == interpreter\n",base,rows,interrupt);
 }
}
