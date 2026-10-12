$ErrorActionPreference='Stop'
if(Get-Process -Name LifeIsStrange -ErrorAction SilentlyContinue){throw 'Close the game first.'}
$manifestPath=Join-Path $env:LOCALAPPDATA 'LISAccess\install.json'
if(-not(Test-Path -LiteralPath $manifestPath)){throw 'No installation ownership record found; nothing removed.'}
$files=Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
$remove=@()
foreach($f in $files){
    $path=[IO.Path]::GetFullPath($f.path)
    # Backward-compatible with the original two-DLL ownership record.
    $game=if($f.gameDirectory){[IO.Path]::GetFullPath($f.gameDirectory)}else{[IO.Path]::GetDirectoryName($path)}
    $allowed=@('dinput8.dll','nvdaControllerClient32.dll','LISAccess\navigation.ini','LISAccess\audio\obstacle.wav','LISAccess\audio\person.wav','LISAccess\audio\object.wav','LISAccess\audio\destination.wav') | ForEach-Object {[IO.Path]::GetFullPath((Join-Path $game $_))}
    if($path -notin $allowed){throw 'Unexpected manifest path; nothing removed.'}
    if(-not(Test-Path -LiteralPath (Join-Path $game 'LifeIsStrange.exe'))){throw 'Manifest is not in a Life Is Strange binary directory.'}
    if(Test-Path -LiteralPath $path){
        if((Get-FileHash -LiteralPath $path).Hash -ne $f.sha256){
            if($f.editable -and [IO.Path]::GetExtension($path) -in @('.ini','.wav')){Write-Output "Preserved custom file: $path"; continue}
            throw "File changed since installation; leaving it alone: $path"
        }
        $remove+=$path
    }
}
foreach($path in $remove){Remove-Item -LiteralPath $path}
Remove-Item -LiteralPath $manifestPath
Write-Output 'Owned prototype files removed. Custom settings/sounds, logs and saves were left alone.'
