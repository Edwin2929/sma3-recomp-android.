# Extended castle route — 2026-10-07

The synthetic 1-4 entry fixture now has a 1,920-frame route of repeated right,
jump and tongue inputs (`panorama-route-actions.json`). This is a bounded route,
not completion of the castle. Yoshi reaches X=1872.996 and remains blocked there;
repeated actions after that point exercise nearby objects rather than add distance.

Before these changes, 1,138 supported-gameplay frames contained visible objects
with unresolved positions. Intermediate fixes reduced that count to 736, 551 and
37. The final run has zero unresolved frames and zero incoherent submissions.
All 36 central 240×160 screenshots match the native-width control byte for byte.
Final CPU, guest memory, I/O, audio and save state match under the existing narrow
inactive-RTC clock exception. Logs finish with zero dispatch misses, interpreted
instructions or healed native code.

`src/widescreen_rotating.h` adds observations for:

- 0809E324 / 0809E774: three- and six-component groups, preserving full X before
  guest clamping and applying the exact subsequent affine attributes.
- 08063B52 / 08063D32: two scale-animation paths, with their 16-pixel origin shift
  and doubled affine bounds. The two paths use different owner registers and
  different intermediate Y values.
- 0809D49E: the third component of an owner sprite, including the guest's observed
  IWRAM mirror writes when the allocation is FFFF. Only this known producer's
  address is normalized; the global raw reader is unchanged.

Observations still require exact final OAM attribute matches before presentation.
No game state, timing, sprite allocation or collision code is changed. Temporary
writer tracing was removed. ASan/UBSan tests cover signed coordinates, guest clamp
boundaries, affine groups, stale metadata, invalid pointers, capacity, both scale
paths and mirror addressing; LeakSanitizer is disabled due to environment limits.

The comparison helper now rejects malformed/truncated gap records and reports the
sampled game states explicitly. Coverage diagnostics apply only to supported
normal gameplay (state 13); the entry transition (state 12) is not qualified by
absence of gap messages. Center comparisons include both states.

The final run is `private-castle-route-complete`; its control is
`private-castle-route-control`. Aggregate evidence is `widescreen-castle-route.json`.
Assets, images and states remain private. The two 900-frame 1-1 / 1-2 entry checks
are repeated after these hooks; their results are recorded separately.

Step 1 is still open for transformations, bosses and broader level coverage.
Expanded object spawning bounds remain disabled. Android stays at 0.9 without
experimental widescreen; this desktop validation does not establish APK readiness
or 120 FPS performance.
