param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Release'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build-x86'
& (Join-Path $PSScriptRoot 'check-architecture.ps1') -SourceRoot (Join-Path $root 'src')
& cmake -S $root -B $build -G 'Visual Studio 17 2022' -A Win32
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
& cmake --build $build --config $Configuration
if ($LASTEXITCODE -ne 0) { throw 'CMake build failed.' }
Write-Output (Join-Path $build "$Configuration\dinput8.dll")
# Deliberately no copying to the game, controller DLL acquisition or game launch.
