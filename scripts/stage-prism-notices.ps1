param([string]$PrismSource=(Join-Path (Split-Path -Parent $PSScriptRoot) 'build-x86\_deps\prism-src'),
      [string]$OutputDirectory=(Join-Path (Split-Path -Parent $PSScriptRoot) 'build-x86\Release\notices'))
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$revision='c30d865faf9b4c38769cc58c2f33cbac78fba3ef'
if(Test-Path -LiteralPath (Join-Path $PrismSource '.git')){
    $actual=& git -C $PrismSource rev-parse HEAD
    if($LASTEXITCODE -ne 0 -or $actual -ne $revision){throw 'Notice staging requires the pinned Prism v0.16.5 source.'}
    $dirty=& git -C $PrismSource status --porcelain --untracked-files=no
    if($LASTEXITCODE -ne 0 -or $dirty){throw 'Prism tracked sources are modified; audit notices before staging.'}
}else{
    # FetchContent uses a SHA-256-pinned archive, not a Git checkout. Verify its
    # extracted bytes as well, so editing a covered source cannot stale notices.
    $source=(Resolve-Path -LiteralPath $PrismSource).Path.TrimEnd('\','/')
    [string[]]$paths=@(Get-ChildItem -LiteralPath $source -Recurse -File -Force | ForEach-Object {$_.FullName.Substring($source.Length+1).Replace('\','/')})
    [Array]::Sort($paths,[StringComparer]::Ordinal)
    $inventory=New-Object Text.StringBuilder
    foreach($path in $paths){
        $hash=(Get-FileHash -LiteralPath (Join-Path $source $path) -Algorithm SHA256).Hash.ToLowerInvariant()
        [void]$inventory.Append($hash+' '+$path+"`n")
    }
    $sha=[Security.Cryptography.SHA256]::Create()
    try{$digest=([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($inventory.ToString())))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()}
    if($digest -ne 'b2d6b0b608814e2bada701419678e047650db1e2711dadcf0d734150899d6fec'){throw 'Prism source archive differs from the pinned snapshot; audit before staging.'}
}
# Fixed inventory, fixed ordering, UTF-8 without BOM, LF endings, no timestamps.
# Include the complete upstream license inventory (Android-only Djinni is labeled
# in our accompanying document), not just the top-level MPL license.
$inputs=@('NOTICE','LICENSE','LICENSES/prism/mpl-2.0.txt',
    'LICENSES/simdutf/apache-2.0.txt','LICENSES/nvdaController/lgpl-2.1.txt',
    'LICENSES/moderncom/LICENSE','LICENSES/moderncom/AUTHORS.md',
    'LICENSES/dr_wav/LICENSE','LICENSES/concurrentqueue/LICENSE.md',
    'LICENSES/nvgt/LICENSE.md','LICENSES/djinni/LICENSE','idl/nvdaController.idl','idl/nvdaController.acf')
$text="Prism v0.16.5 -- $revision`nSource: https://github.com/ethindp/prism/tree/$revision`nSee THIRD-PARTY-LICENSES.md for scope and source-distribution obligations.`n"
foreach($input in $inputs){
    $body=[IO.File]::ReadAllText((Join-Path $PrismSource $input)).Replace("`r`n","`n")
    $text+="`n===== $input =====`n$body`n"
}
# The amalgamated simdutf header contains a separate BSD attribution that is
# absent from LICENSES. Extract its complete comment, failing closed if moved.
$header=[IO.File]::ReadAllText((Join-Path $PrismSource 'third_party/simdutf/simdutf.h')).Replace("`r`n","`n")
$notice=[regex]::Match($header,'(?s)/\* From\nhttps://github\.com/endorno/pytorch/.*?\*/')
if(-not $notice.Success){throw 'Missing embedded simdutf BSD notice; re-audit the pinned source.'}
$text+="`n===== simdutf embedded PyTorch-derived BSD notice =====`n$($notice.Value)`n"
$patch=[IO.File]::ReadAllText((Join-Path $root 'extern/prism-sapi-literal.cmake')).Replace("`r`n","`n")
$text+="`n===== LISAccess reproducible SAPI literal-text modification =====`n$patch`n"
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'Prism-NOTICES.txt'),$text,[Text.UTF8Encoding]::new($false))
$document=[IO.File]::ReadAllText((Join-Path $root 'THIRD-PARTY-LICENSES.md')).Replace("`r`n","`n")
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'THIRD-PARTY-LICENSES.md'),$document,[Text.UTF8Encoding]::new($false))
$license=[IO.File]::ReadAllText((Join-Path $root 'LICENSE')).Replace("`r`n","`n")
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'LICENSE.txt'),$license,[Text.UTF8Encoding]::new($false))
Write-Output "Staged Prism notices in $OutputDirectory"
