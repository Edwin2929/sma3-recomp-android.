#include "localization.h"
#include "localization_data.h"
#include <cassert>
#include <cstdio>
#include <iterator>
int main() {
 for(int lang=0;lang<3;++lang) {
  sma3::language.store(lang);
  for(const auto& p:sma3::text_pointers) {
   uint32_t out=0;
   int hit=sma3::localized_read(0,p.address,4,p.original,&out);
   assert(hit==(lang!=0));
   if(lang) {
    assert(out==(lang==1?p.spanish:p.portuguese));
    uint32_t byte=0;assert(sma3::localized_read(0,out,1,0,&byte));assert(byte<=255);
    uint32_t alias=0;assert(sma3::localized_read(0,out+0x2000000,1,0,&alias));assert(alias==byte);
   }
   assert(!sma3::localized_read(0,p.address,4,p.original^4,&out));
   assert(!sma3::localized_read(0,p.address,2,p.original,&out));
  }
 }
 // A packet already in Portuguese must retain its bytes after switching.
 sma3::language.store(1);
 for(unsigned i=0;i<sizeof(sma3::portuguese_text);++i) {
  uint32_t v=0;assert(sma3::localized_read(0,0x09F00000+i,1,0,&v));assert(v==sma3::portuguese_text[i]);
 }
 uint32_t out=0;
 assert(!sma3::localized_read(0,0x03006394,4,0,&out));
 assert(!sma3::localized_read(0,0x08000000,4,0,&out));
 assert(!sma3::localized_read(0,0x09E00000,3,0,&out));
 std::printf("PASS: %zu pointer slots x 3 languages; immutable packets, mirrors, width and value guards\n",std::size(sma3::text_pointers));
}
