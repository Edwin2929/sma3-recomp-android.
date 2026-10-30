#include "time_trial_ghost.h"
#include "time_trial_ghost_detail.h"
#include "time_trial_ghost_storage.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace sma3;
using namespace sma3::ghost_detail;
namespace {
unsigned checks = 0;
void check(bool yes, const char* message) {
    ++checks; if (!yes) throw std::runtime_error(message);
}
GhostVisualSample sample(int x = 100, unsigned room = 0) {
    GhostVisualSample value{};
    value.visible = true; value.room = std::uint16_t(room); value.world_x = x; value.world_y = -12;
    value.image.width = 3; value.image.height = 2;
    value.image.palette[1] = 0x03e0; value.image.palette[2] = 0x7fff;
    value.image.pixels[0] = 0x11; value.image.pixels[1] = 0x02; value.image.pixels[2] = 0x21;
    return value;
}
std::vector<std::uint8_t> bytes(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
void write_bytes(const std::filesystem::path& p, const std::vector<std::uint8_t>& value) {
    std::ofstream out(p, std::ios::binary); out.write(reinterpret_cast<const char*>(value.data()), std::streamsize(value.size()));
    if (!out) throw std::runtime_error("fixture write failed");
}
void resign(std::vector<std::uint8_t>& value) {
    replace32(value,56,crc32(std::span(value).subspan(64)));
    replace32(value,60,crc32(std::span(value).first(60)));
}
template<class Predicate> void await(Predicate predicate, const char* message) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (!predicate() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    check(predicate(), message);
}
TimeTrialSnapshot timer{};
void tick(std::uint64_t elapsed, TimeTrialStatus status = TimeTrialStatus::Running,
          GhostVisualSample visual = sample()) {
    ++timer[11]; timer[2] = std::uint64_t(status); timer[4] = elapsed;
    time_trial_ghost_frame(timer, visual);
}
void begin(std::uint64_t generation, unsigned raw = 0) {
    timer[3] = raw + 1; tick(0,TimeTrialStatus::Waiting,{});
    await([] { auto s=time_trial_ghost_snapshot(); return s[5] == std::uint64_t(GhostStorage::Ready); }, "worker ready before entry");
    timer[5] = generation; tick(0);
    check(time_trial_ghost_snapshot()[3] == 1, "main entry starts recording");
}
void complete(unsigned duration) {
    ++timer[6]; timer[7] = timer[3]; timer[8] = duration;
    tick(duration,TimeTrialStatus::Finished,{});
}
void settled() {
    await([] { auto s=time_trial_ghost_snapshot(); return !s[10] && s[5] == std::uint64_t(GhostStorage::Ready); }, "worker committed and prepared");
}

void format_tests(const std::filesystem::path& directory) {
    Capture capture; capture.reset(0);
    check(capture.append(0,sample()) == AppendResult::Ok,"first sample");
    check(capture.append(1,sample(101)) == AppendResult::Ok,"second sample");
    check(capture.append(2,{}) == AppendResult::Ok,"invisible transition");
    check(capture.append(3,sample(-32,255)) == AppendResult::Ok,"signed coordinates and room255");
    capture.run.duration = 4;
    check(capture.run.images.size() == 1,"identical sprites deduplicated");
    check(valid_run(capture.run),"complete route valid");
    const auto encoded = encode(capture.run);
    auto decoded = decode(encoded,0);
    check(decoded && encode(*decoded) == encoded,"byte exact codec roundtrip");
    GhostVisualSample output;
    check(visual_at(*decoded,0,output) && output.world_x == 100 && output.world_y == -12,"first interval selection");
    check(!visual_at(*decoded,2,output) && !output.visible,"hidden room transition remains hidden");
    check(visual_at(*decoded,3,output) && output.room == 255 && output.world_x == -32,"room and signed coordinate retained");
    check(!visual_at(*decoded,4,output),"replay ends at completion");
    for (std::size_t i=0;i<encoded.size();++i) {
        auto corrupted=encoded;corrupted[i]^=1;
        check(!decode(corrupted,0),"single-byte corruption rejected");
    }
    for (std::size_t n=0;n<encoded.size();++n)
        check(!decode(std::span(encoded).first(n),0),"every truncation rejected");
    auto changed=encoded;changed.push_back(0);resign(changed);
    check(!decode(changed,0),"trailing bytes rejected");
    for (unsigned offset : {8u,32u,36u,44u,48u,52u}) {
        changed=encoded;replace32(changed,offset,0xffffffff);resign(changed);
        check(!decode(changed,0),"untrusted count/version/clock rejected before allocation");
    }
    changed=encoded;changed[12]^=1;resign(changed);check(!decode(changed,0),"wrong ROM identity rejected");
    changed=encoded;changed[42]=1;resign(changed);check(!decode(changed,0),"unknown flags rejected");
    changed=encoded;changed[64]=65;resign(changed);check(!decode(changed,0),"oversized sprite rejected");
    changed=encoded;changed[66]=1;resign(changed);check(!decode(changed,0),"nontransparent index zero rejected");
    changed=encoded;changed[changed.size()-2]=2;resign(changed);check(!decode(changed,0),"out-of-range image reference rejected");
    check(!decode(encoded,1),"course cannot be relabeled by filename");
    for(unsigned raw=0;raw<72;++raw) {
        capture.run.course=std::uint16_t(raw);
        const auto value=encode(capture.run);
        check((!value.empty())==(raw%12<10),"60 courses plus holes validated");
        if(!value.empty()) check(bool(decode(value,raw)),"supported course codec");
    }
    capture.run.course=0;
    auto invalid=sample();invalid.image.width=0;
    check(capture.append(3,invalid)==AppendResult::Invalid,"invalid capture rejected");
    invalid=sample();invalid.image.palette[1]=0x8000;
    check(capture.append(3,invalid)==AppendResult::Invalid,"invalid palette rejected");
    invalid=sample();invalid.image.pixels.back()=1;
    check(capture.append(3,invalid)==AppendResult::Invalid,"unnormalized image tail rejected");
    check(capture.append(kMaxFrames,sample())==AppendResult::Limit,"capture duration limit enforced");
    check(capture.append(5,sample())==AppendResult::Invalid,"missing interval rejected");
    Capture atlas;atlas.reset(0);
    for(unsigned n=0;n<kMaxImages;++n) {
        auto s=sample();s.image.palette[1]=std::uint16_t(n+1);
        if(atlas.append(n,s)!=AppendResult::Ok) throw std::runtime_error("atlas filled too early");
    }
    auto extra=sample();extra.image.palette[1]=1000;
    check(atlas.append(kMaxImages,extra)==AppendResult::Limit,"atlas limit enforced without growth");
    check(atlas.run.images.size()==kMaxImages,"atlas remains bounded");
    Capture long_run;long_run.reset(0);
    for(unsigned n=0;n<kMaxFrames;++n)
        if(long_run.append(n,sample(int(n%500)))!=AppendResult::Ok) throw std::runtime_error("duration filled too early");
    long_run.run.duration=kMaxFrames;
    check(valid_run(long_run.run),"maximum route accepted");
    check(encode(long_run.run).size()<kMaxFileBytes,"maximum route fits file cap");
    check(long_run.append(kMaxFrames,sample())==AppendResult::Limit,"next interval cannot grow route");
    std::unique_ptr<Run> existing;
    check(write_file(directory,capture.run,existing)==WriteResult::Saved,"atomic initial save");
    const auto path=file_path(directory,0);
    const auto saved=bytes(path);
    check(read_file(path,0,existing)==ReadResult::Valid && encode(*existing)==saved,"file roundtrip");
    capture.run.poses.push_back(capture.run.poses.back());capture.run.duration=5;
    check(write_file(directory,capture.run,existing)==WriteResult::AlreadyBetter && bytes(path)==saved,"slower run cannot replace file");
    write_bytes(path,{1,2,3});
    check(write_file(directory,capture.run,existing)==WriteResult::InvalidExisting && bytes(path)==std::vector<std::uint8_t>({1,2,3}),"corrupt file preserved");
    changed=saved;replace32(changed,8,2);resign(changed);write_bytes(path,changed);
    check(write_file(directory,capture.run,existing)==WriteResult::InvalidExisting && bytes(path)==changed,"future version preserved");
    std::filesystem::remove(path);
    check(read_file(path,0,existing)==ReadResult::Missing,"missing file distinguished");
    const auto blocked=directory/"not-a-directory";write_bytes(blocked,{0});
    check(write_file(blocked,capture.run,existing)==WriteResult::IoError,"write failure reported");
}

void service_tests(const std::filesystem::path& directory) {
    check(!time_trial_ghost_enabled(),"disabled by default");
    check(time_trial_ghost_snapshot()[0]==1 && time_trial_ghost_snapshot().size()==72,"status schema72");
    check(!time_trial_ghost_initialize("relative"),"relative storage rejected");
    TimeTrialGhostBests best{};
    best[0]=kMaxBest+1;check(!time_trial_ghost_set_bests(best),"unbounded personal best rejected");best[0]=0;
    check(time_trial_ghost_initialize(directory.c_str()),"initialize protected directory");
    check(time_trial_ghost_initialize(directory.c_str()),"repeated configuration idempotent");
    check(!time_trial_ghost_initialize((directory.string()+"-other").c_str()),"directory replacement rejected");
    time_trial_ghost_set_enabled(true);
    timer[0]=timer[1]=1;timer[12]=280896;timer[13]=16777216;timer[3]=1;
    tick(0,TimeTrialStatus::Waiting,{});
    check(time_trial_ghost_snapshot()[6]==std::uint64_t(GhostReason::AwaitingBestSync),"records synchronization required");
    best[59]=kMaxBest;
    check(time_trial_ghost_set_bests(best),"long historical best remains valid beyond ghost capture cap");
    begin(1);
    for(unsigned n=1;n<6;++n)tick(n,TimeTrialStatus::Running,sample(100+int(n)));
    complete(6);settled();
    check(time_trial_ghost_snapshot()[12]==0,"ghost cannot invent missing Java record");
    best[0]=6;check(time_trial_ghost_set_bests(best),"publish completed personal best");
    check(time_trial_ghost_snapshot()[12]==6,"exact ghost and PB become eligible");
    const auto path=file_path(directory,0);
    const auto original=bytes(path);
    begin(2);
    GhostVisualSample out;
    check(!time_trial_ghost_playback(out),"first frame awaits presentation latch");
    tick(1,TimeTrialStatus::Running,sample(201));
    check(time_trial_ghost_playback(out)&&out.world_x==100,"ghost uses previous frame and recorded position");
    tick(2,TimeTrialStatus::Running,sample(202));
    check(time_trial_ghost_playback(out)&&out.world_x==101,"replay advances with game intervals");
    time_trial_ghost_frame(timer,sample(999));
    check(time_trial_ghost_playback(out)&&out.world_x==101,"duplicate presentation cannot advance latch");
    tick(3,TimeTrialStatus::Paused,{});
    for(unsigned n=0;n<12;++n)tick(3,TimeTrialStatus::Paused,{});
    check(time_trial_ghost_playback(out)&&out.world_x==103,"held pause freezes replay index");
    check(time_trial_ghost_snapshot()[8]==4,"held pause does not append samples");
    tick(3,TimeTrialStatus::Running,sample(203));
    for(unsigned n=4;n<8;++n)tick(n,TimeTrialStatus::Running,sample(200+int(n)));
    complete(8);settled();
    check(bytes(path)==original,"slower completion preserves best motion");
    begin(3);
    for(unsigned n=1;n<6;++n)tick(n,TimeTrialStatus::Running,sample(300+int(n)));
    complete(6);settled();check(bytes(path)==original,"equal result retains existing route");
    begin(4);
    for(unsigned n=1;n<4;++n)tick(n,TimeTrialStatus::Running,sample(400+int(n)));
    complete(4);best[0]=4;check(time_trial_ghost_set_bests(best),"new PB can race worker save");settled();
    check(time_trial_ghost_snapshot()[12]==4 && bytes(path)!=original,"faster full run replaces replay");
    const auto improved=bytes(path);
    begin(5);tick(1);++timer[10];tick(1,TimeTrialStatus::Invalidated,{});
    check(!time_trial_ghost_snapshot()[3]&&!time_trial_ghost_playback(out),"restore cancels recording and both latches");
    check(bytes(path)==improved,"restored partial never replaces saved route");
    begin(6);tick(1);time_trial_ghost_set_enabled(false);
    check(!time_trial_ghost_playback(out)&&time_trial_ghost_snapshot()[12]==4,"disable hides without deleting saved record");
    time_trial_ghost_set_enabled(true);tick(2);
    check(!time_trial_ghost_snapshot()[3],"mid-level enable cannot start suffix recording");
    begin(7);timer[11]+=5;tick(1);
    check(!time_trial_ghost_snapshot()[3],"clock gap cancels capture");
    auto wrong=best;wrong[0]=5;check(!time_trial_ghost_set_bests(wrong),"personal best cannot regress");
    wrong[0]=0;check(!time_trial_ghost_set_bests(wrong),"personal best cannot be erased");
    best[0]=3;check(time_trial_ghost_set_bests(best),"faster unrecorded PB accepted");
    check(!time_trial_ghost_snapshot()[12]&&!time_trial_ghost_playback(out)&&bytes(path)==improved,"old slower ghost hidden but preserved");
    // Real failed write/retry, while preserving the existing directory bytes.
    timer[3]=2;tick(0,TimeTrialStatus::Waiting,{});
    await([]{return time_trial_ghost_snapshot()[11]==2;},"second course prefetched");
    begin(8,1);tick(1);tick(2);
    const auto moved=directory.string()+"-temporarily-moved";
    std::filesystem::rename(directory,moved);write_bytes(directory,{0});
    complete(3);best[1]=3;check(time_trial_ghost_set_bests(best),"second course PB synchronized");
    await([]{auto s=time_trial_ghost_snapshot();return s[5]==std::uint64_t(GhostStorage::WriteError)&&s[10];},"failed write retained as pending");
    std::filesystem::remove(directory);std::filesystem::rename(moved,directory);
    settled();check(time_trial_ghost_snapshot()[13]==3,"pending write retries when storage recovers");
    check(bytes(path)==improved,"retry preserves another course");
    best[2]=5;check(time_trial_ghost_set_bests(best),"historical numeric best without motion synchronized");
    begin(9,2);for(unsigned n=1;n<6;++n)tick(n);complete(6);settled();
    check(!std::filesystem::exists(file_path(directory,2))&&!time_trial_ghost_snapshot()[14],"slower route cannot impersonate historical PB");
    begin(10,2);for(unsigned n=1;n<5;++n)tick(n);complete(5);settled();
    check(time_trial_ghost_snapshot()[14]==5,"equal time fills missing historical ghost");
    std::thread observer([&]{for(unsigned n=0;n<500;++n){check(time_trial_ghost_set_bests(best),"concurrent idempotent synchronization");
        if(time_trial_ghost_snapshot()[10]>2)throw std::runtime_error("pending writes exceed total queue bound");}});
    // Keep the writer count independent of this observational concurrency check.
    for(unsigned n=0;n<500;++n){GhostVisualSample s;time_trial_ghost_playback(s);}
    observer.join();
    time_trial_ghost_shutdown();
    check(time_trial_ghost_enabled()&&!time_trial_ghost_snapshot()[3]&&!time_trial_ghost_playback(out),"shutdown joins writer and cancels partial without changing preference");
    check(time_trial_ghost_initialize(directory.c_str()),"same-process game relaunch restarts worker");
    begin(11,1);tick(1);
    check(time_trial_ghost_playback(out)&&out.world_x==100,"relaunch reloads saved route and latching");
    time_trial_ghost_shutdown();
    check(!time_trial_ghost_snapshot()[3]&&!time_trial_ghost_playback(out),"shutdown clears an active partial");
}
} // namespace

int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::runtime_error("supply an empty private output directory");
        const auto root=std::filesystem::absolute(argv[1]);
        if(std::filesystem::exists(root)&&!std::filesystem::is_empty(root))throw std::runtime_error("output directory must be empty");
        std::filesystem::create_directories(root/"codec");
        format_tests(root/"codec");service_tests(root/"service");
        std::cout<<"PASS "<<checks<<" ghost codec, storage and lifecycle checks\n";
    } catch(const std::exception& error) {
        time_trial_ghost_shutdown();std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;
    }
}
