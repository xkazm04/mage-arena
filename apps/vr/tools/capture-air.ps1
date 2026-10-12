# T23 stills: Lio (the Tiro final's air mage) on the floor, a Veering Bolt arc telegraph, a Squall volley, Air Form
# active, and the Tempest Lance line telegraph. Success is MAGEVR_CAPTURE_DONE plus the five pngs in runs/T23/shots/.
param([string]$Run = "T23")

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
	"-MageArenaAirCapture",
	"-MageArenaRun=$Run"
)

Write-Host "capture-air: starting UnrealEditor-Cmd run=$Run"
$Process = Start-Process -FilePath $Editor -ArgumentList $ArgumentList -PassThru -WindowStyle Hidden -RedirectStandardOutput $StdoutPath -RedirectStandardError $StderrPath
$Finished = $Process.WaitForExit(540000)
if (-not $Finished) {
	Write-Host "capture-air: 9 minute timeout, stopping the editor"
	& taskkill.exe /T /F /PID $Process.Id | Out-Host
	Start-Sleep -Seconds 2
}

$ShotNames = @(
	"01-lio-on-the-floor.png",
	"02-veering-bolt-arc.png",
	"03-squall-volley.png",
	"04-air-form.png",
	"05-tempest-lance-line.png"
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
	Write-Host "capture-air: ok"
	exit 0
}
Write-Host "capture-air: failed missing=$($Missing -join ',') done=$Done"
exit 1
