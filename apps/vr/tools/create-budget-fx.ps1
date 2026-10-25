# D-G4: builds the six NS_Budget_ Niagara systems under apps/vr/Game/Content/Budget through the CreateBudgetFx
# commandlet (Tools/CreateBudgetFxCommandlet.cpp), from the BudgetStress::FxRows() table. Idempotent: a system that
# already loads is reported and left alone, so a second run writes nothing. -Dump logs every rapid-iteration
# parameter of freshly built systems and saves nothing; -Verify also dumps the saved systems; -Force rebuilds them.
# T58: it also builds M_SpellGlow (apps/vr/Game/Content/Vfx), the spell VFX material, when missing; -SpellVfx rebuilds
# that material alone. T63: with it, T_SpellFlipbook, imported from apps/vr/art/vfx/spell-flipbook.png (written by
# apps/vr/tools/vfx/spell-flipbook.mjs; run its --check first, and git lfs pull if the PNG is a pointer).
# Needs the editor target built (apps/vr/tools/build.ps1). No -nullrhi: the GPU emitter compiles shaders.
param([switch]$Dump, [switch]$Verify, [switch]$Force, [switch]$SpellVfx, [string]$Run = "D-G4-fx")

$ErrorActionPreference = "Stop"
$ToolsDir = $PSScriptRoot
$VrDir = Split-Path -Parent $ToolsDir
$AppsDir = Split-Path -Parent $VrDir
$Repo = Split-Path -Parent $AppsDir
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = Join-Path $VrDir "Game\MageArenaVR.uproject"
$RunDir = Join-Path $Repo "runs\$Run"
$LogPath = Join-Path $RunDir "create-budget-fx.log"

New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
if (Test-Path $LogPath) { Remove-Item -Force $LogPath }

$Extra = @()
if ($Dump) { $Extra += "-dump" }
if ($Force) { $Extra += "-force" }
if ($Verify) { $Extra += "-verify" }
if ($SpellVfx) { $Extra += "-spellvfx" }
Write-Host "create-budget-fx: running the CreateBudgetFx commandlet $($Extra -join ' ')"
& $Editor $Project -run=CreateBudgetFx @Extra -unattended -nosplash -nop4 -stdout -FullStdOutLogOutput "-abslog=$LogPath" | Out-Null
$Code = $LASTEXITCODE
$Pattern = if ($Dump -or $Verify) { "MAGEVR_BUDGET_(DUMP|FX)" } else { "MAGEVR_BUDGET_FX" }
$Summary = Select-String -Path $LogPath -Pattern $Pattern | ForEach-Object { $_.Line }
$Summary | ForEach-Object { Write-Host $_ }
if ($Code -ne 0) {
	Write-Host "create-budget-fx: failed exit=$Code (log: $LogPath)"
	exit 1
}
Write-Host "create-budget-fx: ok"
exit 0
