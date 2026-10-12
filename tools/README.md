# Feasibility research helpers

These are read-only research tools, not an accessibility mod or a supported UE3
SDK. No game should be launched or modified by the commands below. See the
[feasibility report](../FEASIBILITY.md) for observations and limits.

## PE survey

`inspect_pe.py` accepts an executable path and JSON output path. It reads PE
headers/imports and selected ASCII/UTF-16 byte strings; it does not execute code.
It is a bounded investigation helper, not a hardened parser for hostile binaries.

## Package metadata

`inspect_ue3_tables.py` accepts a package path, output JSON path and `--lzo` pointing
to the separately licensed Dissect decoder. It supports the inspected game's
893/21 LZO package format, validates table boundaries and reads metadata, not
UnrealScript bytecode or object property values. Positive-length strings preserve
legacy bytes through Latin-1; this is not a locale/translation decoder.

The reviewed decoder is currently in `artifacts/vendor/dissect-lzo/`, alongside its
Apache-2.0 licence and source revision. The artifacts directory is intentionally
gitignored. A fresh checkout will need that dependency acquired and reviewed again;
these helpers do not silently download executable Python code.

Example, from the repository root:

```powershell
$game = 'C:\Program Files (x86)\Steam\steamapps\common\Life Is Strange'
python tools/inspect_pe.py "$game\Binaries\Win32\LifeIsStrange.exe" artifacts/pe-survey.json
python tools/inspect_ue3_tables.py "$game\LifeIsStrangeGame\CookedPCConsoleFinal\ExampleGame.upk" artifacts/packages/ExampleGame.json --lzo artifacts/vendor/dissect-lzo/lzo.py
```

Full JSON output contains proprietary game metadata. Keep it local; do not add the
artifacts directory to a public repository or release.

## Native tools

`rea-session.mjs` keeps one MCP connection alive for related calls. It uses the
existing agent-scoped REA configuration and its installed SDK dependency, not a
new project/global package installation. Pass the installed REA package directory
and an evidence output directory. Each JSON request names an advertised tool and
its arguments. `{ "schema": "tool_name" }` prints its input schema; `{ "quit": true }`
closes the target. On Windows console input, use carriage return for Enter.

This helper is experimental. The observed REA full-analysis timeout and idle-console
cleanup issue are recorded in the report. Do not assume it guarantees termination
or that arbitrary engine operations are read-only simply because it was used for
static analysis here. Only use requests whose target/effects are authorized.

`LiSNativeProbe.java` is a Ghidra script for selected known addresses, with output
path followed by `address:label` arguments. It was used in a disposable project
with `-noanalysis -readOnly -deleteProject`. It does not identify addresses itself.
The addresses in the report apply only to the recorded executable hash and preferred
image base. Ghidra's guessed signatures are not verified C++ declarations.

For that probe, use a byte-identical, temporary copy in a path without parentheses
to avoid the Windows batch launcher bug; remove the copy afterwards. Check every
`PROBE_OK` marker and script errors: a zero launcher exit code alone is insufficient.
