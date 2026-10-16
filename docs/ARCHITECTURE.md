# LISAccess source architecture

Native offsets, validation, controls and announcement wording are independent
of the output backend. Speech and braille now use a pinned, source-built x86
Prism library; the migration does not add gameplay features or dialogue choices.

Cached UObject addresses are not type identities: the engine can recycle a former
menu address into scenery. `world/menu_state.h` revalidates `SensenMenuBase` before
reading menu-only state. Dialogue and world blocking share this guard; diary
discovery uses the same instance check. Genuine menus with unreadable state still
block input. The regression includes the captured recycled actor and Alyssa's
existing dialogue bindings, without adding an NPC-specific reader.

## Modules and ownership

### Manual guided beacons

`navigation/route_*` contains the engine-free flat-floor planner, calibrated body
sweeps and manual waypoint follower. These reuse the pure geometry work from the
shelved autowalk experiment, not its movement hooks, input observers or binding
machinery. No autowalk source is part of this build.

`guided_scene` refreshes the existing player/menu/control gates on the window
thread and calls the existing verified `Actor.Trace` adapter. Fast refreshes reuse
completed discovery metadata; normal world sampling still drives discovery. A
route sample rejects changed identities, dimensions or player position during its
queries, bounds queries and age, and latches native faults only for guidance.
Navigation traces do not reproduce the native camera visibility check. Temporary
per-query recording and actor-path dumps have been removed; pure planner state
and failure reasons remain available to regression tests.
Interactive goals use the authored inner-zone radius: positive FSM override,
then positive actor override, then the interaction component's radius. Missing
or invalid metadata rejects routing without hiding the scanner target. The
radius is a strict XY bound, not a complete native focus predicate. The guide
checks the floor route and standing-body clearance but does not cast an invented
body-to-target visibility ray. Actual native focus plus action labels distinguish
Arrived from Approach. Approach-only guidance can end across a wall or outside
the required viewing angle; camera-aware endpoint selection is deferred pending
player feedback, not an active research task.
Exit goals retain the real trigger-volume extents rather than treating an elevated
brush centre as a different floor. Reaching an exit requires actual volume
membership, not merely proximity to its centre; it does not activate the exit.

`guided_beacon` carries worker requests and copied results across a mutex mailbox.
Native sampling runs outside that mutex on the game thread. Request generations
prevent cancellation or selection changes during a query from publishing old
waypoints. The existing 50-ms feedback timer advances guidance; the 200-ms world
timer also advances it as a fallback. The worker rejects stale output and never
calls the planner or engine. `beacon_control` owns device-specific announcements
and consumes route results for audio; the direct-target setting bypasses routing.
Reported route failures cancel planner work and latch announced direct-bearing
fallback for that selection, with a one-second cue delay. Retargeting or toggling
off/on clears fallback and retries. Inactive/blocked navigation and expired
waypoint output still stop; fallback never reuses an obsolete waypoint.

Planning permits 64 bookkeeping/probe operations per slice, still capped at
1.5 ms (a running native callback cannot be preempted). The old eight-operation
ceiling spread 10.6 ms of measured search work over 12.8 seconds. A worst-case
slice needs one calibration trace plus three traces per operation: at most 193,
statically checked against the unchanged 256-query native capture cap. Search
order, overall limits and geometry checks are unchanged; slow callbacks still
yield at the time limit. This is a scheduling improvement, not a new route solver.

The route is advisory, not a movement command or guarantee of safety. Floor
sampling cannot exclude sub-sample gaps; target and scanner data refresh less
frequently than player geometry. Routes cover flat local terrain only. In-game
coverage and waypoint ergonomics still need broader playtesting; initial indoor
and outdoor approach-only playtests have been positive.

- **`bootstrap/`** owns DLL entry/export forwarding, executable verification
  coordination, the window hook, timer and worker loop. It connects services and
  features; it must not collect their implementations.
- **`platform/`** owns logging, foreground-process detection and text conversion.
  It knows neither game objects nor accessibility features.
- **`engine/`** owns bounded memory reads, object registries, reflected properties,
  the verified build profile, the GFx query bridge and localization. It knows how
  to access game data, not which accessibility feature should announce it.
- **`input/`** owns keyboard/XInput polling and command decoding. It returns
  commands rather than executing game actions or producing speech.
- **`output/`** owns speech-backend lifetime and audio-cue playback. Backend changes
  belong here, not in individual menus or scanner features.
- **`features/`** owns player-facing decisions. Menus, prompts, dialogue, world
  sampling, navigation, interaction scanning/actions and explicit camera facing
  have their own files.

CMake lists each module's sources explicitly and compiles the reusable modules as
object-library targets. The final `dinput8` DLL contains those modules; this does
not add runtime DLLs or change installation layout.

Prism is configured separately in `extern/`. Its whole-archive interface retains
backend registrations; the raw static archive alone is insufficient. The output
adapter checks runtime availability rather than trusting a cached best backend.
Speech and braille use separate return values, so a failed braille operation
cannot replay speech. The native bridge catches reader/RPC exceptions outside
frames containing C++ objects. Backends are freed before their context; all of
that lifetime stays on the output worker. The backend decision is generic, not
a list limited to NVDA and JAWS. A reproducible SAPI compatibility patch preserves
literal text; the upstream snapshot stays pristine for notice verification.

## Important boundaries

### Game-thread access versus worker output

Menu/prompt/dialogue snapshots, world queries, explicit choice/interaction activation and camera requests retain
their game/window-thread execution. The worker polls input, narrates copied
snapshots and plays sounds. Requests cross the existing locked handoff and are
revalidated on the game thread; moving files is not permission to call native
engine functions from the worker.

`features/world/session.h` is private game-thread lifetime state for discovery,
cached native functions, reflection and fault latches. Public feature entry points
do not expose it. `features/world/world.h` describes the copied snapshot interface.
There are no new arbitrary native-call or action commands in diagnostic IPC.
The reflection cache and input-device owner are noncopyable. Scanner reader
adapters borrow the cache rather than copying its property table for every target.

### Validation and lifetime are behaviour

`features/journal` attaches to the separate native `SensenMenu` movie, not the
frontend `OutGameSensenMenu`. The world discovery list now treats the diary as a
menu for every gameplay consumer. A diary may own input even when its page has
no readable text. Snapshots and UI dispatch run on the game thread; worker-side
cursor and controller policy use copied values and expire stale input.

The pure diary reader consumes the current Flash page's `PageElements` and
selection helpers. It never enumerates static journal/SMS definitions. SMS
`FadeInSMS` and raw response arrays are excluded; the current element's typing
timer/fade flag must permit reading before its body is extracted. Completed
offscreen history can use native localization; unavailable rendered dates are
omitted. Current journal text can likewise fall back to its supplied localized
resource, but only for visible text elements. Images are not guessed or OCRed.

Book text uses the supplied `PositionX/PositionY` to read the left page top-down,
then the right page. Unknown-position items retain their slots. Real principal and
Victoria layouts are regression fixtures, identified by unique export paths/indices
(short UE3 export names are not unique). Native prequel day `-1` is valid; negative
page indices and other invalid day values are not. `CMD_OBJECT_VIEWER` shares the
working `CMD_FILESELECT` text-view extraction, but has inspection-specific wording
and controls. Text is read only after the native description view opens. Both
contexts continue blocking gameplay input, including when not yet readable.

GFx `GetVariable` returns a typed ASValue; missing numbers stay NaN rather than
silently selecting index zero. Native reflection checks its 36-byte parameter
layout and return offset. Tab commands expire after 450 ms, re-read the owning
movie/page, and mirror the original focus-stack key dispatch (including its
nonrepeat pass) with keyboard index zero and `from_joystick=false`. Native handlers
retain loading, page and tab restrictions. Covered pages may be
`DNEMenuPageFocusProxy` objects with no key handler: AS undefined/null are normal
false results, not bridge errors. Typed dispatch results separate that fall-through
from invalid native receivers/functions and unknown value tags. Regression tests
cover proxy chains and a loading page consuming input before the root handler.
`ASValue.Type` is a one-byte reflected field, not a 32-bit enum; `B` at offset4
uses bit mask1. Native copies can carry uninitialized padding/unused bits, so
`ScriptValue` models the byte and padding explicitly and Boolean reads mask bit0.
The dirty-padding regression uses the reflected24-byte layout, not zero-filled
synthetic scalar results. Native tab switching passed the user's retest; the
successful trace did not contain dirty padding, so it does not establish that
padding caused every earlier failure.
Page numbering is appended only to the page-entry announcement, not the shared
paragraph/repeat description.
The reading cursor is deliberately
separate from pixel scrolling. Source-derived tests and stubbed lifecycle tests
do not certify the live native binding or its visual-scroll behaviour.

Dialogue reuses the world's reflected metadata cache, not its traversal or physics
gate. It reads the current actor's filtered UI arguments and sparse binding map,
then localizes those offered keys through the game's existing localization path.
It never enumerates all stored possible answers. Its native reader has a separate
fault boundary. Pause/menu/widget flags, a verified player/actor relationship and
normal rewind state gate availability; actor/node/page, rewind sequence and
bindings identify the request's exact context. Number-key requests expire after
450 ms and are re-read on the game thread immediately before the verified input
listener receives a choice. Native controller buttons are not synthesized.

The policy tests pin the captured Jefferson question and sparse map, including its
empty Y/A slots, and check changed/hidden/unreadable choices. These tests do not
prove the full live native path: timed questions, rewind, transitions and action
delivery beyond the accepted initial playtest still require verification.
Detailed dialogue diagnostics default off after that acceptance.

Dialogue uses `WhatIfInput.bPlayingWithGamepad`, as object actions do, to announce
only the appropriate device's bindings. The current repeat/help command can
override an older native sample immediately. Device mode affects presentation,
not choice identity or activation authority.

The menu and world readers previously used different registry bounds. Their
implementations now share `engine/object_registry`, but explicit `Menu` and
`World` policies preserve those differences. Do not silently weaken world
validation or tighten menu discovery while consolidating code. Regression tests
exercise both policies against synthetic memory inside the test process.

Reset remains deliberately partial. Native faults stay latched; thread binding,
translations and certain cached handles retain their original lifetime. Review
those contracts before changing ownership or reset logic.

The GFx bridge retains its menu-bound entry points and adds explicit verified movie
receivers for announcement queries. Its narrow GFxObject string getter retrieves
clip paths; visibility uses the typed boolean/number getters rather than assuming
string coercion. This is not an arbitrary native-call interface. Native ABI
declarations live in `engine/abi.h`, and verified build RVAs live in
`engine/build_profile.h`.

`features/prompts` has an independent game-thread reader/fault boundary, so a
tutorial can be read while the world-navigation gate is closed. It reads only the
inspected tooltip, quote, prompt, notification and animated-caption channels, not dialogue or
subtitles. Reflected native state and actual Flash visibility gate the readout.
The shipped `DNEOpenAnimationSeq` has its own opening-state enumeration; do not
substitute the menu/native enumeration. Its flat `open_seq.sub_sequences` stores
clip references, including anonymous composition children and decorative shadows.
Popup and prompt extraction tests model that structure; they do not prove that a
live receiver or path is correct. Startup and photo/selfie acquisition have also
passed in-game testing. Detailed prompt-state diagnostics are retained behind
the default-off `TracePrompts` flag for future investigation.

Controller caption restoration reads the live `DynamicInputSwitchManager` mode
and `GamepadActionList` (reflected 20-byte FName/FString rows), not connected-device
presence or an INI default. The shipped table covers all ten action-token names
found across the seven installed HUD translations. `DNETooltipRegister` establishes
the abstract glyph-to-XInput button vocabulary. No individual tutorial sentence
selects a binding. Keyboard mode retains the original rendered caption.

For a visible controller caption, its already-selected `wanted_text` is localized
fresh through `Object.Localize`, using the verified movie receiver's UObject
dispatcher rather than an Actor dispatcher. Source prose with controls removed
must agree with the visible text (ignoring embedded/private-use glyphs) before
restoration; mismatches or missing action mappings retain the visible message and
report unavailable control names. This does not repair missing source IDs or failed
localization. Bounds and recursion limits reject malformed/cyclic descriptions.
Real English/French/German tutorial fixtures, authored action mappings, changed
runtime bindings, keyboard preservation, visibility and repeat are covered by tests;
the native metadata/localization path has also passed its in-game playtest.

Pure extraction and history policy retain timed prompts for repeat and detect
display/text transitions. Exact tutorial IDs select accessibility wording; other
messages keep rendered localization. Match HUD.INT's actual `[Tuto]` section;
`DNENotification` uppercases device-dependent IDs. Adapt each text leaf rather
than replacing a whole notification container with its first label's rewrite.
Widget `Owner` supplies the query receiver, with the original discovery receiver
retained as a fallback. Worker output queues automatic prompts after
menu/scanner announcements. Repeat interrupts without invoking a game action.
The boot page is not a selectable menu: `CMD_BOOTSEQUENCE.SequenceRoot_mc` is
replaced for each timed slide. Read that composition and queue the readouts,
including the automatic title transition, without delaying or advancing the game.
The controller repeat chord has a separate availability flag: allowing it while
navigation is paused must not enable facing or other scanner commands.

## Keeping modules small

Use one responsibility per implementation file. A directory containing a giant
dispatcher is not modular. Prefer narrow interfaces and explicit inputs over a
shared header full of mutable globals or an ever-growing context class.

The build script runs the architecture check before configuring CMake. It rejects
loose root-level C++ files, forbidden lower-layer
dependencies and source/header files over **350 physical lines**. This budget is
a review trigger: do not compress statements or split a cohesive algorithm
arbitrarily to satisfy it. If a larger file is genuinely appropriate, discuss and
document the reason before revising the budget.

Allowed quoted includes are root-relative. Platform depends only on platform;
engine, input and output may each depend on themselves and platform. Features may
use those lower layers and other feature interfaces. Only bootstrap may assemble
all modules. Third-party/system headers use angle-bracket includes.

This check is intentionally limited: it checks direct module include declarations
(including angle-bracket attempts) and file size, not semantic cohesion, hidden
transitive coupling, native ABI safety,
or in-game correctness. It does not make a small class well designed by itself.

## Validation

### Rewind feedback

`features/rewind` samples the real gauge on a separate 50 ms window-thread timer;
it does not increase collision/scan sampling frequency. It reuses world reflection
metadata but not the traversal gate. Manager/controller/world identity, pause/menu
ownership, renderer readiness and the actual RewindBar clip visibility gate output.
The supported binary's native writer (RVA `0xae5b10`) computes fill from
`ReferenceTime / (bForceRewindBarVisible ? OverriddenEndTime : RewindEndTime)`;
the unnamed marker comes from native `LastPlayerAction` pointer +4, NOT a reflected
payload at +0. Reads are bounded/guarded; invalid denominators are not zero progress.

Pure policy retains the starting marker's copied timestamp across marker replacement,
warns once within a five-percent band and reports a reached/passed boundary once.
Recent idle samples can detect a fast first jump; state2 alone never starts rewind
(scripted choices use it too). State3 defers completion. Loss of context, identity or
freshness cancels rather than fabricating a stop. Queue age is independent of snapshot
freshness. A falling boundary cue supersedes an unplayed approach cue. Fast operations
wholly between samples cannot be guaranteed; there is no game-input injection here.

Worker-side output uses a separate rewind audio group, so navigation disabling itself
cannot flush marker sounds. Focus/menu loss flushes rewind audio. Both groups share
the existing volume. Synthesized defaults require no extra installed assets; optional
valid replacement WAVs are user-owned. Policy, queue lifecycle and PCM generation are
tested without game/audio output. The native reader, cues and readout have passed
playtesting; temporary state/event diagnostics have been removed. A completed
readout is remembered by existing prompt history, not a new key.

Explicit sound preview owns one selected voice independently of automatic audio
groups. Disabling navigation/rewind cannot flush that preview; focus loss or leaving
an allowed preview context cancels it. The cycle and label coverage derive from
the cue enum's count; playback completion, not a fixed timeout, ends a preview.
This preview extension still needs its own in-game check; the rewind feedback
itself is already accepted.

### Commands

From the repository root:

```powershell
powershell -NoProfile -File scripts/build.ps1
ctest --test-dir build-x86 -C Release --output-on-failure
```

CTest includes the original narration, geometry, ABI, input and reflected-field
tests, plus object-registry policy and extracted narration fixtures. Speech/braille
routing uses fake providers to cover native RPC faults and reader restarts without
producing speech or requiring a reader. CTest also runs
the architecture gate and a separate test that deliberately injects oversized
files, forbidden dependencies and loose source placement to prove rejection.

To run just the structural check:

```powershell
powershell -NoProfile -File scripts/check-architecture.ps1
```

The installer regression script uses an isolated fixture, not the real game
directory. No build or structural check installs a DLL, launches the game,
selects a save or performs an interaction. Passing these checks does not replace
in-game verification of a refactored build.
