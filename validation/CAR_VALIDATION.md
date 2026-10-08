# Car route — 2026-10-08

`make_morph_fixture.py --vehicle car` creates a private synthetic screen-exit entry
near the original AF bubble at 081CD320, sublevel 77, tile (17,88). The entry is
at (13,88), with diagnostic level ID 18 (its main entrance loads sublevel 0F,
which groups this room in the disassembly). The fixture loads original level
and sprite data; it never writes the transformation field or modifies the ROM.
The earlier helicopter command remains a compatible wrapper and produces the
same byte-identical helicopter fixture.

`panorama-car-actions.json` runs 3,360 frames. Yoshi collects the original bubble,
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
