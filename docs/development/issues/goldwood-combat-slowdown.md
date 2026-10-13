# PERF-001: Goldwood combat slows game updates

- **Severity: critical. Status: open; qualified local preview, owner live verification pending.**
- Reported and last updated: 2026-10-11 UTC.
- Affected: modern-controls v1.1 local preview with original instruction-clock runtime;
  source capture runtime SHA-256 `de2737396c3f840291de8a9155eaed415f6076beaf4d27200be6505cde11c538`.
- [Implementation investigation and research](../../features/pc-controls.md#2026-10-11-critical-goldwood-timing-investigation).
- Related: CAMERA-001 small boom-distance steps; CAMERA-002 large collision pull-ins.

## Symptoms and reproduction

The owner reported lag while fighting Goldwood drones and crossing the bridge after
the first enemy section. Rapid mouse turns made the issue noticeable. The worst
combat window in capture `20261011-011404-f9cc9def804541998e2a5fa4d250df0b` is
approximately 190-210 seconds. The late bridge segment is approximately 237-268
seconds. A steady 30 Hz camera has inherent latency, but it cannot explain the
measured combat collapse to roughly 15 game updates per second.

## Evidence and cause

Host VI ticks continue near 16.7 ms while game updates reach about 67 ms median,
with 85-98 ms gaps in the source capture. CPU graphics submission is about 2.6-3 ms;
this is not GPU completion time and does not rule out all graphics bottlenecks.

The reference execution policy charges Count for modeled N64 instruction and cache
costs, then paces the host to that timeline. It therefore reproduces the modeled
console CPU throughput limit and also spends host CPU time evaluating that model.
A light-trace run measured about 62.3 ms modeled non-idle work per update in the
worst window. This is a demonstrated contributor. It does not by itself explain
all residual host execution or scheduling costs.

Actor trace rows are changes, not complete snapshots. Reconstruct by slot and
apply removals. Corrected source combat counts are 44-58 total table entries and
1-7 enemy entries; these are not visible-on-screen counts. Earlier counts derived
from individual delta rows are invalid.

## Attempts and results

| Experiment / hypothesis | Result | Decision |
|---|---|---|
| Fixed modern desired camera distance | Removed 186 unobstructed fixed-angle relative steps over five units in comparable replay pairs | Separate camera fix delivered; does not repair combat update rate |
| Full versus light capture logging, same runtime/input | Slowdown remains; final RDRAM identical (`757b7094a9d6fabd671e4c90ba7d50e54a52741d3e7ff3ffb57df3f062b93d99`) | Logging is not the sole cause |
| Extra modern camera visibility-query cost | About 0.14 ms modeled / 0.52 ms host per traced invocation | Too small to explain the measured deficit |
| Replace modeled CPU clock with elapsed host clock | Removes modeled per-instruction/cache charges, preserves guest OS/device deadlines | Promising diagnostic candidate, default off |
| Replay old recording strictly by poll number | Different clock changes real input durations | Invalid performance acceptance comparison |
| Replay by absolute recorded VI | Startup menu readiness differs | Invalid route comparison |
| Anchor timed replay at first gameplay pose | Start matches, long open-loop route later diverges into a different level | Do not claim the full-route FPS comparison as Goldwood proof |
| Load the same Goldwood checkpoint saved by the game | Same initial pose, 47 actor entries, two enemy entries, 27 shots each; median 49.96 to 33.61 ms across movement/firing, candidate p95 50.19 ms | Improvement, residual slow intervals remain |
| Change clock only after identical combat prefix | Full RDRAM, Count, VI and input poll identical at boundary; both remain level 21 | Stronger comparison below; still below target |
| Instrument native idle deadlines and wake-up times | Ordinary sleeps overshot by 1.247 ms median / 1.890 ms p95 over 1,868 observed gameplay waits; run then trapped | Scheduling cost demonstrated; incomplete run is not acceptance |
| High-resolution Windows waitable timer | Built after correcting Windows header-order macro pollution; native run trapped at VI 4098 on equal PI/SP/DP deadlines | Timer candidate remains diagnostic; event-order qualification is required |
| Inline native ordinary-instruction bookkeeping | Original 250,000-transition regression plus native no-time-charge / bounded-safepoint / hardware-exception boundary checks passed; runtime rebuild in progress | Avoid a full C++ callback per ordinary native instruction without dropping clock safepoints |

For seconds 1-21 after the identical combat boundary, reference median/p95 are
**67.88/97.78 ms**; candidate median/p95 are
**49.89/51.82 ms**. Median enemy-table count is seven in both.
Later state can diverge as changed cadence changes sampling; identical initial
memory does not imply identical future AI or frame-by-frame behavior.

## Actual resolution

**No final resolution yet.** The local Modern Controls Preview now uses the
qualified native elapsed-time clock and contiguous instruction spans. Its v1.1
launcher and saved profile are unchanged. See the [final candidate measurements
and acceptance](#contiguous-candidate-qualification-and-handoff) below.

`HostCountClock` in
[`include/jfg/boot/host_count_clock.hpp`](../../../include/jfg/boot/host_count_clock.hpp)
and its integration in [`native_boot.cpp`](../../../src/boot/native_boot.cpp)
remove modeled console CPU/cache costs from the private interactive build's
timeline. The explicit `JFG_EXPERIMENTAL_NATIVE_PLAY_TIMING` build option remains
off by default; reference probes retain their deterministic instruction clock.
No release or fix commit is claimed. Residual slow updates and live acceptance
remain open.

The candidate uses a steady clock at 46.875 MHz, preserves fractional timer units,
handles guest wrap with a 64-bit count, excludes frontend pauses, and services
normal hardware/OS deadlines. It retains game simulation and actor processing.
It does not multiply world speed, skip actors, or conceal drops with smoothing.

## Initial verification and acceptance history

These initial results and outstanding checks describe the first clock candidate.
Later sections record subsequent checks, failures and current acceptance.

- 148 clock checks and 18 recorded-input timeline checks passed; the nine targeted
  timing/input test executables passed together.
- Controlled movement is identical under both clocks: 243.04 world units/second
  forward/back and both strafe directions in the tested fixture.
- Held aim survives firing; shot counts agree in movement and Goldwood fixtures.
- Native Save changes only a copied test save; reload restores the saved 95-round
  pistol state. Source/live saves remain unchanged.
- Default reference-clock replay remains byte-identical to the retained baseline.
- Still required: consistent combat cadence, startup/cutscene/audio timing,
  overload and stall recovery, broader controller/input coverage, frontend pause
  and physical focus behavior, and owner verification of the fight and bridge.
- Large collision-driven camera pull-ins and unqualified pivot animations remain
  separate issues. A passed timing test does not resolve them.

Private artifact root: `combat-critical-evidence-20261011`, within the existing
`pc-input-aiming-20261008` guarded investigation. Preserve scripts, runtime hashes,
input/profile provenance and reports; do not publish ROM or personal save data.

## Lessons for other recomp projects and future 60+ FPS work

Distinguish elapsed real time, guest timer units, guest simulation ticks, VI,
rendered frames and presentation. An emulated CPU/cache cost model is a valuable
deterministic reference, but retaining its throughput ceiling as the native game
clock can preserve original slowdown despite abundant host performance.

A host-time clock is compatible with future higher-refresh rendering, but does
**not** itself make the simulation run correctly at 60+ Hz. Keep the game-speed
contract independent of render/present rate. Audit movement, animation, weapon
cadence, AI, audio and input for per-update assumptions before changing simulation
frequency. A possible later design renders interpolated transforms between fixed
simulation steps; camera collision, input latency, discontinuities and state
ownership still require their own design and tests. Do not blindly double ticks
or halve deltas. No interpolation or 60 Hz simulation is implemented here.

Use bounded catch-up and explicit pause/stall policy, not an unbounded backlog.
Timing-policy changes also invalidate poll-count-based real-time comparisons;
use matched checkpoints, time-based input with conserved mouse displacement and
short button edges, and report route divergence. Measure presentation/GPU and
actual update cadence independently; an FPS counter alone does not locate a cause.

Primary architectural references: [N64ModernRuntime timer](https://github.com/N64Recomp/N64ModernRuntime/blob/cdf5abbd5026fef5c364c676e4667c45e42b6863/ultramodern/src/timer.cpp),
[VI scheduler](https://github.com/N64Recomp/N64ModernRuntime/blob/cdf5abbd5026fef5c364c676e4667c45e42b6863/ultramodern/src/events.cpp),
[Perfect Dark timing](https://github.com/perfect-dark-pc-port/perfect_dark/blob/32a1cb9f268dd3ac73016801025c6bbbfa20130f/src/game/timing.c),
and [Fix Your Timestep](https://gafferongames.com/post/fix_your_timestep/).
These guide architecture; they are not drop-in fixes for this runtime.


### Follow-up: simultaneous device events and timer precision

The high-resolution wait run stopped at VI 4098: PI, SP and DP all had absolute
Count deadline 3243790045; current Count was 3243822106. This is an exact deadline
tie, not merely several different deadlines becoming overdue. The original
reference selector only qualifies SP-before-DP ties and deliberately rejects
other simultaneous-source combinations. The failure check remains in place.
Changing clock sampling exposes a native-runtime scheduling requirement that
must be resolved with an explicit, validated event contract. Arbitrary priority
changes or offsetting one deadline to avoid a tie would conceal the issue.

The Windows wait candidate uses a high-resolution one-shot waitable timer,
rounds the relative timeout up to 100 ns units, rechecks steady time after wake,
and keeps the original device deadline. It does not busy-spin or change game
speed. The API requires Windows 10 version 1803 or newer; unsupported creation
currently fails the diagnostic mode rather than silently claiming high precision.
See [CreateWaitableTimerExW](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw)
and [SetWaitableTimerEx](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setwaitabletimerex).
Both recorded wait probes stopped before the route finished. Their partial
observations cannot qualify either a final performance improvement or a release.

The generated native inline rebuild did not complete within this build step; continuation required. Its focused tests passed; this
is not a performance acceptance result. The installed runtime hash was checked
and remains unchanged. Additional owner-approved work continues under the same
production guard and investigation with all prior charges retained.


### Continuation: native scheduling qualification

The owner approved two additional hours in the same investigation, preserving
all guard counters. The first generated inline rebuild exceeded its eight-minute
command budget. The continuation rebuild has its own bounded command inside the
same guarded investigation; this is not a new investigation or a counter reset.

The native event candidate commits all independent device completions sharing the
earliest due timestamp before the guest CPU observes an interrupt. Guest PI/SI
transfers already make their data visible at launch in the retained device
profile; their completion changes status/interrupt state. SP completion finishes
its owned task before the CPU runs again. The original reference policy remains
unchanged, including its rejection of unqualified ties. This is a separate native
elapsed-time contract, not a new claim of cycle-exact N64 timing parity.

Tests cover all nonempty device subsets and their input permutations, duplicate
sources, future/earlier deadlines, integer limits, actual PI/SI/SP completions in
all 24 orders, interrupt masking/acknowledgement, and unchanged reference tie
rejection. Integration and performance outcomes follow below.

- `finish-native-inline-runtime-build`: process exit 127 (zero alone does not establish performance acceptance).

- `native-inline-clock-goldwood-before-batch`: process exit 125 (zero alone does not establish performance acceptance).

- `native-event-batch-candidate`: process exit 125 (zero alone does not establish performance acceptance).

- `native-event-batch-test-build`: process exit 125 (zero alone does not establish performance acceptance).

- `native-event-batch-tests`: process exit 125 (zero alone does not establish performance acceptance).

- `native-event-batch-runtime-build`: process exit 125 (zero alone does not establish performance acceptance).

- `native-event-batch-goldwood-checkpoint`: process exit 125 (zero alone does not establish performance acceptance).

- `native-event-batch-combat-boundary`: process exit 125 (zero alone does not establish performance acceptance).

- `reference-event-batch-combat-boundary`: process exit 125 (zero alone does not establish performance acceptance).

- `compare-event-batch-combat-boundary`: process exit 125 (zero alone does not establish performance acceptance).

No completed matched combat comparison for this revision; acceptance remains pending.


### CPU versus GPU work

RT64 already renders on the GPU. The diagnosed clock charges, ordinary instruction
callbacks and device waits are CPU-side work. Moving AI/physics to GPU compute
would require data/layout and synchronization changes; it is not a demonstrated
remedy for this timing defect. Graphics submission timings do not establish GPU
completion latency. If residual stalls implicate rendering, measure GPU timestamps,
queue/fence waits and presentation separately before proposing further offload.

The guest stack transport also uses parked host OS threads with a single execution
baton. Context-switch overhead is a possible additional cost, not a confirmed
bottleneck in this recording. Do not replace it merely because fibers might be
faster; quantify the cost and preserve context, exceptions, sanitizer support,
thread-local state and guest scheduler behavior before changing the transport.


### Completed scheduling replay pass

The incremental build completed after the command timeout; completed objects
were retained and no original/generated game bodies were changed. The native
inline build without the event batch reproduced an exact PI/SP/DP deadline tie
at VI 3816. After adding the explicit native event batch, its 41,370 focused
checks passed and the saved Goldwood movement/firing replay completed.

Matched combat prefix memory, Count, VI and poll equality: **True**.

- Reference seconds 1-21: update median **67.79 ms**, p95 **98.69 ms**; final audio counters [19124224, 19117568, 8, 0] (queued/consumed/underflows/overflows).

- Native seconds 1-21: update median **50.01 ms**, p95 **54.91 ms**; final audio counters [19124224, 19113728, 2, 0] (queued/consumed/underflows/overflows).

Windows timer deadline/reuse/lifetime check process exit: 0.


The owner approved six additional hours on the same investigation. This does not
reset counters, disable the production guard or change the installed preview.
Critical acceptance remains open until residual costs and the listed regressions
are qualified. Supporting replays alone are not a release claim.


### Instruction-call overhead and GPU measurement audit

The first scheduling revision completed both Goldwood tests. The simpler fixture
held roughly 33.3 ms per frame body, but the matched fight still had a 50.01 ms
median / 54.91 ms p95 update interval (reference 67.79 / 98.69 ms). About 13.0% of
the measured combat wall-time window was spent inside graphics submission; its
full-sync portion was about 6.5%. Those nested values must not be added together.

An opt-in CPU-cost trace then measured 56.74% of guest instructions entering an
out-of-line runtime callback, while only 0.80% entered the full clock/device
handler. Most of those calls therefore performed ordinary instruction bookkeeping.
These are instruction fractions, not fractions of runtime. Whole-process CPU
time includes renderer/helper threads and is quantized; use aggregate values.

The instrumented run itself had a 33.87 ms median / 50.54 ms p95 interval. This
variation from the preceding uninstrumented run means a single favorable replay
cannot prove a performance fix. Compare independent runs with matched initial
state, and compare uninstrumented behavior with instrumented behavior.

The ordinary RT64 workload-profiler snapshot returned no available samples.
Missing samples are not zero GPU cost. The synchronous framebuffer render-to-RAM
path has a separate graphics worker; the regular workload profiler is insufficient
to characterize it. Direct measurement must put timestamps on that worker and
resolve them after its existing fence, without adding another GPU wait.
CPU full-sync timing includes submission, synchronization and readback and is
not interchangeable with GPU execution duration.

The next native candidate inlines the compact bookkeeping path in large generated
functions as well as small ones. It retains the same bounded host-clock sampling,
hardware revision, interrupt, exception, observer and instruction-budget checks.
It does not omit guest instructions or actor work. The original cache/Count model
and its selector remain available for reference comparison.

The new path passed 65,536 differential native transitions alongside the 250,000
reference transitions. A warnings-as-errors failure for a shadowed variable in
the new test was corrected before the runtime rebuild was admitted. The generated
support-header and previous executable hashes were archived; game code bodies
were not edited.

All compared combat prefixes equal: **True**.

- native-event-batch-boundary: median 50.01 ms, p95 54.91 ms, 335 updates over 40 ms / 430 samples.

- native-cpu-gpu-cost-boundary: median 33.87 ms, p95 50.54 ms, 206 updates over 40 ms / 497 samples.

- native-all-inline-boundary: median 50.13 ms, p95 54.16 ms, 398 updates over 40 ms / 399 samples.

- native-all-inline-cost-boundary: median 50.14 ms, p95 54.02 ms, 395 updates over 40 ms / 402 samples.

Reference full-replay RDRAM SHA-256: `757b7094a9d6fabd671e4c90ba7d50e54a52741d3e7ff3ffb57df3f062b93d99`.

The critical issue remains open; installed preview unchanged. Pause/stall, audio, startup/cutscene, movement, Save/reload and owner live verification remain part of promotion acceptance.


### Scheduling phase handoff and bridge comparison

The all-function inline candidate reduced out-of-line instruction callbacks from 56.74% to 0.924%, but both new combat replays remained near 50.1 ms per update (about 20 Hz). It did not solve the remaining slowdown. Reduced callback count is not a demonstrated performance improvement. Preserve the result and measure remaining handler, renderer and execution costs before choosing the next remedy.

Hidden runs so far used a 640x480 surface. The owner recording ended at 1164x873; subsequent GPU comparisons must match that size. Hidden rendering still does not qualify visible presentation/VSync latency.
The late-bridge comparison did not complete; preserve the failed/skipped command outcomes and do not claim acceptance.

The installed preview hash remains `059328317aeedccf1342156b884a6b8221573acd7f669636744649d8669f3cdd`. The native timing policy is diagnostic and default off. The next authorized phase measures the synchronous GPU framebuffer queue and validates pause/resume and host stalls before any promotion. The critical issue remains open.


### Hidden pause fixture correction

The first hidden pause attempt received no acknowledgement: the ordinary non-play probe does not service frontend window commands. This is a harness limitation, not evidence of a broken interactive pause. The existing hidden real-time replay flag was restricted to the older cooperative runtime. Its validation now also admits the unified guest OS when that runtime owns pacing; the shared window service returns before the cooperative frame pacer, preventing double pacing. The corrected bounded fixture remains hidden and uses copied saves, without foreground or physical input changes. Acceptance awaits the corrected run.

The reference bridge replay reached its requested ending, then failed at `guest-capture/final-image`; the paired comparison was skipped. This diagnostic capture failure is retained and not counted as gameplay/timing acceptance.


### Direct GPU measurements and timing qualification

The direct synchronous framebuffer GPU measurement returned valid timestamps:
1,195 submissions over 19.98 seconds occupied 42.37 ms of GPU execution in total
(0.21% of wall time), averaging 0.0355 ms per submission. The remaining combat
update interval was 50.14 ms median / 54.23 ms p95. This rules out that measured
GPU command interval as the dominant cost; it does not measure the separate
regular workload/presentation queue. CPU graphics submission, including its
existing waits, remained about 5.59 ms per update. The native clock/device handler
prefix contributed about 2.68 ms per update after excluding measured waits and
graphics submission. These nested measurements must not be added indiscriminately.

The corrected hidden real-time pause fixture held the guest clock at the same VI
for a 1.263-second frontend pause (four Count ticks of measurement separation),
then resumed at 30 updates in the first second. Held aim survived firing five
shots; saving modified only the copied fixture. Reload is checked separately.

Injected 250 ms and 2,000 ms host stalls exposed overdue video-interrupt delivery
in a burst: respectively 74 and 179 VI events in the following second, with many
within the first millisecond. Gameplay completed without a fatal error, but this
is not acceptable missed-deadline qualification. Native timing remains default
off until the overdue-event policy is corrected and rechecked.

The first capture-size tests stopped before gameplay because Windows produced a
1160x863 client rectangle from an estimated 1164x873 request. The harness now
corrects dimensions using the measured owned hidden window rectangle, without
showing/focusing it. Failed cases remain archived; only a confirmed final surface
size will count as a size-matched GPU comparison.

Source inspection found three dispatch paths that scan every preceding instruction
address to identify a generated relocation's alternate entry point. Per-call
lookup cost is being measured before selecting a remedy. Generated lookup tables
change at overlay lifecycle boundaries, so any cached resolution must preserve
publication, rebasing and invalidation semantics; a blind address cache is unsafe.


### Generated entry lookup cost and remedy

Diagnostic-only measurements found a median 666,854 table probes across 54
relocation-entry resolutions per combat update, costing 2.661 ms (5.3% of the
20-second measured wall-time window). Each dispatch searched every preceding
instruction address to discover whether its target was an alternate entry into
a generated function. The generator already knows that identity.

The candidate now emits each continuation offset with its function-table entry.
That offset is published, rebased, invalidated and collision-selected together
with the corresponding function pointer. Dispatch retrieves the metadata instead
of searching prior addresses. This removes repeated work without changing guest
code, instruction costs, timers or actor simulation. It is one measured overhead,
not a complete explanation of the remaining 50 ms update interval.

The compiled synthetic test exercises normal and alternate entry execution,
1,000 rebase/unload cycles, exact address collisions, misses and reload. It and
the 17 existing generator tests passed. The stale-normalizer build check first
rejected an incomplete incremental support update. Full regeneration then used
the original qualified recompiler header; 5,821 game and shared-header files
were verified byte identical before support/inventory synchronization. The
normalizer revision check remains enabled. Matched performance and full reference
RAM parity checks are separate acceptance requirements.

The pause Save reload also passed: all 250 observed post-load weapon states
retained the saved 95-round count; the source save remained unchanged.

The second surface-size attempt was still one pixel short because the inspecting
process used a different DPI-awareness context from the owned runtime window.
The revised fixture uses that window's DPI context for both sizing and
measurement. These failed size checks did not count as GPU evidence.


### Entry metadata validation, CPU samples and missed video deadlines

The entry metadata remedy passed the full 17,000-VI reference replay with the
same final 4 MiB RAM hash as the earlier reference:
`757b7094a9d6fabd671e4c90ba7d50e54a52741d3e7ff3ffb57df3f062b93d99`.
Candidate executable: `266aa9d0b459e80b36c73bc550877fd2b304490134d146c5cb044b9d03b5e67f`.
Median resolution cost fell from 2.6605 ms / 666,854 probes per update to
0.0028 ms / 57 probes. The combat update median still measured 49.991 ms
(p95 51.308 ms); this is a verified local optimization, not resolution of PERF-001.
Both comparisons began with the same RAM hash at the clock switch. Subsequent
native-time trajectories may differ, so later work totals are not identical inputs.

The generator's object-shape audit initially rejected two exported functions in
one support object. The compatibility lookup wrapper now occupies its own
support source/object; the entry table has one exported metadata accessor.
The audit accepts only the specifically named new accessor in addition to the
two existing lookup roles, while retaining the one-function-per-object rule.
Synthetic tests cover both old and new inventories and reject unrelated extras.
The stale-normalizer, header-lineage, line-ending and object-shape failures are
retained as evidence; no qualification check was bypassed to obtain a build.

The corrected DPI fixture confirmed a 1164x873 hidden client surface and final
capture. Native synchronous framebuffer GPU work occupied 44.91 ms over 19.969 s
(0.225% of wall time; 0.0351 ms/submission). Reference was 27.95 ms over 19.949 s
(0.140%; 0.0348 ms/submission). These measure that command list only. Regular
rendering/presentation GPU cost and visible VSync latency remain separate.

Owned-process CPU sampling collected 1,089 passes over 18 seconds without a
failed sample. On the hottest game thread, 248 of 298 samples attributed to
generated source landed on instruction timing hooks (83.2%). The thread used
6.531 s of user CPU and 0.344 s of kernel CPU. Waiting samples are not CPU cost;
optimized PDB attribution and sampling cadence can bias source attribution.
This is evidence to investigate per-instruction bookkeeping, not proof that
83% of whole-process time is spent there. GPU offloading is not indicated by
these CPU bookkeeping measurements.

The next candidate caches the metadata-independent eligibility predicate at
the full timing handler. Every instruction still checks live CPU Status,
hardware revision, bounded poll budget, FPU availability and metadata boundaries.
CPU register writes and ERET explicitly invalidate it; instruction-limit
remaining allowance caps the next poll budget. It keeps instruction/branch
accounting and the reference cache model, and never skips game logic. Differential
tests, full reference parity and matched combat measurements are required before
calling it a performance improvement.

Native periodic VI scheduling now latches one pending video interrupt and
advances its next deadline over all elapsed edges while preserving phase. It
does not advance Count, replay missed game updates or change reference timing.
VI_CURRENT uses bounded periodic phase even before an overdue event is serviced.
88,126 unit checks passed, including overflow, phase, missed periods and the
existing equal-time event-batch checks. These rules also avoid a burst of old
video messages after a future higher-render-rate build stalls; they do not
implement 60 Hz game simulation or interpolate animation/camera poses.

Private evidence: `native-coalesced-stall-250`; verification result remains in the guarded command spool.

Private evidence: `native-coalesced-stall-2000`; verification result remains in the guarded command spool.

Both injected stalls passed: 250 ms resumed with 60 video events in the next
second; 2,000 ms resumed with 61, compared with 74 and 179 before the fix.
Each had one video event within the first 2 ms. Count continued to track actual
elapsed time to within 0.002 ms of the logged wall interval. The 2-second fixture
is allowed to underflow audio while the entire host thread is deliberately
stopped; this is not a claim of uninterrupted audio through a frozen process.

All fixtures used copied profiles/saves and hidden owned windows. The installed
preview remains unchanged and native timing remains diagnostic/default off.
PERF-001 remains CRITICAL OPEN; final controls, startup/audio, pause/save,
stall regressions and owner live verification are still required for promotion.


### Cached eligibility qualification and rejected Count-store experiment

The cached eligibility candidate built and passed 250,000 reference and 65,536
native transition checks, including Status/I/O invalidation, FPU and metadata
boundaries, bounded clock sampling and instruction-limit refusal. Its full
17,000-VI reference replay still produced RAM SHA-256
`757b7094a9d6fabd671e4c90ba7d50e54a52741d3e7ff3ffb57df3f062b93d99`.
Runtime SHA-256: `2e52d24adc2e5f5707b06d13e66059e757bc20dd97ea71bb7172f9420c478fcb`.

This change did **not** resolve the remaining combat slowdown. The unprofiled
matched-boundary run measured 50.115 ms median / 54.590 ms p95, with 382 of 408
updates exceeding 40 ms. The instrumented sibling measured 49.094 / 53.829 ms.
Both began with the qualified identical combat prefix. Two- and three-VI update
intervals remain; later native-time AI/input trajectories can diverge.

Two Clang synthetic benchmarks had substantially different results: a 32-hook
same-translation-unit body improved about 1%, while a 1,024-hook body with state
and fallbacks compiled separately improved about 73.5%. Both body size and
compilation boundaries changed, and the actual runtime uses MSVC. Those figures
are not gameplay speedup evidence. The actual replay result takes precedence.

A separate, isolated MSVC /O2 experiment compared the current native helper with
removing its redundant Count mirror write. Across eight alternating pairs of
64 million synthetic hooks, medians were 227.563 ms and 229.964 ms (candidate
1.01 times baseline). There was no demonstrated benefit; the runtime source
retains that write. Do not convert a plausible optimization into an accepted fix
without relevant measurements.

The 200-second cold-start/title/attract fixtures completed under both clocks.
Both produced nonzero 22,018 Hz PCM, no recorded host-audio underruns or empty
queue pauses, and preserved the source save. Native PCM duration was 200.446 s;
reference was 200.463 s. This verifies delivery/content presence, not audible
quality or visually judged cutscene synchronization.

Next diagnostics distinguish the generated-dispatch prefix, repeated overlay
inventory validation and regular GPU workload from the previously measured
synchronous framebuffer GPU queue. Regular-workload observations read RT64's
existing timestamp results after its existing fence on the owning worker;
a synchronized snapshot avoids racing its profiler. No extra GPU wait or guest
workload is introduced. Prefix measurements are nested and must not be summed
as independent costs. The separately recorded final-image probe failure is
[DIAG-001](final-probe-frame-capture.md).

PERF-001 remains **CRITICAL OPEN**. The installed preview is unchanged and the
native timing policy remains diagnostic/default off. Hidden qualification does
not replace the owner's reserved physical-input/focus and fight/bridge checks.


### Separate regular GPU and dispatch observations

Runtime `00c1dfaeae5d2f4e12242a69326c531df5edadb010127d5a461d9506a4318434`
measured both existing GPU command intervals. Over the matched 20-second combat
window, 981 synchronous-framebuffer submissions totaled 32.790 ms (0.164% of
wall time), and 650 regular workloads totaled 16.189 ms (0.081%). Mean command
intervals were 0.0334 and 0.0249 ms. These measurements do not include the wait
before a GPU's first marker, visible VSync or desktop presentation latency.
They provide no reason to move CPU bookkeeping to the GPU.

Generated-dispatch prefix timing measured 3.309 ms median per update across
16,756.5 calls. Overlay-inventory validation is nested within that prefix:
0.769 ms and 3,640 checks. Never add those nested measurements. Instrumentation
and host scheduling affect this run, which measured 64.64 ms median updates;
unprofiled repeats take precedence for gameplay-cost claims. The next unprofiled
run measured 45.860 ms median / 54.338 ms p95, with 256 of 470 updates above 40 ms.
The matched reference sibling measured 67.883 / 98.999 ms, with all 267 updates
over 60 ms. Later native/reference actor and RNG paths can differ. This remains
partial improvement, not a fixed 30 Hz combat update rate.

Private evidence: `native-remaining-costs-boundary-detailed-costs.json`,
`certified-combat-comparison.json`, and `certified-bridge-comparison.json`.
The final-image failure is independently verified under [DIAG-001](final-probe-frame-capture.md).

### Control verification corrections and bounded native span candidate

Initial qualification checks made two invalid assumptions: positive expected yaw
for rightward stick input (the implementation uses negative yaw), and that VI
messages delivered by an update's completion equal the movement time consumed
inside that update. Both clocks had the same turn sign. Completion can straddle
an asynchronous video interrupt, and elapsed video edges can be coalesced.
Preserve those failed checks as test defects, not evidence of changed speed.
The trace now records the actual tick argument consumed by `controlPlayer`.
Replacement checks fit displacement and turn angle against accumulated movement
ticks, while separately reporting wall-time and delivered-VI rates. Save/reload
qualification previously skipped because of the failed prerequisite; it must
actually run and pass before being claimed.

The next candidate consolidates native timing bookkeeping across at most sixteen
straight-line instructions. Every guest operation and every original timing hook
remain in generated source; the reference path uses its original hook sequence.
Native admission reserves the full span within the existing sampling quantum.
Branches, labels/alternate entries, CPU register operations, callbacks, observers,
CACHE and exception boundaries end a span. Each memory operation dynamically
checks the full-width address, cached RDRAM extent and alignment. MMIO, uncached
aliases or other addresses first commit the executed prefix, then use individual
hooks before the access, preserving trap PC/Count/branch metadata. Hardware I/O
revision changes originate in that guarded MMIO path, not asynchronous render
callbacks. FPU admission retains the live availability check.

This is an internal opt-in experiment, requiring both native timing and generated
span support; release generation and the preview do not enable it. Synthetic
helper and compiled RAM/MMIO fallback equivalence, original hooks/labels inventory,
full reference RAM parity and same-binary alternating combat runs are required.
Building it is not proof of a performance gain. No actor, collision, animation,
movement or camera work is skipped. Future 60+ Hz rendering still needs separate
simulation/interpolation work; bounded instruction safepoints remain required.


### Conditional native-span qualification (2026-10-11)

Candidate runtime `43e65779b7cbd5da3dbcb5d61f392b57d0675dcd69165c473d08bb63f2f65b2b`
passed the complete 17,000-VI reference replay with unchanged final RAM
`757b7094a9d6fabd671e4c90ba7d50e54a52741d3e7ff3ffb57df3f062b93d99`.
Generated spans retained all 424,827 original hooks and existing labels;
46,829 spans grouped 237,742 instructions with 110,267 dynamic memory guards.

Same-executable ABBA combat measurements, over 20 seconds after the identical
qualified initial state:

| Spans | Updates | Median ms | p95 ms | Updates over 40 ms |
| --- | ---: | ---: | ---: | ---: |
| Off | 466 | 49.427 | 53.759 | 267 |
| On | 557 | 33.585 | 50.287 | 83 |
| On | 502 | 34.001 | 50.515 | 193 |
| Off | 454 | 49.825 | 53.094 | 289 |

The optimization reduces repeated host bookkeeping; no game work is skipped.
About 56.6% of executed combat instructions used admitted spans. Results remain
variable: the repeated enabled run still spent part of combat near 50 ms per
update. The median is not an average frame rate. Changed clock cadence can
alter later input sampling, AI and RNG; these are performance comparisons, not
proof of identical later gameplay state. PERF-001 remains **CRITICAL OPEN**.

A qualification instrumentation error was also caught: the first replacement
movement checks expected a fifteenth trace field that an earlier build did not
actually emit. The source edit had been appended to a private installer after
it ran. Those checks failed and dependent save/reload checks skipped. This
candidate actually includes the consumed movement-tick field; its replacement
checks must pass independently. Failed attempts remain in the private evidence.

A diagnostic clock transcript records exact native-clock observations keyed by
instruction ordinal, PC/opcode and idle deadline. Replaying the transcript with
spans enabled must consume every observation and produce identical final RAM,
Count, instruction total and save. Replay rejects mismatched keys, truncation,
backward time and unused tail records. Transcript replay skips host idle waits
and must never be reported as a performance measurement. The utility is opt-in
and restricted to finite recorded-input probes; it does not change play timing.

Private evidence: `span-qualified-combat-comparison.json`,
`span-qualified-reference-parity`, and the guarded qualification spool.
Installed preview unchanged; no live verification or release claimed.


The native transcript comparison passed: 8,589,363 clock observations were
consumed exactly, with identical final Count (10,200,870,393), instruction total
(11,629,409,591), save and RAM
`98e92f42129b1b7f6f8dd14b1b84280bfecf8814d94ef4efc0045b99153f2331`.
The optimized execution included 508,191,887 grouped instructions. This proves
that recorded sequence, not arbitrary gameplay or a performance claim.

The source audit checked all 115,571 eligible memory expressions against their
decoded base register and signed offset, with no mismatches. Its initial parser
flagged 84 zero-register forms incorrectly; the corrected audit and initial
report are both preserved. The next generator also rejects mismatched,
relocated, compound or unknown operands conservatively instead of assuming
its decoded address matches emitted C. Compiled fallback tests cover a base
register changing between accesses, including RAM-to-MMIO and MMIO-to-RAM.

Dual-stick/reference/native movement qualification and KBM aiming, frontend
pause, native Save and reload all passed. Movement remained 4.0625 world units
per consumed game tick in all four directions; half-stick yaw was -128/+128
angle units per tick. Firing retained aim, and the saved 95-round ammunition
state reloaded. A 1.262-second frontend pause advanced Count by only 0.0006 ms,
kept VI unchanged, and resumed with 29 updates in the next second. These hidden
fixtures do not replace the owner's physical-input/focus verification.

With actor logging disabled, the 20-second comparison completed 480 updates
without spans and 562 with spans; medians were 37.968 and 33.507 ms, and p95
remained 50.699 and 50.205 ms. The residual slowdown is not explained by actor
logging alone. Private evidence: `span-light-combat-comparison.json`,
`native-span-transcript-parity.json`, `native-span-memory-expression-audit.json`
and the `span-tick-*` fixtures.


### Contiguous native instruction candidate

The next candidate keeps admitted native operations together in generated code,
followed by the exact original fallback sequence. Unsafe memory access commits
the executed prefix and jumps to that instruction's original hook. This removes
per-instruction fast/fallback joins from the admitted block and lets the compiler
optimize consecutive operations without repeated opaque observer checks. Every
original operation, hook and alternate-entry label remains. No enemy, collision,
animation or input work is omitted.

The stricter generator proves each allowed memory expression has the decoded
base/offset and access width. Unknown expressions end a span. All 46,829 spans,
237,742 grouped instructions and 110,267 memory guards survived this stricter
classification. Six generator tests passed, including compiled RAM/MMIO
transitions and rejection of changed base, offset, width and relocation forms.
A quoting error in the newly added test initially stopped installation before
compilation. The failed command and skipped dependencies were retained; the
corrected tests passed before generation resumed under the same guard.

A default-off `JFG_EXPERIMENTAL_NATIVE_PLAY_TIMING` CMake option makes an explicit
private interactive preview use native elapsed timing and batching without
inherited environment flags. It requires the live runtime and a span-generated
root, identifies itself as `NativeTimingPreview`, and emits a support-log policy
marker. Finite reference probes keep original timing unless explicitly requested
otherwise. This engine policy applies to both control styles; it is not a new
controller setting or a released default.

This build must still pass full reference RAM parity, the existing exact-clock
transcript, same-binary alternating combat measurements, movement/aiming,
pause/Save/reload, bridge, audio and stall checks. An owned hidden interactive
startup must confirm activation without diagnostic clock environment variables.
Physical input/focus and visible/audible verification remain reserved for the
owner. A successful compile does not resolve PERF-001.

Candidate binary SHA-256: `251700d98118a137fdcbae8af50bdc4351c0276e9c4d0d1c525a65a18505f554`. Installed preview unchanged at build completion.


### Contiguous candidate qualification and handoff

Runtime SHA-256: `251700d98118a137fdcbae8af50bdc4351c0276e9c4d0d1c525a65a18505f554`.

The complete reference replay retains RAM SHA-256
`757b7094a9d6fabd671e4c90ba7d50e54a52741d3e7ff3ffb57df3f062b93d99`.
The previously recorded 8,589,363 native-clock observations replayed exactly
across the changed binary, ending with identical RAM, Count, instruction total
and save. This isolates code-generation semantics from host scheduling changes.

Same-binary alternating combat results (20-second qualified window):

| Spans | Updates | Median ms | p95 ms | Updates over 40 ms |
| --- | ---: | ---: | ---: | ---: |
| Off | 503 | 33.840 | 50.593 | 192 |
| On | 553 | 33.533 | 50.295 | 93 |
| Off | 476 | 46.798 | 50.626 | 247 |
| On | 594 | 33.440 | 37.513 | 12 |

The qualified bridge window measured 33.448 ms median / 33.865 ms p95 across 594 updates.
Its initial memory, Count and input poll match the previously qualified reference.
Later trajectories can diverge as cadence changes; the clock transcript supplies
the separate exact-state proof. Median update time is not average frame rate.

The interim analysis wrapper initially rejected a UTF-8 byte-order mark before
launching the second enabled replay. That failed command is retained. Reading
its source as UTF-8 with BOM support fixed the wrapper; the four completed runs
were executed off/on/off/on. No runtime replay failure was hidden or discarded.

Dual-stick movement/turning, KBM aiming/fire, frontend pause, native Save and
reload passed. The new binary preserved movement per consumed game tick and
reloaded the 95-round ammunition state. Source saves stayed unchanged.
The frontend pause lasted 1.260 seconds, advanced Count by 0.000085 ms,
and resumed with 30 updates in the next second.
A deliberate two-second host stall resumed with 61 video events in the next second
and 1 within the first 2 ms, preserving the no-catch-up-burst contract.

The native startup/attract fixture delivered the expected nonzero PCM duration
without reported underruns or empty-queue pauses. Its reference comparison used
the previously qualified reference binary, explicitly recorded in the evidence.
Both produced 200.463 seconds of PCM at 22,018 Hz. The longest interval between
queue submissions was 384.694 ms for this candidate versus 192.072 ms for the
reference fixture; there were no recorded underruns or empty-queue pauses.
These counters do not establish audible quality or visible cutscene sync.
An owned hidden interactive window also started with the preview policy without
diagnostic timing environment variables and closed cleanly through its window.
No desktop focus, global mouse/keyboard input or user window was changed by these
checks. They do not verify audible quality, visible latency or physical devices.

**PERF-001 remains CRITICAL OPEN, with the candidate verified in the stated hidden
fixtures and owner live verification pending.** Remaining slow updates are not
a claimed stable 30 Hz result. This work does not provide 60+ Hz simulation or
animation/camera interpolation. It removes modeled CPU cost from native elapsed
timing and reduces host bookkeeping without skipping simulation work.

For future recomp implementations: keep hardware time separate from host
instruction throughput, retain bounded safepoints and forced hardware boundaries,
preserve phase while coalescing a pending periodic interrupt after a host stall,
and use an exact clock transcript for optimizer parity. Prove effective memory
operands before bypassing per-instruction observers; uncertain forms must fall
back. A faster binary or completed replay alone is not acceptance evidence.

Diagnostic limit: ordinary live captures retain inputs and timing traces, but
do not record the exact Count transcript. Native elapsed-time runs can therefore
diverge on replay even with identical input recordings. The exact transcript
utility remains restricted to finite offline probes; RAM alone is not a complete
CPU/scheduler snapshot. Future deterministic live replay needs an explicit,
bounded clock-recording design rather than assuming input records are sufficient.

Private evidence: `native-preview-qualification.json`,
`contiguous-combat-comparison.json`, `contiguous-native-span-transcript-parity.json`,
`contiguous-bridge-comparison.json`, and the linked qualification fixtures.

The local Modern Controls Preview now uses this verified runtime with the
existing v1.1 launcher. Its previous runtime is backed up, package file hashes
were verified, and profile/settings/save hashes are unchanged. No public release
or Git commit is claimed. The desktop preview and capture shortcuts retain their
existing targets. Owner acceptance remains the drone fight, bridge, physical
KBM/XInput and focus, and audible/visible timing checks.


### Remaining costs in the final candidate

The final candidate received a separate CPU sampling and detailed timing pass
from the same qualified combat boundary. Profiling suspends/resumes owned
process threads and adds counters; these timings locate remaining costs and
are **not** performance acceptance measurements. Parked-thread samples are not
CPU usage, and nested counters must not be added together.

| Instrumented host work | Median per update (ms) | p95 (ms) |
| --- | ---: | ---: |
| Generated dispatch prefix | 1.887 | 2.339 |
| Overlay inventory (inside dispatch prefix) | 0.502 | 0.679 |
| Native handler prefix | 2.671 | 3.302 |
| CPU graphics submission | 5.343 | 6.820 |
| Explicit idle waits | 6.874 | 20.514 |

| Existing GPU timestamp interval | Completed submissions | Total GPU ms | Fraction of measured wall interval |
| --- | ---: | ---: | ---: |
| Framebuffer work | 1704 | 58.956 | 0.295% |
| Regular graphics work | 1123 | 28.337 | 0.142% |

These GPU queries reuse completed timestamps and add no new GPU wait. They do
not measure visible presentation/VSync latency or queue delay before the first
GPU marker. Rendering already runs on the GPU; these measurements do not justify
moving game logic, input or scheduler bookkeeping to GPU compute.

The owned-thread sampler completed 1,085 passes with no failed samples. It
recorded 109 instruction-pointer samples in `memcmp`, but flat samples do not
identify its callers or establish that these comparisons caused slow updates.
Before optimizing that work, attribute call sites and distinguish required
code/overlay integrity checks from avoidable repetition. Do not remove checks
or cache guest memory without a complete invalidation/lifetime contract.

The remaining work is to locate the CPU execution and scheduling boundaries in
slow updates on the final candidate, while retaining the same correctness
contracts. Broader pure-operation span eligibility is one possible experiment,
but excluded helpers must first be proven free of hardware callbacks and other
observable effects. It is not an implemented fix or a measured explanation.
Do not trade deadline checks, collision tests, actors or animation updates for
a higher reported frame rate.

Private evidence: `contiguous-remaining-costs-detailed-costs.json` and
`contiguous-remaining-costs-cpu-sampling.json`. The latter retains source locations
and owned-thread CPU accounting for subsequent investigation.


### Owner live feedback after native preview

The owner reports very smooth gameplay on 2026-10-11, but intermittent crackle
in background music. This is encouraging gameplay feedback, not a blanket timing
acceptance. Track the audible problem as [AUDIO-001](intermittent-music-crackle.md).
