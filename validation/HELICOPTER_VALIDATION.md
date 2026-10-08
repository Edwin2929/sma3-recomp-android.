# Helicopter cycle — 2026-10-07

A reproducible private fixture enters sublevel 3B of 1-2 near its original sprite
B1 (helicopter bubble), found in the ROM's sprite data at 081C2E1C. The fixture
uses the original screen-exit loader with a synthetic exit at tile (63,70).
`make_helicopter_fixture.py` changes four IWRAM fields and six exit-table bytes
in a copy of an initialized state. It does not set Yoshi's transformation, add a
bubble, or alter ROM data. This is synthetic entry, not a naturally played journey
through the preceding rooms.

`panorama-helicopter-actions.json` collects the bubble through movement and a jump,
waits for the animation, flies right, up and left, then waits on the starting
platform for reversion to normal Yoshi. The 4,260-frame session samples the sequence
0 → 6 → 0 in the actual transformation field (03006DB2), always with gameplay state
13 at the sampled checkpoints. Rightward and upward movement occur while mode 6
is active. The normal form is sampled again at frame 9059 and remains active
through frame 10859. No reset or static dispatch failure occurs in this route.

The first 32 coverage gaps occurred during entry, before collection. A scale
animation modifies an otherwise cleared OAM staging slot (A0=160, A1=0). Its zero
X has no world-position provenance. The observer now explicitly preserves native
clipping for this exact producer and final attribute triple. This is a known
native-only artifact, not a recovered world position or permission to extend it
into the margins. Unknown or mismatched objects retain conservative fallback.
Unit checks verify both classification and rejection of changed attributes.

The full continuous wide run is `private-helicopter-return`, compared against
`private-helicopter-return-control` (240 pixels). The aggregate report is
`widescreen-helicopter.json`. The driver now records transformation state after
each action; the comparator reports its observed sequence. State comparisons
retain the existing eight-byte inactive-RTC clock exception and viewport-specific
PPU exclusion, with central pixels checked separately.

Exploratory routes that missed the bubble or flew over a pit were not successful
cycle evidence. Reloading a mid-transformation state also loses observer history
for the first frames; the final comparison uses a continuous run from entry.

This validates one helicopter collection/flight/reversion route on the desktop
runner. Other transformations, bosses, broader levels, lateral spawning bounds,
Android integration and performance remain unqualified. Android release stays at
0.9; no new widescreen APK is produced. ROM, BIOS, savestates and images stay private.
