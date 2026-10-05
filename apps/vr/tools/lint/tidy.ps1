# On-demand clang-tidy over the C++ files changed on this branch (checks: apps/vr/Game/Source/.clang-tidy).
# UBT writes the compile database under Game/Intermediate (never committed); clang-tidy reads it.
# Not a pre-push gate: it needs the UE toolchain. Warnings are reported; only clang-tidy errors fail the run.
# Usage: powershell -NoProfile -File apps/vr/tools/lint/tidy.ps1 [-Base master] [-Files Source/.../X.cpp,...]
param([string]$Base = "master", [string[]]$Files = @())
$ErrorActionPreference = 'Stop'

$VrRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$RepoRoot = Split-Path -Parent (Split-Path -Parent $VrRoot)
$Project = Join-Path $VrRoot 'Game\MageArenaVR.uproject'
$DbDir = Join-Path $VrRoot 'Game\Intermediate\ClangDatabase'
$BuildBat = 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat'

function Find-ClangTidy {
    if ($env:CLANG_TIDY -and (Test-Path -LiteralPath $env:CLANG_TIDY)) { return $env:CLANG_TIDY }
    $onPath = Get-Command clang-tidy -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }
    $vs = Join-Path $env:ProgramFiles 'Microsoft Visual Studio\*\*\VC\Tools\Llvm\x64\bin\clang-tidy.exe'
    $hit = Get-ChildItem -Path $vs -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($hit) { return $hit.FullName }
    $llvm = Join-Path $env:ProgramFiles 'LLVM\bin\clang-tidy.exe'
    if (Test-Path -LiteralPath $llvm) { return $llvm }
    throw 'clang-tidy not found. Install the Visual Studio "C++ Clang tools for Windows" component or LLVM, or set CLANG_TIDY.'
}

if (-not $Files.Count) {
    $mergeBase = (& git -C $RepoRoot merge-base HEAD $Base)
    if ($LASTEXITCODE -ne 0) { throw "tidy: no merge base between HEAD and $Base" }
    $changed = & git -C $RepoRoot diff --name-only --diff-filter=AMR $mergeBase.Trim() -- 'apps/vr/Game/Source'
    # A changed header is checked through the .cpp that shares its stem, which is the TU that includes it first.
    $Files = $changed | ForEach-Object {
        if ($_ -match '\.cpp$') { $_ }
        elseif ($_ -match '\.h$') {
            $cpp = $_ -replace '\.h$', '.cpp'
            if (Test-Path -LiteralPath (Join-Path $RepoRoot $cpp)) { $cpp }
        }
    } | Sort-Object -Unique
}
if (-not $Files) {
    Write-Host "tidy: no changed C++ files since $Base"
    exit 0
}

$tidy = Find-ClangTidy
New-Item -ItemType Directory -Force -Path $DbDir | Out-Null
& $BuildBat MageArenaVREditor Win64 Development "-Project=$Project" -mode=GenerateClangDatabase "-OutputDir=$DbDir" -WaitMutex
if ($LASTEXITCODE -ne 0) { throw "tidy: UBT GenerateClangDatabase failed ($LASTEXITCODE)" }

$full = $Files | ForEach-Object { Join-Path $RepoRoot $_ }
Write-Host ('tidy: ' + $tidy + ' on ' + $full.Count + ' file(s)')
& $tidy -p $DbDir --quiet @full
exit $LASTEXITCODE
