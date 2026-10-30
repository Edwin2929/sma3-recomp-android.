#include "video_scaler.h"
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {
const char* context = "initialization";
int geometry_cases = 0, render_cases = 0;
void check(bool ok, const char* message) {
    if (!ok) {
        std::fprintf(stderr, "FAIL [%s] %s: %s\n", context, message, SDL_GetError());
        std::exit(1);
    }
}
bool equal(const SDL_Rect& a, const SDL_Rect& b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

void geometry() {
    struct Case { const char* name; int w, h, sw, sh; SDL_Rect expected; };
    const Case cases[] = {
        {"GBA native", 240, 160, 240, 160, {0, 0, 240, 160}},
        {"GBA integer scale", 720, 480, 240, 160, {0, 0, 720, 480}},
        {"landscape 16:9", 1920, 1080, 240, 160, {150, 0, 1620, 1080}},
        {"landscape phone", 2400, 1080, 240, 160, {390, 0, 1620, 1080}},
        {"portrait phone", 1080, 2400, 240, 160, {0, 840, 1080, 720}},
        {"square", 1080, 1080, 240, 160, {0, 180, 1080, 720}},
        {"landscape tablet 4:3", 2048, 1536, 240, 160, {0, 85, 2048, 1365}},
        {"portrait tablet", 1536, 2048, 240, 160, {0, 512, 1536, 1024}},
        {"ultrawide", 3440, 1440, 240, 160, {640, 0, 2160, 1440}},
        {"odd landscape", 2341, 1081, 240, 160, {360, 0, 1621, 1081}},
        {"odd portrait", 1081, 2341, 240, 160, {0, 810, 1081, 720}},
        // Choose the limiting axis before rounding. These previously gained a pixel.
        {"rounding boundary native", 241, 160, 240, 160, {0, 0, 240, 160}},
        {"rounding boundary scaled", 1081, 720, 240, 160, {0, 0, 1080, 720}},
        {"rounding boundary odd", 1001, 667, 240, 160, {0, 0, 1000, 667}},
        {"downscale", 120, 80, 240, 160, {0, 0, 120, 80}},
        {"tiny representable", 3, 2, 240, 160, {0, 0, 3, 2}},
        // Less than one raster pixel is intentionally an empty dimension.
        {"tiny unrepresentable", 1, 1, 240, 160, {0, 0, 1, 0}},
        {"largest GBA surface", INT_MAX, INT_MAX, 240, 160,
         {0, 357913941, INT_MAX, 1431655764}},
        {"large surface tall source", INT_MAX, 1080, 160, 240,
         {1073741463, 0, 720, 1080}},
        {"extreme tall source", INT_MAX, 1080, 1, INT_MAX,
         {1073741823, 0, 0, 1080}},
        {"extreme wide source", INT_MAX, 1080, INT_MAX, 1,
         {0, 539, INT_MAX, 1}},
    };
    for (const auto& c : cases) {
        context = c.name;
        const auto r = sma3::video_destination(c.w, c.h, c.sw, c.sh, false);
        check(equal(r, c.expected), "expected aspect-preserving rectangle");
        check(r.x >= 0 && r.y >= 0 && r.w >= 0 && r.h >= 0 &&
              r.w <= c.w && r.h <= c.h && r.x <= c.w - r.w && r.y <= c.h - r.h,
              "contained rectangle, including extreme inputs");
        check(std::abs((c.w - r.w - r.x) - r.x) <= 1 &&
              std::abs((c.h - r.h - r.y) - r.y) <= 1, "centered within one pixel");
        check(equal(sma3::video_destination(c.w, c.h, c.sw, c.sh, true),
                    {0, 0, c.w, c.h}), "stretch fills the entire surface");
        std::printf("PASS geometry %s surface=%dx%d source=%dx%d viewport=%d,%d %dx%d\n",
                    c.name, c.w, c.h, c.sw, c.sh, r.x, r.y, r.w, r.h);
        ++geometry_cases;
    }
    context = "invalid dimensions";
    for (int bad : {0, -1, INT_MIN}) for (int axis = 0; axis < 4; ++axis)
        for (bool stretch : {false, true}) {
            int dims[] = {2400, 1080, 240, 160};
            dims[axis] = bad;
            check(equal(sma3::video_destination(dims[0], dims[1], dims[2], dims[3], stretch),
                        {0, 0, 0, 0}), "invalid dimensions return an empty rectangle");
            ++geometry_cases;
        }
    std::puts("PASS geometry invalid inputs=24");
}

struct Color { Uint8 r, g, b; };
constexpr Color red{255, 0, 0}, green{0, 255, 0}, blue{0, 0, 255};
constexpr Color yellow{255, 255, 0}, white{255, 255, 255}, black{0, 0, 0};
Color pixel(SDL_Surface* surface, int x, int y) {
    check(x >= 0 && x < surface->w && y >= 0 && y < surface->h, "pixel inside surface");
    const auto value = *reinterpret_cast<Uint32*>(
        static_cast<Uint8*>(surface->pixels) + y * surface->pitch + x * 4);
    Uint8 r, g, b, a;
    SDL_GetRGBA(value, surface->format, &r, &g, &b, &a);
    return {r, g, b};
}
void expect_pixel(SDL_Surface* surface, int x, int y, Color expected) {
    const auto actual = pixel(surface, x, y);
    check(actual.r == expected.r && actual.g == expected.g && actual.b == expected.b,
          "interior, image edge, or margin pixel has expected color");
}
SDL_Texture* make_pattern(SDL_Renderer* renderer) {
    auto* source = SDL_CreateRGBSurfaceWithFormat(0, 240, 160, 32, SDL_PIXELFORMAT_RGBA32);
    check(source != nullptr, "source surface");
    const SDL_Rect rects[] = {{0, 0, 120, 80}, {120, 0, 120, 80},
                              {0, 80, 120, 80}, {120, 80, 120, 80}, {100, 60, 40, 40}};
    const Color colors[] = {red, green, blue, yellow, white};
    for (int i = 0; i < 5; ++i)
        check(SDL_FillRect(source, &rects[i], SDL_MapRGBA(source->format,
              colors[i].r, colors[i].g, colors[i].b, 255)) == 0, "fill calibration pattern");
    auto* texture = SDL_CreateTextureFromSurface(renderer, source);
    SDL_FreeSurface(source);
    check(texture != nullptr, "source texture");
    return texture;
}
void verify_pattern(SDL_Surface* image, const SDL_Rect& dst, bool stretch) {
    // Four quadrants verify both axes and source edges. The central 40x40 square
    // measures displayed geometry after the intermediate 720p/1080p buffers.
    expect_pixel(image, dst.x + dst.w / 4, dst.y + dst.h / 4, red);
    expect_pixel(image, dst.x + dst.w * 3 / 4, dst.y + dst.h / 4, green);
    expect_pixel(image, dst.x + dst.w / 4, dst.y + dst.h * 3 / 4, blue);
    expect_pixel(image, dst.x + dst.w * 3 / 4, dst.y + dst.h * 3 / 4, yellow);
    expect_pixel(image, dst.x, dst.y + dst.h / 4, red);
    expect_pixel(image, dst.x + dst.w - 1, dst.y + dst.h / 4, green);
    expect_pixel(image, dst.x + dst.w / 4, dst.y, red);
    expect_pixel(image, dst.x + dst.w / 4, dst.y + dst.h - 1, blue);
    if (dst.x > 0) expect_pixel(image, dst.x - 1, image->h / 2, black);
    if (dst.x + dst.w < image->w) expect_pixel(image, dst.x + dst.w, image->h / 2, black);
    if (dst.y > 0) expect_pixel(image, image->w / 2, dst.y - 1, black);
    if (dst.y + dst.h < image->h) expect_pixel(image, image->w / 2, dst.y + dst.h, black);
    // Half-intensity locates each filtered edge without counting only the
    // shrunken, fully white interior of a linearly filtered square.
    auto is_white = [](Color c) { return c.r >= 128 && c.g >= 128 && c.b >= 128; };
    // Measure inside the square, ten source pixels away from the quadrant
    // boundaries; a filtered blue/yellow boundary could otherwise appear gray.
    const int sample_x = dst.x + dst.w / 2 + dst.w / 24;
    const int sample_y = dst.y + dst.h / 2 + dst.h / 16;
    int square_w = 0, square_h = 0;
    for (int x = dst.x; x < dst.x + dst.w; ++x)
        square_w += is_white(pixel(image, x, sample_y));
    for (int y = dst.y; y < dst.y + dst.h; ++y)
        square_h += is_white(pixel(image, sample_x, y));
    check(std::abs(square_w - dst.w / 6.0) <= 2.0 &&
          std::abs(square_h - dst.h / 4.0) <= 2.0,
          "square follows final destination scale on both axes");
    if (!stretch) check(std::abs(square_w - square_h) <= 2,
                        "square remains square within raster/filter tolerance");
}

void rendering() {
    context = "renderer setup";
    auto* screen = SDL_CreateRGBSurfaceWithFormat(0, 2400, 1080, 32, SDL_PIXELFORMAT_RGBA32);
    check(screen != nullptr, "screen surface");
    auto* renderer = SDL_CreateSoftwareRenderer(screen);
    check(renderer != nullptr, "real SDL software renderer");
    SDL_RendererInfo info{};
    check(SDL_GetRendererInfo(renderer, &info) == 0 &&
          (info.flags & SDL_RENDERER_TARGETTEXTURE), "renderer supports target textures");
    std::printf("INFO renderer=%s target-textures=yes; host validation, no Android device\n", info.name);
    auto* source = make_pattern(renderer);
    sma3::VideoScaler scaler;
    // One renderer/source/scaler survives dimension changes. Target textures allow
    // portrait/landscape transitions without a display server; Android is not run.
    const SDL_Point sizes[] = {{2400, 1080}, {1080, 2400}, {1080, 1080},
        {2048, 1536}, {1536, 2048}, {2341, 1081}, {1081, 2341},
        {241, 160}, {1081, 720}, {240, 160}, {1280, 720}, {1920, 1080}, {2400, 1080}};
    for (unsigned index = 0; index < sizeof(sizes) / sizeof(sizes[0]); ++index) {
        context = "output size change";
        const auto size = sizes[index];
        // First case exercises restoration to the actual screen (nullptr).
        SDL_Texture* canvas = index == 0 ? nullptr : SDL_CreateTexture(renderer,
            SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, size.x, size.y);
        check(index == 0 || canvas != nullptr, "resized output target");
        check(SDL_SetRenderTarget(renderer, canvas) == 0, "select output target");
        int output_w, output_h;
        check(SDL_GetRendererOutputSize(renderer, &output_w, &output_h) == 0 &&
              output_w == size.x && output_h == size.y, "current drawable dimensions");
        auto* readback = SDL_CreateRGBSurfaceWithFormat(0, size.x, size.y, 32, SDL_PIXELFORMAT_RGBA32);
        check(readback != nullptr, "readback surface");
        // Repeated 720/Auto transitions verify recreation/release of cached buffers.
        for (int quality : {0, 720, 1080, 720, 0}) for (bool stretch : {false, true}) {
            char label[100];
            std::snprintf(label, sizeof(label), "%dx%d quality=%d stretch=%d",
                          size.x, size.y, quality, stretch);
            context = label;
            check(SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255) == 0 &&
                  SDL_RenderClear(renderer) == 0, "clear output margins");
            const auto dst = sma3::video_destination(output_w, output_h, 240, 160, stretch);
            check(scaler.draw(renderer, source, 240, 160, dst, quality), "render pipeline");
            check(SDL_GetRenderTarget(renderer) == canvas, "restored original render target");
            const bool needs_output = quality &&
                (dst.w != quality * 16 / 9 || dst.h != quality);
            if (needs_output) {
                int w, h;
                check(SDL_QueryTexture(scaler.output_texture, nullptr, nullptr, &w, &h) == 0,
                      "query HD buffer");
                check(w == quality * 16 / 9 && h == quality, "720p/1080p intermediate buffer");
            } else check(scaler.output_texture == nullptr && scaler.ow == 0 && scaler.oh == 0,
                         "automatic or exact-size output releases the redundant HD buffer");
            check(SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32,
                  readback->pixels, readback->pitch) == 0, "read final displayed pixels");
            verify_pattern(readback, dst, stretch);
            std::printf("PASS render %s buffer=%dx%d viewport=%d,%d %dx%d pattern=ok\n",
                        label, scaler.ow, scaler.oh, dst.x, dst.y, dst.w, dst.h);
            ++render_cases;
        }
        context = "invalid render destination";
        check(!scaler.draw(renderer, source, 240, 160, {0, 0, 0, 160}, 0), "reject zero width");
        check(!scaler.draw(renderer, source, 240, 160, {0, 0, 240, 0}, 0), "reject zero height");
        check(!scaler.draw(renderer, source, 0, 160, {0, 0, 240, 160}, 0), "reject invalid source");
        check(SDL_GetRenderTarget(renderer) == canvas, "invalid draw preserves output target");
        check(SDL_SetRenderTarget(renderer, nullptr) == 0, "release output target");
        SDL_DestroyTexture(canvas);
        SDL_FreeSurface(readback);
    }
    scaler.reset();
    check(scaler.integer_texture == nullptr && scaler.output_texture == nullptr &&
          scaler.iw == 0 && scaler.ih == 0 && scaler.ow == 0 && scaler.oh == 0, "scaler reset");
    SDL_DestroyTexture(source);
    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(screen);
}
} // namespace

int main() {
    check(SDL_Init(0) == 0, "SDL init");
    SDL_version version;
    SDL_GetVersion(&version);
    std::printf("INFO SDL runtime=%u.%u.%u GBA=240x160 aspect=3:2\n",
                version.major, version.minor, version.patch);
    geometry();
    rendering();
    SDL_Quit();
    std::printf("PASS summary geometry=%d render=%d\n", geometry_cases, render_cases);
}
