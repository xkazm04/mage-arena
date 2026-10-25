# T57: builds the crowd assets under apps/vr/Game/Content/Environment/ArenaKit/Crowd (T_Crowd_Atlas and M_Crowd_Atlas;
# the card is the engine plane) through the CreateCrowdAssets commandlet (Greybox/CreateCrowdAssetsCommandlet.cpp), from
# apps/vr/art/environment-kit/crowd/crowd_atlas.png. Idempotent: an asset that already loads is reported and left alone;
# -Force rebuilds both. Needs the editor target built (apps/vr/tools/build.ps1) and the atlas pulled from LFS.
param([switch]$Force, [string]$Run = "T57")

$ErrorActionPreference = "Stop"
$CrowdDir = $PSScriptRoot
$ToolsDir = Split-Path -Parent $CrowdDir
$VrDir = Split-Path -Parent $ToolsDir
$AppsDir = Split-Path -Parent $VrDir
$Repo = Split-Path -Parent $AppsDir
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = Join-Path $VrDir "Game\MageArenaVR.uproject"
$RunDir = Join-Path $Repo "runs\$Run"
$LogPath = Join-Path $RunDir "create-crowd-assets.log"

$Atlas = Join-Path $VrDir "art\environment-kit\crowd\crowd_atlas.png"
$Bytes = [System.IO.File]::ReadAllBytes($Atlas)
if ($Bytes.Length -lt 8 -or $Bytes[1] -ne 0x50 -or $Bytes[2] -ne 0x4E -or $Bytes[3] -ne 0x47) {
	Write-Host "create-crowd-assets: $Atlas is not a PNG (a Git LFS pointer?). Run git lfs pull."
	exit 1
}

New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
if (Test-Path $LogPath) { Remove-Item -Force $LogPath }
$Extra = @()
if ($Force) { $Extra += "-force" }
Write-Host "create-crowd-assets: running the CreateCrowdAssets commandlet $($Extra -join ' ')"
& $Editor $Project -run=CreateCrowdAssets @Extra -unattended -nosplash -nop4 -stdout -FullStdOutLogOutput "-abslog=$LogPath" | Out-Null
$Code = $LASTEXITCODE
Select-String -Path $LogPath -Pattern "MAGEVR_CROWD_ASSETS" | ForEach-Object { Write-Host $_.Line }
if ($Code -ne 0 -or -not (Select-String -Path $LogPath -Pattern "MAGEVR_CROWD_ASSETS done" -Quiet)) {
	Write-Host "create-crowd-assets: failed exit=$Code (log: $LogPath)"
	exit 1
}
Write-Host "create-crowd-assets: ok"
exit 0
