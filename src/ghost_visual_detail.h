#pragma once

#include "ghost_visual.h"
#include "yoshi_color_palette.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace sma3::ghost_visual_detail {

using yoshi_color_detail::read16;
using yoshi_color_detail::read32;

struct Memory {
    const std::uint8_t* iwram = nullptr;
    const std::uint8_t* oam = nullptr;
    const std::uint8_t* vram = nullptr;
    const std::uint8_t* pal = nullptr;
    const std::uint8_t* io = nullptr;
    std::size_t iwram_size = 0, oam_size = 0, vram_size = 0, pal_size = 0, io_size = 0;
};

struct Scene {
    bool visible = false;
    std::uint16_t room = 0;
    int camera_x = 0, camera_y = 0;
};

struct FrameContext {
    Scene scene{};
    bool pillarbox = true;
    bool valid = false;
};

// The runtime presents a VBlank-latched image after publishing line zero for
// the upcoming frame. This history is shared by the production hook and tests.
class ContextLatch {
public:
    void clear() noexcept {
        current_ = {}; displayed_ = {}; source_ = nullptr; have_frame_ = false;
    }
    bool publish(Scene scene, bool pillarbox, const void* source,
                 std::uint64_t frame, std::uint64_t epoch) noexcept {
        if (!source) { clear(); return false; }
        if (have_frame_ && (source_ != source || epoch_ != epoch || frame < frame_)) clear();
        if (have_frame_ && frame_ == frame) return false;
        displayed_ = current_;
        current_ = {scene,pillarbox,true};
        source_ = source; frame_ = frame; epoch_ = epoch; have_frame_ = true;
        return true;
    }
    const FrameContext& displayed() const noexcept { return displayed_; }
private:
    FrameContext current_{}, displayed_{};
    const void* source_ = nullptr;
    std::uint64_t frame_ = 0, epoch_ = 0;
    bool have_frame_ = false;
};

inline bool objects_unrestricted(const std::uint8_t* io, unsigned display) noexcept {
    if (!(display & 0xe000)) return true;
    const auto inside = read16(io + 0x48), outside = read16(io + 0x4a);
    // Host chrome cannot reconstruct a per-scanline OBJ-window stencil. A
    // region that can hide OBJ closes the overlay rather than leaking it.
    return (outside & 0x10) && (!(display & 0x2000) || (inside & 0x10)) &&
        (!(display & 0x4000) || (inside & 0x1000)) &&
        (!(display & 0x8000) || (outside & 0x1000));
}

inline Scene scene(const Memory& m) noexcept {
    Scene out;
    if (!m.iwram || m.iwram_size < 0x8000 || !m.io || m.io_size < 0x56) return out;
    out.room = read16(m.iwram + 0x4cb8);
    out.camera_x = read16(m.iwram + 0x69d4);
    out.camera_y = read16(m.iwram + 0x69dc);
    const auto display = read16(m.io);
    const auto blend = read16(m.io + 0x50);
    // The ordinary player atlas is the already-qualified palette-mod domain.
    // Menus, transforms, Fuzzy, fades and restricted windows stay untouched.
    out.visible = out.room <= 255 && m.iwram[0x6b05] == 0x0d &&
        yoshi_color_detail::supported_scene(m.iwram, m.iwram_size, display) &&
        (display & 0x1080) == 0x1000 && m.iwram[0x48b5] == 0 &&
        (m.iwram[0x4d46] & 15) == 0 && read32(m.iwram + 0x7294) == 0 &&
        objects_unrestricted(m.io, display) &&
        !((blend & 0x10) && ((blend >> 6) & 3) >= 2 && (read16(m.io + 0x54) & 31));
    return out;
}

inline unsigned pixel(const GhostImage& image, unsigned x, unsigned y) noexcept {
    const unsigned i = y * image.width + x;
    return (image.pixels[i / 2] >> ((i & 1) * 4)) & 15;
}

inline void set_pixel(GhostImage& image, unsigned x, unsigned y, unsigned value) noexcept {
    const unsigned i = y * image.width + x, shift = (i & 1) * 4;
    image.pixels[i / 2] = std::uint8_t((image.pixels[i / 2] & ~(15u << shift)) | (value << shift));
}

struct Object {
    unsigned index = 0, a0 = 0, a1 = 0, a2 = 0;
    int x = 0, y = 0, width = 0, height = 0, box_width = 0, box_height = 0;
};

inline GhostVisualSample capture(const Memory& m) noexcept {
    GhostVisualSample out;
    const auto context = scene(m);
    out.room = context.room;
    if (!context.visible || !m.oam || m.oam_size < 0x400 ||
        !m.vram || m.vram_size < 0x18000 || !m.pal || m.pal_size < 0x400) return out;
    constexpr int dimensions[3][4][2] = {
        {{8,8},{16,16},{32,32},{64,64}},
        {{16,8},{32,8},{32,16},{64,32}},
        {{8,16},{8,32},{16,32},{32,64}}
    };
    std::array<Object, 32> objects{};
    unsigned count = 0;
    int left = 512, top = 256, right = -512, bottom = -256;
    for (unsigned i = 0; i < 128; ++i) {
        const auto* p = m.oam + i * 8;
        const auto a0 = read16(p), a1 = read16(p + 2), a2 = read16(p + 4);
        if (!yoshi_color_detail::player_object(a0, a2) ||
            (!(a0 & 0x100) && (a0 & 0x200)) || (a0 & 255) == 160) continue;
        const unsigned shape = a0 >> 14, size = a1 >> 14;
        if (shape >= 3 || count == objects.size() ||
            ((a0 & 0x1000) && (read16(m.io + 0x4c) & 0xff00))) return out;
        auto& o = objects[count++];
        o = {i, a0, a1, a2, int(a1 & 511), int(a0 & 255),
             dimensions[shape][size][0], dimensions[shape][size][1], 0, 0};
        if (o.x >= 256) o.x -= 512;
        if (o.y >= 160) o.y -= 256;
        const int scale = (a0 & 0x300) == 0x300 ? 2 : 1;
        o.box_width = o.width * scale; o.box_height = o.height * scale;
        left = std::min(left, o.x); top = std::min(top, o.y);
        right = std::max(right, o.x + o.box_width); bottom = std::max(bottom, o.y + o.box_height);
    }
    if (!count || right - left <= 0 || bottom - top <= 0 || right - left > 64 || bottom - top > 64)
        return out;
    GhostImage canvas;
    canvas.width = std::uint8_t(right - left); canvas.height = std::uint8_t(bottom - top);
    const bool one_dimensional = (read16(m.io) & 0x40) != 0;
    unsigned opaque = 0;
    // Lower OAM index wins an OBJ overlap, as in the original renderer.
    for (unsigned n = count; n > 0; --n) {
        const auto& o = objects[n - 1];
        const bool affine = (o.a0 & 0x100) != 0;
        const unsigned group = (o.a1 >> 9) & 31;
        const auto* matrix = m.oam + group * 32;
        const int pa = std::int16_t(read16(matrix + 6)), pb = std::int16_t(read16(matrix + 14));
        const int pc = std::int16_t(read16(matrix + 22)), pd = std::int16_t(read16(matrix + 30));
        for (int y = 0; y < o.box_height; ++y) {
            for (int x = 0; x < o.box_width; ++x) {
                int tx = x, ty = y;
                if (affine) {
                    const int dx = x - o.box_width / 2, dy = y - o.box_height / 2;
                    tx = ((pa * dx + pb * dy) >> 8) + o.width / 2;
                    ty = ((pc * dx + pd * dy) >> 8) + o.height / 2;
                } else {
                    if (o.a1 & 0x1000) tx = o.width - x - 1;
                    if (o.a1 & 0x2000) ty = o.height - y - 1;
                }
                if (tx < 0 || tx >= o.width || ty < 0 || ty >= o.height) continue;
                const unsigned tile = (o.a2 & 1023) + unsigned(ty / 8) *
                    (one_dimensional ? unsigned(o.width / 8) : 32u) + unsigned(tx / 8);
                const unsigned offset = 0x10000 + tile * 32 + unsigned(ty & 7) * 4 + unsigned(tx & 7) / 2;
                if (offset >= m.vram_size) return out;
                const unsigned color = (m.vram[offset] >> ((tx & 1) * 4)) & 15;
                if (!color) continue;
                set_pixel(canvas, unsigned(o.x + x - left), unsigned(o.y + y - top), color);
                ++opaque;
            }
        }
    }
    if (!opaque) return out;
    // Crop transparent borders so translations do not create new atlas poses.
    unsigned x0 = canvas.width, y0 = canvas.height, x1 = 0, y1 = 0;
    for (unsigned y = 0; y < canvas.height; ++y) for (unsigned x = 0; x < canvas.width; ++x)
        if (pixel(canvas, x, y)) { x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x + 1); y1 = std::max(y1, y + 1); }
    if (x0 >= x1 || y0 >= y1) return out;
    out.image.width = std::uint8_t(x1 - x0); out.image.height = std::uint8_t(y1 - y0);
    std::array<bool,16> used{};
    for (unsigned y = y0; y < y1; ++y) for (unsigned x = x0; x < x1; ++x) {
        const unsigned value = pixel(canvas, x, y);
        set_pixel(out.image, x - x0, y - y0, value); used[value] = true;
    }
    for (unsigned i = 1; i < 16; ++i) if (used[i]) out.image.palette[i] = read16(m.pal + 0x2a0 + i * 2) & 0x7fff;
    out.world_x = context.camera_x + left + int(x0);
    out.world_y = context.camera_y + top + int(y0);
    out.visible = true;
    return out;
}

struct Run { std::uint8_t x = 0, y = 0, width = 0, height = 0, color = 0; };
constexpr std::size_t kMaxRuns = 512;

struct Rectangle { float x = 0, y = 0, width = 0, height = 0; };
struct Projection {
    float origin_x = 0, origin_y = 0, scale_x = 0, scale_y = 0;
    float left = 0, top = 0, right = 0, bottom = 0;
    bool valid = false;
};

// Use the drawable's actual image rectangle and independent axis scales:
// the runtime's View-space primitive assumes uniform scale, unlike Stretch.
inline Projection projection(int drawable_width, int drawable_height,
                             int x, int y, int width, int height,
                             int view_width, int view_height,
                             bool pillarbox, bool cinematic) noexcept {
    Projection out;
    if (drawable_width <= 0 || drawable_height <= 0 || width <= 0 || height <= 0 ||
        view_width < 240 || view_width > 4096 || view_height != 160) return out;
    out.scale_x = float(width) / float(view_width);
    out.scale_y = float(height) / float(view_height);
    const int native_left = (view_width - 240) / 2;
    out.origin_x = float(x) + float(native_left) * out.scale_x;
    out.origin_y = float(y);
    out.left = std::max(0.0f, pillarbox ? out.origin_x : float(x));
    out.right = std::min(float(drawable_width), pillarbox ?
        out.origin_x + 240.0f * out.scale_x : float(x) + float(width));
    const float bars = cinematic ? float(height / 10) : 0.0f;
    out.top = std::max(0.0f, float(y) + bars);
    out.bottom = std::min(float(drawable_height), float(y) + float(height) - bars);
    out.valid = out.left < out.right && out.top < out.bottom;
    return out;
}

inline bool project(const Projection& p, const Run& run, std::int32_t world_x,
                    std::int32_t world_y, int camera_x, int camera_y,
                    Rectangle& out) noexcept {
    out = {};
    if (!p.valid) return false;
    const auto sx = std::int64_t(world_x) - camera_x + run.x;
    const auto sy = std::int64_t(world_y) - camera_y + run.y;
    const float left = std::max(p.left, p.origin_x + float(sx) * p.scale_x);
    const float top = std::max(p.top, p.origin_y + float(sy) * p.scale_y);
    const float right = std::min(p.right, p.origin_x + float(sx + run.width) * p.scale_x);
    const float bottom = std::min(p.bottom, p.origin_y + float(sy + run.height) * p.scale_y);
    if (left >= right || top >= bottom) return false;
    out = {left, top, right - left, bottom - top};
    return true;
}

// Merge identical adjacent rows as well as equal horizontal texels. Reject a
// pathological image before drawing anything; never exceed 512 host calls.
inline bool runs(const GhostImage& image, std::array<Run, kMaxRuns>& output,
                 std::size_t& count) noexcept {
    count = 0;
    if (!image.width || image.width > 64 || !image.height || image.height > 64) return false;
    std::array<std::size_t,64> prior{}, current{};
    std::size_t prior_count = 0;
    for (unsigned y = 0; y < image.height; ++y) {
        std::size_t current_count = 0;
        for (unsigned x = 0; x < image.width;) {
            const unsigned color = pixel(image, x, y), start = x++;
            while (x < image.width && pixel(image, x, y) == color) ++x;
            if (!color) continue;
            std::size_t match = count;
            for (std::size_t i = 0; i < prior_count; ++i) {
                const auto& r = output[prior[i]];
                if (r.x == start && r.width == x - start && r.color == color) { match = prior[i]; break; }
            }
            if (match == count) {
                if (count == output.size()) { count = 0; return false; }
                output[count++] = {std::uint8_t(start),std::uint8_t(y),std::uint8_t(x-start),1,std::uint8_t(color)};
            } else ++output[match].height;
            current[current_count++] = match;
        }
        prior = current; prior_count = current_count;
    }
    return count != 0;
}

}  // namespace sma3::ghost_visual_detail
