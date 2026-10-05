#include "localization.h"
#include "localization_data.h"
#include <iterator>
namespace sma3 {
int localized_read(uint32_t, uint32_t address, uint32_t width, uint32_t original,uint32_t* output) {
    if(!output || (width!=1 && width!=2 && width!=4)) return 0;
    // ROM wait-state mirrors map to the same read-only localization bank.
    if(address<0x08000000u || address>=0x0E000000u) return 0;
    address=0x08000000u+(address&0x01FFFFFFu);
    const uint8_t* bytes=nullptr; uint32_t offset=0,size=0;
    if(address>=0x09E00000u && address<0x09E00000u+sizeof(spanish_text)) {
        bytes=spanish_text;offset=address-0x09E00000u;size=sizeof(spanish_text);
    } else if(address>=0x09F00000u && address<0x09F00000u+sizeof(portuguese_text)) {
        bytes=portuguese_text;offset=address-0x09F00000u;size=sizeof(portuguese_text);
    } else if(address>=0x082F684Cu && address<0x082F684Cu+sizeof(portuguese_glyphs)) {
        // Reserved blank glyphs 60..63. Keep them available to a Portuguese
        // message already open when the user chooses another language.
        bytes=portuguese_glyphs;offset=address-0x082F684Cu;size=sizeof(portuguese_glyphs);
    }
    if(bytes) {
        uint32_t value=0;
        for(uint32_t i=0;i<width;++i) value |= uint32_t(offset+i<size ? bytes[offset+i] : 0xFFu)<<(8*i);
        *output=value;return 1;
    }
    const int lang=language.load(std::memory_order_relaxed);
    if(!lang || width!=4) return 0;
    unsigned lo=0,hi=std::size(text_pointers);
    while(lo<hi) {unsigned mid=lo+(hi-lo)/2;if(text_pointers[mid].address<address)lo=mid+1;else hi=mid;}
    if(lo==std::size(text_pointers) || text_pointers[lo].address!=address || text_pointers[lo].original!=original)return 0;
    *output=lang==1 ? text_pointers[lo].spanish : text_pointers[lo].portuguese;
    return 1;
}
}
