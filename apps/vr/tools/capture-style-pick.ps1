# T16 style-pick capture: one scripted Brennic duel (seed 1) per style, recorded frame by frame, then an MP4 and four
# stills per style. Success is all of: MAGEVR_CAPTURE_DONE and no MAGEVR_CAPTURE_FAIL in every log, every style's
# shots-<style>/manifest.json listing a contiguous frame sequence that is on disk and past the pixel floor, a non-empty
# event log per style identical across the styles, an MP4 per style that ffmpeg wrote with exit code 0, and four stills
# per style cut from the frame whose manifest sim tick is the chosen event tick. Anything else exits 1. The editor Quit
# code is not the gate (T45).
# The editors run one at a time (CLAUDE.md: one Unreal process at a time; the 60 fps cap needs the machine).
# -Styles runs a subset (a smoke run; the art page is only written from a full four-style run). -ExtraArgs is appended
# to every editor's command line (the T45 negative proofs use it).
param([string[]]$Styles = @("greybox", "13", "25", "30"), [string]$ExtraArgs = "", [int]$TimeoutMinutes = 20)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$VrDir = (Resolve-Path "$ScriptDir\..").Path
$Project = Join-Path $VrDir "Game\MageArenaVR.uproject"
$RunsDir = Join-Path $VrDir "..\..\runs\T16"
$OutputShotsDir = Join-Path $RunsDir "shots"
$AllStyles = @("greybox", "13", "25", "30")
# The event indices the four stills are cut at (the first event is index 0).
$StillEvents = @(2, 5, 15, 30)

$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

if (-not (Test-Path $RunsDir)) { New-Item -ItemType Directory -Path $RunsDir | Out-Null }
if (-not (Test-Path $OutputShotsDir)) { New-Item -ItemType Directory -Path $OutputShotsDir | Out-Null }

$Failures = @()
function Add-Failure([string]$Why) {
	Write-Host "capture-style-pick: FAIL $Why"
	$script:Failures += $Why
}

foreach ($Style in $Styles) {
	Write-Host "capture-style-pick: running style $Style"
	$ShotsDir = Join-Path $RunsDir "shots-$Style"
	if (Test-Path $ShotsDir) { Remove-Item -Recurse -Force $ShotsDir }
	$EventsLog = Join-Path $RunsDir "events-$Style.jsonl"
	if (Test-Path $EventsLog) { Remove-Item -Force $EventsLog }
	$LogPath = Join-Path $RunsDir "capture-$Style.log"
	if (Test-Path $LogPath) { Remove-Item -Force $LogPath }

	$ArgumentList = @(
		$Project,
		"-game",
		"-RenderOffScreen",
		"-ResX=1920",
		"-ResY=1080",
		"-unattended",
		"-nosplash",
		"-nohmd",
		"-stdout",
		"-log",
		"-abslog=$LogPath",
		"-MageArenaStylePickCapture",
		"-MageArenaStyle=$Style",
		"-usefixedtimestep",
		"-fps=60",
		"-ExecCmds=`"t.MaxFPS 60; sg.PostProcessQuality 3`""
	)
	if ($ExtraArgs) { $ArgumentList += $ExtraArgs }

	$Process = Start-Process -FilePath $Editor -ArgumentList $ArgumentList -PassThru -WindowStyle Hidden -RedirectStandardOutput "$RunsDir\stdout-$Style.log" -RedirectStandardError "$RunsDir\stderr-$Style.log"
	if (-not $Process.WaitForExit($TimeoutMinutes * 60000)) {
		& taskkill.exe /T /F /PID $Process.Id | Out-Host
		Start-Sleep -Seconds 2
		Add-Failure "$Style ran past the $TimeoutMinutes minute timeout and was stopped"
		continue
	}

	$LogText = ""
	if (Test-Path $LogPath) { $LogText = Get-Content -Raw -Path $LogPath }
	if (Test-Path $LogPath) { Select-String -Path $LogPath -Pattern "MAGEVR_CAPTURE|MAGEVR_STYLEPICK" | ForEach-Object { Write-Host $_.Line } }
	if ($LogText -notmatch "MAGEVR_CAPTURE_DONE") { Add-Failure "missing MAGEVR_CAPTURE_DONE in $LogPath" }
	if ($LogText -match "MAGEVR_CAPTURE_FAIL") { Add-Failure "$LogPath contains MAGEVR_CAPTURE_FAIL" }
}

# Per style: the manifest is the list of frames. Frames are checked against it, never against a hard-coded count.
$Manifests = @{}
foreach ($Style in $Styles) {
	$ManifestPath = Join-Path $RunsDir "shots-$Style\manifest.json"
	if (-not (Test-Path $ManifestPath)) { Add-Failure "missing $ManifestPath"; continue }
	$Manifest = Get-Content -Raw -Path $ManifestPath | ConvertFrom-Json
	$Shots = @($Manifest.shots)
	if ($Shots.Count -eq 0) { Add-Failure "$Style manifest lists no frames"; continue }
	$Bad = 0
	for ($i = 0; $i -lt $Shots.Count; $i++) {
		$Shot = $Shots[$i]
		$Expected = "seq/frame_{0:D4}.png" -f ($i + 1)
		if ($Shot.name -ne $Expected) { Add-Failure "$Style manifest frame $($i + 1) is $($Shot.name), expected $Expected"; $Bad++ }
		elseif (-not (Test-Path $Shot.path)) { Add-Failure "$Style $($Shot.name) is not on disk"; $Bad++ }
		elseif ((Get-Item $Shot.path).Length -ne [int64]$Shot.bytes) { Add-Failure "$Style $($Shot.name) is not the size the driver measured"; $Bad++ }
		elseif ($Shot.pixel.ok -ne $true) { Add-Failure "$Style $($Shot.name) failed the pixel floor ($($Shot.pixel.why))"; $Bad++ }
		if ($Bad -ge 5) { Add-Failure "$Style stopped checking frames after 5 failures"; break }
	}
	Write-Host ("capture-style-pick: {0} manifest frames={1} commit={2} seed={3}" -f $Style, $Shots.Count, $Manifest.commit, $Manifest.seed)
	if ($Bad -eq 0) { $Manifests[$Style] = $Manifest }
}

# Event logs: present, not empty, identical across the styles that ran.
$FirstHash = $null
$FirstStyle = $null
foreach ($Style in $Styles) {
	$EventsLog = Join-Path $RunsDir "events-$Style.jsonl"
	if (-not (Test-Path $EventsLog)) { Add-Failure "events-$Style.jsonl is missing"; continue }
	if ((Get-Item $EventsLog).Length -eq 0) { Add-Failure "events-$Style.jsonl is empty"; continue }
	$Hash = (Get-FileHash -Algorithm MD5 $EventsLog).Hash
	Write-Host "capture-style-pick: events-$Style.jsonl md5=$Hash"
	if ($null -eq $FirstHash) { $FirstHash = $Hash; $FirstStyle = $Style }
	elseif ($Hash -ne $FirstHash) { Add-Failure "events-$Style.jsonl differs from events-$FirstStyle.jsonl" }
}

# ffmpeg reports on stderr; Windows PowerShell 5.1 turns that into an error record under Stop.
$ErrorActionPreference = "Continue"
foreach ($Style in $Styles) {
	if (-not $Manifests.ContainsKey($Style)) { continue }
	$FramesDir = Join-Path $RunsDir "shots-$Style\seq"
	$Mp4Path = Join-Path $RunsDir "style-$Style.mp4"
	if (Test-Path $Mp4Path) { Remove-Item -Force $Mp4Path }
	Write-Host "capture-style-pick: encoding $Mp4Path"
	& ffmpeg -hide_banner -loglevel error -y -framerate 60 -i "$FramesDir\frame_%04d.png" -c:v libx264 -pix_fmt yuv420p $Mp4Path
	if ($LASTEXITCODE -ne 0) { Add-Failure "ffmpeg exited $LASTEXITCODE encoding $Mp4Path" }
	elseif (-not (Test-Path $Mp4Path) -or (Get-Item $Mp4Path).Length -eq 0) { Add-Failure "$Mp4Path is missing or empty" }

	# The stills: the frame whose manifest sim tick is the event's tick, or the run fails.
	$EventsLog = Join-Path $RunsDir "events-$Style.jsonl"
	if (-not (Test-Path $EventsLog) -or (Get-Item $EventsLog).Length -eq 0) { continue }
	$Events = @(Get-Content $EventsLog | ConvertFrom-Json)
	$Need = ($StillEvents | Measure-Object -Maximum).Maximum + 1
	if ($Events.Count -lt $Need) { Add-Failure "$Style logged $($Events.Count) events, the stills need $Need"; continue }
	for ($i = 0; $i -lt $StillEvents.Count; $i++) {
		$Tick = [int]$Events[$StillEvents[$i]].tick
		$Frame = @($Manifests[$Style].shots | Where-Object { [int]$_.tick -eq $Tick }) | Select-Object -First 1
		$DestPath = Join-Path $OutputShotsDir ("{0}-0{1}.png" -f $Style, ($i + 1))
		if (Test-Path $DestPath) { Remove-Item -Force $DestPath }
		if (-not $Frame) { Add-Failure "$Style has no frame at event tick $Tick (event $($StillEvents[$i]))"; continue }
		Copy-Item $Frame.path $DestPath
		Write-Host "capture-style-pick: still $DestPath = $($Frame.name) tick=$Tick"
	}
}

if ($Failures.Count -gt 0) {
	Write-Host "capture-style-pick: failed ($($Failures.Count) failures)"
	exit 1
}

if (@(Compare-Object $Styles $AllStyles).Count -ne 0) {
	Write-Host "capture-style-pick: ok for $($Styles -join ', ') (a subset: the art page is not written)"
	exit 0
}

$ArtDir = Join-Path $VrDir "art\style-pick"
if (-not (Test-Path $ArtDir)) { New-Item -ItemType Directory -Path $ArtDir | Out-Null }
foreach ($Style in $Styles) {
	Copy-Item "$RunsDir\style-$Style.mp4" "$ArtDir\" -Force
	for ($i = 1; $i -le 4; $i++) {
		Copy-Item "$OutputShotsDir\$Style-0$i.png" "$ArtDir\" -Force
	}
}

Write-Host "capture-style-pick: ok"
exit 0
