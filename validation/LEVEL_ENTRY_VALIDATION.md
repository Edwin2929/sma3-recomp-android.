# Short level-entry validation — 2026-10-07

These diagnostic desktop sessions enter levels 1-1, 1-2 and 1-4 through the game's
normal level initializer. A copy of an initialized private state changes only
level ID (03006288), entrance index (03006A52), and game state (03006B05 = 0A).
This is synthetic entry, not a naturally played level transition. ROM, BIOS,
savestates and screenshots remain private.

Each session runs 900 frames: wait 600, right 180, A+right 120. Level and sublevel
IDs are recorded after each action. Compare viewport 240 against 356 using
`compare_panorama_sessions.py`. The action list is
`panorama-level-entry-actions.json`; fixture generator is `make_level_fixture.py`.

| Entry | Central screenshots equal | Visible unresolved frames | Coherence failures |
| --- | --- | --- | --- |
| 1-1 (ID 0) | 3/3 | 0 | 0 |
| 1-2 (ID 1) | 3/3 | 0 | 0 |
| 1-4 (ID 3), after fix | 3/3 | 0 | 0 |

The castle run originally had 41 visible gaps, frames 7459–7499, staging slot 38.
Routine 08084D92 modifies a secondary component after generic emission. The new
observer captures its full world-minus-camera X and predicts the final three OAM
attributes; existing exact matching still guards their use. Unit checks include
+272 and -240, which have identical nine-bit OAM coordinates, plus invalid slots
and mismatched component pointers. ASan and UBSan pass (LSan disabled due to the
execution environment).

CPU0, BUS0, IO_0, AUD0, SAV0 and META match at the end. Only the inactive RTC's
serialized wall-clock seconds (eight bytes) are exempted in BUS0; all guest
memory remains compared. This offset and bus size are pinned to engine
 e7728148c6829ba526f682876430a0c9022dc6c0. PPU0 varies with viewport width;
central pixels are compared instead. Candidate logs include completed static
execution counters with zero dispatch misses, interpreted instructions or healed
native code. The three reports contain aggregate evidence only.

1-1 and 1-2 were recorded before the isolated castle hook; 1-4 was repeated after
it. All tests keep expanded object bounds disabled. Coverage diagnostics apply
only to supported gameplay scenes. These short runs do not qualify complete
levels, transformations, bosses, Android performance, or a new APK. Step 1 remains
open; Android delivery remains 0.9 without experimental widescreen.
