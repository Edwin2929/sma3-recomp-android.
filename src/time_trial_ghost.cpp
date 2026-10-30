#include "time_trial_ghost.h"
#include "time_trial_ghost_detail.h"
#include "time_trial_ghost_storage.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <thread>

namespace sma3 {
namespace {
using namespace ghost_detail;
using Clock = std::chrono::steady_clock;

class GhostService {
public:
    ~GhostService() { shutdown(); }
    bool initialize(const char* path) noexcept {
        try {
            if (!path || !*path || std::char_traits<char>::length(path) > 4096 ||
                    !std::filesystem::path(path).is_absolute()) return false;
            std::lock_guard lock(mutex_);
            if (configured_ && directory_ != path) return false;
            if (configured_ && !stopping_) return true;
            if (configured_) {
                if (worker_.joinable()) return false; // previous shutdown is still joining
                stopping_ = false; scan_done_ = false; write_error_ = false; writing_ = false;
                durations_.fill(0); blocked_.fill(false); loaded_course_ = desired_course_ = -1;
                loaded_.reset(); have_clock_ = false; seen_generation_ = seen_completion_ = 0;
                current_course_ = 0; cancel(GhostReason::AwaitingFullStart);
            }
            directory_ = path; configured_ = true;
            try { worker_ = std::thread([this] { worker(); }); }
            catch (...) { configured_ = false; directory_.clear(); return false; }
            return true;
        } catch (...) { return false; }
    }
    bool enabled() const noexcept { return enabled_.load(std::memory_order_relaxed); }
    void set_enabled(bool value) noexcept {
        std::lock_guard lock(mutex_);
        if (value == enabled()) return;
        enabled_.store(value, std::memory_order_relaxed);
        cancel(value ? GhostReason::AwaitingFullStart : GhostReason::None);
        have_clock_ = false; seen_generation_ = 0;
        condition_.notify_one();
    }
    bool set_bests(const TimeTrialGhostBests& value) noexcept {
        std::lock_guard lock(mutex_);
        for (std::size_t i = 0; i < value.size(); ++i) {
            if (value[i] > kMaxBest || (bests_ready_ && bests_[i] && (!value[i] || value[i] > bests_[i]))) {
                reason_ = GhostReason::InvalidBestSync; return false;
            }
        }
        bests_ = value; bests_ready_ = true;
        if (reason_ == GhostReason::AwaitingBestSync || reason_ == GhostReason::InvalidBestSync)
            reason_ = GhostReason::AwaitingFullStart;
        if (active_playback_ && !eligible(*active_playback_)) {
            retire(active_playback_); current_visual_ = {}; display_visual_ = {};
        }
        condition_.notify_one(); return true;
    }
    void frame(const TimeTrialSnapshot& timer, const GhostVisualSample& visual) noexcept {
        std::lock_guard lock(mutex_);
        if (!enabled() || stopping_) return;
        if (timer[0] != 1 || timer[12] != 280896 || timer[13] != 16777216 || timer[2] > 5 || timer[3] > 72) {
            cancel(GhostReason::Cancelled); return;
        }
        if (have_clock_ && timer[11] == last_clock_ && timer[10] == last_epoch_) return;
        const bool restored = have_clock_ && (timer[10] != last_epoch_ ||
            last_clock_ == UINT64_MAX || timer[11] != last_clock_ + 1);
        last_clock_ = timer[11]; last_epoch_ = timer[10]; have_clock_ = true;
        if (restored) cancel(GhostReason::Cancelled);
        const auto status = TimeTrialStatus(timer[2]);
        const int index = timer[3] ? course_index(unsigned(timer[3] - 1)) : -1;
        if (index >= 0 && desired_course_ != int(timer[3] - 1)) {
            desired_course_ = int(timer[3] - 1); condition_.notify_one();
        }
        current_course_ = index >= 0 ? timer[3] : 0;
        if (!timer[1] || status == TimeTrialStatus::Disabled || status == TimeTrialStatus::Invalidated) {
            cancel(GhostReason::Cancelled); return;
        }
        if (status == TimeTrialStatus::Waiting) {
            cancel(bests_ready_ ? GhostReason::AwaitingFullStart : GhostReason::AwaitingBestSync); return;
        }
        if (index < 0) { cancel(GhostReason::Cancelled); return; }
        if (status == TimeTrialStatus::Finished) {
            if (capture_ && timer[5] == active_generation_ && timer[6] != seen_completion_ &&
                    timer[7] == timer[3] && timer[8] == timer[4]) {
                seen_completion_ = timer[6];
                if (timer[4] && timer[4] <= kMaxFrames && capture_->run.poses.size() == timer[4] &&
                        !capture_->run.images.empty() && qualifying(timer[4], unsigned(index))) {
                    capture_->run.duration = std::uint32_t(timer[4]);
                    enqueue();
                } else cancel(timer[4] > kMaxFrames ? GhostReason::Limit : GhostReason::MissingGhostForBest);
            } else if (capture_) cancel(GhostReason::Cancelled);
            retire(active_playback_); current_visual_ = {}; display_visual_ = {};
            return;
        }
        if (timer[5] != seen_generation_) {
            seen_generation_ = timer[5];
            cancel(GhostReason::AwaitingFullStart);
            // Enabling midway cannot turn a suffix into a record.
            if (!restored && timer[4] == 0 && timer[5] && bests_ready_ && scan_done_ && !blocked_[index]) {
                latched_best_ = bests_[index]; active_generation_ = timer[5];
                if (ready_) {
                    capture_ = std::move(ready_); capture_->reset(unsigned(timer[3] - 1));
                    reason_ = GhostReason::None;
                } else reason_ = GhostReason::BufferUnavailable;
                attach_playback(unsigned(index));
            } else if (!bests_ready_) reason_ = GhostReason::AwaitingBestSync;
        }
        if (capture_ && (timer[5] != active_generation_ || capture_->run.course + 1u != timer[3]))
            cancel(GhostReason::Cancelled);
        if (!capture_) {
            current_visual_ = {}; display_visual_ = {};
            condition_.notify_one(); return;
        }
        if (timer[4] >= kMaxFrames) { cancel(GhostReason::Limit); condition_.notify_one(); return; }
        // Pauses do not advance the index. Replacing the current endpoint on
        // resume lets the unpaused visual replace a hidden inventory endpoint.
        const auto appended = capture_->append(timer[4], visual);
        if (appended != AppendResult::Ok) {
            cancel(appended == AppendResult::Limit ? GhostReason::Limit : GhostReason::CaptureUnavailable);
            condition_.notify_one(); return;
        }
        attach_playback(unsigned(index));
        // The runtime hook prepares the next rasterized frame. The host image
        // being presented now uses the previous frame's OAM/camera context.
        display_visual_ = current_visual_;
        current_visual_ = {};
        if (selected())
            visual_at(*active_playback_, timer[4], current_visual_);
        else display_visual_ = {};
    }
    bool playback(GhostVisualSample& output) noexcept {
        std::lock_guard lock(mutex_); output = {};
        if (!enabled() || !selected() || !display_visual_.visible) return false;
        output = display_visual_; return true;
    }
    TimeTrialGhostSnapshot snapshot() noexcept {
        std::lock_guard lock(mutex_);
        TimeTrialGhostSnapshot out{};
        out[0] = 1; out[1] = enabled(); out[2] = bests_ready_; out[3] = bool(capture_);
        out[4] = enabled() && selected() && display_visual_.visible;
        out[5] = std::uint64_t(storage()); out[6] = std::uint64_t(reason_); out[7] = current_course_;
        out[8] = capture_ ? capture_->run.poses.size() : 0;
        out[9] = selected() ? active_playback_->duration : 0;
        out[10] = pending_count() + unsigned(writing_);
        out[11] = loaded_course_ < 0 ? 0 : unsigned(loaded_course_) + 1;
        if (bests_ready_) for (std::size_t i = 0; i < durations_.size(); ++i)
            if (!blocked_[i] && bests_[i] && durations_[i] == bests_[i]) out[12 + i] = durations_[i];
        return out;
    }
    void shutdown() noexcept {
        {
            std::lock_guard lock(mutex_);
            if (!configured_ || stopping_) return;
            stopping_ = true; cancel(GhostReason::Cancelled); condition_.notify_one();
        }
        if (worker_.joinable()) worker_.join();
    }

private:
    bool eligible(const Run& run) const noexcept {
        const int i = course_index(run.course);
        return bests_ready_ && i >= 0 && bests_[i] && run.duration == bests_[i] && !blocked_[i];
    }
    bool qualifying(std::uint64_t duration, unsigned i) const noexcept {
        return bests_ready_ && !blocked_[i] && (!bests_[i] || duration <= bests_[i]);
    }
    bool selected() const noexcept {
        return active_playback_ && active_playback_->course + 1u == current_course_ && eligible(*active_playback_);
    }
    void retire(std::shared_ptr<const Run>& value) noexcept {
        if (!value) return;
        for (const auto& old : retired_) if (old == value) { value.reset(); return; }
        for (auto& old : retired_) if (!old) {
            old = std::move(value); condition_.notify_one(); return;
        }
        // Backpressure keeps the last owner alive rather than freeing a large
        // route on the guest/UI thread. selected() hides an obsolete course.
        condition_.notify_one();
    }
    void attach_playback(unsigned i) noexcept {
        if (active_playback_ && !selected()) retire(active_playback_);
        if (!active_playback_ && loaded_ && loaded_->course + 1u == current_course_ &&
                eligible(*loaded_) && latched_best_ == bests_[i]) active_playback_ = loaded_;
    }
    unsigned pending_count() const noexcept {
        unsigned count = 0; for (const auto& job : pending_) count += bool(job); return count;
    }
    GhostStorage storage() const noexcept {
        if (!configured_) return GhostStorage::NotConfigured;
        if (write_error_) return GhostStorage::WriteError;
        if (std::any_of(blocked_.begin(), blocked_.end(), [](auto b) { return b; })) return GhostStorage::ReadError;
        if (pending_count() || writing_) return GhostStorage::Saving;
        if (!scan_done_ || (enabled() && !capture_ && !ready_)) return GhostStorage::Loading;
        return GhostStorage::Ready;
    }
    void cancel(GhostReason reason) noexcept {
        if (capture_) {
            capture_->reset(0);
            // The worker only prepares a buffer when neither of these exists.
            ready_ = std::move(capture_);
        }
        retire(active_playback_); current_visual_ = {}; display_visual_ = {};
        reason_ = reason;
    }
    void enqueue() noexcept {
        // A bounded queue prevents an unavailable disk from growing RAM usage.
        for (auto& job : pending_) {
            if (job && job->run.course == capture_->run.course) {
                if (job->run.duration <= capture_->run.duration) { cancel(GhostReason::MissingGhostForBest); return; }
                // Recycling the displaced buffer keeps allocations off frames.
                auto previous = std::move(job); job = std::move(capture_);
                previous->reset(0); ready_ = std::move(previous);
                reason_ = GhostReason::None; condition_.notify_one(); return;
            }
        }
        // Count the in-flight write too. Its reserved queue slot must remain
        // free so a failed write can return without losing completed motion.
        if (pending_count() + unsigned(writing_) >= pending_.size()) {
            cancel(GhostReason::BufferUnavailable); return;
        }
        for (auto& job : pending_) if (!job) {
            job = std::move(capture_); reason_ = GhostReason::None;
            condition_.notify_one(); return;
        }
        cancel(GhostReason::BufferUnavailable);
    }
    void publish_read(int raw, ReadResult result, std::unique_ptr<Run> run) {
        std::shared_ptr<const Run> immutable;
        std::array<std::shared_ptr<const Run>, 4> retired;
        if (run) immutable = std::move(run); // control block allocated on worker
        {
            std::lock_guard lock(mutex_);
            retired.swap(retired_);
            const auto index = course_index(unsigned(raw));
            blocked_[index] = result == ReadResult::Invalid || result == ReadResult::IoError;
            durations_[index] = immutable ? immutable->duration : 0;
            if (desired_course_ == raw) { loaded_course_ = raw; loaded_ = std::move(immutable); }
        }
    }
    void worker() noexcept {
        try {
            for (unsigned i = 0; i < 60; ++i) {
                std::unique_ptr<Run> run;
                const auto raw = raw_course(i);
                const auto result = read_file(file_path(directory_, raw), raw, run);
                publish_read(int(raw), result, std::move(run));
            }
            {
                std::lock_guard lock(mutex_); scan_done_ = true;
            }
            for (;;) {
                std::unique_ptr<Capture> job;
                std::array<std::shared_ptr<const Run>, 4> retired;
                int load_course = -1; bool prepare = false;
                {
                    std::unique_lock lock(mutex_);
                    const auto ready = [&] {
                        return stopping_ || std::any_of(retired_.begin(), retired_.end(), [](const auto& p) { return bool(p); }) ||
                            (enabled() && !ready_ && !capture_) ||
                            (desired_course_ >= 0 && desired_course_ != loaded_course_) ||
                            (pending_count() && Clock::now() >= retry_after_);
                    };
                    if (!ready()) {
                        if (pending_count()) condition_.wait_until(lock, retry_after_, ready);
                        else condition_.wait(lock, ready);
                    }
                    retired.swap(retired_);
                    if (stopping_ && !pending_count()) break;
                    if (!stopping_ && enabled() && !ready_ && !capture_) prepare = true;
                    else if (!stopping_ && desired_course_ >= 0 && desired_course_ != loaded_course_)
                        load_course = desired_course_;
                    else if (pending_count() && (stopping_ || Clock::now() >= retry_after_)) {
                        for (auto& candidate : pending_) if (candidate) { job = std::move(candidate); break; }
                        writing_ = true;
                    }
                }
                if (prepare) {
                    auto buffer = std::make_unique<Capture>();
                    std::lock_guard lock(mutex_);
                    if (!ready_ && !capture_) ready_ = std::move(buffer);
                    continue;
                }
                if (load_course >= 0) {
                    std::unique_ptr<Run> run;
                    const auto result = read_file(file_path(directory_, unsigned(load_course)), unsigned(load_course), run);
                    publish_read(load_course, result, std::move(run)); continue;
                }
                if (!job) continue;
                bool keep;
                {
                    std::lock_guard lock(mutex_);
                    keep = qualifying(job->run.duration, unsigned(course_index(job->run.course)));
                }
                std::unique_ptr<Run> existing;
                const auto result = keep ? write_file(directory_, job->run, existing) : WriteResult::AlreadyBetter;
                std::shared_ptr<const Run> saved;
                if (result == WriteResult::Saved) saved = std::make_shared<Run>(std::move(job->run));
                else if (existing) saved = std::move(existing);
                {
                    std::lock_guard lock(mutex_);
                    writing_ = false;
                    const auto raw = saved ? saved->course : job->run.course;
                    const auto index = course_index(raw);
                    if (result == WriteResult::InvalidExisting) blocked_[index] = true;
                    if (saved) {
                        durations_[index] = saved->duration; blocked_[index] = false;
                        if (desired_course_ == raw) { loaded_course_ = raw; loaded_ = std::move(saved); }
                    }
                    write_error_ = result == WriteResult::IoError;
                    if (write_error_ && !stopping_) {
                        // Failed bytes remain queued for retry; a newer better
                        // completion for this course may supersede this job.
                        bool retained = false;
                        for (auto& queued : pending_) if (queued && queued->run.course == job->run.course) {
                            if (queued->run.duration > job->run.duration) queued.swap(job);
                            retained = true; break;
                        }
                        if (!retained) for (auto& queued : pending_) if (!queued) { queued = std::move(job); break; }
                        retry_after_ = Clock::now() + std::chrono::seconds(1);
                    } else retry_after_ = Clock::time_point{};
                }
                // Destruction and any deallocation stay on this worker.
            }
        } catch (...) {
            std::lock_guard lock(mutex_); write_error_ = true; scan_done_ = true; writing_ = false;
        }
    }

    std::atomic_bool enabled_{false};
    std::mutex mutex_;
    std::condition_variable condition_;
    std::thread worker_;
    std::filesystem::path directory_;
    bool configured_ = false, stopping_ = false, scan_done_ = false, bests_ready_ = false;
    bool have_clock_ = false, writing_ = false, write_error_ = false;
    TimeTrialGhostBests bests_{}, durations_{};
    std::array<bool, 60> blocked_{};
    std::unique_ptr<Capture> ready_, capture_;
    std::array<std::unique_ptr<Capture>, 2> pending_{};
    std::shared_ptr<const Run> loaded_, active_playback_;
    std::array<std::shared_ptr<const Run>, 4> retired_{};
    GhostVisualSample current_visual_{}, display_visual_{};
    int desired_course_ = -1, loaded_course_ = -1;
    std::uint64_t current_course_ = 0, active_generation_ = 0, seen_generation_ = 0;
    std::uint64_t seen_completion_ = 0, latched_best_ = 0, last_clock_ = 0, last_epoch_ = 0;
    GhostReason reason_ = GhostReason::None;
    Clock::time_point retry_after_{};
};

GhostService service;
}  // namespace

bool time_trial_ghost_initialize(const char* path) noexcept { return service.initialize(path); }
bool time_trial_ghost_enabled() noexcept { return service.enabled(); }
void time_trial_ghost_set_enabled(bool value) noexcept { service.set_enabled(value); }
bool time_trial_ghost_set_bests(const TimeTrialGhostBests& bests) noexcept { return service.set_bests(bests); }
void time_trial_ghost_frame(const TimeTrialSnapshot& timer, const GhostVisualSample& sample) noexcept { service.frame(timer, sample); }
bool time_trial_ghost_playback(GhostVisualSample& output) noexcept { return service.playback(output); }
TimeTrialGhostSnapshot time_trial_ghost_snapshot() noexcept { return service.snapshot(); }
void time_trial_ghost_shutdown() noexcept { service.shutdown(); }

}  // namespace sma3
