# LISAccess source architecture

Native offsets, validation, controls and announcement wording are independent
of the output backend. Speech and braille now use a pinned, source-built x86
Prism library; the migration does not add gameplay features or dialogue choices.

## Modules and ownership

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
- **`features/`** owns player-facing decisions. Menus, prompts, world sampling,
  navigation, interaction scanning/actions and explicit camera facing have their own files.
  A future dialogue feature belongs here, not in the bootstrap worker.

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

Menu/prompt snapshots, world queries, interaction activation and camera requests retain
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
