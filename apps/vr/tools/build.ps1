# Builds the editor target (or pass another target, e.g. MageArenaVR for the game).
# Engine root: $env:UE_ROOT if set, else the default install. A missing engine is a LOUD failure with a
# non-zero exit: before, the `&` call to a missing Build.bat raised a non-terminating error, $LASTEXITCODE
# stayed $null and `exit $LASTEXITCODE` exited 0, so the documented build gate reported success for a build
# that never ran on any host without UE (Lynx; docs/DECISIONS.md 2026-10-20 "Unreal host").
# On an ARM64 host the engine's GetDotnetPath.bat picks DotNet\<ver>\win-arm64, which the installed engine
# does not ship (only win-x64), so Build.bat used to exit 150 (0x80008096) with no UnrealBuildTool output.
# The dotnet pre-flight below falls back to the bundled win-x64 SDK, or fails LOUD naming the missing
# directory (docs/research/UE-ON-LYNX-2026-10-22.md). -ResolveOnly prints the resolution and never builds.
param([string]$Target = "MageArenaVREditor", [string]$Config = "Development", [switch]$ResolveOnly)
$ErrorActionPreference = 'Stop'
# PowerShell 7.4+ would otherwise turn Build.bat's own non-zero exit into a terminating error and lose its
# exit code. The build's exit code is the result we report, so keep the 5.1 semantics on every host.
$PSNativeCommandUseErrorActionPreference = $false
$repo = Split-Path -Parent $PSScriptRoot
$engineRoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "C:\Program Files\Epic Games\UE_5.8" }
$buildBat = Join-Path $engineRoot "Engine\Build\BatchFiles\Build.bat"
if (-not (Test-Path -LiteralPath $buildBat -PathType Leaf)) {
  [Console]::Error.WriteLine("build.ps1: no Unreal Engine build script at '$buildBat'.")
  [Console]::Error.WriteLine("build.ps1: NOTHING WAS BUILT. Set `$env:UE_ROOT to the engine root, or build on the UE host (Wolf).")
  exit 2
}

# Dotnet pre-flight: mirror GetDotnetPath.bat's resolver (same env inputs, same order) and check the result.
function Fail-Dotnet([string]$why) {
  [Console]::Error.WriteLine("build.ps1: $why")
  [Console]::Error.WriteLine("build.ps1: NOTHING WAS BUILT. No usable dotnet for UnrealBuildTool.")
  exit 4
}
$getDotnet = Join-Path $engineRoot "Engine\Build\BatchFiles\GetDotnetPath.bat"
$dotnetVersion = $null
if (Test-Path -LiteralPath $getDotnet -PathType Leaf) {
  $m = Select-String -LiteralPath $getDotnet -Pattern '^\s*set UE_DOTNET_VERSION=(\S+)' | Select-Object -First 1
  if ($m) { $dotnetVersion = $m.Matches[0].Groups[1].Value }
}
if ($env:UE_USE_SYSTEM_DOTNET -eq '1') {
  $resolution = "system dotnet (UE_USE_SYSTEM_DOTNET=1; the engine verifies it)"
} elseif (-not $dotnetVersion) {
  $resolution = "not checked (no 'set UE_DOTNET_VERSION=' in '$getDotnet'; engine resolver left as is)"
} elseif ($env:UE_DOTNET_VERSION -eq $dotnetVersion) {
  # The engine's resolver exits early here and trusts the caller's DOTNET_ROOT.
  if (-not $env:DOTNET_ROOT -or -not (Test-Path -LiteralPath (Join-Path $env:DOTNET_ROOT "dotnet.exe") -PathType Leaf)) {
    Fail-Dotnet "UE_DOTNET_VERSION=$dotnetVersion is preset but DOTNET_ROOT '$env:DOTNET_ROOT' holds no dotnet.exe."
  }
  $resolution = "preset DOTNET_ROOT '$env:DOTNET_ROOT'"
} else {
  $arch = "win-x64"
  if ($env:PROCESSOR_ARCHITECTURE -eq "ARM64") { $arch = "win-arm64" }
  elseif ($env:PROCESSOR_ARCHITECTURE -eq "AMD64" -and (Test-Path -LiteralPath 'env:ProgramFiles(Arm)')) { $arch = "win-arm64" }
  $dotnetBase = Join-Path $engineRoot "Engine\Binaries\ThirdParty\DotNet\$dotnetVersion"
  $engineDir = Join-Path $dotnetBase $arch
  $x64Dir = Join-Path $dotnetBase "win-x64"
  if (Test-Path -LiteralPath (Join-Path $engineDir "dotnet.exe") -PathType Leaf) {
    $resolution = "engine default '$engineDir'"
  } elseif ($arch -ne "win-x64" -and (Test-Path -LiteralPath (Join-Path $x64Dir "dotnet.exe") -PathType Leaf)) {
    # Short-circuits GetDotnetPath.bat at its "UE_DOTNET_VERSION already set" early exit.
    $env:UE_DOTNET_VERSION = $dotnetVersion
    $env:DOTNET_ROOT = $x64Dir
    $env:DOTNET_MULTILEVEL_LOOKUP = "0"
    $env:DOTNET_ROLL_FORWARD = "LatestMajor"
    $env:PATH = "$x64Dir;$env:PATH"
    $resolution = "win-x64 fallback '$x64Dir' (engine wanted '$engineDir', which is missing; UBT runs emulated)"
  } else {
    Fail-Dotnet "the engine resolves dotnet to '$engineDir', which holds no dotnet.exe, and there is no win-x64 fallback at '$x64Dir'."
  }
}
Write-Host "build.ps1: dotnet: $resolution"
if ($ResolveOnly) { exit 0 }

& $buildBat $Target Win64 $Config "-Project=$repo\Game\MageArenaVR.uproject" -WaitMutex
if ($null -eq $LASTEXITCODE) {
  [Console]::Error.WriteLine("build.ps1: '$buildBat' did not run and reported no exit code. NOTHING WAS BUILT.")
  exit 3
}
exit $LASTEXITCODE
