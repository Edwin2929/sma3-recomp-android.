# Local achievements: 26-item state and Android UI validation

Target: Android **0.10.11-beta**. The catalogue now has **26 achievements**, with
`yo_shi_pude` appended after the 25 existing stable identifiers. This report
qualifies the Java model, persistence migration, catalogue/status display and
achievement banner. Native campaign observation and automatic suspend proof
have separate validation; these tests do not establish a complete natural
playthrough.

## Results

| Check | Result |
| --- | --- |
| Pure Java model | **2,055 checks passed**, 26 definitions |
| Robolectric Android host suite | **22 tests passed**: 13 catalogue/store/status, 9 banner |
| Runtime framework | Robolectric, Android API 34; compiled against API 35 |
| Genuine previous encoder fixture | **1 test passed** with the unmodified 0.10.10 model/store |
| Previous catalogue text | All 25 IDs, titles and descriptions preserved exactly |
| Device coverage | Host Android framework tests and renders; no physical-device claim |

Reproduce the combined model and framework checks using
[`run_achievements_ui_test.py`](run_achievements_ui_test.py), which builds the
production Java classes with the test JNI boundary. The test sources are
[`AchievementsModelTest.java`](AchievementsModelTest.java),
[`AchievementsUiTest.java`](AchievementsUiTest.java) and
[`AchievementBannerTest.java`](AchievementBannerTest.java).

## Event and storage contracts

The earned-progress JNI snapshot is schema **2**, eight longs: the schema and
seven monotonic metrics. Its event metric is now a bit field: mask `0x1`
(bit 0) means a Fuzzy recovery; mask `0x2` (bit 1) means the native tracker
verified “Yo shi pude”. The
Fuzzy achievement explicitly tests its own bit. A clean campaign event cannot
accidentally unlock the Fuzzy achievement, and the reverse is also tested.

The record written to `AtomicFile` is schema **3**, with all 26 stable IDs.
The reader accepts these exact earlier shapes:

| File schema | IDs | Migration |
| --- | --- | --- |
| 1 | 10 | Preserve the first 10 dates; recognize eligible additions from the old metrics; append “Yo shi pude” locked |
| 2 | 25 | Preserve all 25 dates, recognition origins, metrics and notification preference; append “Yo shi pude” locked |
| 3 | 26 | Read and validate the current record |

Old formats cannot carry the new event bit. The earlier recognition mask is
frozen to IDs 10–24 and never includes the new challenge. A pre-existing save,
old achievement record or completed-progress totals cannot retroactively
certify a campaign with no Baby Mario separation.

Migration is silent: it does not consult JNI or replay unlock notices. If its
write fails, the previous record remains intact and a later successful poll
retries the rewrite. Existing earned dates remain monotonic across process
restarts. Corrupt, future, incompatible and forged records are not replaced by
an empty record. The framework tests cover **23 expanded/forged record
variants**, in addition to the generic corrupt/future-record cases.

## Authentic migration evidence

The schema-2 fixture was emitted by the actual, unmodified 0.10.10
`AchievementsStore.Disk.encode`, using model/store sources from commit
`cdf8223ebdfc3317f3b00c5bfd5811943c555bc5`. It is embedded byte-for-byte in the
current framework test and its hash is asserted before migration.

| Fixture | Size | SHA-256 |
| --- | --- | --- |
| 0.10.10, schema 2 | 944 bytes | `fdbb7044d0e6241a36bd460a32c0a372b500ec349d6db607453e21262537a790` |
| 0.10.9, schema 1 | 464 bytes | `e66c5b7e85bf7102fbe35b7918912cf160e9c68d92ef9a880c47f9337f104cc8` |

The earlier schema-1 fixture comes from commit
`2c5dbc94a707460da3ae1aac1b93cb6d2fcdaa32` and is retained from the previous
validation. Both fixtures contain synthetic test progress, not player data.

The schema-2 fixture contains 18 unlocked items, mixed original/recognized
dates, all seven metric types and disabled notices. Migration retains exactly
18 unlocked items and the original dates/preferences; item 26 remains locked.
An independently emitted event mask `0x2` then unlocks only the new item once,
with its date preserved after reopening the store. The schema-1 fixture has
8 original unlocks and recognizes 9 eligible additions, for 17 total; it also
leaves the new challenge locked.

## Challenge status and catalogue

The four-long native status response is validated separately from earned
progress: `[schema=1, status, save_slot_plus_one, completed_main_courses]`.
Malformed responses, unavailable JNI and invalid counts are displayed as
unavailable without erasing earned progress. Status alone never awards an
achievement. The persisted item-26 earned date takes priority over an idle or
unavailable current attempt.

Framework tests render active file 2 with 17 of 48 main courses, failed Baby
Mario protection, unverifiable continuity, completed status and malformed
responses. The rules distinguish starting a new file, the 48 main courses and
final boss, loss even if Mario is recovered, ordinary scripted handoffs, and
verified automatic suspend/resume. Catalogue filters, all-earned progress,
narrow portrait, compact landscape and scrolling are also exercised.

## Unlock animation

The banner presents each earned ID individually in order, deduplicates pending
items and supports all 26 bits. Tests exercise entry, expansion, readable hold,
collapse and exit. The standard readable hold is 3.4 seconds and creates no
animation/frame work during that interval. System-disabled animation keeps a
static readable notice; accessibility can extend the reading duration.

Disabling the preference, pausing or destroying the activity clears pending
notices. A delayed worker callback from an earlier foreground session cannot
replay stale notices. The overlay does not intercept normal touches or
multitouch. Safe insets, rotation, narrow widths and 200% text size are covered.
Rendered intermediate phases were inspected to verify that the moving egg
medallion and title do not overlap.

## Actual native suspend and damage integration

The final native host candidate was additionally exercised with the actual
runtime and game, using the same suspend callbacks as Android. Its SHA-256 is
`8288c3080c6e9ec498cfa392d060642dd9dd2f618a77338cd7adcc3e0f88323d`.
The private event helper pushes ordinary SDL lifecycle events; it never writes
game RAM or achievement state. A recorded input sequence creates a new file
through the real title and intro code.

| Actual runtime case | Result |
| --- | --- |
| Background, kill while SDL waits, launch again | Active attempt resumes from the exact automatic state; status `[1,1,1,0]` |
| Replay that automatic state after its proof was consumed | Gameplay continues with no eligible attempt; status `[1,0,0,0]`; no new challenge event |
| Background and foreground in the same process | Active attempt remains active; proof and pending marker are consumed |

Both final background cases saved at frame 752. The automatic game state was
553,424 bytes and the separate proof 296 bytes. The resumed process advanced
past the saved frame with the eligible attempt intact. The runtime's epoch
change on restoration did not falsely invalidate it. These were real files,
processes and SDL waits, with dummy video/software rendering on the host.
They are not Android-device or power-failure measurements.
The sanitized case results and trace hashes are in
[`yo-shi-pude-lifecycle-report.json`](yo-shi-pude-lifecycle-report.json).

A separate controlled gameplay fixture changes the hurt timer to `0xF0`.
The game decrements it to `0xEF` and executes the original Baby Mario damage
path. Actual RAM changes from attached flags `0x8000`, active state 5 to flags
0, active state 8, Mario state 0, then bubble states 10 and 11. The game
continues for 120 frames. The production native observer/tracker consumes the
resulting trace through
[`replay_no_mario_loss_trace.cpp`](replay_no_mario_loss_trace.cpp): with an
explicitly synthetic eligible prefix, its first observed loss at PPU frame
6601 permanently changes the attempt to `MarioLost` and never awards the
challenge. TCP observation saw the game write at frame 6600; the frame observer
samples it at the following boundary. This is evidence of the executed damage
routine and detector behavior, not natural enemy contact or a full clean run.

Native observation, Super Star return and final ending evidence are reported
in [`yo-shi-pude-native-report.json`](yo-shi-pude-native-report.json).

## Focused existing-gameplay regressions

The final candidate also repeated the existing Fuzzy recovery and complete car
transformation replays. Each was compared with the recorded 0.10.9 native
reference after validating the exact ROM, BIOS, configuration, input state,
input sequence and binary hashes. The reference was reused explicitly; the
candidate was executed again. A private copy of the existing validator only
updates the expected native snapshot schema to 2 and its import paths.

| Replay | Matching trace rows | Matching saved states | Sampled images with zero changed pixels | Existing achievement event |
| --- | --- | --- | --- | --- |
| Fuzzy, width 240 | 1,901 | 41 | 40 | Recovery mask `0x1` |
| Car, width 356 | 1,527 | 34 | 33 | Car transformation mask `0x1`; event flags 0 |

The Fuzzy effect remained active for 782 frames, recovered normally, and ended
with 101 normal frames without a restart or death. The car replay included
activation, vehicle play and the return to Yoshi. Both stayed fully native
with zero dispatch misses, interpreted instructions, healed instructions or
native failures. Guest state sections and PPU timing matched the reference;
only the previously pinned inactive RTC wall-clock field is excluded from
state comparison. Detailed input hashes and results are in
[`achievement-0.10.11-focused-native-regressions.json`](achievement-0.10.11-focused-native-regressions.json).

## Scope

These are real production Java classes exercised under a host Android
framework, with controlled JNI evidence. They verify persistence and display
behavior, not the native detector's interpretation of every game scene.
Filesystem error injection and `AtomicFile` recovery tests do not reproduce a
physical storage controller or every power-loss condition. Native gameplay,
automatic suspend proof, build/signature validation and any physical Android
installation must be reported with their own evidence.
