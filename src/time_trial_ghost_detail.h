#pragma once

#include "ghost_visual.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <vector>

namespace sma3::ghost_detail {

constexpr std::uint32_t kMaxFrames = 72000;
constexpr std::uint32_t kMaxImages = 512;
constexpr std::size_t kMaxFileBytes = 4 * 1024 * 1024;
constexpr std::uint16_t kHidden = 0xffff;
constexpr std::uint64_t kMaxBest = std::numeric_limits<std::int64_t>::max() / 4389000;
constexpr std::array<std::uint8_t, 20> kRom = {
    0x73,0x52,0xd2,0xbd,0x06,0x4d,0x9e,0xba,0xec,0x57,
    0x9e,0x26,0x42,0x28,0xaa,0x21,0xc7,0x34,0x5b,0x80
};

struct Pose {
    std::int32_t x = 0, y = 0;
    std::uint16_t room = 0, image = kHidden;
};
struct Run {
    std::uint16_t course = 0;
    std::uint32_t duration = 0;
    std::vector<GhostImage> images;
    // Index is the time-trial interval, not presentation or wall-clock time.
    std::vector<Pose> poses;
};

inline int course_index(unsigned raw) noexcept {
    const unsigned world = raw / 12, stage = raw % 12;
    return world < 6 && stage < 10 ? int(world * 10 + stage) : -1;
}
inline unsigned raw_course(unsigned compact) noexcept {
    return compact / 10 * 12 + compact % 10;
}
inline bool coordinate(std::int32_t n) noexcept { return n >= -65536 && n <= 131071; }
inline std::size_t pixel_bytes(const GhostImage& image) noexcept {
    return (std::size_t(image.width) * image.height + 1) / 2;
}
inline bool valid_image(const GhostImage& image) noexcept {
    if (!image.width || !image.height || image.width > 64 || image.height > 64 || image.palette[0]) return false;
    for (auto color : image.palette) if (color > 0x7fff) return false;
    const auto count = pixel_bytes(image);
    if (((unsigned(image.width) * image.height) & 1) && (image.pixels[count - 1] & 0xf0)) return false;
    if (std::any_of(image.pixels.begin() + count, image.pixels.end(), [](auto n) { return n != 0; })) return false;
    return std::any_of(image.pixels.begin(), image.pixels.begin() + count, [](auto n) { return n != 0; });
}
inline bool same_image(const GhostImage& a, const GhostImage& b) noexcept {
    return a.width == b.width && a.height == b.height && a.palette == b.palette && a.pixels == b.pixels;
}
inline std::uint64_t image_hash(const GhostImage& image) noexcept {
    std::uint64_t h = 14695981039346656037ull;
    const auto add = [&](std::uint8_t n) { h = (h ^ n) * 1099511628211ull; };
    add(image.width); add(image.height);
    for (auto c : image.palette) { add(std::uint8_t(c)); add(std::uint8_t(c >> 8)); }
    for (std::size_t i = 0; i < pixel_bytes(image); ++i) add(image.pixels[i]);
    return h;
}

enum class AppendResult { Ok, Limit, Invalid };
struct Capture {
    Run run;
    std::array<std::int16_t, 1024> slots{};
    std::array<std::uint64_t, 1024> hashes{};
    Capture() {
        slots.fill(-1);
        run.poses.reserve(kMaxFrames);
        run.images.reserve(kMaxImages);
    }
    void reset(unsigned course) noexcept {
        run.course = std::uint16_t(course); run.duration = 0;
        run.images.clear(); run.poses.clear(); slots.fill(-1); hashes.fill(0);
    }
    AppendResult append(std::uint64_t tick, const GhostVisualSample& visual) noexcept {
        if (tick >= kMaxFrames) return AppendResult::Limit;
        if (tick > run.poses.size() || (tick + 1 < run.poses.size())) return AppendResult::Invalid;
        Pose pose;
        if (visual.visible) {
            if (visual.room > 255 || !coordinate(visual.world_x) || !coordinate(visual.world_y) ||
                    !valid_image(visual.image)) return AppendResult::Invalid;
            const auto hash = image_hash(visual.image);
            std::size_t slot = hash & (slots.size() - 1);
            while (slots[slot] >= 0) {
                const auto index = std::uint16_t(slots[slot]);
                if (hashes[slot] == hash && same_image(run.images[index], visual.image)) {
                    pose.image = index; break;
                }
                slot = (slot + 1) & (slots.size() - 1);
            }
            if (pose.image == kHidden) {
                if (run.images.size() >= kMaxImages) return AppendResult::Limit;
                pose.image = std::uint16_t(run.images.size());
                run.images.push_back(visual.image); // capacity was reserved by the worker
                slots[slot] = std::int16_t(pose.image); hashes[slot] = hash;
            }
            pose.x = visual.world_x; pose.y = visual.world_y; pose.room = visual.room;
        }
        if (tick == run.poses.size()) run.poses.push_back(pose);
        else run.poses[std::size_t(tick)] = pose;
        return AppendResult::Ok;
    }
};

inline bool valid_run(const Run& run) noexcept {
    if (course_index(run.course) < 0 || !run.duration || run.duration > kMaxFrames ||
            run.poses.size() != run.duration || run.images.empty() || run.images.size() > kMaxImages) return false;
    for (const auto& image : run.images) if (!valid_image(image)) return false;
    bool visible = false;
    for (const auto& p : run.poses) {
        if (p.image == kHidden) {
            if (p.x || p.y || p.room) return false;
        } else {
            if (p.image >= run.images.size() || p.room > 255 || !coordinate(p.x) || !coordinate(p.y)) return false;
            visible = true;
        }
    }
    return visible;
}
inline bool visual_at(const Run& run, std::uint64_t tick, GhostVisualSample& output) noexcept {
    output = {};
    if (tick >= run.poses.size()) return false;
    const auto& p = run.poses[std::size_t(tick)];
    if (p.image == kHidden || p.image >= run.images.size()) return false;
    output.visible = true; output.room = p.room; output.world_x = p.x; output.world_y = p.y;
    output.image = run.images[p.image];
    return true;
}

inline std::uint32_t crc32(std::span<const std::uint8_t> bytes) noexcept {
    std::uint32_t crc = 0xffffffff;
    for (auto byte : bytes) {
        crc ^= byte;
        for (unsigned j = 0; j < 8; ++j) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return ~crc;
}
inline void put16(std::vector<std::uint8_t>& out, std::uint16_t n) {
    out.push_back(std::uint8_t(n)); out.push_back(std::uint8_t(n >> 8));
}
inline void put32(std::vector<std::uint8_t>& out, std::uint32_t n) {
    put16(out, std::uint16_t(n)); put16(out, std::uint16_t(n >> 16));
}
inline std::uint16_t get16(std::span<const std::uint8_t> in, std::size_t at) noexcept {
    return std::uint16_t(in[at] | (unsigned(in[at + 1]) << 8));
}
inline std::uint32_t get32(std::span<const std::uint8_t> in, std::size_t at) noexcept {
    return get16(in, at) | (std::uint32_t(get16(in, at + 2)) << 16);
}
inline void replace32(std::vector<std::uint8_t>& out, std::size_t at, std::uint32_t n) noexcept {
    for (unsigned j = 0; j < 4; ++j) out[at + j] = std::uint8_t(n >> (j * 8));
}

// Explicit little-endian, independent of C++ object padding and pointers.
// Every size is bounded before allocation; unknown or damaged files are kept.
inline std::vector<std::uint8_t> encode(const Run& run) {
    if (!valid_run(run)) return {};
    std::vector<std::uint8_t> out;
    out.reserve(64 + run.images.size() * 2082 + run.poses.size() * 12);
    const std::array<std::uint8_t, 8> magic{'S','M','A','3','G','H','O','1'};
    out.insert(out.end(), magic.begin(), magic.end()); put32(out, 1);
    out.insert(out.end(), kRom.begin(), kRom.end());
    put32(out, 280896); put32(out, 16777216); put16(out, run.course); put16(out, 0);
    put32(out, run.duration); put32(out, std::uint32_t(run.images.size()));
    put32(out, std::uint32_t(run.poses.size())); put32(out, 0); put32(out, 0);
    for (const auto& image : run.images) {
        out.push_back(image.width); out.push_back(image.height);
        for (auto color : image.palette) put16(out, color);
        out.insert(out.end(), image.pixels.begin(), image.pixels.begin() + pixel_bytes(image));
    }
    for (const auto& p : run.poses) {
        put32(out, std::uint32_t(p.x)); put32(out, std::uint32_t(p.y));
        put16(out, p.room); put16(out, p.image);
    }
    replace32(out, 56, crc32(std::span(out).subspan(64)));
    replace32(out, 60, crc32(std::span(out).first(60)));
    return out;
}
inline std::unique_ptr<Run> decode(std::span<const std::uint8_t> in, unsigned expected_course) {
    if (in.size() < 64 || in.size() > kMaxFileBytes || course_index(expected_course) < 0) return {};
    const std::array<std::uint8_t, 8> magic{'S','M','A','3','G','H','O','1'};
    if (!std::equal(magic.begin(), magic.end(), in.begin()) || get32(in, 8) != 1 ||
            !std::equal(kRom.begin(), kRom.end(), in.begin() + 12) ||
            get32(in, 32) != 280896 || get32(in, 36) != 16777216 ||
            get16(in, 40) != expected_course || get16(in, 42) ||
            get32(in, 60) != crc32(in.first(60)) || get32(in, 56) != crc32(in.subspan(64))) return {};
    const auto duration = get32(in, 44), images = get32(in, 48), poses = get32(in, 52);
    if (!duration || duration > kMaxFrames || poses != duration || !images || images > kMaxImages ||
            std::uint64_t(images) * 35 + std::uint64_t(poses) * 12 > in.size() - 64) return {};
    auto run = std::make_unique<Run>(); run->course = std::uint16_t(expected_course); run->duration = duration;
    run->images.reserve(images); run->poses.reserve(poses);
    std::size_t at = 64;
    for (std::uint32_t n = 0; n < images; ++n) {
        if (at + 34 > in.size()) return {};
        GhostImage image{}; image.width = in[at++]; image.height = in[at++];
        if (!image.width || !image.height || image.width > 64 || image.height > 64) return {};
        for (auto& color : image.palette) { color = get16(in, at); at += 2; }
        const auto count = pixel_bytes(image);
        if (count > in.size() - at) return {};
        std::copy_n(in.begin() + at, count, image.pixels.begin()); at += count;
        if (!valid_image(image)) return {};
        run->images.push_back(image);
    }
    if (std::uint64_t(poses) * 12 != in.size() - at) return {};
    for (std::uint32_t n = 0; n < poses; ++n) {
        const auto x = get32(in, at), y = get32(in, at + 4);
        // Decode signed two's-complement values without implementation-defined casts.
        const auto signed32 = [](std::uint32_t value) {
            return value <= std::uint32_t(INT32_MAX) ? std::int32_t(value)
                : std::int32_t(-1 - std::int64_t(~value));
        };
        run->poses.push_back({signed32(x), signed32(y), get16(in, at + 8), get16(in, at + 10)}); at += 12;
    }
    if (!valid_run(*run)) return {};
    return run;
}

}  // namespace sma3::ghost_detail
