param([string]$SupportedExecutable='C:\Program Files (x86)\Steam\steamapps\common\Life Is Strange\Binaries\Win32\LifeIsStrange.exe')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$temporary=[IO.Path]::GetFullPath((Join-Path $root ('artifacts\install-test-'+[guid]::NewGuid().ToString('N'))))
$workspace=[IO.Path]::GetFullPath($root)+[IO.Path]::DirectorySeparatorChar
if(-not $temporary.StartsWith($workspace,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path $temporary)){throw 'Unsafe test directory'}
$game=Join-Path $temporary 'game'
$package=Join-Path $temporary 'package'
$oldLocal=$env:LOCALAPPDATA
# Only fixture destinations are written. The supported game EXE is copied read-only
# and still passes through the installer's original hash gate. Dummy DLL bytes are
# deliberate: this suite tests ownership, not PE loading or output integration.
function Get-Process { param([string]$Name) if($Name -ne 'LifeIsStrange'){throw 'Unexpected process query in test'} }
function Assert($condition,[string]$message){if(-not $condition){throw $message}}
function Expect-Blocked([scriptblock]$action,[string]$message){
    $blocked=$false
    try { & $action } catch { $blocked=$_.Exception.Message -match $message }
    Assert $blocked "Expected refusal matching: $message"
}
try {
    $env:LOCALAPPDATA=Join-Path $temporary 'state'
    foreach($directory in @($game,(Join-Path $package 'scripts'),(Join-Path $package 'build-x86\Release\notices'),(Join-Path $package 'assets\audio'))){
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }
    Copy-Item -LiteralPath $SupportedExecutable -Destination (Join-Path $game 'LifeIsStrange.exe')
    foreach($script in @('install-prototype.ps1','remove-prototype.ps1')){
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $script) -Destination (Join-Path $package "scripts\$script")
    }
    $install=Join-Path $package 'scripts\install-prototype.ps1'
    $uninstall=Join-Path $package 'scripts\remove-prototype.ps1'
    $sourceDll=Join-Path $package 'build-x86\Release\dinput8.dll'
    [IO.File]::WriteAllText($sourceDll,'Prism-linked DLL fixture; not executable')
    [IO.File]::WriteAllText((Join-Path $package 'assets\navigation.ini'),'; fixture defaults')
    foreach($cue in @('obstacle','person','object','destination')){
        [IO.File]::WriteAllText((Join-Path $package "assets\audio\$cue.wav"),'dummy audio fixture')
    }
    foreach($notice in @('Prism-NOTICES.txt','THIRD-PARTY-LICENSES.md','LICENSE.txt')){
        [IO.File]::WriteAllText((Join-Path $package "build-x86\Release\notices\$notice"),'notice fixture; not distribution license text')
    }
    $dll=Join-Path $game 'dinput8.dll'
    $legacy=Join-Path $game 'nvdaControllerClient32.dll'
    $manifestPath=Join-Path $env:LOCALAPPDATA 'LISAccess\install.json'
    $noticePath=Join-Path $game 'LISAccess\notices\Prism-NOTICES.txt'
    $licensePath=Join-Path $game 'LISAccess\notices\LICENSE.txt'
    $config=Join-Path $game 'LISAccess\navigation.ini'
    $audio=Join-Path $game 'LISAccess\audio\person.wav'

    # Missing notices fail preflight, before any game files are changed.
    $noticeSource=Join-Path $package 'build-x86\Release\notices\Prism-NOTICES.txt'
    Move-Item -LiteralPath $noticeSource -Destination ($noticeSource+'.held')
    Expect-Blocked { & $install -GameDirectory $game } 'Missing build/input'
    Assert (-not(Test-Path $dll)) 'Missing-notice preflight wrote a DLL'
    Move-Item -LiteralPath ($noticeSource+'.held') -Destination $noticeSource
    $licenseSource=Join-Path $package 'build-x86\Release\notices\LICENSE.txt'
    Move-Item -LiteralPath $licenseSource -Destination ($licenseSource+'.held')
    Expect-Blocked { & $install -GameDirectory $game } 'Missing build/input'
    Assert (-not(Test-Path $dll)) 'Missing-MIT preflight wrote a DLL'
    Move-Item -LiteralPath ($licenseSource+'.held') -Destination $licenseSource
    & $install -GameDirectory $game
    Assert ((Get-FileHash -LiteralPath $licensePath).Hash -eq (Get-FileHash -LiteralPath $licenseSource).Hash) 'MIT licence missing from installation'
    Assert (-not(Test-Path $legacy)) 'Fresh install deployed an external controller'
    $before=Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
    & $install -GameDirectory $game
    foreach($entry in $before){Assert ((Get-FileHash -LiteralPath $entry.path).Hash -eq $entry.sha256) 'Unchanged update altered file contents'}

    [IO.File]::AppendAllText($dll,'tampered-fixture')
    Expect-Blocked { & $install -GameDirectory $game } 'Refusing to replace'
    Expect-Blocked { & $uninstall } 'File changed'
    Assert (Test-Path $noticePath) 'Failed removal deleted notices before preflight finished'
    Copy-Item -LiteralPath $sourceDll -Destination $dll
    [IO.File]::AppendAllText($noticePath,'tampered-notice')
    Expect-Blocked { & $install -GameDirectory $game } 'Refusing to replace'
    Expect-Blocked { & $uninstall } 'File changed'
    Copy-Item -LiteralPath $noticeSource -Destination $noticePath
    [IO.File]::AppendAllText($config,"`r`n; user customization")
    [IO.File]::AppendAllText($audio,'custom sound')
    $customHash=(Get-FileHash -LiteralPath $config).Hash
    $audioHash=(Get-FileHash -LiteralPath $audio).Hash
    & $install -GameDirectory $game
    Assert ((Get-FileHash -LiteralPath $config).Hash -eq $customHash) 'Update overwrote customized settings'
    Assert ((Get-FileHash -LiteralPath $audio).Hash -eq $audioHash) 'Update overwrote customized audio'
    $unknownNotice=Join-Path $game 'LISAccess\notices\user.txt'
    [IO.File]::WriteAllText($unknownNotice,'unowned notice')
    & $uninstall
    Assert (-not(Test-Path $dll) -and -not(Test-Path $noticePath)) 'Owned DLL/notice cleanup failed'
    Assert (-not(Test-Path $licensePath)) 'Owned MIT licence was not removed'
    Assert (Test-Path $unknownNotice) 'Uninstaller recursively removed unowned notices'
    Assert ((Get-FileHash -LiteralPath $config).Hash -eq $customHash) 'Uninstaller removed customized settings'
    Assert ((Get-FileHash -LiteralPath $audio).Hash -eq $audioHash) 'Uninstaller removed customized audio'
    Assert (-not(Test-Path (Join-Path $game 'LISAccess\audio\obstacle.wav'))) 'Unchanged cue was not removed'

    # Original two-DLL schema has neither gameDirectory nor editable fields.
    foreach($scenario in @('owned','changed','unowned')){
        [IO.File]::WriteAllText($dll,'old proxy fixture')
        [IO.File]::WriteAllText($legacy,'dummy legacy client bytes')
        $old=@(@{path=$dll;sha256=(Get-FileHash -LiteralPath $dll).Hash})
        if($scenario -ne 'unowned'){$old+=@{path=$legacy;sha256=(Get-FileHash -LiteralPath $legacy).Hash}}
        ConvertTo-Json -InputObject $old | Set-Content -Encoding UTF8 -LiteralPath $manifestPath
        if($scenario -eq 'changed'){[IO.File]::AppendAllText($legacy,'user modification')}
        $legacyHash=(Get-FileHash -LiteralPath $legacy).Hash
        & $install -GameDirectory $game
        $installed=@(Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json)
        Assert (@($installed | Where-Object {$_.path -eq $legacy}).Count -eq 0) 'Migration adopted or retained legacy ownership'
        Assert (@($installed | Where-Object {$_.path -in @($config,$audio)}).Count -eq 0) 'Update adopted unowned customization'
        if($scenario -eq 'owned'){
            Assert (-not(Test-Path $legacy)) 'Unchanged owned legacy client was not retired'
        }else{
            Assert ((Get-FileHash -LiteralPath $legacy).Hash -eq $legacyHash) 'Migration touched changed/unowned legacy client'
        }
        & $install -GameDirectory $game
        & $uninstall
        if($scenario -ne 'owned'){
            Assert ((Get-FileHash -LiteralPath $legacy).Hash -eq $legacyHash) 'Removal touched changed/unowned legacy client'
            Remove-Item -LiteralPath $legacy
        }
        Assert (-not(Test-Path $dll) -and -not(Test-Path $noticePath)) 'Migration removal left owned files'
    }
    # A fresh install must also ignore an unrelated pre-existing controller.
    [IO.File]::WriteAllText($legacy,'unowned fresh-install legacy fixture')
    $legacyHash=(Get-FileHash -LiteralPath $legacy).Hash
    & $install -GameDirectory $game
    & $uninstall
    Assert ((Get-FileHash -LiteralPath $legacy).Hash -eq $legacyHash) 'Fresh install/removal touched unowned legacy client'
    [IO.File]::WriteAllText($dll,'unowned-fixture')
    Expect-Blocked { & $install -GameDirectory $game } 'Refusing to replace'
    Assert (-not(Test-Path $manifestPath)) 'Unowned-file refusal created ownership'
    Assert ((Get-FileHash -LiteralPath (Join-Path $game 'LifeIsStrange.exe')).Hash -eq (Get-FileHash -LiteralPath $SupportedExecutable).Hash) 'Uninstaller touched game executable'
    Write-Output 'PASS: fresh install without controller, notice preflight/ownership, unchanged update, old-manifest migration, changed/unowned legacy safety, DLL tamper rejection, custom/unowned config/audio preservation and safe removal.'
} finally {
    $env:LOCALAPPDATA=$oldLocal
    if(-not ([IO.Path]::GetFullPath($temporary)).StartsWith($workspace,[StringComparison]::OrdinalIgnoreCase)){throw 'Cleanup escaped workspace'}
    if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary -Recurse -Force}
}
