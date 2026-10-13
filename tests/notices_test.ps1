param([Parameter(Mandatory=$true)][string]$PrismSource)
$ErrorActionPreference='Stop'
$stage=Join-Path (Split-Path -Parent $PSScriptRoot) 'scripts/stage-prism-notices.ps1'
$parent=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\','/')
$root=Join-Path $parent ('lis-notices-'+[guid]::NewGuid().ToString('N'))
if(Test-Path -LiteralPath $root){throw 'Fixture already exists'}
try {
    $bad=Join-Path $root 'changed-source'
    New-Item -ItemType Directory -Path $bad -Force | Out-Null
    [IO.File]::WriteAllText((Join-Path $bad 'modified.cpp'),'not the pinned source')
    $blocked=$false
    try {& $stage -PrismSource $bad -OutputDirectory (Join-Path $root 'rejected')}
    catch {if($_.Exception.Message -notmatch 'differs from the pinned snapshot'){throw};$blocked=$true}
    if(-not $blocked){throw 'Source verification missed injected alteration'}
    $first=Join-Path $root 'first';$second=Join-Path $root 'second'
    & $stage -PrismSource $PrismSource -OutputDirectory $first
    & $stage -PrismSource $PrismSource -OutputDirectory $second
    foreach($name in @('Prism-NOTICES.txt','THIRD-PARTY-LICENSES.md','LICENSE.txt')) {
        if((Get-FileHash -LiteralPath (Join-Path $first $name)).Hash -ne
            (Get-FileHash -LiteralPath (Join-Path $second $name)).Hash){throw 'Nondeterministic notice staging'}
    }
    $license=[IO.File]::ReadAllText((Join-Path (Split-Path -Parent $PSScriptRoot) 'LICENSE')).Replace("`r`n","`n")
    if([IO.File]::ReadAllText((Join-Path $first 'LICENSE.txt')) -cne $license){throw 'Staged MIT licence differs from source'}
    if($license -notmatch 'MIT License' -or $license -notmatch 'Copyright \(c\) 2026 LordLuceus'){throw 'Missing MIT licence/attribution'}
    Write-Output 'PASS: altered source rejected; pinned source accepted; notices deterministic'
} finally {
    if(-not ([IO.Path]::GetFullPath($root)).StartsWith($parent+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {throw 'Unsafe fixture cleanup'}
    if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}
}
