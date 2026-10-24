# T26 capture. Two editor runs in -game:
#   stills: 01-hound-ember.png (creatures bout), 02-fire-and-water-bolts.png (Brennic semifinal), 03-air-squall.png
#           (Lio's Squall), then 04-side-by-side.png (the three in one frame, ffmpeg hstack) in runs/<run>/shots/.
#   audio:  runs/<run>/audio/brennic-opening.wav (30 s of the Brennic semifinal from the master submix) and
#           brennic-opening-cues.tsv (every play request: game time, sim time, cue, event, source, position, mixer time).
# Success: MAGEVR_CAPTURE_DONE and no MAGEVR_CAPTURE_FAIL in both logs, each run's manifest.json (every shot on disk at
# the size the driver measured, every shot past the pixel floor), the four pngs, and a WAV whose peak is above -60 dBFS
# and whose duration is within $DurationToleranceS of $RecordSeconds. A missing or unparsable max_volume line, or
# -inf, is a failure, never a pass (T45). The editor Quit code is not the gate.
# -CheckWav <path> runs only the audio check over an existing WAV, so the check is provable without Unreal.
param([string]$Run = "T26", [switch]$SkipStills, [switch]$SkipAudio, [string]$CheckWav = "")

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
# The same numbers as RecordSeconds (Session/ColourAudioCapture.cpp) and MageArenaCapture::AudioToleranceS
# (Session/CaptureEvidence.h, where the three Wolf takes that justify 1 s are listed): the driver records 30.8 s of game
# time and the WAV must hold that within 1 s.
$RecordSeconds = 30.8
$DurationToleranceS = 1.0
$SilentPeakDb = -60.0

# Fails on silence, on a WAV ffmpeg cannot measure, and on a duration outside the tolerance. Prints one line for each
# failure and returns $true only when the recording passes.
function Test-Recording([string]$Wav) {
	$Pass = $true
	$Probe = @(& ffprobe -v error -show_entries "format=duration:stream=sample_rate,channels" -of default=noprint_wrappers=1 $Wav 2>&1)
	$Probe | ForEach-Object { Write-Host "capture-colour-audio: $_" }
	$DurationLine = $Probe | Where-Object { "$_" -match '^duration=[0-9.]+$' } | Select-Object -First 1
	if (-not $DurationLine) {
		Write-Host "capture-colour-audio: no duration from ffprobe, the WAV cannot be measured"
		$Pass = $false
	} else {
		$Duration = [double]::Parse(("$DurationLine" -replace '^duration=', ''), [Globalization.CultureInfo]::InvariantCulture)
		if ([Math]::Abs($Duration - $RecordSeconds) -gt $DurationToleranceS) {
			Write-Host ("capture-colour-audio: the recording is {0:F3} s, expected {1:F1} s within {2:F1} s" -f $Duration, $RecordSeconds, $DurationToleranceS)
			$Pass = $false
		}
	}
	$Volume = @((& ffmpeg -hide_banner -nostats -i $Wav -af volumedetect -f null NUL 2>&1) | Select-String "max_volume|mean_volume")
	$Volume | ForEach-Object { Write-Host "capture-colour-audio: $($_.Line.Trim())" }
	$PeakLine = $Volume | Where-Object { $_.Line -match "max_volume" } | Select-Object -First 1
	if (-not $PeakLine) {
		Write-Host "capture-colour-audio: no max_volume line, the peak cannot be measured"
		$Pass = $false
	} elseif ($PeakLine.Line -match 'max_volume: *-inf dB') {
		Write-Host "capture-colour-audio: the recording is silent (peak -inf dB)"
		$Pass = $false
	} elseif ($PeakLine.Line -notmatch 'max_volume: *(-?[0-9]+(\.[0-9]+)?) dB') {
		Write-Host "capture-colour-audio: unparsable max_volume line: $($PeakLine.Line.Trim())"
		$Pass = $false
	} else {
		$Peak = [double]::Parse($Matches[1], [Globalization.CultureInfo]::InvariantCulture)
		if ($Peak -le $SilentPeakDb) {
			Write-Host "capture-colour-audio: the recording is silent (peak $Peak dB)"
			$Pass = $false
		}
	}
	return $Pass
}

# Every expected shot is listed, every listed shot is on disk at the size the driver measured and passed the floor.
function Test-Manifest([string]$Path, [string[]]$Expected) {
	if (-not (Test-Path $Path)) { Write-Host "capture-colour-audio: missing manifest $Path"; return $false }
	$Manifest = Get-Content -Raw -Path $Path | ConvertFrom-Json
	$Pass = $true
	$Names = @($Manifest.shots | ForEach-Object { $_.name })
	foreach ($Name in $Expected) {
		if ($Names -notcontains $Name) { Write-Host "capture-colour-audio: manifest $Path lists no $Name"; $Pass = $false }
	}
	foreach ($Shot in @($Manifest.shots)) {
		if (-not (Test-Path $Shot.path)) { Write-Host "capture-colour-audio: manifest shot $($Shot.name) is not on disk"; $Pass = $false; continue }
		if ((Get-Item $Shot.path).Length -ne [int64]$Shot.bytes) { Write-Host "capture-colour-audio: manifest shot $($Shot.name) is not the size the driver measured"; $Pass = $false }
		if ($Shot.pixel.ok -ne $true) { Write-Host "capture-colour-audio: manifest shot $($Shot.name) failed the pixel floor ($($Shot.pixel.why))"; $Pass = $false }
	}
	return $Pass
}

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
		Write-Host "capture-colour-audio: $Mode 10 minute timeout, stopping the editor (a failure)"
		& taskkill.exe /T /F /PID $Process.Id | Out-Host
		Start-Sleep -Seconds 2
		return $false
	}
	$Text = ""
	if (Test-Path $LogPath) {
		$Text = Get-Content -Raw -Path $LogPath
		Select-String -Path $LogPath -Pattern "MAGEVR_T26|MAGEVR_CAPTURE" | ForEach-Object { Write-Host $_.Line }
	}
	$Done = $Text -match "MAGEVR_CAPTURE_DONE"
	$Failed = $Text -match "MAGEVR_CAPTURE_FAIL"
	if (-not $Done) { Write-Host "capture-colour-audio: missing MAGEVR_CAPTURE_DONE in $LogPath" }
	if ($Failed) { Write-Host "capture-colour-audio: $LogPath contains MAGEVR_CAPTURE_FAIL" }
	return ($Done -and -not $Failed)
}

# ffmpeg and ffprobe report on stderr; Windows PowerShell 5.1 turns that into an error record under Stop.
$ErrorActionPreference = "Continue"
if ($CheckWav) {
	if ((Test-Path $CheckWav) -and (Test-Recording $CheckWav)) { Write-Host "capture-colour-audio: ok"; exit 0 }
	if (-not (Test-Path $CheckWav)) { Write-Host "capture-colour-audio: no WAV at $CheckWav" }
	Write-Host "capture-colour-audio: failed"
	exit 1
}
New-Item -ItemType Directory -Force -Path $ShotDir | Out-Null
New-Item -ItemType Directory -Force -Path $AudioDir | Out-Null
$Ok = $true
if (-not $SkipStills) {
	Get-ChildItem -Path $ShotDir -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force
	Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $ShotDir "manifest.json")
	$Done = Invoke-Capture "stills"
	$Names = @("01-hound-ember.png", "02-fire-and-water-bolts.png", "03-air-squall.png")
	$ManifestOk = $Done -and (Test-Manifest (Join-Path $ShotDir "manifest.json") $Names)
	$Missing = @($Names | Where-Object { -not (Test-Path (Join-Path $ShotDir $_)) })
	if ($ManifestOk -and $Missing.Count -eq 0) {
		$Inputs = $Names | ForEach-Object { "-i"; (Join-Path $ShotDir $_) }
		$Sheet = Join-Path $ShotDir "04-side-by-side.png"
		& ffmpeg -hide_banner -loglevel error -y @Inputs -filter_complex "[0:v]scale=960:-1[a];[1:v]scale=960:-1[b];[2:v]scale=960:-1[c];[a][b][c]hstack=inputs=3" $Sheet
		if ($LASTEXITCODE -ne 0 -or -not (Test-Path $Sheet)) { Write-Host "capture-colour-audio: the side-by-side sheet was not written (ffmpeg exit $LASTEXITCODE)"; $Ok = $false }
	} else {
		Write-Host "capture-colour-audio: stills failed done=$Done manifest=$ManifestOk missing=$($Missing -join ',')"
		$Ok = $false
	}
}
if (-not $SkipAudio) {
	$Wav = Join-Path $AudioDir "brennic-opening.wav"
	if (Test-Path $Wav) { Remove-Item -Force $Wav }
	Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $AudioDir "manifest.json")
	$Done = Invoke-Capture "audio"
	$ManifestOk = $Done -and (Test-Manifest (Join-Path $AudioDir "manifest.json") @())
	if ($ManifestOk -and (Test-Path $Wav)) {
		if (-not (Test-Recording $Wav)) { $Ok = $false }
	} else {
		Write-Host "capture-colour-audio: audio failed done=$Done manifest=$ManifestOk wav=$(Test-Path $Wav)"
		$Ok = $false
	}
}
if ($Ok) { Write-Host "capture-colour-audio: ok"; exit 0 }
Write-Host "capture-colour-audio: failed"
exit 1
