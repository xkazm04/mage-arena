# T58 capture: posed stills of the spell VFX from the seated view (Session/SpellVfxCapture.h). One editor run in -game,
# real RHI, offscreen 1920x1080: 11 pngs in runs/<run>/shots/ (each school's bolt, absorb and perfect absorb, the
# unblockable telegraph, a steel stone) and manifest.json.
# Success: MAGEVR_CAPTURE_DONE and no MAGEVR_CAPTURE_FAIL in the log, and the manifest lists every shot on disk at the
# size the driver measured and past the pixel floor. The editor Quit code is not the gate.
# Usage: capture-spell-vfx.ps1 [-Run T58-before] [-ExtraArgs "-MageArenaSpellVfx=0"]   (=0: the presentation before T58)
param([ValidatePattern('^[A-Za-z0-9_-]+$')][string]$Run = "T58", [string[]]$ExtraArgs = @())

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
$Expected = @(
	"01-water-bolt.png", "02-water-absorb.png", "03-water-perfect.png",
	"04-fire-bolt.png", "05-fire-absorb.png", "06-fire-perfect.png",
	"07-air-bolt.png", "08-air-absorb.png", "09-air-perfect.png",
	"10-unblockable-telegraph.png", "11-steel-stone.png"
)

# One Unreal process machine-wide (CLAUDE.md): wait for any other to exit first.
while (Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -match "Unreal" }) {
	Write-Host "capture-spell-vfx: another Unreal process runs, waiting"
	Start-Sleep -Seconds 15
}

New-Item -ItemType Directory -Force -Path $ShotDir | Out-Null
Get-ChildItem -Path $ShotDir -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force
if (Test-Path $LogPath) { Remove-Item -Force $LogPath }

$ArgumentList = @(
	$Project, "-game", "-RenderOffScreen", "-ResX=1920", "-ResY=1080", "-unattended", "-nosplash", "-nohmd",
	"-stdout", "-FullStdOutLogOutput", "-log", "-abslog=$LogPath",
	"-MageArenaSpellVfxCapture", "-MageArenaRun=$Run"
) + $ExtraArgs
Write-Host "capture-spell-vfx: run=$Run"
$Process = Start-Process -FilePath $Editor -ArgumentList $ArgumentList -PassThru -WindowStyle Hidden `
	-RedirectStandardOutput (Join-Path $RunDir "capture.stdout.log") -RedirectStandardError (Join-Path $RunDir "capture.stderr.log")
if (-not $Process.WaitForExit(600000)) {
	Write-Host "capture-spell-vfx: 10 minute timeout, stopping the editor (a failure)"
	& taskkill.exe /T /F /PID $Process.Id | Out-Host
	exit 1
}
$Text = ""
if (Test-Path $LogPath) {
	$Text = Get-Content -Raw -Path $LogPath
	Select-String -Path $LogPath -Pattern "MAGEVR_T58|MAGEVR_CAPTURE|MAGEVR_SPELLVFX" | ForEach-Object { Write-Host $_.Line }
}
$Pass = ($Text -match "MAGEVR_CAPTURE_DONE") -and -not ($Text -match "MAGEVR_CAPTURE_FAIL")
$ManifestPath = Join-Path $ShotDir "manifest.json"
if (-not (Test-Path $ManifestPath)) {
	Write-Host "capture-spell-vfx: missing manifest $ManifestPath"
	$Pass = $false
} else {
	$Manifest = Get-Content -Raw -Path $ManifestPath | ConvertFrom-Json
	$Names = @($Manifest.shots | ForEach-Object { $_.name })
	foreach ($Name in $Expected) {
		if ($Names -notcontains $Name) { Write-Host "capture-spell-vfx: manifest lists no $Name"; $Pass = $false }
	}
	foreach ($Shot in @($Manifest.shots)) {
		if (-not (Test-Path $Shot.path)) { Write-Host "capture-spell-vfx: $($Shot.name) is not on disk"; $Pass = $false; continue }
		if ((Get-Item $Shot.path).Length -ne [int64]$Shot.bytes) { Write-Host "capture-spell-vfx: $($Shot.name) is not the size the driver measured"; $Pass = $false }
		if ($Shot.pixel.ok -ne $true) { Write-Host "capture-spell-vfx: $($Shot.name) failed the pixel floor ($($Shot.pixel.why))"; $Pass = $false }
	}
}
Write-Host ("capture-spell-vfx: {0}" -f $(if ($Pass) { "OK" } else { "FAIL" }))
if ($Pass) { exit 0 } else { exit 1 }
