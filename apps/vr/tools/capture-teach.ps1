# Records the cold start, the three teach steps, the pause, and the 3-2-1 count.
# Success is MAGEVR_CAPTURE_DONE plus the six pngs in runs/T13/shots/. The editor Quit code is not the gate.
param([string]$Run = "T13")

$ErrorActionPreference = "Stop"
$ToolsDir = $PSScriptRoot
$VrDir = Split-Path -Parent $ToolsDir
$AppsDir = Split-Path -Parent $VrDir
$Repo = Split-Path -Parent $AppsDir
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = Join-Path $VrDir "Game\MageArenaVR.uproject"
$RunDir = Join-Path $Repo "runs\$Run"
$ShotDir = Join-Path $RunDir "shots"
$LogPath = Join-Path $RunDir "capture.log"
$StdoutPath = Join-Path $RunDir "capture.stdout.log"
$StderrPath = Join-Path $RunDir "capture.stderr.log"

New-Item -ItemType Directory -Force -Path $ShotDir | Out-Null
Get-ChildItem -Path $ShotDir -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force
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
	"-FullStdOutLogOutput",
	"-log",
	"-abslog=$LogPath",
	"-MageArenaTeachCapture",
	"-MageArenaRun=$Run"
)

Write-Host "capture-teach: starting UnrealEditor-Cmd run=$Run"
$Process = Start-Process -FilePath $Editor -ArgumentList $ArgumentList -PassThru -WindowStyle Hidden -RedirectStandardOutput $StdoutPath -RedirectStandardError $StderrPath
$Finished = $Process.WaitForExit(420000)
if (-not $Finished) {
	Write-Host "capture-teach: 7 minute timeout, stopping the editor"
	& taskkill.exe /T /F /PID $Process.Id | Out-Host
	Start-Sleep -Seconds 2
}

$ShotNames = @(
	"01-cold-start.png",
	"02-ward-perfect.png",
	"03-sigil-dummy.png",
	"04-blink-lane.png",
	"05-pause.png",
	"06-count.png"
)
$Missing = @()
foreach ($Name in $ShotNames) {
	$Path = Join-Path $ShotDir $Name
	if (-not (Test-Path $Path) -or (Get-Item $Path).Length -le 0) {
		$Missing += $Name
	}
}

$LogText = ""
if (Test-Path $LogPath) { $LogText = Get-Content -Raw -Path $LogPath }
$Done = $LogText -match "MAGEVR_CAPTURE_DONE"
if ($Missing.Count -eq 0 -and $Done) {
	Write-Host "capture-teach: ok"
	exit 0
}
Write-Host "capture-teach: failed missing=$($Missing -join ',') done=$Done"
exit 1
