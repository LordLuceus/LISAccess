param(
    [string]$SourceRoot = (Join-Path (Split-Path -Parent $PSScriptRoot) 'src'),
    [int]$MaxLines = 350,
    [switch]$Quiet
)
# Direct include/placement/size checks, not a C++ parser. This cannot prove
# semantic cohesion, transitive coupling, native ABI safety or thread correctness.
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath $SourceRoot).Path.TrimEnd('\','/')
$modules=@('bootstrap','engine','features','input','output','platform')
$allowed=@{
    bootstrap=$modules
    engine=@('engine','platform')
    features=@('engine','features','input','output','platform')
    input=@('input','platform')
    output=@('output','platform')
    platform=@('platform')
}
$problems=@()
$files=@(Get-ChildItem -LiteralPath $root -File -Recurse | Where-Object {$_.Extension -in '.cpp','.h'})
if(-not $files.Count){throw 'No C++ sources found; architecture check would be vacuous'}
foreach($file in $files) {
    $relative=$file.FullName.Substring($root.Length+1).Replace('\','/')
    $parts=$relative.Split('/')
    if($parts.Count -lt 2 -or $parts[0] -notin $modules){$problems+="Source outside a module: $relative";continue}
    $module=$parts[0]
    $lines=@(Get-Content -LiteralPath $file.FullName -Encoding UTF8)
    if($lines.Count -gt $MaxLines){$problems+="$relative has $($lines.Count) lines; review the $MaxLines-line module budget"}
    foreach($line in $lines) {
        if($line -match '^\s*#\s*include\s+(["<])([^">]+)[">]') {
            $quoted=$Matches[1] -eq '"'
            $include=$Matches[2]
            $dependency=$include.Split('/')[0]
            if(($quoted -or $dependency -in $modules) -and
                ($include.Contains('..') -or $dependency -notin $modules -or $dependency -notin $allowed[$module])) {
                $problems+="Forbidden module dependency: $relative -> $include"
            }
        }
    }
}
if($problems.Count){throw ($problems -join "`n")}
if(-not $Quiet){Write-Output "PASS: $($files.Count) source files obey module dependencies and the $MaxLines-line review budget"}
