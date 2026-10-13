$ErrorActionPreference='Stop'
$checker=Join-Path (Split-Path -Parent $PSScriptRoot) 'scripts\check-architecture.ps1'
$parent=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/')
$root=Join-Path $parent ('lis-architecture-'+[guid]::NewGuid().ToString('N'))
if(Test-Path -LiteralPath $root){throw 'Fixture already exists'}
$utf8=New-Object Text.UTF8Encoding($false)
function ExpectFailure([scriptblock]$operation,[string]$reason) {
    $failed=$false
    try {& $operation}catch {$failed=$true}
    if(-not $failed){throw "Gate missed injected fault: $reason"}
}
try {
    New-Item -ItemType Directory -Path (Join-Path $root 'engine') | Out-Null
    $source=Join-Path $root 'engine\fixture.cpp'
    [IO.File]::WriteAllText($source,"#include `"engine/fixture.h`"`n#include <string>`n",$utf8)
    & $checker -SourceRoot $root -Quiet
    [IO.File]::WriteAllText($source,"#include `"features/world/world.h`"`n",$utf8)
    ExpectFailure {& $checker -SourceRoot $root -Quiet} 'engine depending on gameplay'
    [IO.File]::WriteAllText($source,"#include <features/world/world.h>`n",$utf8)
    ExpectFailure {& $checker -SourceRoot $root -Quiet} 'angle-bracket dependency bypass'
    [IO.File]::WriteAllText($source,((1..351 | ForEach-Object {'// oversized fixture'}) -join "`n"),$utf8)
    ExpectFailure {& $checker -SourceRoot $root -Quiet} 'oversized implementation'
    [IO.File]::WriteAllText($source,"// valid`n",$utf8)
    [IO.File]::WriteAllText((Join-Path $root 'loose.cpp'),"// invalid placement`n",$utf8)
    ExpectFailure {& $checker -SourceRoot $root -Quiet} 'loose root source'
    Write-Output 'PASS: architecture gate rejects forbidden dependencies, oversized files and loose root files'
} finally {
    $resolved=[IO.Path]::GetFullPath($root)
    if(-not $resolved.StartsWith($parent+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetFileName($resolved) -notlike 'lis-architecture-*'){throw 'Unsafe fixture cleanup path'}
    if(Test-Path -LiteralPath $resolved){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
