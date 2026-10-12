param([string]$GameDirectory='C:\Program Files (x86)\Steam\steamapps\common\Life Is Strange\Binaries\Win32',
      [Parameter(Mandatory=$true)][string]$NvdaClient)
$ErrorActionPreference='Stop'
if(Get-Process -Name LifeIsStrange -ErrorAction SilentlyContinue){throw 'Close the game before installing.'}
$root=Split-Path -Parent $PSScriptRoot
$exe=Join-Path $GameDirectory 'LifeIsStrange.exe'
$expected='0973A7E193728088ED438C02DD1F99B490EB35355AD8BE0F2240A3D2690FA233'
if((Get-FileHash -LiteralPath $exe).Hash -ne $expected){throw 'Unsupported executable. No files changed.'}
$files=@(
    @{Source=(Join-Path $root 'build-x86\Release\dinput8.dll');Name='dinput8.dll'},
    @{Source=$NvdaClient;Name='nvdaControllerClient32.dll'}
)
$state=Join-Path $env:LOCALAPPDATA 'LISAccess'
$manifestPath=Join-Path $state 'install.json'
$previous=@()
if(Test-Path -LiteralPath $manifestPath){$previous=Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json}
foreach($f in $files){
    if(-not(Test-Path -LiteralPath $f.Source)){throw "Missing build/input: $($f.Source)"}
    $dest=Join-Path $GameDirectory $f.Name
    if(Test-Path -LiteralPath $dest){
        $owned=@($previous | Where-Object {$_.path -eq $dest})
        if($owned.Count -ne 1 -or (Get-FileHash -LiteralPath $dest).Hash -ne $owned[0].sha256){throw "Refusing to replace an unowned or changed file: $dest"}
    }
}
New-Item -ItemType Directory -Path $state -Force | Out-Null
$manifest=@($previous | ForEach-Object {$_})
foreach($f in $files){
    $dest=Join-Path $GameDirectory $f.Name
    Copy-Item -LiteralPath $f.Source -Destination $dest
    $hash=(Get-FileHash -LiteralPath $dest).Hash
    if($hash -ne (Get-FileHash -LiteralPath $f.Source).Hash){throw "Copy verification failed: $dest"}
    $manifest=@($manifest | Where-Object {$_.path -ne $dest})
    $manifest+=@{path=$dest;sha256=$hash}
    # Record each successful copy so an interrupted install remains removable.
    $temporary=$manifestPath+'.tmp'
    ConvertTo-Json -InputObject $manifest | Set-Content -Encoding UTF8 -LiteralPath $temporary
    Move-Item -LiteralPath $temporary -Destination $manifestPath -Force
}
Write-Output 'Prototype installed. Launch Life Is Strange from Steam, app 319630.'
