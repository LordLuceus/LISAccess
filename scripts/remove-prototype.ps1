$ErrorActionPreference='Stop'
if(Get-Process -Name LifeIsStrange -ErrorAction SilentlyContinue){throw 'Close the game first.'}
$manifestPath=Join-Path $env:LOCALAPPDATA 'LISAccess\install.json'
if(-not(Test-Path -LiteralPath $manifestPath)){throw 'No installation ownership record found; nothing removed.'}
$files=Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
foreach($f in $files){
    $path=[IO.Path]::GetFullPath($f.path)
    if([IO.Path]::GetFileName($path) -notin @('dinput8.dll','nvdaControllerClient32.dll')){throw 'Unexpected manifest filename; nothing removed.'}
    if(-not(Test-Path -LiteralPath (Join-Path ([IO.Path]::GetDirectoryName($path)) 'LifeIsStrange.exe'))){throw 'Manifest is not in a Life Is Strange binary directory.'}
    if((Test-Path -LiteralPath $path) -and (Get-FileHash -LiteralPath $path).Hash -ne $f.sha256){throw "File changed since installation; leaving it alone: $path"}
}
foreach($f in $files){if(Test-Path -LiteralPath $f.path){Remove-Item -LiteralPath $f.path}}
Remove-Item -LiteralPath $manifestPath
Write-Output 'Only the two owned prototype DLLs were removed. Logs and saves were left alone.'
