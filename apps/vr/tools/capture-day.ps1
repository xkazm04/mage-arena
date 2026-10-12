# T21 stills: the collar ritual (Septima, then both palms), the Bout 3 and Bout 4 intros, and the aftermath tablet.
# Success is MAGEVR_CAPTURE_DONE plus the five pngs in runs/T21/shots/.
param([string]$Run = "T21")

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
	"-MageArenaDayCapture",
	"-MageArenaRun=$Run"
)

Write-Host "capture-day: starting UnrealEditor-Cmd run=$Run"
$Process = Start-Process -FilePath $Editor -ArgumentList $ArgumentList -PassThru -WindowStyle Hidden -RedirectStandardOutput $StdoutPath -RedirectStandardError $StderrPath
$Finished = $Process.WaitForExit(540000)
if (-not $Finished) {
	Write-Host "capture-day: 9 minute timeout, stopping the editor"
	& taskkill.exe /T /F /PID $Process.Id | Out-Host
	Start-Sleep -Seconds 2
}

$ShotNames = @(
	"01-ritual-septima.png",
	"02-ritual-both-palms.png",
	"03-bout3-intro.png",
	"04-bout4-intro.png",
	"05-aftermath-tablet.png"
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
	Write-Host "capture-day: ok"
	exit 0
}
Write-Host "capture-day: failed missing=$($Missing -join ',') done=$Done"
exit 1
