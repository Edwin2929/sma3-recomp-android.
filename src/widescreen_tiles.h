#pragma once
#include <cstdint>

namespace sma3::wide {
// SMA3 USA: read the complete level map instead of the partially uploaded
// hardware scrolling ring. Reader is a bounded, side-effect-free memory view.
template<class Reader>
bool foreground_tile(Reader read, int x, int y, int layer, uint16_t* output) {
    if (!output || layer < 0 || layer > 1) return false;
    if (x < 0 || x >= 4096 || y < 0 || y >= 2048) {
        *output = 0x60ff;
        return true;
    }
    const unsigned screen = unsigned(y >> 8)*16 + unsigned(x >> 8);
    const unsigned index = read(0x0201b800 + screen, 1) & 0x3f;
    const unsigned map = read(0x03007010, 4);
    if (map != 0x0200000c) return false;
    const unsigned cell = unsigned((y >> 4)&15)*16 + unsigned((x >> 4)&15);
    const unsigned tile = read(map + index*512 + cell*2, 2);
    const unsigned quadrant = unsigned((y >> 3)&1)*2 + unsigned((x >> 3)&1);
    if (layer == 0) {
        const unsigned flags = read(0x081bc444 + (tile >> 8)*4, 4);
        if (flags < 0x08000000 || flags > 0x083fffff - (tile&255)) return false;
        if (!(read(flags + (tile&255), 1) & (8 >> quadrant))) {
            *output = 0x60ff;
            return true;
        }
    }
    const unsigned tiles = read(0x081bad20 + (tile >> 8)*4, 4);
    const unsigned offset = (tile&255)*8 + quadrant*2;
    if (tiles < 0x08000000 || tiles > 0x083ffffe - offset) return false;
    *output = uint16_t(read(tiles + offset, 2));
    return true;
}

template<class Reader>
bool background_tile(Reader read, int x, int y, int layer, unsigned rows, uint16_t* output) {
    if (!output || layer < 2 || layer > 3 || !rows || rows > 128) return false;
    const unsigned ux = unsigned(x)&511;
    const unsigned uy = unsigned(y)&65535;
    const unsigned index = ((uy >> 4)%rows)*32 + (ux >> 4);
    const unsigned entry = read((layer == 2 ? 0x0201bc00u : 0x0201dc00u)+index*2,2);
    const unsigned qx = ((ux >> 3)&1) ^ ((entry >> 10)&1);
    const unsigned qy = ((uy >> 3)&1) ^ ((entry >> 11)&1);
    *output = uint16_t(entry + qx + 16*qy);
    return true;
}
}
