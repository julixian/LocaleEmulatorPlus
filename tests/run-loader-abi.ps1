param(
    [string]$CoreDir = (Join-Path $PSScriptRoot '..\..\LocaleEmulatorPlus-Core'),
    [string]$Compiler = 'D:\VisualStudio2026\MSBuild\Current\Bin\Roslyn\csc.exe'
)
# Build Core and run its tests/run-timezone.ps1 first to generate the native probe.
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputRoot = Join-Path $repoRoot 'Build\loader-abi-test'
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
foreach ($arch in @('x86','x64')) {
    foreach ($name in @("LoaderDll_$arch.dll", "LocaleEmulatorPlus_$arch.dll")) {
        Copy-Item -LiteralPath "$CoreDir\out\$arch\$name" -Destination (Join-Path $outputRoot $name) -Force
    }
}
foreach ($arch in @('x86','x64')) {
    $probe = "$outputRoot\loader-abi-$arch.exe"
    & $Compiler /nologo /codepage:65001 "/platform:$arch" "/out:$probe" "$PSScriptRoot\loader-abi-probe.cs" "$repoRoot\LEPProc\LoaderWrapper.cs" "$repoRoot\LEPProc\Amemiya.Extensions.ArrayExtensions.cs"
    if ($LASTEXITCODE -ne 0) { throw "Managed probe compile failed: $arch" }
    foreach ($case in @('tokyo','standard-only','daylight-only','overrides','pacific','southern','max-names')) {
        & $probe $case "$CoreDir\out\timezone-probe-$arch.exe"
        if ($LASTEXITCODE -ne 0) { throw "Managed ABI probe failed: $arch/$case exit=$LASTEXITCODE" }
    }
}
