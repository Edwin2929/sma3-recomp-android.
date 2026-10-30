// Deterministic before/after recovery benchmark, not a phone/GPU FPS claim.
// Compatible with both the 0.10.11 one-argument cost API and bounded-cost API.
// Build once with the old presentation_pacing.h first on the include path, then
// with the current src directory. Each executable prints six JSON records.
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
    Clock::time_point midpoint_preparation_deadline()const{
        return next-period/2-budget.preparation()-sma3::kPresentationServiceLead;
    }
    bool midpoint_preparation_due()const{
        return now>=midpoint_preparation_deadline();
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
template<class Budget> void record_preparation(Budget& budget,Ns cost){
    if constexpr(requires { budget.observe_preparation(cost,period); })
        budget.observe_preparation(cost,period);
    else budget.observe_preparation(cost);
}
template<class Budget> void record_swap(Budget& budget,Ns cost){
    if constexpr(requires { budget.observe_swap(cost,period); })
        budget.observe_swap(cost,period);
    else budget.observe_swap(cost);
}
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
        now+=prep;record_preparation(clock->budget,prep);
        if(deferred){if(!clock->wait_for_midpoint())return false;}
        else clock->wait_for_next_frame();
        samples.push_back({deferred,data[0],now});
        now+=swap;record_swap(clock->budget,swap);return true;
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
int main(){
 for(int kind=0;kind<3;++kind) {
  const char* name=kind==0?"guest":kind==1?"preparation":"swap";
  Harness h;
  for(int i=0;i<60;++i){h.guest(Ns{10000000});h.boundary(i%200);}
  h.guest(kind==0?Ns{68000000}:Ns{10000000});
  if(kind==1)h.prep=Ns{68000000};
  if(kind==2)h.swap=Ns{68000000};
  h.boundary(60);h.prep=Ns{750000};h.swap=Ns{250000};
  unsigned first_recovered=0,extras=0;
  for(unsigned i=1;i<=30;++i){
   auto before=h.pacer.middle;
   h.guest(Ns{10000000});h.boundary(60+i);
   if(h.pacer.middle!=before){if(!first_recovered)first_recovered=i; ++extras;}
  }
  auto began=h.now;auto first=h.pacer.full;
  for(unsigned i=1;i<=120;++i){h.guest(Ns{10000000});h.boundary(90+i);}
  auto stable_fps=(h.pacer.full-first)/std::chrono::duration<double>(h.now-began).count();
  std::printf("{\"source\":\"%s\",\"stall_us\":68000,\"first_recovered\":%u,\"extras_next_30\":%u,\"stable_guest_fps\":%.6f}\n",name,first_recovered,extras,stable_fps);
  Harness overloaded;
  if(kind==1)overloaded.prep=Ns{25000000};
  if(kind==2)overloaded.swap=Ns{25000000};
  Ns work=kind==0?Ns{25000000}:Ns{10000000};
  for(int i=0;i<60;++i){overloaded.guest(work);overloaded.boundary(i);}
  auto optional=overloaded.pacer.middle;
  for(int i=0;i<120;++i){overloaded.guest(work);overloaded.boundary(i);}
  std::printf("{\"sustained_source\":\"%s\",\"cost_us\":25000,\"extras_next_120\":%u}\n",name,overloaded.pacer.middle-optional);
 }
}
