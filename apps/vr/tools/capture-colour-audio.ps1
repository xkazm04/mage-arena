# T26 capture. Two editor runs in -game:
#   stills: 01-hound-ember.png (creatures bout), 02-fire-and-water-bolts.png (Brennic semifinal), 03-air-squall.png
#           (Lio's Squall), then 04-side-by-side.png (the three in one frame, ffmpeg hstack) in runs/<run>/shots/.
#   audio:  runs/<run>/audio/brennic-opening.wav (30 s of the Brennic semifinal from the master submix) and
#           brennic-opening-cues.tsv (every play request: wall time, sim time, cue, event, source, position).
# Success: MAGEVR_CAPTURE_DONE in both logs, the four pngs, and a WAV whose peak is above -60 dBFS (not silent).
param([string]$Run = "T26", [switch]$SkipStills, [switch]$SkipAudio)

$ErrorActionPreference = "Stop"
$ToolsDir = $PSScriptRoot
$VrDir = Split-Path -Parent $ToolsDir
$AppsDir = Split-Path -Parent $VrDir
$Repo = Split-Path -Parent $AppsDir
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = Join-Path $VrDir "Game\MageArenaVR.uproject"
$RunDir = Join-Path $Repo "runs\$Run"
$ShotDir = Join-Path $RunDir "shots"
$AudioDir = Join-Path $RunDir "audio"
New-Item -ItemType Directory -Force -Path $ShotDir | Out-Null
New-Item -ItemType Directory -Force -Path $AudioDir | Out-Null

function Invoke-Capture([string]$Mode) {
	$LogPath = Join-Path $RunDir "capture-$Mode.log"
	if (Test-Path $LogPath) { Remove-Item -Force $LogPath }
	$ArgumentList = @(
		$Project, "-game", "-RenderOffScreen", "-ResX=1920", "-ResY=1080", "-unattended", "-nosplash", "-nohmd",
		"-stdout", "-FullStdOutLogOutput", "-log", "-abslog=$LogPath",
		"-MageArenaColourAudioCapture", "-MageArenaT26Mode=$Mode", "-MageArenaRun=$Run"
	)
	# Four queued output buffers instead of one: the render thread can fall behind a busy frame without the mixer
	# dropping a buffer, and a dropped buffer is a gap in the recording.
	if ($Mode -eq "audio") { $ArgumentList += "-ini:Engine:[/Script/WindowsTargetPlatform.WindowsTargetSettings]:AudioNumBuffersToEnqueue=4" }
	Write-Host "capture-colour-audio: $Mode run=$Run"
	$Process = Start-Process -FilePath $Editor -ArgumentList $ArgumentList -PassThru -WindowStyle Hidden `
		-RedirectStandardOutput (Join-Path $RunDir "capture-$Mode.stdout.log") -RedirectStandardError (Join-Path $RunDir "capture-$Mode.stderr.log")
	if (-not $Process.WaitForExit(600000)) {
		Write-Host "capture-colour-audio: $Mode 10 minute timeout, stopping the editor"
		& taskkill.exe /T /F /PID $Process.Id | Out-Host
		Start-Sleep -Seconds 2
	}
	$Text = ""
	if (Test-Path $LogPath) { $Text = Get-Content -Raw -Path $LogPath }
	Select-String -Path $LogPath -Pattern "MAGEVR_T26|MAGEVR_CAPTURE" | ForEach-Object { Write-Host $_.Line }
	return ($Text -match "MAGEVR_CAPTURE_DONE")
}

# ffmpeg and ffprobe report on stderr; Windows PowerShell 5.1 turns that into an error record under Stop.
$ErrorActionPreference = "Continue"
$Ok = $true
if (-not $SkipStills) {
	Get-ChildItem -Path $ShotDir -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force
	$Done = Invoke-Capture "stills"
	$Names = @("01-hound-ember.png", "02-fire-and-water-bolts.png", "03-air-squall.png")
	$Missing = @($Names | Where-Object { -not (Test-Path (Join-Path $ShotDir $_)) })
	if ($Done -and $Missing.Count -eq 0) {
		$Inputs = $Names | ForEach-Object { "-i"; (Join-Path $ShotDir $_) }
		$Sheet = Join-Path $ShotDir "04-side-by-side.png"
		& ffmpeg -hide_banner -loglevel error -y @Inputs -filter_complex "[0:v]scale=960:-1[a];[1:v]scale=960:-1[b];[2:v]scale=960:-1[c];[a][b][c]hstack=inputs=3" $Sheet
		if (-not (Test-Path $Sheet)) { Write-Host "capture-colour-audio: the side-by-side sheet was not written"; $Ok = $false }
	} else {
		Write-Host "capture-colour-audio: stills failed done=$Done missing=$($Missing -join ',')"
		$Ok = $false
	}
}
if (-not $SkipAudio) {
	$Wav = Join-Path $AudioDir "brennic-opening.wav"
	if (Test-Path $Wav) { Remove-Item -Force $Wav }
	$Done = Invoke-Capture "audio"
	if ($Done -and (Test-Path $Wav)) {
		$Probe = & ffprobe -v error -show_entries "format=duration:stream=sample_rate,channels" -of default=noprint_wrappers=1 $Wav
		$Probe | ForEach-Object { Write-Host "capture-colour-audio: $_" }
		$Volume = (& ffmpeg -hide_banner -nostats -i $Wav -af volumedetect -f null NUL 2>&1) | Select-String "max_volume|mean_volume"
		$Volume | ForEach-Object { Write-Host "capture-colour-audio: $($_.Line.Trim())" }
		$Peak = [double](($Volume | Where-Object { $_.Line -match "max_volume" } | Select-Object -First 1).Line -replace ".*max_volume: *([-0-9.]+) dB.*", '$1')
		if ($Peak -le -60.0) { Write-Host "capture-colour-audio: the recording is silent (peak $Peak dB)"; $Ok = $false }
	} else {
		Write-Host "capture-colour-audio: audio failed done=$Done wav=$(Test-Path $Wav)"
		$Ok = $false
	}
}
if ($Ok) { Write-Host "capture-colour-audio: ok"; exit 0 }
Write-Host "capture-colour-audio: failed"
exit 1
