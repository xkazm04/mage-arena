# T27: imports the C2 voice lines (apps/vr/art/audio/voice, one row per cue in manifest.json, the WAV at the row's
# "file") as USoundWave assets under apps/vr/Game/Content/Audio/Voice, through the ImportSfx commandlet with -kind=voice
# (Tools/ImportSfxCommandlet.cpp). Idempotent like import-sfx.ps1: a cue whose asset already records the same source MD5
# is skipped, so a second run writes nothing. A row with no WAV yet is counted as missing, not failed.
# With no WAV in the folder at all (audio deferred, docs/DECISIONS.md 2026-10-24) it says so and exits 0 without
# starting Unreal; -Force runs the commandlet anyway (it then reports every row missing).
# Needs the editor target built (apps/vr/tools/build.ps1) and real WAVs (git lfs pull if they are pointers).
param([string]$Run = "T27", [switch]$Force)

$ErrorActionPreference = "Stop"
$ToolsDir = $PSScriptRoot
$VrDir = Split-Path -Parent $ToolsDir
$AppsDir = Split-Path -Parent $VrDir
$Repo = Split-Path -Parent $AppsDir
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = Join-Path $VrDir "Game\MageArenaVR.uproject"
$RunDir = Join-Path $Repo "runs\$Run"
$LogPath = Join-Path $RunDir "import-voice.log"
$VoiceDir = Join-Path $VrDir "art\audio\voice"

$Wavs = @(Get-ChildItem -Path $VoiceDir -Filter *.wav -Recurse -ErrorAction SilentlyContinue)
foreach ($Wav in $Wavs) {
	$Bytes = [System.IO.File]::ReadAllBytes($Wav.FullName)
	if ($Bytes.Length -lt 44 -or [System.Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne "RIFF") {
		Write-Host "import-voice: $($Wav.Name) is not a RIFF WAV (a Git LFS pointer?). Run git lfs pull."
		exit 1
	}
}
if ($Wavs.Count -eq 0 -and -not $Force) {
	Write-Host "import-voice: no WAV under art/audio/voice (audio deferred, docs/DECISIONS.md 2026-10-24); nothing to import"
	exit 0
}

New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
if (Test-Path $LogPath) { Remove-Item -Force $LogPath }
Write-Host "import-voice: running the ImportSfx commandlet on $($Wavs.Count) WAV(s)"
& $Editor $Project -run=ImportSfx "-source=$VoiceDir" -destination=/Game/Audio/Voice -kind=voice -unattended -nosplash -nullrhi -nop4 -stdout -FullStdOutLogOutput "-abslog=$LogPath" | Out-Null
$Code = $LASTEXITCODE
$Summary = Select-String -Path $LogPath -Pattern "MAGEVR_IMPORT_SFX (done|fail)" | ForEach-Object { $_.Line }
$Summary | ForEach-Object { Write-Host $_ }
if ($Code -ne 0) {
	Write-Host "import-voice: failed exit=$Code"
	exit 1
}
Write-Host "import-voice: ok"
exit 0
