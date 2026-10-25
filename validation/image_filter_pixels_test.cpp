// ROM-free Scale2x/Scale3x specification checks. C++20; no SDL dependency.
// Build: c++ -std=c++20 -O2 -I src validation/image_filter_pixels_test.cpp -o test
#include "image_filters.h"
#include <array>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
std::uint64_t checks=0, neighborhoods=0, images=0;
void require(bool ok,const char* what) {
    ++checks;
    if(!ok) { std::fprintf(stderr,"FAIL %s\n",what); std::exit(1); }
}
using Color=std::uint32_t;
Color load(const std::vector<std::uint8_t>& image,int width,int x,int y) {
    const auto at=(std::size_t(y)*width+x)*3;
    return (Color(image[at])<<16)|(Color(image[at+1])<<8)|image[at+2];
}
void put(std::vector<std::uint8_t>& image,std::size_t pixel,Color color) {
    image[pixel*3]=std::uint8_t(color>>16);
    image[pixel*3+1]=std::uint8_t(color>>8);
    image[pixel*3+2]=std::uint8_t(color);
}
// Direct, unoptimized specification; production uses one shared cardinal gate.
// These rules intentionally use differently structured conditions to catch
// mistaken optimization, diagonal selection, and row/column indexing.
std::array<Color,9> reference(const std::array<Color,9>& n,int factor) {
    const auto [a,b,c,d,e,f,g,h,i]=n;
    const bool nw=d==b && b!=f && d!=h;
    const bool ne=b==f && b!=d && f!=h;
    const bool sw=d==h && d!=b && h!=f;
    const bool se=h==f && d!=h && b!=f;
    if(factor==2) return {nw?d:e,ne?f:e,sw?d:e,se?f:e};
    return {nw?d:e,((nw && e!=c)||(ne && e!=a))?b:e,ne?f:e,
        ((nw && e!=g)||(sw && e!=a))?d:e,e,((ne && e!=i)||(se && e!=c))?f:e,
        sw?d:e,((sw && e!=i)||(se && e!=g))?h:e,se?f:e};
}
void exhaustive_neighborhoods() {
    // The algorithms only compare colors for equality. Restricted-growth
    // strings enumerate all 21,147 partitions of nine pixels, covering every
    // possible equality pattern (including four through nine distinct colors).
    constexpr std::array<Color,9> palette={0x010203,0xff8007,0x050607,0,0xffffff,
                                          0xff0000,0x00ff00,0x0000ff,0x800102};
    std::vector<std::uint8_t> source(27),out(9*9*3);
    std::array<int,9> labels{};
    const auto visit=[&]() {
        std::array<Color,9> pixels{};
        for(int at=0;at<9;++at) { pixels[at]=palette[labels[at]];put(source,at,pixels[at]); }
        const auto original=source;
        for(int factor:{2,3}) {
            require(sma3::scale_pixels_rgb24(source.data(),source.size(),3,3,factor,out.data(),out.size()),"valid 3x3 neighborhood");
            auto expected=reference(pixels,factor);
            for(int y=0;y<factor;++y) for(int x=0;x<factor;++x)
                require(load(out,3*factor,factor+x,factor+y)==expected[y*factor+x],"exhaustive center rules");
            require(source==original,"input bytes unchanged");
            ++neighborhoods;
        }
    };
    const auto enumerate=[&](const auto& self,int at,int largest)->void {
        if(at==9) { visit();return; }
        for(int label=0;label<=largest+1;++label) {
            labels[at]=label;self(self,at+1,std::max(largest,label));
        }
    };
    enumerate(enumerate,1,0);
    require(neighborhoods==42294,"all 21,147 neighborhood partitions for both filters");
}
void whole_images() {
    std::uint32_t random=0x8b0a81u;
    for(const auto& size:{std::pair{1,1},std::pair{1,9},std::pair{11,1},std::pair{2,2},
                         std::pair{3,5},std::pair{7,3},std::pair{240,160},std::pair{347,160},std::pair{356,160}}) {
        const auto [w,h]=size;
        std::vector<std::uint8_t> source(std::size_t(w)*h*3);
        for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
            random^=random<<13; random^=random>>17; random^=random<<5;
            constexpr std::array<Color,4> colors={0,0xff0000,0x01ff03,0xfefefd};
            put(source,std::size_t(y)*w+x,colors[random%colors.size()]);
        }
        const auto original=source;
        for(int factor:{2,3}) {
            const auto byte_count=source.size()*factor*factor;
            std::vector<std::uint8_t> guarded(byte_count+34,0xa5);
            // Deliberately unaligned output pointer; surrounding bytes are guards.
            require(sma3::scale_pixels_rgb24(source.data(),source.size(),w,h,factor,
                                           guarded.data()+17,byte_count),"whole image valid");
            require(source==original,"whole source immutable");
            for(int guard=0;guard<17;++guard)
                require(guarded[guard]==0xa5 && guarded[byte_count+17+guard]==0xa5,"output bounds guards");
            std::vector<std::uint8_t> out(guarded.begin()+17,guarded.begin()+17+byte_count);
            for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
                std::array<Color,9> neighbors{};
                for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx)
                    neighbors[(dy+1)*3+dx+1]=load(source,w,std::clamp(x+dx,0,w-1),std::clamp(y+dy,0,h-1));
                const auto expected=reference(neighbors,factor);
                for(int dy=0;dy<factor;++dy) for(int dx=0;dx<factor;++dx)
                    require(load(out,w*factor,x*factor+dx,y*factor+dy)==expected[dy*factor+dx],"whole-image border/interior oracle");
            }
            ++images;
        }
    }
}
void invalid_inputs() {
    std::array<std::uint8_t,80> pixels{}; pixels.fill(0xc7);
    const auto original=pixels;
    const auto invalid=[&](const std::uint8_t* src,std::size_t bytes,int w,int h,int factor,
                           std::uint8_t* dst,std::size_t capacity) {
        require(!sma3::scale_pixels_rgb24(src,bytes,w,h,factor,dst,capacity),"invalid request rejected");
        require(pixels==original,"invalid request cannot modify input/output");
    };
    invalid(nullptr,3,1,1,2,pixels.data(),12);
    invalid(pixels.data(),3,1,1,2,nullptr,12);
    invalid(pixels.data(),2,1,1,2,pixels.data()+16,12);
    invalid(pixels.data(),3,1,1,2,pixels.data()+16,11);
    invalid(pixels.data(),3,1,1,3,pixels.data()+16,26);
    invalid(pixels.data(),3,1,1,2,pixels.data(),12);
    invalid(pixels.data(),3,1,1,2,pixels.data()+2,12);
    invalid(pixels.data()+4,3,1,1,2,pixels.data(),12);
    for(int bad:{INT_MIN,-1,0,4097,INT_MAX}) {
        invalid(pixels.data(),3,bad,1,2,pixels.data()+16,12);
        invalid(pixels.data(),3,1,bad,2,pixels.data()+16,12);
    }
    for(int bad:{INT_MIN,-1,0,1,4,INT_MAX})
        invalid(pixels.data(),3,1,1,bad,pixels.data()+16,12);
    invalid(pixels.data(),3,4096,4096,3,pixels.data()+16,12);
    int w=1,h=1;std::size_t bytes=1;
    require(!sma3::filtered_image_size(INT_MAX,INT_MAX,3,w,h,bytes)&&w==0&&h==0&&bytes==0,"rejected dimensions cleared");
    require(sma3::filtered_image_size(356,160,3,w,h,bytes)&&w==1068&&h==480&&bytes==1537920,"maximum current panorama 3x bytes");
    for(int id:{INT_MIN,-1,7,INT_MAX})
        require(sma3::validated_image_filter(id)==sma3::ImageFilter::Balanced,"invalid ID default");
    for(int id=0;id<7;++id)
        require(static_cast<int>(sma3::validated_image_filter(id))==id,"stable IDs");
}
}
int main() {
    invalid_inputs(); exhaustive_neighborhoods(); whole_images();
    std::printf("{\"result\":\"PASS\",\"neighborhood_cases\":%llu,\"whole_images\":%llu,\"assertions\":%llu,\"android_device_tested\":false}\n",
        static_cast<unsigned long long>(neighborhoods),static_cast<unsigned long long>(images),static_cast<unsigned long long>(checks));
}
