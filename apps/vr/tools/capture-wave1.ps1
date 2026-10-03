# Records the scripted Wave 1: five stills, a short frame sequence, budget.json.
# Success is MAGEVR_CAPTURE_DONE plus the five pngs. The editor Quit code is not the gate.
$ErrorActionPreference = "Stop"
$ToolsDir = $PSScriptRoot
$VrDir = Split-Path -Parent $ToolsDir
$AppsDir = Split-Path -Parent $VrDir
$Repo = Split-Path -Parent $AppsDir
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = Join-Path $VrDir "Game\MageArenaVR.uproject"
$RunDir = Join-Path $Repo "runs\T08"
$ShotDir = Join-Path $RunDir "shots"
$SeqDir = Join-Path $ShotDir "seq"
$BudgetPath = Join-Path $RunDir "budget.json"
$LogPath = Join-Path $RunDir "capture.log"
$StdoutPath = Join-Path $RunDir "capture.stdout.log"
$StderrPath = Join-Path $RunDir "capture.stderr.log"
$VideoPath = Join-Path $RunDir "wave1.mp4"

New-Item -ItemType Directory -Force -Path $SeqDir | Out-Null
Get-ChildItem -Path $ShotDir -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force
Get-ChildItem -Path $SeqDir -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force
if (Test-Path $BudgetPath) { Remove-Item -Force $BudgetPath }
if (Test-Path $LogPath) { Remove-Item -Force $LogPath }
if (Test-Path $VideoPath) { Remove-Item -Force $VideoPath }

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
	"-FullStdOutLogOutput",
	"-log",
	"-abslog=$LogPath",
	"-MageArenaWave1Capture"
)

Write-Host "capture-wave1: starting UnrealEditor-Cmd"
$Process = Start-Process -FilePath $Editor -ArgumentList $ArgumentList -PassThru -WindowStyle Hidden -RedirectStandardOutput $StdoutPath -RedirectStandardError $StderrPath
$Finished = $Process.WaitForExit(360000)
if (-not $Finished) {
	Write-Host "capture-wave1: 6 minute timeout, stopping the editor"
	& taskkill.exe /T /F /PID $Process.Id | Out-Host
	Start-Sleep -Seconds 2
}

$ShotNames = @(
	"01-first-soldiers.png",
	"02-ward-meets-stone.png",
	"03-sigil-hit.png",
	"04-blink.png",
	"05-victory.png"
)

$Missing = @()
foreach ($Name in $ShotNames) {
	$Path = Join-Path $ShotDir $Name
	if (-not (Test-Path $Path) -or (Get-Item $Path).Length -le 0) {
		$Missing += $Name
	}
}

$Frames = @(Get-ChildItem -Path $SeqDir -Filter "frame_*.png" -ErrorAction SilentlyContinue | Sort-Object Name)
if ($Frames.Count -ge 2) {
	$Ffmpeg = Get-Command ffmpeg -ErrorAction SilentlyContinue
	if ($Ffmpeg) {
		& ffmpeg -y -framerate 8 -i (Join-Path $SeqDir "frame_%04d.png") -c:v libx264 -pix_fmt yuv420p $VideoPath
		if (Test-Path $VideoPath) {
			Write-Host "capture-wave1: wrote $VideoPath"
		}
	} else {
		Write-Host "capture-wave1: ffmpeg not on PATH; frames left in shots/seq"
	}
} else {
	Write-Host "capture-wave1: no frame sequence"
}

$LogText = ""
if (Test-Path $LogPath) { $LogText = Get-Content -Raw -Path $LogPath }
$Done = $LogText -match "MAGEVR_CAPTURE_DONE"
if ($Missing.Count -eq 0 -and $Done) {
	Write-Host "capture-wave1: ok"
	exit 0
}
Write-Host "capture-wave1: failed missing=$($Missing -join ',') done=$Done"
exit 1
