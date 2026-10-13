# Code licence, third-party components and audio restrictions

This prototype is not a public release package.

## LISAccess code

Original LISAccess code and documentation are covered by the MIT licence in
`LICENSE`, copyright 2026 LordLuceus. The staged/installed copy is `LICENSE.txt`.
This licence does **not** cover ElevenLabs-generated sound effects or third-party
components. Their respective terms and obligations remain in force.

## Prism and bundled source

`dinput8.dll` statically links source-built Prism v0.16.5, pinned to
[`c30d865faf9b4c38769cc58c2f33cbac78fba3ef`](https://github.com/ethindp/prism/tree/c30d865faf9b4c38769cc58c2f33cbac78fba3ef).
The Windows build uses standard backends with legacy backends, Godot extension,
demos, tests, shims and Winelib builds disabled. It does not redistribute a
separate Prism DLL or `nvdaControllerClient32.dll`. The Windows SDK MIDL compiler
builds NVDA RPC stubs from Prism's `idl/nvdaController.idl` and `.acf` into Prism;
`rpcrt4` is a Windows dependency, not an external NVDA controller binary.

The pinned CMake/source inventory includes:

- **Prism:** Mozilla Public License 2.0 (MPL-2.0).
- **simdutf:** Apache-2.0; its amalgamated header also contains a PyTorch-derived
  CPU detection BSD-3-Clause attribution. Both notices are staged.
- **Moderncom:** MIT, Alexander Bessonov and contributors; Windows COM helpers.
  Both the license and AUTHORS file are staged.
- **dr_wav / dr_libs:** David Reid; upstream Prism selects the public-domain
  alternative. The complete upstream dual-license text is retained.
- **concurrentqueue:** Cameron Desrochers; upstream selects Simplified BSD.
  The complete upstream dual-license text is retained; Windows UIA uses this code.
- **NVGT utility code:** Copyright 2022-2025 Sam Tupy, zlib-style license,
  in `source/backends/utils.h` and `.cpp`. Its license is staged even though
  Prism's top-level NOTICE does not list it.
- **NVDA controller RPC inputs/generated output:** Copyright 2006-2023 NV Access
  Limited, Leonard de Ruijter. The IDL retains LGPL-2.1 text but has an MPL-2.0
  SPDX header. Prism's pinned NOTICE explicitly states that permission was
  obtained to license those inputs and generated outputs under MPL-2.0.
  This build relies on that upstream statement; independent permission records
  have not been audited. The original LGPL text and RPC inputs are retained
  in the staged notices, not silently discarded.
- **Djinni:** Apache-2.0, Android-only and not linked in this Windows build;
  included solely to preserve the complete upstream LICENSES inventory.

Windows COM/SAPI/UI Automation and RPC are OS services. Prism also generates
import libraries for optional screen-reader/bridge DLLs (ZDSR, Boy PC Reader,
PC-Talker and Wine bridges); these are not copies of those vendor runtimes and
this installer does not distribute them. Their presence in delay-load imports
is not a claim that all runtime dependencies are statically bundled. Legacy
System Access/Window-Eyes backends are disabled. No vendor SDK/runtime license
is granted by these notices.

LISAccess makes one narrow covered-source modification: ordinary SAPI text uses
`SPF_IS_NOT_XML` at neutral pitch, retaining the earlier mod's literal-text
behaviour. Upstream's explicit pitch/XML path is unchanged. The reproducible
recipe is `extern/prism-sapi-literal.cmake`; CMake builds the modified copy at
`build-x86/extern/prism-sapi.cpp`, leaving the pinned source archive untouched.
The recipe is included in `Prism-NOTICES.txt`. A corresponding-source release
must include this modification, not just link to pristine upstream sources.

### Deterministic notice staging and installation

The normal build stages these notices automatically. To regenerate them manually
after configuring the pinned source build, run:

```powershell
powershell -NoProfile -File scripts/stage-prism-notices.ps1
```

The default source is `build-x86/_deps/prism-src`; an explicit `-PrismSource`
may point to the same clean pinned checkout used by a source override. Extracted
archives are verified against a deterministic source-tree fingerprint; Git
checkouts must have the correct revision and unmodified tracked files. The script
rejects altered dependency sources and generates fixed
UTF-8/LF files without timestamps in `build-x86/Release/notices`:
`Prism-NOTICES.txt` (upstream notices, full license inventory, RPC attribution
inputs and embedded BSD notice) and `THIRD-PARTY-LICENSES.md` (this document).
Stage again whenever this document changes. Ship both files with the DLL.

The installer requires both staged files and installs them under
`LISAccess/notices` with hash ownership. Removal recognizes only those two
notice paths, not arbitrary files or whole notice directories. Changed notices
block replacement/removal just like changed binary files. Upgrades delete the
old external controller only when a unique old manifest entry matches its
current SHA-256; changed/unowned legacy clients stay on disk, are explicitly
reported, and are not owned by the new Prism installation.

### Source and redistribution obligations / remaining limits

Notice staging alone is **not** a complete public distribution compliance kit.
For MPL-2.0 executable distribution, make the corresponding covered source
(including changes) available under MPL-2.0 and tell recipients how to obtain
it in a reasonable, timely manner. The pinned upstream source link above
identifies the unmodified dependency; preserve an exact source snapshot,
local patches, generated-source inputs and reproducible build instructions for
any release. This script stages notices, not source archives or a source offer.
MPL's file-level obligations do not by themselves require relinkable object
files for the rest of a larger work merely because Prism is statically linked.

Do not infer that all NVDA code is generally MPL-licensed: reliance here is on
Prism's explicit statement for these particular RPC inputs/generated outputs.
If that permission cannot be relied on, or an LGPL-2.1 controller implementation
is substituted, review LGPL-2.1 sections 4 and 6 before distribution: supply the
corresponding library source and changes, applicable notices and license, allow
modification/reverse engineering for debugging those modifications, and use a
permitted linking arrangement. Static linkage generally requires sufficient
application object/source material and tools/instructions to relink against a
modified library (or another valid section 6 option). A license file or upstream
URL alone does not satisfy those obligations. No such LGPL relinking kit or
verified written offer is produced here. The formerly reused external controller
binary had unverified source provenance; it is no longer a packaging input.

Before public release, verify the final link/import map and actual packaged
files, preserve traceable corresponding source and any required relink materials,
and review vendor/SDK permissions and the upstream relicensing basis. These
notes describe inspected source, not a legal certification of a built artifact.

## Navigation audio

Playback uses the Windows-provided XAudio2 runtime; no XAudio2 redistributable is
bundled. The four cues were generated using the developer's authorized ElevenLabs
CLI access. Prompts and processing are recorded in `assets/audio/provenance.json`;
original provider bytes and processed WAVs are retained as private build inputs,
not public repository files.

The raw PCM response did not identify channel count, so the original channel
interpretation remains provisional; these are processed placeholders, not a claim
of exact provider-original timing or pitch. Listening feedback will decide whether
to keep or replace them. These assets are not public-domain audio.

[ElevenCreative Terms, Exhibit 4: Sound Effects](https://elevenlabs.io/creative-terms)
permits incorporating Sound Effects output into games and applications, subject
to the underlying agreement and Prohibited Use Policy. Commercial use is allowed
only where the applicable plan permits it. The same exhibit restricts standalone
exploitation and making output available as isolated audio files, samples or sound
libraries, apart from its stated Community Sharing or express-permission exceptions.

LISAccess distributes processed cues only as functional parts of complete mod
release archives, combined with the mod's code and other material. It does not
publish a standalone sound pack, individual audio downloads, raw PCM outputs or
audio binaries in public Git history. Release archives must carry this notice and
the MIT and third-party licence notices. The code's MIT licence does not grant
independent reuse rights to these sounds; do not redistribute them separately
without rights permitting that use. This distribution policy does not waive any
underlying ElevenLabs terms or applicable commercial-plan restrictions.

## Research-only software

Dissect's Apache-2.0 LZO decoder and JPEXS FFDec (GPL-3.0-or-later) were used to
inspect local assets. They are kept with provenance/licence materials in ignored
research artifacts and are not linked into the prototype DLL. Ghidra was used for
static analysis and is likewise not a runtime dependency.

Extracted game metadata, Flash scripts and decompilation are local research records,
not release assets. The mod must not redistribute them or the game's executable.
