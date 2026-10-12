# Third-party components — private prototype

This prototype is not a public release package.

## NVDA controller client

Speech loads `nvdaControllerClient32.dll` dynamically from beside the mod. The
private test installation reuses the x86 controller client already present in the
developer's Phoenix Wright accessibility-mod release. Its SHA-256 is
`a0d193883dcfbeae6e69aa00a0c5aa52a194b1a265617b8f920d0c7d4164c587`.

The [historical NV Access controller API documentation](https://github.com/nvaccess/nvda/blob/release-2010.1/extras/controllerClient/readme.html)
identifies the controller client as LGPL-2.1. The [NVDA source repository](https://github.com/nvaccess/nvda)
is maintained by NV Access and contributors.

The local binary's exact source revision/authenticity has **not** been independently
verified. Before public distribution, obtain a controller build with traceable
corresponding source and include its exact licence notices and source-access
information. Do not describe this private reused binary as a verified official
release. The installer requires an explicit local client path and does not download
or overwrite an unrelated installed controller.

SAPI fallback uses the Windows-provided speech API; no SAPI runtime is bundled.

## Research-only software

Dissect's Apache-2.0 LZO decoder and JPEXS FFDec (GPL-3.0-or-later) were used to
inspect local assets. They are kept with provenance/licence materials in ignored
research artifacts and are not linked into the prototype DLL. Ghidra was used for
static analysis and is likewise not a runtime dependency.

Extracted game metadata, Flash scripts and decompilation are local research records,
not release assets. The mod must not redistribute them or the game's executable.
