# Runs the D-G4 stress scene (docs/PROJECT-PLAN.md D-G4): greybox arena, 20 enemy proxies, 100 projectiles,
# both hands from a looping clip. Real RHI, offscreen 1920x1080, -nohmd, the project's forward renderer.
# Each launch samples 60 s after a warm-up and writes runs/DG4/<run>/ (gitignored).
# Success per launch is MAGEVR_BUDGET_DONE plus both MAGEVR_BUDGET_POP lines at full count. The editor exit code is not the gate.
# Usage: capture-budget.ps1 [-Runs 3] [-Prefix run] [-ExtraArgs "-csvCategories=MeshDrawCommandStats", ...]
# Per-pass GPU-culled triangle counts come from r.MeshDrawCommands.DumpStats frames the driver requests every 10 s.
param(
	[int]$Runs = 3,
	[string]$Prefix = "run",
	[string[]]$ExtraArgs = @()
)
$ErrorActionPreference = "Stop"
$ToolsDir = $PSScriptRoot
$VrDir = Split-Path -Parent $ToolsDir
$AppsDir = Split-Path -Parent $VrDir
# tools -> vr -> apps -> git root. runs/DG4 lives at the git root.
$Repo = Split-Path -Parent $AppsDir
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = Join-Path $VrDir "Game\MageArenaVR.uproject"
$Profiling = Join-Path $VrDir "Game\Saved\Profiling"
$Root = Join-Path $Repo "runs\DG4"
$TimeoutMs = 600000

function Get-Spread([double[]]$Values) {
	if (-not $Values -or $Values.Count -eq 0) {
		return [ordered]@{ n = 0; min = 0; median = 0; p95 = 0; max = 0 }
	}
	$Sorted = @($Values | Sort-Object)
	$Count = $Sorted.Count
	$Mid = [int][Math]::Floor($Count / 2)
	$Median = $(if (($Count % 2) -eq 0) { 0.5 * ($Sorted[$Mid - 1] + $Sorted[$Mid]) } else { $Sorted[$Mid] })
	$P95 = $Sorted[[Math]::Min($Count - 1, [Math]::Max(0, [int][Math]::Ceiling(0.95 * $Count) - 1))]
	return [ordered]@{ n = $Count; min = $Sorted[0]; median = $Median; p95 = $P95; max = $Sorted[$Count - 1] }
}

# UE csv profiler files: a header row (first column EVENTS), numeric rows, then metadata rows starting with "[",
# and with [HasHeaderRowAtEnd] a final, complete header. Columns are found by name in the last header.
function Read-ProfileCsv([string]$Path) {
	$Lines = Get-Content -Path $Path
	if ($Lines.Count -lt 2) { return $null }
	$HeaderLine = $Lines | Where-Object { $_.StartsWith("EVENTS,") } | Select-Object -Last 1
	if (-not $HeaderLine) { $HeaderLine = $Lines[0] }
	$Header = $HeaderLine.Split(",")
	$Wanted = [ordered]@{
		drawCalls = '^(RHI/)?DrawCalls$'
		drawCallsBasePass = '^DrawCall/Basepass$'
		drawCallsPrepass = '^DrawCall/Prepass$'
		drawCallsOcclusionTests = '^DrawCall/BeginOcclusionTests$'
		drawCallsSlateUI = '^DrawCall/SlateUI$'
		rhiPrimitivesDrawn = '^(RHI/)?PrimitivesDrawn$'
		meshDrawCommandVisiblePrimitives = '^MeshDrawCommandStats/Untracked$'
		frameTimeMs = '^FrameTime$'
		gameThreadMs = '^GameThreadTime$'
		renderThreadMs = '^RenderThreadTime$'
		gpuMs = '^GPUTime$'
		physicalUsedMB = '^(Memory/)?PhysicalUsedMB$'
	}
	$Columns = [ordered]@{}
	foreach ($Key in $Wanted.Keys) {
		$Index = -1
		for ($Col = 0; $Col -lt $Header.Count; $Col++) {
			if ($Header[$Col].Trim() -match $Wanted[$Key]) { $Index = $Col; break }
		}
		$Columns[$Key] = $Index
	}
	$Series = [ordered]@{}
	foreach ($Key in $Wanted.Keys) { $Series[$Key] = New-Object System.Collections.Generic.List[double] }
	for ($Row = 1; $Row -lt $Lines.Count; $Row++) {
		$Line = $Lines[$Row]
		if ($Line.StartsWith("[") -or $Line.StartsWith("EVENTS,")) { continue }
		$Cells = $Line.Split(",")
		$Second = 0.0
		if ($Cells.Count -lt 2 -or -not [double]::TryParse($Cells[1], [ref]$Second)) { continue }
		foreach ($Key in $Wanted.Keys) {
			$Col = $Columns[$Key]
			if ($Col -lt 0 -or $Col -ge $Cells.Count) { continue }
			$Value = 0.0
			if ([double]::TryParse($Cells[$Col], [ref]$Value)) { $Series[$Key].Add($Value) }
		}
	}
	$Out = [ordered]@{ columns = [ordered]@{} }
	foreach ($Key in $Wanted.Keys) {
		$Out.columns[$Key] = $(if ($Columns[$Key] -ge 0) { $Header[$Columns[$Key]].Trim() } else { "missing" })
		$Out[$Key] = Get-Spread $Series[$Key].ToArray()
	}
	return $Out
}

# r.MeshDrawCommands.DumpStats files: one frame each, a row per visible draw with its pass and GPU-culled primitive count.
function Read-MeshDrawDumps([System.IO.FileInfo[]]$Files) {
	$PerPass = @{}
	$Totals = New-Object System.Collections.Generic.List[double]
	foreach ($File in $Files) {
		$Rows = Import-Csv -Path $File.FullName
		$Frame = @{}
		$Total = 0.0
		foreach ($Row in $Rows) {
			$Count = 0.0
			if (-not [double]::TryParse($Row.VisiblePrimitiveCount, [ref]$Count)) { continue }
			$Pass = $Row.Pass
			if (-not $Frame.ContainsKey($Pass)) { $Frame[$Pass] = 0.0 }
			$Frame[$Pass] += $Count
			$Total += $Count
		}
		$Totals.Add($Total)
		foreach ($Pass in $Frame.Keys) {
			if (-not $PerPass.ContainsKey($Pass)) { $PerPass[$Pass] = New-Object System.Collections.Generic.List[double] }
			$PerPass[$Pass].Add($Frame[$Pass])
		}
	}
	$Passes = [ordered]@{}
	foreach ($Pass in ($PerPass.Keys | Sort-Object)) { $Passes[$Pass] = Get-Spread $PerPass[$Pass].ToArray() }
	return [ordered]@{ frames = $Files.Count; allPasses = (Get-Spread $Totals.ToArray()); perPass = $Passes }
}

function Wait-UnrealLane {
	$Names = @("UnrealEditor", "UnrealEditor-Cmd", "UnrealBuildTool", "ShaderCompileWorker")
	for ($Poll = 0; $Poll -le 30; $Poll++) {
		$Busy = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $Names -contains $_.ProcessName })
		if ($Busy.Count -eq 0) { return $true }
		Write-Host ("lane busy: {0}; waiting 60 s" -f (($Busy | ForEach-Object { "$($_.ProcessName):$($_.Id)" }) -join ", "))
		Start-Sleep -Seconds 60
	}
	return $false
}

New-Item -ItemType Directory -Force -Path $Root | Out-Null
$AllOk = $true
$Results = @()
for ($Index = 1; $Index -le $Runs; $Index++) {
	$Run = "$Prefix$Index"
	$RunDir = Join-Path $Root $Run
	if (Test-Path $RunDir) { Remove-Item -Recurse -Force $RunDir }
	New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
	$LogPath = Join-Path $RunDir "capture.log"
	$StdoutPath = Join-Path $RunDir "capture.stdout.log"
	$StderrPath = Join-Path $RunDir "capture.stderr.log"

	if (-not (Wait-UnrealLane)) {
		Write-Host "budget: the Unreal lane stayed busy for 30 minutes; stopping before $Run"
		exit 2
	}

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
		"-MageArenaBudgetStress",
		"-MageArenaBudgetRun=$Run",
		"-csvprofile"
	) + $ExtraArgs
	$Command = "`"$Editor`" " + ($ArgumentList -join " ")
	Write-Host "budget: $Run starting UnrealEditor-Cmd"
	$Started = Get-Date
	$Process = Start-Process -FilePath $Editor -ArgumentList $ArgumentList -PassThru -WindowStyle Hidden -RedirectStandardOutput $StdoutPath -RedirectStandardError $StderrPath
	$Finished = $Process.WaitForExit($TimeoutMs)
	if (-not $Finished) {
		Write-Host "budget: $Run hit the 10 minute timeout, stopping the editor it started (pid $($Process.Id))"
		& taskkill.exe /T /F /PID $Process.Id | Out-Host
		Start-Sleep -Seconds 2
	}
	$Ended = Get-Date

	$LogText = ""
	if (Test-Path $LogPath) { $LogText = Get-Content -Raw -Path $LogPath }
	$Done = $LogText -match "MAGEVR_BUDGET_DONE"
	$PopLines = @([regex]::Matches($LogText, "MAGEVR_BUDGET_POP enemies=(\d+) projectiles=(\d+) hands=(\d+) phase=(\w+)[^\r\n]*") | ForEach-Object { $_.Value })
	$PopOk = $PopLines.Count -ge 2
	foreach ($Line in $PopLines) {
		if ($Line -match "enemies=(\d+) projectiles=(\d+) hands=(\d+)") {
			if ([int]$Matches[1] -lt 20 -or [int]$Matches[2] -lt 100 -or [int]$Matches[3] -lt 2) { $PopOk = $false }
		}
	}
	$FailLines = @([regex]::Matches($LogText, "MAGEVR_BUDGET_FAIL[^\r\n]*") | ForEach-Object { $_.Value })

	$DumpDir = Join-Path $RunDir "mesh-draw-dumps"
	New-Item -ItemType Directory -Force -Path $DumpDir | Out-Null
	$Dumps = @()
	if (Test-Path $Profiling) {
		$Dumps = @(Get-ChildItem -Path $Profiling -Filter "MeshDrawCommandStats-DG4$Run-*.csv" -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -ge $Started })
		foreach ($Dump in $Dumps) { Move-Item -Force -Path $Dump.FullName -Destination $DumpDir }
		$Dumps = @(Get-ChildItem -Path $DumpDir -Filter *.csv)
	}

	$DriverPath = Join-Path $RunDir "driver.json"
	$Driver = $null
	if (Test-Path $DriverPath) { $Driver = Get-Content -Raw -Path $DriverPath | ConvertFrom-Json }
	$CsvFile = Get-ChildItem -Path $RunDir -Filter "dg4-*.csv" -ErrorAction SilentlyContinue | Select-Object -First 1
	$Csv = $null
	if ($CsvFile) { $Csv = Read-ProfileCsv $CsvFile.FullName }
	$MeshDraw = $null
	if ($Dumps.Count -gt 0) { $MeshDraw = Read-MeshDrawDumps $Dumps }

	$Ok = $Done -and $PopOk -and $Driver -and $Driver.ok
	if (-not $Ok) { $AllOk = $false }
	$Summary = [ordered]@{
		run = $Run
		label = "desktop proxy, synthetic clip-driven hands, not Quest truth; cannot prove 72 Hz"
		ok = [bool]$Ok
		doneMarker = [bool]$Done
		populationLines = $PopLines
		failLines = $FailLines
		started = $Started.ToString("s")
		wallSeconds = [Math]::Round(($Ended - $Started).TotalSeconds, 1)
		timedOut = -not $Finished
		editorExitCode = $(if ($Finished) { $Process.ExitCode } else { $null })
		command = $Command.Replace($Repo, "<repo>")
		driver = $Driver
		csvProfile = $Csv
		meshDrawCommandDumps = $MeshDraw
	}
	$SummaryPath = Join-Path $RunDir "summary.json"
	# The summary is small enough to commit, so local paths become <repo>.
	$Json = $Summary | ConvertTo-Json -Depth 10
	$Json = $Json.Replace($Repo.Replace("\", "\\"), "<repo>").Replace($Repo.Replace("\", "/"), "<repo>")
	$Json | Set-Content -Path $SummaryPath -Encoding utf8
	$Results += $Summary

	Write-Host ("budget: {0} ok={1} done={2} pop={3}" -f $Run, $Ok, $Done, ($PopLines -join " | "))
	if ($Driver) {
		Write-Host ("  frames n={0} warm-up {1:N1} s" -f $Driver.frames, $Driver.warmupS)
		Write-Host ("  draw calls min/median/p95/max: {0} / {1} / {2} / {3}" -f $Driver.drawCalls.min, $Driver.drawCalls.median, $Driver.drawCalls.p95, $Driver.drawCalls.max)
		Write-Host ("  RHI primitives (lower bound) median/max: {0} / {1}" -f $Driver.rhiPrimitivesDrawn.median, $Driver.rhiPrimitivesDrawn.max)
		Write-Host ("  LOD0 in-frustum (estimate) median/max: {0} / {1}" -f $Driver.lod0InFrustumTriangles.median, $Driver.lod0InFrustumTriangles.max)
		Write-Host ("  frame ms median/p95: {0:N2} / {1:N2}; gpu ms median/p95: {2:N2} / {3:N2}; peak working set {4:N0} MB" -f $Driver.frameMs.median, $Driver.frameMs.p95, $Driver.gpuMs.median, $Driver.gpuMs.p95, $Driver.peakWorkingSetMB)
	}
	if ($Csv) {
		Write-Host ("  csv mesh-draw-command visible primitives (all passes) median/max: {0} / {1} [{2}]" -f $Csv.meshDrawCommandVisiblePrimitives.median, $Csv.meshDrawCommandVisiblePrimitives.max, $Csv.columns.meshDrawCommandVisiblePrimitives)
		Write-Host ("  csv game thread ms median/p95: {0} / {1} [{2}]" -f $Csv.gameThreadMs.median, $Csv.gameThreadMs.p95, $Csv.columns.gameThreadMs)
	}
	if ($MeshDraw) {
		foreach ($Pass in $MeshDraw.perPass.Keys) {
			Write-Host ("  dump pass {0}: median {1} max {2} (n={3})" -f $Pass, $MeshDraw.perPass[$Pass].median, $MeshDraw.perPass[$Pass].max, $MeshDraw.perPass[$Pass].n)
		}
	}
	foreach ($Line in $FailLines) { Write-Host "  $Line" }
}

if ($AllOk) {
	Write-Host "budget: ok ($Runs runs)"
	exit 0
}
Write-Host "budget: failed"
exit 1
