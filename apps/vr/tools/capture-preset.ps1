# T20 stills: the Tide Orb IV pick on the intermission stones, a reflected fire bolt, and a Leviathan Orb with its path.
# Success is MAGEVR_CAPTURE_DONE plus the five pngs in runs/T20/shots/.
param([string]$Run = "T20")

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
	"-MageArenaPresetCapture",
	"-MageArenaRun=$Run"
)

Write-Host "capture-preset: starting UnrealEditor-Cmd run=$Run"
$Process = Start-Process -FilePath $Editor -ArgumentList $ArgumentList -PassThru -WindowStyle Hidden -RedirectStandardOutput $StdoutPath -RedirectStandardError $StderrPath
$Finished = $Process.WaitForExit(540000)
if (-not $Finished) {
	Write-Host "capture-preset: 9 minute timeout, stopping the editor"
	& taskkill.exe /T /F /PID $Process.Id | Out-Host
	Start-Sleep -Seconds 2
}

$ShotNames = @(
	"01-intermission-stones.png",
	"02-leviathan-stone-hold.png",
	"03-reflected-firebolt.png",
	"04-leviathan-telegraph.png",
	"05-leviathan-orb-path.png"
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
	Write-Host "capture-preset: ok"
	exit 0
}
Write-Host "capture-preset: failed missing=$($Missing -join ',') done=$Done"
exit 1
