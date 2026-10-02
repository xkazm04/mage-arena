# Builds the editor target (or pass another target, e.g. MageArenaVR for the game).
param([string]$Target = "MageArenaVREditor", [string]$Config = "Development")
$repo = Split-Path -Parent $PSScriptRoot
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" $Target Win64 $Config "-Project=$repo\Game\MageArenaVR.uproject" -WaitMutex
exit $LASTEXITCODE
