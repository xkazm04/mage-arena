# Builds the editor target (or pass another target, e.g. MageArenaVR for the game).
# Engine root: $env:UE_ROOT if set, else the default install. A missing engine is a LOUD failure with a
# non-zero exit: before, the `&` call to a missing Build.bat raised a non-terminating error, $LASTEXITCODE
# stayed $null and `exit $LASTEXITCODE` exited 0, so the documented build gate reported success for a build
# that never ran on any host without UE (Lynx; docs/DECISIONS.md 2026-10-20 "Unreal host").
param([string]$Target = "MageArenaVREditor", [string]$Config = "Development")
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
& $buildBat $Target Win64 $Config "-Project=$repo\Game\MageArenaVR.uproject" -WaitMutex
if ($null -eq $LASTEXITCODE) {
  [Console]::Error.WriteLine("build.ps1: '$buildBat' did not run and reported no exit code. NOTHING WAS BUILT.")
  exit 3
}
exit $LASTEXITCODE
