# T26: imports the A05 starter pack (apps/vr/art/audio/sfx/*.wav, ids from manifest.json) as USoundWave assets under
# apps/vr/Game/Content/Audio/SFX, through the ImportSfx commandlet (Tools/ImportSfxCommandlet.cpp). Idempotent: a cue
# whose asset already records the same source MD5 and loop flag is skipped, so a second run writes nothing.
# Needs the editor target built (apps/vr/tools/build.ps1) and real WAVs (git lfs pull if they are pointers).
param([string]$Run = "T26")

$ErrorActionPreference = "Stop"
$ToolsDir = $PSScriptRoot
$VrDir = Split-Path -Parent $ToolsDir
$AppsDir = Split-Path -Parent $VrDir
$Repo = Split-Path -Parent $AppsDir
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = Join-Path $VrDir "Game\MageArenaVR.uproject"
$RunDir = Join-Path $Repo "runs\$Run"
$LogPath = Join-Path $RunDir "import-sfx.log"

New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
if (Test-Path $LogPath) { Remove-Item -Force $LogPath }

$SfxDir = Join-Path $VrDir "art\audio\sfx"
foreach ($Wav in Get-ChildItem -Path $SfxDir -Filter *.wav) {
	$Bytes = [System.IO.File]::ReadAllBytes($Wav.FullName)
	if ($Bytes.Length -lt 44 -or [System.Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne "RIFF") {
		Write-Host "import-sfx: $($Wav.Name) is not a RIFF WAV (a Git LFS pointer?). Run git lfs pull."
		exit 1
	}
}

Write-Host "import-sfx: running the ImportSfx commandlet"
& $Editor $Project -run=ImportSfx -unattended -nosplash -nullrhi -nop4 -stdout -FullStdOutLogOutput "-abslog=$LogPath" | Out-Null
$Code = $LASTEXITCODE
$Summary = Select-String -Path $LogPath -Pattern "MAGEVR_IMPORT_SFX (done|fail)" | ForEach-Object { $_.Line }
$Summary | ForEach-Object { Write-Host $_ }
if ($Code -ne 0) {
	Write-Host "import-sfx: failed exit=$Code"
	exit 1
}
Write-Host "import-sfx: ok"
exit 0
