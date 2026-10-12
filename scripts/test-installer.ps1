$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$temporary=[IO.Path]::GetFullPath((Join-Path $root ('artifacts\install-test-'+[guid]::NewGuid().ToString('N'))))
$workspace=[IO.Path]::GetFullPath($root)+[IO.Path]::DirectorySeparatorChar
if(-not $temporary.StartsWith($workspace,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $temporary)){throw 'Unsafe test directory'}
$game=Join-Path $temporary 'game'
$client=$env:LISACCESS_LEGACY_NVDA_CLIENT; if(-not $client){throw 'Set LISACCESS_LEGACY_NVDA_CLIENT to an x86 NVDA controller DLL for this historical test.'}
$oldLocal=$env:LOCALAPPDATA
New-Item -ItemType Directory -Path $game -Force | Out-Null
# Only this test's fixture is installed into. Do not require the unrelated live
# game to close: isolate both the target and the ownership-record directory.
function Get-Process { param([string]$Name) if($Name -ne 'LifeIsStrange'){throw 'Unexpected process query in test'} }
try {
    $env:LOCALAPPDATA=Join-Path $temporary 'state'
    Copy-Item -LiteralPath 'C:\Program Files (x86)\Steam\steamapps\common\Life Is Strange\Binaries\Win32\LifeIsStrange.exe' -Destination (Join-Path $game 'LifeIsStrange.exe')
    & (Join-Path $PSScriptRoot 'install-prototype.ps1') -GameDirectory $game -NvdaClient $client
    & (Join-Path $PSScriptRoot 'install-prototype.ps1') -GameDirectory $game -NvdaClient $client
    $dll=Join-Path $game 'dinput8.dll'
    [IO.File]::AppendAllText($dll,'tampered-fixture')
    $blocked=$false
    try { & (Join-Path $PSScriptRoot 'install-prototype.ps1') -GameDirectory $game -NvdaClient $client } catch { $blocked=$_.Exception.Message -match 'Refusing to replace' }
    if(-not $blocked){throw 'Changed DLL replacement was not rejected'}
    $blocked=$false
    try { & (Join-Path $PSScriptRoot 'remove-prototype.ps1') } catch { $blocked=$_.Exception.Message -match 'File changed' }
    if(-not $blocked){throw 'Changed DLL removal was not rejected'}
    Copy-Item -LiteralPath (Join-Path $root 'build-x86\Release\dinput8.dll') -Destination $dll
    $config=Join-Path $game 'LISAccess\navigation.ini'
    [IO.File]::AppendAllText($config,"`r`n; user customization")
    $customHash=(Get-FileHash -LiteralPath $config).Hash
    & (Join-Path $PSScriptRoot 'install-prototype.ps1') -GameDirectory $game -NvdaClient $client
    if((Get-FileHash -LiteralPath $config).Hash -ne $customHash){throw 'Update overwrote customized settings'}
    & (Join-Path $PSScriptRoot 'remove-prototype.ps1')
    if((Test-Path $dll) -or (Test-Path (Join-Path $game 'nvdaControllerClient32.dll'))){throw 'Owned DLL cleanup failed'}
    if(-not(Test-Path (Join-Path $game 'LifeIsStrange.exe'))){throw 'Uninstaller touched the game executable'}
    if((Get-FileHash -LiteralPath $config).Hash -ne $customHash){throw 'Uninstaller removed customized settings'}
    if(Test-Path (Join-Path $game 'LISAccess\audio\obstacle.wav')){throw 'Unchanged cue was not removed'}
    [IO.File]::WriteAllText($dll,'unowned-fixture')
    $blocked=$false
    try { & (Join-Path $PSScriptRoot 'install-prototype.ps1') -GameDirectory $game -NvdaClient $client } catch { $blocked=$_.Exception.Message -match 'Refusing to replace' }
    if(-not $blocked -or (Test-Path (Join-Path $game 'nvdaControllerClient32.dll'))){throw 'Unowned-file gate failed'}
    Write-Output 'PASS: fresh install, owned update, tamper rejection, custom-settings preservation, safe removal and unowned-file refusal.'
} finally {
    $env:LOCALAPPDATA=$oldLocal
    if(-not ([IO.Path]::GetFullPath($temporary)).StartsWith($workspace,[StringComparison]::OrdinalIgnoreCase)){throw 'Cleanup escaped workspace'}
    Remove-Item -LiteralPath $temporary -Recurse -Force
}
