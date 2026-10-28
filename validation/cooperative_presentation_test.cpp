// Deterministic cooperative scheduler regression tests; no phone/GPU FPS claims.
#include "presentation_pacing.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

using Clock=std::chrono::steady_clock;
using Ns=std::chrono::nanoseconds;
constexpr Ns period{16742706};
unsigned checks=0;
void require(bool ok,const char* label){++checks;if(!ok){std::fprintf(stderr,"FAIL %s\n",label);std::exit(1);}}
struct Pacer {
    Clock::time_point& now;Clock::time_point next{};
    sma3::OptionalPresentationBudget budget;
    unsigned full=0,middle=0,resets=0;
    bool unlimited=false;
    explicit Pacer(Clock::time_point& time):now(time){}
    Clock::time_point current_time()const{return now;}
    Ns preparation_cost()const{return budget.preparation();}
    Ns swap_cost()const{return budget.swap();}
    Ns frame_period()const{return period;}
    void reset(){++resets;next=now+period;}
    bool uncapped()const{return unlimited;}
    std::uint64_t schedule_generation()const{return resets;}
    bool midpoint_has_budget()const{return budget.permits(now,next,period);}
    bool midpoint_preparation_due()const{
        return now>=next-period/2-budget.preparation()-sma3::kPresentationServiceLead;
    }
    bool wait_for_midpoint(){
        if(!sma3::midpoint_is_timely(now,next,period))return false;
        now=std::max(now,next-period/2);++middle;return true;
    }
    void wait_for_next_frame(){
        ++full;
        if(now>next+period){next=now+period;return;}
        now=std::max(now,next);
        next=now>next+Ns{1500000}?now+period:next+period;
    }
};
struct Harness {
    Clock::time_point now{};
    Pacer pacer{now};
    sma3::PresentationPacing scheduling;
    std::vector<unsigned char> pixels=std::vector<unsigned char>(3,0);
    struct Sample {bool deferred;unsigned char pixel;Clock::time_point time;};
    std::vector<Sample> samples;
    Ns prep{750000},swap{250000},poll{500000};
    unsigned frame=0;std::uint64_t epoch=0;
    bool doubled=true,realtime=true;
    auto presenter(){return [&](const unsigned char*data,Pacer*clock,bool deferred){
        if(!clock){samples.push_back({deferred,data[0],now});return true;}
        if(deferred&&!clock->midpoint_has_budget())return false;
        now+=prep;clock->budget.observe_preparation(prep);
        if(deferred){if(!clock->wait_for_midpoint())return false;}
        else clock->wait_for_next_frame();
        samples.push_back({deferred,data[0],now});
        now+=swap;clock->budget.observe_swap(swap);return true;
    };}
    void guest(Ns work,bool service=true){
        while(work>Ns::zero()){
            auto chunk=std::min(work,poll);now+=chunk;work-=chunk;
            if(service)scheduling.service(&pacer,doubled,realtime,epoch,presenter());
        }
    }
    void boundary(unsigned char pixel){
        // Match runtime requirement: pending output dealt with before resize/copy.
        scheduling.service(&pacer,doubled,realtime,epoch,presenter(),true);
        pixels.assign(pixels.size(),pixel);
        scheduling.present(&pacer,pixels.data(),pixels.size(),++frame,doubled,realtime,now,epoch,presenter());
    }
};
void workload(const char*name,Ns work,bool service=true,Ns poll=Ns{500000}){
    Harness h;h.poll=poll;
    for(int i=0;i<60;++i){h.guest(work,service);h.boundary(i%200);}
    auto began=h.now;auto firstfull=h.pacer.full,firstmiddle=h.pacer.middle;
    for(int i=0;i<1200;++i){h.guest(work,service);h.boundary(i%200);}
    auto elapsed=std::chrono::duration<double>(h.now-began).count();
    auto full=h.pacer.full-firstfull,middle=h.pacer.middle-firstmiddle;
    std::printf("{\"name\":\"%s\",\"type\":\"synthetic_deterministic_clock_not_phone_measurement\",\"guest_work_ms\":%.3f,\"service\":%s,\"full_deadlines\":%u,\"deferred_images\":%u,\"guest_fps\":%.6f,\"presentation_fps\":%.6f}\n",name,work.count()/1e6,service?"true":"false",full,middle,full/elapsed,(full+middle)/elapsed);
    if(service&&work<=Ns{14000000}){
        require(full==1200&&middle==1200,"every halfslot reached when totalworkfits");
        require(std::abs(full/elapsed-1e9/period.count())<.00001,"native guest speed unchanged");
    }
    if(work>=Ns{14200000})require(middle==0,"near-capacity or overloaded guest does not pay for extra images");
    if(work<Ns{15000000})require(std::abs(full/elapsed-1e9/period.count())<.00001,"optional work cannot slow near-capacity guest");
}
void cases(){
    Harness h;h.boundary(10);h.guest(Ns{10000000});h.boundary(20);
    h.guest(Ns{10000000});h.boundary(30);h.guest(Ns{10000000});
    require(h.samples.size()==6,"three pairs produced");
    const unsigned char expected[]{10,10,15,20,25,30};
    for(unsigned i=0;i<6;++i){
        require(h.samples[i].pixel==expected[i],"ordered genuine blends and full pixels");
        require(h.samples[i].deferred==bool(i%2),"mandatory and deferred slots alternate");
        if(i)require(h.samples[i].time-h.samples[i-1].time>=period/2&&
            h.samples[i].time-h.samples[i-1].time<=period/2+Ns{1},"uniform halfperiod spacing");
    }
    Harness immutable;immutable.boundary(10);immutable.pixels.assign(3,240);
    immutable.guest(Ns{10000000});
    require(immutable.samples.back().pixel==10,"guest mutation cannot change pending full image");
    Harness early;early.boundary(10);early.guest(Ns{1000000});
    auto before=early.now;auto count=early.samples.size();
    early.scheduling.service(&early.pacer,true,true,early.epoch,early.presenter());
    require(early.now==before&&early.samples.size()==count,"early polling never sleeps or renders");
    early.boundary(20);
    require(early.samples[1].pixel==10&&early.samples[2].pixel==15,"fast completion flushes old full before next mandatory blend");
    Harness late;late.boundary(10);late.guest(Ns{11000000},false);late.boundary(20);
    require(late.samples.size()==2&&late.samples.back().pixel==20,"missed deferred slot falls back to current raw image");
    late.guest(Ns{10000000});late.boundary(30);
    require(late.samples.back().pixel==25,"service recovers blend sequence after missed slot");
    Harness mode;mode.boundary(10);mode.doubled=false;mode.guest(Ns{10000000});mode.boundary(20);
    require(mode.samples.size()==2&&!mode.samples.back().deferred&&mode.samples.back().pixel==20,"mode off clears pending full and shows raw");
    require(mode.pacer.resets==2,"mode switch resets phase");
    Harness epoch;epoch.boundary(10);++epoch.epoch;epoch.guest(Ns{10000000});epoch.boundary(20);
    require(epoch.samples.size()==2&&epoch.samples.back().pixel==20,"new surface epoch never shows stale pending image");
    Harness geometry;geometry.boundary(10);geometry.pixels.resize(6);geometry.guest(Ns{10000000},false);geometry.boundary(20);
    require(geometry.pacer.resets==2&&geometry.samples.back().pixel==20,"geometry discontinuity clears stale blending");
    Harness cut;cut.boundary(10);cut.guest(Ns{10000000});cut.boundary(240);
    require(cut.samples.back().pixel==240,"scene cut never blends old scene");
    Harness reset;reset.boundary(10);reset.now+=std::chrono::seconds(2);reset.pacer.reset();
    reset.guest(Ns{10000000});
    require(reset.samples.size()==1,"external reset invalidates pending full before service");
    reset.boundary(20);
    require(reset.samples.back().pixel==20,"external reset discards stale blend history");
    Harness aged;aged.boundary(10);aged.now+=std::chrono::seconds(2);
    aged.guest(Ns{10000000});
    require(aged.samples.size()==1,"old pending full is never displayed after long stall");
    Harness regression;regression.boundary(10);regression.now-=std::chrono::seconds(2);
    regression.scheduling.service(&regression.pacer,true,true,0,regression.presenter(),true);
    require(regression.samples.size()==1,"clock regression cannot service stale pending full");
    Harness paused;paused.boundary(10);paused.realtime=false;paused.guest(Ns{10000000});
    require(paused.samples.size()==1,"pause cancels pending full without a frame-boundary present");
    Harness failed;failed.boundary(10);failed.now+=Ns{7000000};
    failed.scheduling.service(&failed.pacer,true,true,0,
        [](const auto*,auto*,bool){return false;},true);
    failed.boundary(20);
    require(failed.samples.back().pixel==20,"failed optional rendering recovers to raw current image");
    // Admission happens before rendering the mandatory image. A more costly
    // mandatory render is already known before the next guest starts: it must
    // cancel an extra image that no longer fits, including higher swap cost.
    for (int increased_preparation_us : {3400,4000,2800}) {
        Harness rising;rising.prep=Ns{2800000};rising.swap=Ns{0};
        for(int i=0;i<20;++i){rising.guest(Ns{10000000});rising.boundary(i);}
        rising.guest(Ns{10000000});
        rising.prep=Ns{increased_preparation_us*1000};
        if(increased_preparation_us==2800)rising.swap=Ns{1000000};
        rising.boundary(20);
        const auto optional_before=rising.pacer.middle;
        const auto mandatory_before=rising.pacer.full;
        const auto began=rising.now;
        rising.guest(Ns{10000000});rising.boundary(21);
        require(rising.pacer.middle==optional_before,"fresh mandatory cost cancels over-budget deferred image");
        require(rising.pacer.full==mandatory_before+1,"cost rise preserves exactly one mandatory wait");
        require(rising.now-began<=period,"known higher cost cannot cause avoidable next-frame overrun");
        require(rising.samples.back().pixel==21,"cancelled extra recovers to current raw image");
    }
    Harness fast;fast.boundary(10);fast.realtime=false;fast.guest(Ns{10000000});fast.boundary(20);
    require(fast.pacer.full==1&&fast.pacer.middle==0&&fast.samples.back().pixel==20,"fastforward does not add waits or use stale pending");
}
int main(){
    workload("two_ms_guest",Ns{2000000});
    workload("ten_ms_guest",Ns{10000000});
    workload("twelve_ms_guest",Ns{12000000});
    workload("fourteen_ms_guest",Ns{14000000});
    workload("overloaded_seventeen_ms_guest",Ns{17000000});
    workload("overloaded_twenty_ms_guest",Ns{20000000});
    workload("missing_service_10ms",Ns{10000000},false);
    workload("near_capacity_14_2ms_finepoll",Ns{14200000},true,Ns{100000});
    workload("near_capacity_14_4ms_finepoll",Ns{14400000},true,Ns{100000});
    workload("near_capacity_14_5ms_finepoll",Ns{14500000},true,Ns{100000});
    workload("ten_ms_guest_one_ms_poll",Ns{10000000},true,Ns{1000000});
    cases();std::printf("PASS cooperative presentation checks=%u\n",checks);
}
