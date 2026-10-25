// ROM-free exact arithmetic checks for presentation-only color profiles.
#include "mod_color_profile.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

static std::uint64_t checks=0, cases=0;
static void require(bool ok,const char* message) {
    ++checks;
    if(!ok) { std::fprintf(stderr,"FAIL %s\n",message);std::exit(1); }
}
static void verify(const std::array<int,9>& matrix,std::size_t pixels) {
    sma3::mods::ColorProfile profile; profile.matrix=matrix;
    std::vector<std::uint8_t> source(pixels*3),expected(source.size());
    for(std::size_t pixel=0;pixel<pixels;++pixel) {
        source[pixel*3]=std::uint8_t(pixel);
        source[pixel*3+1]=std::uint8_t(pixel*73+19);
        source[pixel*3+2]=std::uint8_t(255-pixel);
    }
    for(std::size_t pixel=0;pixel<pixels;++pixel) for(int channel=0;channel<3;++channel) {
        long long sum=0;
        for(int input=0;input<3;++input) sum+=static_cast<long long>(matrix[channel*3+input])*source[pixel*3+input];
        if(sum<0) sum=0;
        if(sum>25500) sum=25500;
        expected[pixel*3+channel]=std::uint8_t((sum+50)/100);
    }
    std::vector<std::uint8_t> guarded(source.size()+34,0xa5);
    const auto original=source;
    profile.map(source.data(),guarded.data()+17,pixels);
    require(source==original,"source remains unchanged");
    require(std::equal(expected.begin(),expected.end(),guarded.begin()+17),"exact independent matrix oracle");
    for(int i=0;i<17;++i)
        require(guarded[i]==0xa5 && guarded[guarded.size()-1-i]==0xa5,"destination bounds");
    profile.map(source.data(),source.data(),pixels);
    require(source==expected,"in-place transform has identical result");
    ++cases;
}

int main() {
    // Every allowed coefficient and every possible input value in every
    // independent channel, including clipping and negative coefficients.
    for(int coefficient=-200;coefficient<=200;++coefficient) {
        const int green=(coefficient+337)%401-200;
        const int blue=(coefficient+451)%401-200;
        verify({coefficient,0,0,0,green,0,0,0,blue},256);
    }
    const std::array<std::array<int,9>,5> profiles={{{100,0,0,0,100,0,0,0,100},
        {108,0,0,0,100,0,0,0,88},{-200,0,0,0,200,0,0,0,1},
        {103,-14,11,13,84,3,-1,18,91},{200,200,200,-200,-200,-200,1,-1,1}}};
    for(const auto& matrix:profiles) for(std::size_t pixels:{0u,1u,255u,256u,257u,38400u,56960u})
        verify(matrix,pixels);
    unsigned random=0x619724u;
    for(int trial=0;trial<200;++trial) {
        std::array<int,9> matrix{};
        for(int& value:matrix) {
            random^=random<<13;random^=random>>17;random^=random<<5;
            value=int(random%401)-200;
        }
        verify(matrix,513);
    }
    std::vector<std::uint8_t> source(240*160*3,123),scratch(17,77);
    const auto initial=scratch;
    sma3::mods::publish(nullptr);
    require(sma3::mods::apply(source.data(),240,160,scratch)==source.data() && scratch==initial,"disabled profile remains allocation-free passthrough");
    sma3::mods::publish(std::make_shared<const sma3::mods::ColorProfile>());
    require(sma3::mods::apply(source.data(),240,160,scratch)==source.data() && scratch==initial,"identity profile remains allocation-free passthrough");
    sma3::mods::publish(nullptr);
    std::printf("{\"result\":\"PASS\",\"matrix_cases\":%llu,\"assertions\":%llu,\"android_device_tested\":false}\n",
        static_cast<unsigned long long>(cases),static_cast<unsigned long long>(checks));
}
