#pragma once

#include "touch_appearance.h"
#include "host_overlay.h"
#include <array>
#include <cstddef>
#include <cstring>

namespace sma3::touch {

using gbarecomp::HostOverlay;
using gbarecomp::OverlayColor;
using gbarecomp::OverlayPoint;
constexpr auto space = gbarecomp::OverlaySpace::Drawable;

struct Box { float x, y, w, h; };

inline void glyph_line(HostOverlay& overlay, Box box, float x0, float y0,
                       float x1, float y1, float thickness, OverlayColor color) {
    overlay.line(box.x + x0 * box.w, box.y + y0 * box.h,
                 box.x + x1 * box.w, box.y + y1 * box.h, thickness, color, space);
}

template<std::size_t N>
inline void glyph_path(HostOverlay& overlay, Box box,
                       const std::array<OverlayPoint, N>& points,
                       float thickness, OverlayColor color) {
    std::array<OverlayPoint, N> mapped{};
    for (std::size_t i = 0; i < N; ++i)
        mapped[i] = {box.x + points[i].x * box.w, box.y + points[i].y * box.h};
    overlay.polyline(mapped.data(), mapped.size(), thickness, color, space);
}

// Small vector labels, deliberately independent of SDL's unavailable font
// stack on Android. The editor mirrors these glyphs in TouchAppearance.java.
inline void letter(HostOverlay& overlay, char character, Box box,
                   float thickness, OverlayColor color) {
    auto line = [&](float a, float b, float c, float d) {
        glyph_line(overlay, box, a, b, c, d, thickness, color);
    };
    switch (character) {
    case 'A':
        line(0, 1, .5f, 0); line(.5f, 0, 1, 1); line(.23f, .58f, .77f, .58f); break;
    case 'B':
        line(0, 0, 0, 1); line(0, 0, .72f, 0); line(.72f, 0, 1, .2f);
        line(1, .2f, .72f, .5f); line(.72f, .5f, 0, .5f);
        line(.72f, .5f, 1, .75f); line(1, .75f, .72f, 1); line(.72f, 1, 0, 1); break;
    case 'C':
        line(1, .1f, .25f, .1f); line(.25f, .1f, 0, .3f);
        line(0, .3f, 0, .7f); line(0, .7f, .25f, .9f); line(.25f, .9f, 1, .9f); break;
    case 'E':
        line(0, 0, 0, 1); line(0, 0, 1, 0); line(0, .5f, .8f, .5f); line(0, 1, 1, 1); break;
    case 'L': line(0, 0, 0, 1); line(0, 1, 1, 1); break;
    case 'R':
        line(0, 0, 0, 1); line(0, 0, .72f, 0); line(.72f, 0, 1, .2f);
        line(1, .2f, .72f, .5f); line(.72f, .5f, 0, .5f); line(.5f, .5f, 1, 1); break;
    case 'S':
        line(1, .1f, .2f, .1f); line(.2f, .1f, 0, .28f); line(0, .28f, .2f, .5f);
        line(.2f, .5f, .8f, .5f); line(.8f, .5f, 1, .72f);
        line(1, .72f, .8f, .9f); line(.8f, .9f, 0, .9f); break;
    case 'T': line(0, 0, 1, 0); line(.5f, 0, .5f, 1); break;
    default: break;
    }
}

inline void label(HostOverlay& overlay, const char* text, Box box, OverlayColor color) {
    const std::size_t count = std::strlen(text);
    if (!count || box.w <= 0 || box.h <= 0) return;
    const float height = std::min(box.h * .52f, box.w * .78f / (count * .78f - .18f));
    const float width = height * .60f;
    const float advance = height * .78f;
    const float x = box.x + (box.w - (advance * count - height * .18f)) * .5f;
    const float y = box.y + (box.h - height) * .5f;
    const float thickness = std::max(.65f, height * .105f);
    for (std::size_t i = 0; i < count; ++i)
        letter(overlay, text[i], {x + advance * i, y, width, height}, thickness, color);
}

inline void symbol(HostOverlay& overlay, int id, Box bounds, OverlayColor color) {
    const float side = std::min(bounds.w, bounds.h) * .62f;
    if (side <= 0) return;
    const Box box{bounds.x + (bounds.w - side) * .5f,
                  bounds.y + (bounds.h - side) * .5f, side, side};
    const float thickness = std::max(.75f, side * .105f);
    auto line = [&](float a, float b, float c, float d) {
        glyph_line(overlay, box, a, b, c, d, thickness, color);
    };
    auto dot = [&](float x, float y, float radius) {
        overlay.fill_circle(box.x + x * side, box.y + y * side, radius * side, color, space);
    };
    switch (id) {
    case 1:  // A: jump / hold to flutter.
        line(.5f, .12f, .5f, .76f); line(.22f, .4f, .5f, .12f);
        line(.5f, .12f, .78f, .4f); line(.16f, .92f, .84f, .92f); break;
    case 2: {  // B: tongue / eat / spit.
        constexpr std::array<OverlayPoint, 9> tongue{{
            {.12f,.22f},{.12f,.68f},{.26f,.80f},{.76f,.80f},{.90f,.66f},
            {.90f,.52f},{.78f,.40f},{.26f,.40f},{.26f,.22f}}};
        glyph_path(overlay, box, tongue, thickness, color);
        line(.53f, .43f, .53f, .63f);
        break;
    }
    case 3:  // L: lock / unlock egg aiming cursor.
        overlay.stroke_circle(box.x + side * .5f, box.y + side * .5f,
                              side * .28f, thickness, color, space);
        line(.5f, .02f, .5f, .24f); line(.5f, .76f, .5f, .98f);
        line(.02f, .5f, .24f, .5f); line(.76f, .5f, .98f, .5f);
        dot(.5f, .5f, .055f); break;
    case 4: {  // R: aim / throw a spotted egg.
        constexpr std::array<OverlayPoint, 13> egg{{
            {.5f,.06f},{.34f,.10f},{.22f,.24f},{.14f,.46f},{.14f,.70f},
            {.25f,.86f},{.5f,.94f},{.75f,.86f},{.86f,.70f},{.86f,.46f},
            {.78f,.24f},{.66f,.10f},{.5f,.06f}}};
        glyph_path(overlay, box, egg, thickness * .8f, color);
        dot(.38f, .40f, .075f); dot(.65f, .63f, .09f); dot(.4f, .77f, .055f);
        break;
    }
    case 5:  // Select: status screen.
        for (float y : {.22f, .5f, .78f}) {
            dot(.16f, y, .055f); line(.35f, y, .9f, y);
        }
        break;
    case 6:  // Start: pause.
        overlay.fill_rect(box.x + .2f * side, box.y + .1f * side,
                          .2f * side, .8f * side, 0, color, space);
        overlay.fill_rect(box.x + .6f * side, box.y + .1f * side,
                          .2f * side, .8f * side, 0, color, space);
        break;
    default: break;
    }
}

template<class Layout>
inline void draw_pad(HostOverlay& overlay, const Layout& pad, std::uint16_t held,
                     float mm, TouchAppearance appearance) {
    const auto alpha = touch_alpha(appearance);
    const OverlayColor backing{20, 35, 27, alpha};
    const OverlayColor outline{226, 242, 222, alpha};
    const OverlayColor pressed{169, 222, 99, alpha};
    const OverlayColor ink{255, 255, 255, alpha};
    const float line = std::max(1.0f, .38f * mm);
    auto on = [held](int bit) { return (held & (1u << bit)) != 0; };
    const float arm = pad.dpad.r, thick = pad.dpad_thick;
    const float cx = pad.dpad.x, cy = pad.dpad.y, half = thick * .5f;
    // Non-overlapping cross pieces keep the backing uniformly translucent.
    overlay.fill_rect(cx - half, cy - half, thick, thick, 0, backing, space);
    struct Arm { int bit; float x, y, w, h; };
    const Arm arms[]{{6,cx-half,cy-arm,thick,arm-half},
                     {7,cx-half,cy+half,thick,arm-half},
                     {5,cx-arm,cy-half,arm-half,thick},
                     {4,cx+half,cy-half,arm-half,thick}};
    for (const auto& part : arms)
        overlay.fill_rect(part.x, part.y, part.w, part.h, 0,
                          on(part.bit) ? pressed : backing, space);
    const std::array<OverlayPoint, 13> edge{{
        {cx-half,cy-arm},{cx+half,cy-arm},{cx+half,cy-half},
        {cx+arm,cy-half},{cx+arm,cy+half},{cx+half,cy+half},
        {cx+half,cy+arm},{cx-half,cy+arm},{cx-half,cy+half},
        {cx-arm,cy+half},{cx-arm,cy-half},{cx-half,cy-half},{cx-half,cy-arm}}};
    overlay.polyline(edge.data(), edge.size(), line, outline, space);
    const float arrow = std::min(thick * .22f, (arm - half) * .3f);
    const float offset = half + (arm - half) * .58f;
    const float stroke = std::max(.75f, arrow * .35f);
    for (int direction = 0; direction < 4; ++direction) {
        const float dx = direction == 2 ? -1.f : direction == 3 ? 1.f : 0.f;
        const float dy = direction == 0 ? -1.f : direction == 1 ? 1.f : 0.f;
        const float x = cx + dx * offset, y = cy + dy * offset;
        overlay.line(x - dx*arrow - dy*arrow, y - dy*arrow + dx*arrow,
                     x + dx*arrow, y + dy*arrow, stroke, ink, space);
        overlay.line(x + dx*arrow, y + dy*arrow,
                     x - dx*arrow + dy*arrow, y - dy*arrow - dx*arrow, stroke, ink, space);
    }
    constexpr const char* labels[]{"", "A", "B", "L", "R", "SELECT", "START"};
    auto draw_glyph = [&](int id, Box box) {
        if (appearance.style == TouchStyle::Symbols) symbol(overlay, id, box, ink);
        else label(overlay, labels[id], box, ink);
    };
    auto round = [&](const auto& circle, int id, int bit, OverlayColor tint) {
        overlay.fill_circle(circle.x, circle.y, circle.r, on(bit) ? pressed : tint, space);
        overlay.stroke_circle(circle.x, circle.y, circle.r, line, outline, space);
        draw_glyph(id, {circle.x-circle.r,circle.y-circle.r,2*circle.r,2*circle.r});
    };
    round(pad.a, 1, 0, {177,49,65,alpha});
    round(pad.b, 2, 1, {36,105,171,alpha});
    auto pill = [&](const auto& rectangle, int id, int bit) {
        overlay.fill_rect(rectangle.x, rectangle.y, rectangle.w, rectangle.h,
                          rectangle.h*.25f, on(bit) ? pressed : backing, space);
        overlay.stroke_rect(rectangle.x, rectangle.y, rectangle.w, rectangle.h,
                            rectangle.h*.25f, line, outline, space);
        draw_glyph(id, {rectangle.x,rectangle.y,rectangle.w,rectangle.h});
    };
    pill(pad.l, 3, 9); pill(pad.r, 4, 8);
    pill(pad.select, 5, 2); pill(pad.start, 6, 3);
}

}  // namespace sma3::touch
