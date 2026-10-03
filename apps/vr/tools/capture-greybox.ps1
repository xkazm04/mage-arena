# Runs the greybox capture: real RHI, offscreen 1920x1080, scripted shots, desktop budget.
# Success is MAGEVR_CAPTURE_DONE plus the five pngs and budget.json. The editor Quit code is not the gate.
$ErrorActionPreference = "Stop"
$ToolsDir = $PSScriptRoot
$VrDir = Split-Path -Parent $ToolsDir
$AppsDir = Split-Path -Parent $VrDir
# tools -> vr -> apps -> git root. runs/A02 lives at the git root.
$Repo = Split-Path -Parent $AppsDir
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = Join-Path $VrDir "Game\MageArenaVR.uproject"
$RunDir = Join-Path $Repo "runs\A02"
$ShotDir = Join-Path $RunDir "shots"
$BudgetPath = Join-Path $RunDir "budget.json"
$LogPath = Join-Path $RunDir "capture.log"
$StdoutPath = Join-Path $RunDir "capture.stdout.log"
$StderrPath = Join-Path $RunDir "capture.stderr.log"

New-Item -ItemType Directory -Force -Path $ShotDir | Out-Null
Get-ChildItem -Path $ShotDir -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force
if (Test-Path $BudgetPath) { Remove-Item -Force $BudgetPath }
if (Test-Path $LogPath) { Remove-Item -Force $LogPath }
if (Test-Path $StdoutPath) { Remove-Item -Force $StdoutPath }
if (Test-Path $StderrPath) { Remove-Item -Force $StderrPath }

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
	"-MageArenaGreyboxCapture",
	"-csvprofile",
	"-csvCaptureFrames=240"
)

Write-Host "capture: starting UnrealEditor-Cmd"
$Process = Start-Process -FilePath $Editor -ArgumentList $ArgumentList -PassThru -WindowStyle Hidden -RedirectStandardOutput $StdoutPath -RedirectStandardError $StderrPath
$Finished = $Process.WaitForExit(720000)
if (-not $Finished) {
	Write-Host "capture: 12 minute timeout, stopping the editor"
	& taskkill.exe /T /F /PID $Process.Id | Out-Host
	Start-Sleep -Seconds 2
}

$ShotNames = @(
	"01-initial-view.png",
	"02-sigil-line2-cast.png",
	"03-ward-arc.png",
	"04-threats-midflight.png",
	"05-blink-left-pad.png"
)

function Get-Median([double[]]$Values) {
	if ($Values.Count -eq 0) { return 0.0 }
	$Sorted = @($Values | Sort-Object)
	$Mid = [int][Math]::Floor($Sorted.Count / 2)
	if (($Sorted.Count % 2) -eq 0) {
		return 0.5 * ([double]$Sorted[$Mid - 1] + [double]$Sorted[$Mid])
	}
	return [double]$Sorted[$Mid]
}

function Repair-BudgetFromCsv {
	if (-not (Test-Path $BudgetPath)) { return }
	$Budget = Get-Content -Raw -Path $BudgetPath | ConvertFrom-Json
	# The C++ capture writes the LOD0 sum. Never replace that with RHI or CSV primitive counts.
	$Names = @($Budget.PSObject.Properties.Name)
	if ($Names -contains "plausibilityOk" -or $Names -contains "measuredTriangles" -or $Names -contains "visibleLod0Triangles") {
		return
	}
	$DrawMax = 0
	$TriMax = 0
	if ($Budget.drawCalls) { $DrawMax = [double]$Budget.drawCalls.max }
	if ($Budget.triangles) { $TriMax = [double]$Budget.triangles.max }
	if ($DrawMax -gt 0 -or $TriMax -gt 0) { return }

	$Profiling = Join-Path $VrDir "Game\Saved\Profiling"
	if (-not (Test-Path $Profiling)) {
		Write-Host "capture: RHI counters were 0 and no CSV profile was written"
		return
	}
	$Csv = Get-ChildItem -Path $Profiling -Filter *.csv -Recurse -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
	if (-not $Csv) {
		Write-Host "capture: RHI counters were 0 and no CSV file was found"
		return
	}

	$Lines = Get-Content -Path $Csv.FullName
	$HeaderIndex = -1
	for ($Index = 0; $Index -lt [Math]::Min($Lines.Count, 40); $Index++) {
		if ($Lines[$Index] -match "DrawCalls" -and $Lines[$Index] -match "PrimitivesDrawn") {
			$HeaderIndex = $Index
			break
		}
	}
	if ($HeaderIndex -lt 0) {
		Write-Host "capture: CSV has no DrawCalls column: $($Csv.FullName)"
		return
	}
	$Header = $Lines[$HeaderIndex].Split(",")
	$DrawCol = [array]::IndexOf($Header, ($Header | Where-Object { $_ -match "DrawCalls" } | Select-Object -First 1))
	$TriCol = [array]::IndexOf($Header, ($Header | Where-Object { $_ -match "PrimitivesDrawn" } | Select-Object -First 1))
	$TimeCol = [array]::IndexOf($Header, ($Header | Where-Object { $_ -match "FrameTime" } | Select-Object -First 1))
	$Draws = New-Object System.Collections.Generic.List[double]
	$Tris = New-Object System.Collections.Generic.List[double]
	$Times = New-Object System.Collections.Generic.List[double]
	for ($Row = $HeaderIndex + 1; $Row -lt $Lines.Count; $Row++) {
		if ([string]::IsNullOrWhiteSpace($Lines[$Row])) { continue }
		$Cells = $Lines[$Row].Split(",")
		if ($DrawCol -ge 0 -and $DrawCol -lt $Cells.Count) {
			$Value = 0.0
			if ([double]::TryParse($Cells[$DrawCol], [ref]$Value) -and $Value -gt 0) { $Draws.Add($Value) }
		}
		if ($TriCol -ge 0 -and $TriCol -lt $Cells.Count) {
			$Value = 0.0
			if ([double]::TryParse($Cells[$TriCol], [ref]$Value) -and $Value -gt 0) { $Tris.Add($Value) }
		}
		if ($TimeCol -ge 0 -and $TimeCol -lt $Cells.Count) {
			$Value = 0.0
			if ([double]::TryParse($Cells[$TimeCol], [ref]$Value) -and $Value -gt 0) { $Times.Add($Value) }
		}
	}
	if ($Draws.Count -eq 0 -and $Tris.Count -eq 0) {
		Write-Host "capture: CSV rows had no positive RHI counts"
		return
	}
	$TimeNote = "csv FrameTime column"
	if ($Times.Count -gt 0 -and (Get-Median $Times.ToArray()) -lt 1.0) {
		for ($Index = 0; $Index -lt $Times.Count; $Index++) { $Times[$Index] = $Times[$Index] * 1000.0 }
		$TimeNote = "csv FrameTime looked like seconds and was scaled to milliseconds"
	}
	$DrawArr = $Draws.ToArray()
	$TriArr = $Tris.ToArray()
	$TimeArr = $Times.ToArray()
	$MaxDraw = $(if ($DrawArr.Count) { ($DrawArr | Measure-Object -Maximum).Maximum } else { 0 })
	$MaxTri = $(if ($TriArr.Count) { ($TriArr | Measure-Object -Maximum).Maximum } else { 0 })
	$Within = ($MaxDraw -le 150 -and $MaxTri -le 350000 -and $MaxDraw -gt 0)
	$Payload = [ordered]@{
		label = "desktop proxy - not Quest truth"
		source = "CSV profiler RHI/DrawCalls and RHI/PrimitivesDrawn. C++ RHI counters were 0."
		sampleCount = [Math]::Max($DrawArr.Count, $TriArr.Count)
		targets = [ordered]@{
			drawCalls = 150
			triangles = 350000
			frameTimeMs = 13.9
			frameTimeNote = "13.9 ms is the Quest 72 Hz budget. This file is a desktop proxy, not Quest truth. $TimeNote"
		}
		drawCalls = [ordered]@{
			min = $(if ($DrawArr.Count) { ($DrawArr | Measure-Object -Minimum).Minimum } else { 0 })
			median = $(Get-Median $DrawArr)
			max = $MaxDraw
		}
		triangles = [ordered]@{
			min = $(if ($TriArr.Count) { ($TriArr | Measure-Object -Minimum).Minimum } else { 0 })
			median = $(Get-Median $TriArr)
			max = $MaxTri
		}
		frameTimeMs = [ordered]@{
			min = $(if ($TimeArr.Count) { ($TimeArr | Measure-Object -Minimum).Minimum } else { 0 })
			median = $(Get-Median $TimeArr)
			p95 = $(if ($TimeArr.Count) { $Sorted = @($TimeArr | Sort-Object); $Sorted[[Math]::Min($Sorted.Count - 1, [Math]::Max(0, [Math]::Ceiling(0.95 * $Sorted.Count) - 1))] } else { 0 })
			max = $(if ($TimeArr.Count) { ($TimeArr | Measure-Object -Maximum).Maximum } else { 0 })
		}
		withinCountBudget = [bool]$Within
		shots = @($ShotNames)
		csv = $Csv.FullName
	}
	$Payload | ConvertTo-Json -Depth 6 | Set-Content -Path $BudgetPath -Encoding utf8
	Write-Host "capture: rewrote budget.json from $($Csv.Name)"
}

$LogText = ""
if (Test-Path $LogPath) { $LogText = Get-Content -Raw -Path $LogPath }
$Done = $LogText -match "MAGEVR_CAPTURE_DONE"
Repair-BudgetFromCsv

$Missing = @()
foreach ($Name in $ShotNames) {
	$Path = Join-Path $ShotDir $Name
	if (-not (Test-Path $Path)) { $Missing += $Name }
}
$HasBudget = Test-Path $BudgetPath

$Plausible = $false
$ColoursOk = $false
if ($HasBudget) {
	$Budget = Get-Content -Raw -Path $BudgetPath | ConvertFrom-Json
	Write-Host ("budget label: {0}" -f $Budget.label)
	Write-Host ("samples: {0}" -f $Budget.sampleCount)
	Write-Host ("draw calls min/median/max: {0} / {1} / {2}" -f $Budget.drawCalls.min, $Budget.drawCalls.median, $Budget.drawCalls.max)
	Write-Host ("triangles min/median/max: {0} / {1} / {2}" -f $Budget.triangles.min, $Budget.triangles.median, $Budget.triangles.max)
	Write-Host ("frame ms min/median/p95/max: {0} / {1} / {2} / {3}" -f $Budget.frameTimeMs.min, $Budget.frameTimeMs.median, $Budget.frameTimeMs.p95, $Budget.frameTimeMs.max)
	Write-Host ("within count budget: {0}" -f $Budget.withinCountBudget)
	if ($Budget.plausibilityOk -eq $true) { $Plausible = $true }
	if ($Budget.coloursOk -eq $true) { $ColoursOk = $true }
	Write-Host ("plausibility: {0}" -f $Plausible)
	Write-Host ("colours: {0}" -f $ColoursOk)
}

if ($Done -and $Missing.Count -eq 0 -and $HasBudget -and $Plausible -and $ColoursOk) {
	Write-Host "capture: ok"
	exit 0
}

Write-Host "capture: failed"
if (-not $Done) { Write-Host "missing MAGEVR_CAPTURE_DONE in $LogPath" }
if ($Missing.Count -gt 0) { Write-Host ("missing shots: {0}" -f ($Missing -join ", ")) }
if (-not $HasBudget) { Write-Host "missing $BudgetPath" }
if ($HasBudget -and -not $Plausible) { Write-Host "budget plausibilityOk is not true" }
if ($HasBudget -and -not $ColoursOk) { Write-Host "budget coloursOk is not true" }
if ($LogText -match "MAGEVR_CAPTURE_FAIL") { Write-Host "log contains MAGEVR_CAPTURE_FAIL" }
exit 1
