# Car complete-cycle validation — 2026-10-09

## Status: public checks passed; revised native route NOT yet executed

The 2026-10-08 result is a useful differential reference, **not** evidence of a
route ending in normal Yoshi. Its 3,360 frames, 38 central images and observed
`0 -> 2 -> 0 -> 2` sequence are retained without relabeling them as a complete cycle.

- `panorama-car-actions-reference.json` is a byte-identical copy of the former
  `panorama-car-actions.json` (Git blob `85b0ec9ecebabccd477267ed891b37c2b09fe9e5`).
- `widescreen-car.json` is unchanged (Git blob
  `367ff788ef54d39188bf6ae33d1eb51b1dadb012`). It still reports the OLD route.
- `widescreen-car-cycle-status.json` records the actual public checks and the
  unexecuted private phase. Its native measurements and pass flags are `null`,
  not invented successes or reused historical measurements.

The base revision is `3e0be96b273adf30bd46d8e7cf260adf7d34dcce`. This change affects
validation only. No Android source, APK, version, release, renderer, game logic,
collision, timing or native object observer is modified. **Android stays 0.9.**

## Revised input route

The active `panorama-car-actions.json` preserves the first seven actions:
entry wait, bubble approach, jump/activation, car movement and wheel extension.
The old 90-frame leftward return (`keys=991`) becomes a rightward movement
(`keys=1007`), avoiding a deliberate drive back toward the bubble.

Next, a bounded conditional idle action requires that the current mode is car
(`from_transformation=2`). It advances one frame at a time, stopping on the FIRST
observed normal frame (`until_transformation=0`), with a maximum of 2,400 frames.
The next input is 90 frames to the right, followed by two 600-frame waits with all
buttons released (`keys=1023`). The maximum session length is **5,250 frames**;
the actual length depends on when recovery occurs. A missed starting mode or a
recovery timeout fails explicitly, rather than silently skipping the condition.

This is a **candidate escape route, not a calibrated gameplay result**. Obstacles,
return-animation input lockout or a different recovery location can make it fail.
The private run must establish that the movement actually separates Yoshi from
the bubble. No transformation field is written, no object is removed, and no
saved state is restored mid-session to force normal form.

The JSON remains a list of actions. Legacy `{n, keys}` actions retain their
1..600-frame limit and default batched execution. Conditional actions additionally
require both transformation fields; their budget may be larger, while the
whole-session 6,000-frame and 200-action limits still apply. With per-frame tracing,
`session.json` records actual executed durations and `actions.json` retains the
requested budgets. Both presentations must execute exactly the same actual inputs.

## Acceptance contract

Use `car-cycle-expectations.json` in BOTH runs and in the comparator. It binds the
reviewed action list by canonical JSON SHA-256 and requires:

1. Exactly `0 -> 2 -> 0` in the complete, contiguous per-frame transformation trace,
   including the initial observation. A one-frame reactivation fails even if the
   final mode subsequently returns to zero.
2. Normal Yoshi at the end, with at least **1,200 consecutive final frames** in
   mode 0 / gameplay state 13 / no buttons pressed. Level 18 and sublevel 119
   (`0x77`) must remain unchanged after the initial 600-frame entry warm-up.
3. At least 32 pixels of measured rightward displacement from the recovery point
   to the final position. This rejects ineffective movement; it is NOT a bubble
   hitbox calculation. The idle window establishes bounded non-reactivation,
   not permanent safety for all future inputs.
4. Identical inputs, frame counters, transformation fields, gameplay states,
   levels, player coordinates and velocities between control and candidate on
   every frame. Missing observations, resets and mismatched checkpoints fail.
5. Byte-identical 240x160 central RGB images from the 240-wide control and the
   center of the 356-wide candidate. After warm-up, captures are mandatory every
   60 frames, at each transformation change, and at every action endpoint.
   Image equality is sampled at those checkpoints, not claimed for every frame.
6. Equal CPU, guest memory, I/O, audio, save and metadata sections at EACH capture
   checkpoint and at the final state. The saved gameplay fields must also agree
   with the corresponding trace. PPU viewport state remains excluded; the
   existing guarded exception covers only eight inactive-RTC wall-clock bytes.
7. Zero raw object-gap reports, visible unresolved positions and incoherent
   submissions for complete-cycle acceptance, plus zero static dispatch misses,
   interpreted instructions and healed-native counts. Missing or malformed
   diagnostic records fail instead of being treated as clean evidence.

`cycle_pass` measures the gameplay contract. `differential_pass` and
`coverage_pass` retain their separate meanings. `complete_cycle_pass` requires
all of them AND zero raw gaps. Comparing historical sampled sessions without a
cycle contract leaves the cycle fields `null`; a generic differential pass does
not imply a completed transformation cycle. Checks use explicit exceptions and
remain active under `python -O`.

The runner writes private `trace.json`, `captures.json`, checkpoint states and
input/build fingerprints. On failure it retains partial evidence and a failure
status. Incomplete executions cannot qualify. Session directories must be empty,
and the comparator refuses to overwrite an existing report.

## Checks actually executed in this environment

Python/Pillow and the fetched public validation sources were available. The
supported ROM, BIOS, initialized private gameplay state and compiled native
`build/sma3_runner` were not available in the execution environment.

- `python -m unittest discover -s validation -p test_car_cycle.py -v`: **21 passed**.
- `python -O -m unittest discover -s validation -p test_car_cycle.py -v`: **21 passed**.
- `py_compile` for the runner, comparator, shared checks and test module: passed.
- Reference-action and historical-report Git blob identity checks: passed.
- Native preflight with missing assets: rejected with exit code 2 before creating
  a session directory; this is NOT a successful native execution.

The tests use a clearly labeled input-aware protocol stub, generated solid-color
images and synthetic GBAS containers. They test validator behavior, not the game.
They exercise successful and failed cycles, immediate post-recovery input,
timeouts, one-frame reactivation, ineffective escape, too-short final waits,
intermediate unsupported states, missing frames, input mismatches, corrupted
central images, intermediate/final saved-state mismatches, guarded RTC handling,
raw/visible/incoherent position diagnostics and report overwrite protection.
**No new real central-image count, object-coverage result or native cycle pass is
claimed.** The native build, sanitizer reruns and Android performance tests were
not performed. The prior native results below remain historical evidence only.

## Reproduce the private phase

Use the repository's existing native build procedure and legally supplied assets.
Keep `PRIVATE` outside the checkout. `BASE_STATE` must be an initialized normal-
gameplay source accepted by the existing fixture generator. Use the same build,
ROM, BIOS, fixture and configuration for both widths, and fresh output directories.
The variables below are local private paths, not files to upload to GitHub.

```sh
# ROM, BIOS, BASE_STATE and PRIVATE must already name your local private files/folder.
python validation/make_morph_fixture.py \
  "$BASE_STATE" "$PRIVATE/car-cycle-entry.state" --vehicle car

python validation/run_panorama_session.py \
  --rom "$ROM" --bios "$BIOS" --state "$PRIVATE/car-cycle-entry.state" \
  --actions validation/panorama-car-actions.json \
  --expectations validation/car-cycle-expectations.json \
  --width 240 --output "$PRIVATE/car-cycle-control"

python validation/run_panorama_session.py \
  --rom "$ROM" --bios "$BIOS" --state "$PRIVATE/car-cycle-entry.state" \
  --actions validation/panorama-car-actions.json \
  --expectations validation/car-cycle-expectations.json \
  --width 356 --output "$PRIVATE/car-cycle-wide"

python validation/compare_panorama_sessions.py \
  "$PRIVATE/car-cycle-control" "$PRIVATE/car-cycle-wide" \
  "$PRIVATE/widescreen-car-cycle.json" \
  --expectations validation/car-cycle-expectations.json
```

Only a real result with `complete_cycle_pass: true` closes the private phase.
If the route fails, inspect the private trace and captures, adjust the input
route and update its digest without weakening the sequence, final-idle or
coverage gates. Re-run BOTH widths from the same initial fixture, not from the
failed final state. Never replace `widescreen-car.json` with new-route evidence.
Publish only the new aggregate after reviewing it; ROM, BIOS, screenshots,
savestates, raw traces and private paths remain private.

To reproduce the OLD test, use `panorama-car-actions-reference.json` without
`--expectations` or `--trace-every-frame`, in separate fresh directories. This
preserves its 38 action-endpoint samples; it does not turn it into a complete
cycle test. Other transformations, bosses, whole levels and Android remain out
of scope. Lateral spawning bounds remain disabled.

---

## Historical record — 2026-10-08 (unchanged findings)

The archived text below describes the reference action file, not the revised
active route. Its original native results have not been rerun here.

`make_morph_fixture.py --vehicle car` creates a private synthetic screen-exit entry
near the original AF bubble at 081CD320, sublevel 77, tile (17,88). The entry is
at (13,88), with diagnostic level ID 18 (its main entrance loads sublevel 0F,
which groups this room in the disassembly). The fixture loads original level
and sprite data; it never writes the transformation field or modifies the ROM.
The earlier helicopter command remains a compatible wrapper and produces the
same byte-identical helicopter fixture.

`panorama-car-actions-reference.json` runs 3,360 frames. Yoshi collects the original bubble,
becomes the car, drives right, extends the wheels with jump input and drives back.
The final wait is sampled every 60 frames. The observed mode sequence is
0 → 2 → 0 → 2: normal Yoshi is sampled from 8819 through 9179, then the nearby
bubble causes another transformation. This is not a run ending in normal form.
No reset or static-dispatch failure occurs in this route.

Changes:

- 080A4B0E: capture the complete signed X of each of fourteen ring particles
  from its trigonometric displacement and sprite anchor, before the OAM mask.
- 080424F0: preserve exact position metadata when the car's body is copied to
  another staging slot; reject stale attributes and clear the old observation.
- 08042608 / 0804273E: capture both pairs and the repeated extending wheel
  supports. Internal loop branches can remain within one recompiled function,
  so an observer predicts the remaining loop iterations, within bounded capacity.

All observations still require the exact final attribute triple at submission
and display. No game logic, timing, collision, sprite spawning or guest state is
changed. Temporary writer tracing was removed. ASan/UBSan unit checks cover signed
coordinates, wrapped-X ambiguity, body copies, stale attributes, flips, paired
supports, multiple layers and invalid addresses/capacity. LSan is disabled due
to the execution environment.

The final candidate `private-car-loops` and control `private-car-control` have
38 byte-identical central images. CPU, guest memory, I/O, audio and save state
match under the documented inactive-RTC-clock exception; viewport PPU state is
excluded and central pixels compared instead. There are zero raw/visible position
gaps or incoherent submissions in supported gameplay. Aggregate evidence is
`widescreen-car.json`; input checkpoints are all gameplay state 13.

This validates one synthetic-entry car route. Together with the separately
validated helicopter cycle it expands transformation coverage, but does not
qualify mole tank, train, submarine, bosses, whole levels or all animations.
Lateral spawning bounds remain disabled. Android remains 0.9; no widescreen APK
or Android performance qualification is produced. ROM, BIOS, screenshots and
savestates stay private.
