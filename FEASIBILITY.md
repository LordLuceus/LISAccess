# Original Life Is Strange: accessibility-modding feasibility

This is the initial static investigation. Subsequent prototype work has begun;
its current runtime/build status is documented separately. Statements below about
not launching/injecting describe this original investigation, not later testing.

Investigated 12 October 2026. **Verdict: a credible, worthwhile target for a native
screen-reader mod. Proceed with a bounded in-game proof of concept, not a promise
that the whole episode is already solvable.**

The shipped game exposes substantially more useful information than screenshots:
semantic interaction objects, action labels and eligibility fields, dialogue-choice
data, timers, rewind state and localized text. The substantial work is obtaining
safe live access to those systems and making exploration and timed sequences playable.

## Scope and identity

- Original Steam game, app 319630, installed build **2304417**; not the remaster
  and not Before the Storm.
- Inspected the supplied Episode 1 installation, without launching the game,
  injecting a DLL, changing saves, or modifying its installation.
- `Binaries/Win32/LifeIsStrange.exe`: native **x86 PE**, 20,134,400 bytes,
  preferred image base `0x00400000`; no managed CLR header.
- SHA-256: `0973a7e193728088ed438c02dd1f99b490eb35355ad8be0f2240a3d2690fa233`.
- **Unreal Engine 3**, with Dontnod's custom game classes and Scaleform/GFx UI.
  Package format version 893, licensee version 21; package engine version 8623.
- 287 `.upk` packages. Their LZO compression is readable; it was not an encryption
  barrier. The many `.bin` files include shader binaries, not evidence of a separate
  gameplay scripting runtime.

BepInEx, MelonLoader, Harmony and UE4SS are not drop-in frameworks for this build.
A native 32-bit DLL is the practical starting point. The original Before the Storm
uses a different engine; its existing accessibility mod is not a compatible base.

## Evidence strength

Three kinds of evidence were kept distinct:

1. **Direct shipped-data observations:** executable headers/imports, localization,
   validated package name/import/export tables, actual class/function/property
   definitions, and scene actor exports.
2. **Targeted native static analysis:** four functions identified through native
   registration name/pointer pairs and decompiled with Ghidra.
3. **Proposed runtime integration:** not yet exercised. A class definition or a
   decompiled wrapper does not prove a safe hook, an active instance, a correct ABI,
   or that every visual update passes through that wrapper.

The package reader validated table boundaries, name indices, references and LZO
block sizes. All four framework/game packages and all **37 Episode 1 scene-root
and gameplay-layer packages** in the selected set parsed successfully. Initial
UTF-8 decoding of legacy 8-bit names failed on one package; the reader now preserves
those bytes losslessly instead of treating them as UTF-8.

## Per-feature assessment

### Interaction discovery and action selection: strong candidate

This is the strongest concrete lead. `DNEWhatIfInteractionComponent` declares:

- `RegisteredActors`
- `ActorInteractionFocused`
- `CurrentInteraction`
- `CurrentDialog`
- Inner, outer and far-zone radii, plus screen-centre thresholds.

`WhatIfInteractionActor` declares an `ObjectName`, an `Actions` array,
`bIsInteractionActive`, per-action enabled flags, and current/pending action state.
Each `SAction` includes `ActionName`, `OverrideDisplayName`, `OverrideActionButton`,
`bEnabled`, fact/actor conditions, angle restrictions and rewind-related flags.

The displayed interaction widget has four action-name strings, four visibility
flags and an interaction-name string. Its `SetActionNames` function takes four
button-index/name pairs. `SetInteractionObjectName` supplies the object label.

Across the 37 inspected scene/gameplay packages, there are **332
`WhatIfInteractionActor` exports**. The classroom layer alone has 26; the adjoining
corridor has 45. These are exported actor records, **not 332 simultaneously active,
unique or currently usable targets**.

This supports a general nearby-object scanner rather than a hand-written list of
objects per room. Position/direction guidance and selection would belong in that
same scanner. Runtime filtering must use the game's real state: do not announce
disabled, hidden, future or conditionally unavailable interactions from static data.

**Control barrier:** the shipped tutorial explicitly describes holding interact,
dragging the mouse and releasing to choose a contextual action. The alternate
keyboard interact key does not, by itself, solve action selection. A mod must expose
the currently valid actions through keyboard controls and route activation through
the real game input/action logic. `SetButtonClicked` is not proven to activate a
gameplay action; it may only update presentation.

### Dialogue choices and subtitles: strong candidate, state-sensitive

`WhatIfDialog.SDialogChoiceDef` contains `ChoiceText`, `OverrideButton`,
`bIsDefaultChoiceOnTimeOut`, conditions and an outgoing link. Its choice node has
`DialogChoiceDefs` and `ChoiceTimeOut`.

`WhatIfDialogActor` exposes current asset/node/phase/wave/choice/page state,
`BindingChoiceIndexMap`, `TotalChoiceTimeout`, `CurrentTimeout`, `CuePlaying`, and
callbacks including `OnDialogChoiceChoosed(ChoiceIndex)` and timeout callbacks.
There are **32 dialogue-actor exports** in the inspected scene/gameplay set.

The English files contain 246 `[Dial]` assignments in `HUD.INT` and 2,068 cue
assignments across eleven `CU_E1_*.INT` files, plus 90 generic Max cues. These counts
are assignments, not a count of unique on-screen choices or guaranteed spoken lines.

The mod should observe the active choices and their actual binding/order, not read
every choice in the localization database. Text access is promising. Matching cue
timing, avoiding speech over voiced dialogue, and detecting choices that disappear
or time out still require a live test.

### Menus and settings: promising, focus extraction still unproven

Readable menu/settings labels and keyboard arrow/accept/cancel mappings are shipped.
The game package contains `SensenMenuBase`, `OutGameSensenMenu`, `PauseMenu` and
`GFxMoviePlayerManager`. The latter has `MoviePlayers`; menus expose their root
movie clip and modal/opening state.

`GFxUI.upk` includes object text getters/setters, variable getters, ActionScript
invocation and widget/focus callbacks. **Movie focus is not the same as the selected
menu item.** The actual ActionScript variable/callback carrying item focus, settings
values and confirmation-dialog selection has not yet been identified at runtime.

This is a semantic GFx integration candidate, not a reason to begin with OCR.

### Journal, messages, objectives and readable objects: promising but incomplete

English `Menus.INT` has 15 objective, 156 diary, 126 journal and 36 SMS assignments.
`Sensen.INT` supplies 91 action, 583 interaction and 63 character assignments.
These show useful text availability, not complete coverage of every illustrated
page or physical object. Some base/shared localization covers later content; it
does not prove that later episodes are installed or supported.

Read only unlocked/current entries. Picture-based clues, photographs, handwriting
or environmental storytelling may need authored descriptions where no equivalent
text exists. No image-content completeness audit has been performed.

### Rewind and timed sequences: plausible, more complex

The controller exposes current/last rewind values, rewind-enable state, current
gameplay type and `RewindLastAction`. A separate rewind manager and dialogue rewind
structures exist. Dialogue timeout state is explicitly represented.

This offers a route to meaningful rewind/status announcements and potentially
timing assistance. It does **not** establish that modifying a timer or pausing one
subsystem preserves every narrative sequence. Accessibility event caches must reset
or resynchronize after rewind, scene changes and save/load; otherwise speech will
describe a future state that no longer exists.

A dedicated live rewind/timed-choice test is essential. Do not promise a blanket
“disable timers” option from property names alone.

### Navigation: largest unresolved blocker

UE3's generic engine package contains navigation classes/functions. However, the
37 inspected Episode 1 scene-root/gameplay packages contain **no exports whose
class names match the surveyed standard navigation/path-node/pylon/navmesh families**.
That is a bounded negative result, not proof that no custom or embedded path data
exists anywhere in the game.

Do not promise automatic walking simply because stock UE3 supports pathfinding.
Live collision queries, movement/camera state and route data need investigation.
Direction and distance to an object alone do not solve getting around obstacles.

There is also an immediate scope trap: the opening scene root and its cliff gameplay
layer contain **zero normal interaction-actor exports**. The opening localization
explicitly directs the player towards a lighthouse. An interaction-only scanner
would therefore not cover even the first traversal. Non-interactive goals,
landmarks, triggers and safe routes must be covered too; a speaking classroom is
not proof that a new game is playable.

## Native access route and what was actually verified

The executable imports `DINPUT8.dll` and `XINPUT1_3.dll`. A reversible x86 proxy/ASI
loader is a plausible entry route, subject to a real launch/forwarding test. No
loader was installed, and native injection has not been verified on this build.

The executable retains native registration strings with adjacent code pointers.
Four examples were resolved and decompiled:

| Registration | Preferred-base VA | Observed wrapper dispatch |
|---|---:|---:|
| `UWhatIfInteractionWidgetexecSetActionNames` | `0x00d3cd50` | Virtual slot byte offset `0x1b0` |
| `UWhatIfInteractionWidgetexecSetInteractionObjectName` | `0x00d3c6e0` | `0x19c` |
| `AWhatIfDialogActorexecOnDialogChoiceChoosed` | `0x00d41050` | `0x4b8` |
| `AWhatIfControllerexecGetRewindManager` | `0x00d3e690` | `0x6b8`, stores returned object pointer |

These are build-specific static addresses, not ASLR-safe hook code. Labels were
assigned from the registration table, not recovered debug symbols. The decompiler
has unresolved calls and inferred/incorrectly reconstructed parameter details;
its C output must not be copied as a verified ABI. Package metadata independently
identifies the action-name and choice-index parameters.

`GObjects`, `GNames`, object/property memory layouts and `ProcessEvent` have **not**
been located and validated in a running game. A mod may use reflection plus focused
native/GFx hooks; intercepting `ProcessEvent` alone is not guaranteed to see direct
native UI updates.

### Tool outcome, including limits

- REA admitted the exact executable as an x86 PE and recorded its matching SHA-256.
- The first provider-defined string query triggered whole-program Ghidra analysis
  and timed out after approximately **600.7 seconds**. No function analysis was
  obtained from that REA request.
- Switched to ordinary Ghidra headless tooling, **not REA evidence**, importing a
  byte-identical temporary copy with `-noanalysis -readOnly -deleteProject`.
- Four targeted wrappers decompiled successfully in a **26.2-second invocation**.
  All four explicit success markers were checked; Ghidra can otherwise exit zero
  even after a script error.
- Its Windows batch launcher mishandled the original parentheses-containing path;
  the verified temporary copy avoided that. Equals-sign arguments were also split
  by the batch launcher; the research script uses `address:label` instead.
- The REA target was closed. The idle console helper required interruption after
  successful close. This helper is experimental research tooling, not a mod loader.
- No gameplay, injection, save/load, screen-reader output or playthrough was tested.

## Recommended first proof of concept

**Go ahead with a small diagnostic native prototype. Do not start a broad feature
implementation until these checks pass.**

1. **Load and semantic readout:** load/unload safely, speak a startup confirmation,
   identify the current scene/player, and read a focused menu item's actual label
   and value. Fail closed on an unrecognized executable rather than using stale offsets.
2. **One real interaction and choice:** expose the current object and only its valid
   actions; select one using ordinary game rules. Speak one real dialogue choice
   set and verify the chosen index. Exercise both before and after a rewind.
3. **Early navigation gate:** demonstrate reaching a non-interactive opening goal
   and an interaction target around an obstacle. Establish whether native route data
   exists; if not, price a general collision-aware or authored-landmark solution
   before promising full Episode 1 playability.
4. **State/lifecycle gate:** verify a timed choice, scene transition, save/reload and
   rewind without stale speech, wrong targets, blocked input or crashes.

This is materially harder to bootstrap than a managed Unity mod, but not a blind
reverse-engineering gamble: we have concrete, validated semantic structures and
native entry-point candidates. The unresolved cost is reliable runtime integration
and playable navigation, not translating pixels into text.

## Local research records and sources

Raw extracted metadata/decompilation stays under gitignored `artifacts/`; it is not
intended for redistribution. Reusable research helpers and this report accompany
the prototype source; extracted evidence stays local.

- `artifacts/pe-survey.json`: headers, import names, relevant strings and identity.
- `artifacts/rea/001-open_binary.json`: actual REA open result (no Evidence ID was
  returned by that operation). `002-search_strings.json` retains the timeout.
- `artifacts/packages/`: validated tables, including complete symbol paths.
- `artifacts/scene-summary.json`: per-package actor counts for the 37-scene set.
- `artifacts/native-probe.txt` and `ghidra-probe-success.log`: targeted Ghidra output.
- Shipped `LifeIsStrangeGame/Config/DefaultInput.ini` and `Localization/INT/`.
- [UEViewer's UE3 package serialization](https://github.com/gildor2/UEViewer/blob/master/Unreal/UnrealPackage/UnPackage3.cpp): format cross-check, not a claim that UEViewer decoded this investigation's data.
- [Dissect's LZO decoder at the inspected revision](https://github.com/fox-it/dissect.util/blob/2fbbb593b6b03e84adb35a8ca338e7ae20f9c4f4/dissect/util/compression/lzo.py): Apache-2.0 decoder used without installing a global package; source, licence and provenance retained locally.
- [Before the Storm Access](https://github.com/ObjectInSpace/BeforeTheStormAccess): a real related accessibility project, explicitly for the different Unity-based game.
- [CosmoemLIS supported-game list](https://github.com/LimeGradient/CosmoemLIS): the inspected README did not mark original Life Is Strange supported; it is not evidence of a ready-made original-game SDK.
