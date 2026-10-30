# Presentation work reduction — 0.10.12

Baseline: published 0.10.11 commit `42b7b6e463a9fdfe258e4a1cd3ea260b6235276e`.
These changes remove redundant presentation work and preserve displayed pixels.
They do **not** establish 120 FPS on an Android device. Results, source hashes,
and all timing rows are in [render-0.10.12-results.json](render-0.10.12-results.json).

## Changes

`HostWindow::present_paced` previously uploaded the native RGB24 framebuffer
even when Scale2x/Scale3x subsequently uploaded and drew a separate filtered
texture. The filtered path now performs just its required upload. It takes one
filter snapshot for the entire presentation, so a concurrent settings change
cannot select the native texture after its upload was skipped. If allocation,
upload, or rendering of the filtered image fails, the current native image is
uploaded before the existing fallback draws it. Texture recreation keeps this
behavior.

`VideoScaler::draw` also skips the final output texture when the destination
already exactly equals the selected 1280 × 720 or 1920 × 1080 surface. That
final copy was 1:1. The preceding operation now draws directly to the same
destination; the user's quality and filter settings are unchanged. Destinations
that actually require the intermediate resampling retain it.

The opt-in host diagnostic build can exercise the Android geometry and scaler
without enabling Android JNI, insets, or other platform APIs. Ordinary desktop
builds retain their existing presentation route.

## Correctness and work counts

The old and new actual `HostWindow` implementations produced **568 identical
complete RGBA outputs**. Every one of **4,564,684,800 output bytes** was compared
before swapping buffers. SHA-256 hashes were retained as additional evidence;
the equality check was a full byte comparison.

The matrix covered native 240 × 160 and panoramic 356 × 160 sources; 960 × 540,
2400 × 1080, 1080 × 2400, 3120 × 1440, 1280 × 720, and 1920 × 1080 outputs; all
seven filters; Auto/720p/1080p quality; fit/stretch; hidden and visible controls;
held-button variants; changes between real grass, Fuzzy, and castle images;
filter switches; texture recreation; and injected filtered-upload,
target-allocation, and target-binding failures. Native-width test inputs were
centered crops of those prior panoramic screenshots, with no resampling.

| Verified case | Before | After |
| --- | ---: | ---: |
| Successful Scale2x/Scale3x presentation | 2 texture uploads | 1 texture upload |
| Exact 720p/1080p, Balanced/Scale2x/Scale3x/CRT/cinematic | 3 render copies | 2 render copies |
| Exact 720p/1080p, nearest/bilinear | 2 render copies | 1 render copy |
| Noneligible copy route | Same calls | Same calls |

Across the matrix, 170 unnecessary native uploads and 14 final copies were
eliminated. Failed filters still displayed the fresh fallback image.

Public regression checks passed:

- `presentation_upload_test.cpp`: **699 checks**, including source immutability,
  a settings change during rendering, three injected failures, subsequent
  recovery, and recreation of streaming/target textures.
- `video_scaler_test.cpp`: **45 geometry cases and 130 render cases**, including
  exact 720p/1080p output, buffer release, restored render targets, aspect ratio,
  image boundaries, and exterior margins.

## Timing evidence and limits

Measurements used SDL software rendering on the Linux host and thread CPU time,
with both versions pinned to the same logical CPU. Upload measurements used an
ABBA order, two rounds of 60 changing frames per case in each process. Copy
measurements alternated the versions over five rounds of five frames.

The removed native upload itself consumed median **24.6–37.9 microseconds** per
Scale2x/Scale3x presentation in these runs. Its call count becomes zero on the
successful filtered path. Whole-presentation upload-benchmark totals were noisy:
even the unchanged nearest control moved from 203.5 to 292.1 microseconds at
240 pixels wide. Those totals do not support an overall FPS improvement claim.

Representative copy-path CPU medians, in milliseconds:

| Exact destination | Filter | Before | After |
| --- | --- | ---: | ---: |
| 1280 × 720 | Balanced | 15.894 | 16.913 |
| 1280 × 720 | Nearest | 10.829 | 8.702 |
| 1280 × 720 | Bilinear | 5.170 | 4.608 |
| 1920 × 1080 | Balanced | 30.456 | 32.317 |
| 1920 × 1080 | Nearest | 21.160 | 18.729 |
| 1920 × 1080 | Bilinear | 11.465 | 8.283 |

The software renderer's rasterization and format-conversion costs produce mixed
total results despite fewer calls. The change is retained for the verified
removal of a redundant target and copy, with exact displayed output; these host
times must not be presented as Android GPU measurements or a universal speedup.
The full table, including unchanged-route controls and all seven filters, is in
the JSON results.

The audit also reviewed frame blending, color profiles, Yoshi recoloring, and
touch overlays. Existing filter buffers and color-profile fast paths already
reuse their allocations. The earlier
[touch-control investigation](CONTROLS_0.10.6_PERFORMANCE.md) measured small
enqueue costs and explains why SDL API call counts cannot be equated with GPU
draw calls. This change does not add another control-geometry cache or alter
the selected image filter to obtain a favorable frame-rate number.

## Reproduce the focused regressions

From the project root with the sibling engine and SDL2 development files:

```sh
c++ -std=c++20 -O2 -ffunction-sections -fdata-sections -pthread \
  -DGBARECOMP_HAVE_SDL2=1 -DSMA3_FRAME_INTERPOLATION=1 \
  -DSMA3_MOD_PROFILES=1 -DSMA3_PRESENTATION_DIAGNOSTICS=1 \
  -I src -I ../gbarecomp/src/runtime \
  validation/presentation_upload_test.cpp \
  ../gbarecomp/src/runtime/host_platform.cpp \
  ../gbarecomp/src/runtime/touch_input.cpp \
  ../gbarecomp/src/runtime/color_lut.cpp \
  -Wl,--gc-sections $(pkg-config --cflags --libs sdl2) \
  -o /tmp/presentation-upload-test
/tmp/presentation-upload-test

c++ -std=c++20 -O2 -I src validation/video_scaler_test.cpp \
  $(pkg-config --cflags --libs sdl2) -o /tmp/video-scaler-test
/tmp/video-scaler-test
```
