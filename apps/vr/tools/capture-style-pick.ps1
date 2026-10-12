param()

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$VrDir = (Resolve-Path "$ScriptDir\..").Path
$Project = Join-Path $VrDir "Game\MageArenaVR.uproject"
$RunsDir = Join-Path $VrDir "..\..\runs\T16"
$OutputShotsDir = Join-Path $RunsDir "shots"

$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"

if (-not (Test-Path $RunsDir)) { New-Item -ItemType Directory -Path $RunsDir | Out-Null }
if (-not (Test-Path $OutputShotsDir)) { New-Item -ItemType Directory -Path $OutputShotsDir | Out-Null }

$Styles = @("greybox", "13", "25", "30")

$Processes = @{}
foreach ($Style in $Styles) {
	Write-Host "Running capture for style: $Style"
	
	$FramesDir = Join-Path $RunsDir "shots-$Style\seq"
	if (Test-Path $FramesDir) { Remove-Item -Recurse -Force "$FramesDir\*" }
	
	$LogPath = Join-Path $RunsDir "capture-$Style.log"
	
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
	
	$Process = Start-Process -FilePath $Editor -ArgumentList $ArgumentList -PassThru -WindowStyle Hidden -RedirectStandardOutput "$RunsDir\stdout-$Style.log" -RedirectStandardError "$RunsDir\stderr-$Style.log"
	$Processes[$Style] = $Process
}

foreach ($Style in $Styles) {
	$Process = $Processes[$Style]
	$Finished = $Process.WaitForExit(720000)
	if (-not $Finished) {
		Write-Host "capture: 12 minute timeout, stopping the editor for $Style"
		& taskkill.exe /T /F /PID $Process.Id | Out-Host
		Start-Sleep -Seconds 2
	}
}

foreach ($Style in $Styles) {
	Write-Host "Encoding mp4 for style: $Style"
	$FramesDir = Join-Path $RunsDir "shots-$Style\seq"
	$Mp4Path = Join-Path $RunsDir "style-$Style.mp4"
	
	# encode with ffmpeg
	& ffmpeg -y -framerate 60 -i "$FramesDir\frame_%04d.png" -c:v libx264 -pix_fmt yuv420p $Mp4Path
	
	Write-Host "Extracting 4 stills for style: $Style"
	$EventsLog = Join-Path $RunsDir "events-$Style.jsonl"
	if (Test-Path $EventsLog) {
		# pick 4 event ticks
		$Events = Get-Content $EventsLog | ConvertFrom-Json
		if ($Events.Count -ge 4) {
			$Ticks = @($Events[2].tick, $Events[5].tick, $Events[15].tick, $Events[30].tick)
			for ($i = 0; $i -lt 4; $i++) {
				$Tick = $Ticks[$i]
				$FrameNum = $Tick
				$FrameName = "frame_{0:D4}.png" -f $FrameNum
				$SourceFrame = Join-Path $FramesDir $FrameName
				if (-not (Test-Path $SourceFrame)) {
					# Fallback if frame num doesn't match exactly
					$SourceFrame = Get-ChildItem $FramesDir | Select-Object -First 1
				}
				$DestName = "$Style-0$($i + 1).png"
				$DestPath = Join-Path $OutputShotsDir $DestName
				Copy-Item $SourceFrame $DestPath
			}
		}
	}
}

# Verify identical event times
Write-Host "Verifying identical event times across skins..."
$GreyboxEvents = Join-Path $RunsDir "events-greybox.jsonl"
$Identical = $true
if (-not (Test-Path $GreyboxEvents)) {
	Write-Host "ERROR: events-greybox.jsonl is missing."
	$Identical = $false
} else {
	$GreyboxHash = (Get-FileHash $GreyboxEvents).Hash
	foreach ($Style in @("13", "25", "30")) {
		$StyleLog = Join-Path $RunsDir "events-$Style.jsonl"
		if (-not (Test-Path $StyleLog)) {
			Write-Host "ERROR: events-$Style.jsonl is missing."
			$Identical = $false
		} else {
			$StyleHash = (Get-FileHash $StyleLog).Hash
			if ($StyleHash -ne $GreyboxHash) {
				Write-Host "ERROR: Mismatch in events between greybox and $Style"
				$Identical = $false
			}
		}
	}
}

if ($Identical) {
	Write-Host "SUCCESS: Event logs are identical across all skins."
} else {
	Write-Host "ERROR: Event logs differ across skins."
	exit 1
}

$ArtDir = Join-Path $VrDir "art\style-pick"
if (-not (Test-Path $ArtDir)) { New-Item -ItemType Directory -Path $ArtDir | Out-Null }
foreach ($Style in $Styles) {
	Copy-Item "$RunsDir\style-$Style.mp4" "$ArtDir\" -Force
	for ($i = 1; $i -le 4; $i++) {
		$ImgName = "$Style-0$i.png"
		if (Test-Path "$OutputShotsDir\$ImgName") {
			Copy-Item "$OutputShotsDir\$ImgName" "$ArtDir\" -Force
		}
	}
}

Write-Host "Capture complete."

