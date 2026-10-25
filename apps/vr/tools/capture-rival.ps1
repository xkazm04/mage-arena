# T38 the first animated opponent (apps/vr/tasks/T38-first-animated-opponent.md, docs/research/T38-RIVAL-2026-10-25.md).
# One run: imports Brennic, the cinder hound and the mire maw into the scratch folder
# apps/vr/Game/Content/CreatureImportScratch through the ImportCreatureMesh commandlet, as capture-creature-import.ps1
# does; stills the hound and the maw at 8 m under the T56 dusk; replays the seated Brennic duel with his rigged figure
# (-MageArenaCreatureImportCapture -MageArenaRivalReplay, Greybox/CreatureImportCapture.h): seed 1, the reference seated
# script, then seed 2, the lowest seed where the same script downs him (Brennic wins seed 1, DF-011, so it never shows
# Death); with -Budget, re-runs the D-G4 budget with the figures in the scene (capture-budget.ps1 -ExtraArgs
# -MageArenaCreatureImportScene -MageArenaCreatureSkip=Brennic: the session draws Brennic as the figure, the scene adds
# the hound and the maw); and moves everything it made to -Staging: the imported .uasset files, runs/T38/ (the frames,
# the crop sheets, the manifest, the clip timeline, SHA256SUMS.txt), the budget runs and the logs. The scratch folder is
# deleted before the script exits, success or not. The DINOv3 licence hold stands: nothing imported is ever committed.
# Success is MAGEVR_CAPTURE_DONE with no MAGEVR_CAPTURE_FAIL, every manifest shot landed and past the pixel floor, a full
# frame set for each of the four clips, and, with -Budget, capture-budget.ps1's own ok.
# Usage: capture-rival.ps1 [-Run T38-rival] [-Seeds 1,2] [-Budget] [-BudgetRuns 3] [-BudgetPrefix rival] [-Staging <dir>]
# The helpers (the Unreal lane wait, the Blender and ComfyUI refusal, the scratch removal) and the sha256-pinned sources
# are capture-creature-import.ps1's: they are read from that script, not copied.
param(
	[ValidatePattern('^[A-Za-z0-9_-]+$')][string]$Run = "T38-rival",
	[ValidatePattern('^[0-9]+(,[0-9]+)*$')][string]$Seeds = "1,2",
	[switch]$Budget,
	[int]$BudgetRuns = 3,
	[ValidatePattern('^[A-Za-z0-9_-]+$')][string]$BudgetPrefix = "rival",
	[string]$Staging = "C:\Users\kazda\mage-art-staging\t38-rival-2026-10-25",
	[string]$BrennicDir = "C:\Users\kazda\mage-art-staging\brennic-mesh-rework-2026-10-24\rig",
	[string]$HoundDir = "C:\Users\kazda\mage-art-staging\creature-mesh-2026-10-24\hound\rig",
	[string]$MawDir = "C:\Users\kazda\mage-art-staging\maw-seam-2026-10-24\rig"
)

$ErrorActionPreference = "Stop"
$ToolsDir = $PSScriptRoot
$VrDir = Split-Path -Parent $ToolsDir
$AppsDir = Split-Path -Parent $VrDir
$Repo = Split-Path -Parent $AppsDir
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$Project = Join-Path $VrDir "Game\MageArenaVR.uproject"
$Scratch = Join-Path $VrDir "Game\Content\CreatureImportScratch"
$RunDir = Join-Path $Repo "runs\$Run"
$StageRuns = Join-Path $Staging "runs\T38"

# capture-creature-import.ps1's helpers and its $Sources table (each FBX and texture with the sha256 its research doc
# records), taken from that script's syntax tree so there is one copy. A missing piece is a loud failure.
$HelperScript = Join-Path $ToolsDir "capture-creature-import.ps1"
$ParseErrors = $null
$Ast = [System.Management.Automation.Language.Parser]::ParseFile($HelperScript, [ref]$null, [ref]$ParseErrors)
if ($ParseErrors.Count -gt 0) { throw "capture-creature-import.ps1 does not parse: $($ParseErrors[0])" }
foreach ($Name in @("Wait-UnrealLane", "Assert-NoNeighbours", "Remove-Scratch")) {
	$Function = $Ast.Find({ param($Node) $Node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $Node.Name -eq $Name }, $true)
	if (-not $Function) { throw "capture-creature-import.ps1 has no function $Name" }
	. ([ScriptBlock]::Create($Function.Extent.Text))
}
$SourcesAst = $Ast.Find({ param($Node) $Node -is [System.Management.Automation.Language.AssignmentStatementAst] -and $Node.Left.Extent.Text -eq '$Sources' }, $true)
if (-not $SourcesAst) { throw "capture-creature-import.ps1 has no `$Sources table" }
. ([ScriptBlock]::Create($SourcesAst.Extent.Text))
if ($Sources.Count -ne 3) { throw "capture-creature-import.ps1's `$Sources lists $($Sources.Count) creatures, not 3" }

function Get-Sha([string]$Path) { (Get-FileHash -Algorithm SHA256 -Path $Path).Hash.ToLower() }

if (Test-Path $RunDir) { Remove-Item -Recurse -Force $RunDir }
New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
New-Item -ItemType Directory -Force -Path $Staging | Out-Null

foreach ($Source in $Sources) {
	foreach ($Pair in @(@($Source.Fbx, $Source.FbxSha), @($Source.Texture, $Source.TextureSha))) {
		if (-not (Test-Path $Pair[0])) { throw "missing input $($Pair[0]) (stop and report; never re-run Blender here)" }
		$Sha = Get-Sha $Pair[0]
		if ($Sha -ne $Pair[1]) { throw "sha256 mismatch on $($Pair[0]): $Sha, the research doc records $($Pair[1])" }
		Write-Host "rival: input ok $($Pair[0]) $Sha"
	}
}

$Ok = $false
try {
	Remove-Scratch
	foreach ($Source in $Sources) {
		Assert-NoNeighbours
		$Log = Join-Path $RunDir "import-$($Source.Name).log"
		Write-Host "rival: importing $($Source.Name)"
		& $Editor $Project -run=ImportCreatureMesh "-name=$($Source.Name)" "-fbx=$($Source.Fbx)" "-texture=$($Source.Texture)" -unattended -nosplash -nop4 -stdout -FullStdOutLogOutput "-abslog=$Log" | Out-Null
		$Code = $LASTEXITCODE
		Select-String -Path $Log -Pattern "MAGEVR_CREATURE_IMPORT (mesh|clip|done|fail)" | ForEach-Object { Write-Host $_.Line }
		if ($Code -ne 0 -or -not (Select-String -Path $Log -Pattern "MAGEVR_CREATURE_IMPORT done" -Quiet)) {
			throw "import of $($Source.Name) failed exit=$Code (log: $Log)"
		}
	}

	Assert-NoNeighbours
	$LogPath = Join-Path $RunDir "capture.log"
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
		"-MageArenaCreatureImportCapture",
		"-MageArenaRivalReplay",
		"-MageArenaRun=$Run",
		"-MageArenaRivalSeeds=$Seeds",
		"-usefixedtimestep",
		"-fps=60",
		"-ExecCmds=`"DisableAllScreenMessages; t.MaxFPS 60`""
	)
	Write-Host "rival: starting the replay run=$Run seeds=$Seeds"
	$Process = Start-Process -FilePath $Editor -ArgumentList $ArgumentList -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $RunDir "capture.stdout.log") -RedirectStandardError (Join-Path $RunDir "capture.stderr.log")
	if (-not $Process.WaitForExit(1620000)) {
		Write-Host "rival: 27 minute timeout, stopping the editor it started"
		& taskkill.exe /T /F /PID $Process.Id | Out-Host
		Start-Sleep -Seconds 2
	}
	$LogText = $(if (Test-Path $LogPath) { Get-Content -Raw -Path $LogPath } else { "" })
	if (Test-Path $LogPath) { Select-String -Path $LogPath -Pattern "MAGEVR_(RIVAL_FIGURE|RIVAL_REPLAY|CAPTURE)" | ForEach-Object { Write-Host $_.Line } }
	$Done = $LogText -match "MAGEVR_CAPTURE_DONE"
	$Failed = $LogText -match "MAGEVR_CAPTURE_FAIL"

	# The manifest is the list of frames; each must be on disk at the size the driver measured and past the pixel floor.
	$Problems = @()
	$Manifest = $null
	$ManifestPath = Join-Path $RunDir "manifest.json"
	if (Test-Path $ManifestPath) { $Manifest = Get-Content -Raw -Path $ManifestPath | ConvertFrom-Json } else { $Problems += "no manifest" }
	if ($Manifest) {
		foreach ($Shot in @($Manifest.shots)) {
			if (-not (Test-Path $Shot.path)) { $Problems += "$($Shot.name) is not on disk" }
			elseif ((Get-Item $Shot.path).Length -ne [int64]$Shot.bytes) { $Problems += "$($Shot.name) is not the size the driver measured" }
			elseif ($Shot.pixel.ok -ne $true) { $Problems += "$($Shot.name) failed the pixel floor ($($Shot.pixel.why))" }
		}
		# A clip counts when one attempt holds its whole set (the driver's offsets: Idle 4, Cast 5, Hit 4, Death 8).
		$Want = @{ idle = 4; cast = 5; hit = 4; death = 8 }
		foreach ($Clip in $Want.Keys) {
			$Names = @($Manifest.shots | Where-Object { $_.name -like "rival/$Clip/*" } | ForEach-Object { $_.name })
			$Groups = @($Names | Group-Object { ($_ -split '/')[-1] -replace '-\d+\.png$', '' } | Where-Object { $_.Count -ge $Want[$Clip] })
			if ($Groups.Count -eq 0) { $Problems += "no full $Clip set ($($Names.Count) frames)" }
			else { Write-Host ("rival: {0} set {1} ({2} frames)" -f $Clip, $Groups[0].Name, $Groups[0].Count) }
		}
		foreach ($Creature in @("creatures/01-hound-8m.png", "creatures/02-maw-8m.png", "creatures/03-hound-and-maw-8m.png")) {
			if (-not (@($Manifest.shots | Where-Object { $_.name -eq $Creature }).Count)) { $Problems += "missing $Creature" }
		}
	}
	$CaptureOk = $Done -and -not $Failed -and $Problems.Count -eq 0
	foreach ($Problem in $Problems) { Write-Host "rival: FAIL $Problem" }
	Write-Host "rival: capture ok=$CaptureOk done=$Done failed=$Failed"

	# Crop sheets: each clip's full set side by side, cut around Brennic where the driver projected him on the set's
	# first frame (the camera never moves). ffmpeg reports on stderr, which Stop would turn into an error record.
	if ($Manifest -and (Get-Command ffmpeg -ErrorAction SilentlyContinue)) {
		$ErrorActionPreference = "Continue"
		foreach ($Clip in @("idle", "cast", "hit", "death")) {
			$Names = @($Manifest.shots | Where-Object { $_.name -like "rival/$Clip/*" } | ForEach-Object { $_.name })
			$Group = @($Names | Group-Object { ($_ -split '/')[-1] -replace '-\d+\.png$', '' } | Sort-Object Count -Descending) | Select-Object -First 1
			if (-not $Group) { continue }
			$Frames = @($Group.Group | Sort-Object)
			$Crop = $Manifest.notes."crop.$($Frames[0])"
			if (-not $Crop) { continue }
			$Box = @($Crop -split ' ' | ForEach-Object { [int]$_ })
			$Inputs = @()
			foreach ($Frame in $Frames) { $Inputs += @("-i", (Join-Path $RunDir $Frame)) }
			$Chains = @()
			for ($i = 0; $i -lt $Frames.Count; $i++) { $Chains += "[${i}:v]crop=$($Box[2]):$($Box[3]):$($Box[0]):$($Box[1]),scale=360:360[c$i]" }
			$Stack = ((0..($Frames.Count - 1)) | ForEach-Object { "[c$_]" }) -join ""
			$Filter = ($Chains -join ";") + ";${Stack}hstack=inputs=$($Frames.Count)[out]"
			$Sheet = Join-Path $RunDir "rival\sheet-$Clip.png"
			& ffmpeg -hide_banner -loglevel error -y @Inputs -filter_complex $Filter -map "[out]" $Sheet
			Write-Host "rival: sheet $Clip exit=$LASTEXITCODE $Sheet ($($Frames.Count) frames, crop $Crop)"
		}
		$ErrorActionPreference = "Stop"
	}

	$BudgetOk = $true
	if ($Budget) {
		Assert-NoNeighbours
		& (Join-Path $ToolsDir "capture-budget.ps1") -Runs $BudgetRuns -Prefix $BudgetPrefix -ExtraArgs @("-MageArenaCreatureImportScene", "-MageArenaCreatureSkip=Brennic")
		if ($LASTEXITCODE -ne 0) { $BudgetOk = $false }
		$BudgetStage = Join-Path $Staging "budget"
		New-Item -ItemType Directory -Force -Path $BudgetStage | Out-Null
		for ($Index = 1; $Index -le $BudgetRuns; $Index++) {
			$Source = Join-Path $Repo "runs\DG4\$BudgetPrefix$Index"
			if (-not (Test-Path $Source)) { continue }
			$Target = Join-Path $BudgetStage "$BudgetPrefix$Index"
			New-Item -ItemType Directory -Force -Path $Target | Out-Null
			Copy-Item -Force -Path (Join-Path $Source "*.json") -Destination $Target -ErrorAction SilentlyContinue
			Copy-Item -Force -Path (Join-Path $Source "capture.log") -Destination $Target -ErrorAction SilentlyContinue
		}
	}

	# Everything this run made goes to staging; nothing of it stays in the worktree.
	if (Test-Path $Scratch) {
		$AssetStage = Join-Path $Staging "uasset"
		if (Test-Path $AssetStage) { Remove-Item -Recurse -Force $AssetStage }
		Copy-Item -Recurse -Force -Path $Scratch -Destination $AssetStage
	}
	if (Test-Path $StageRuns) { Remove-Item -Recurse -Force $StageRuns }
	New-Item -ItemType Directory -Force -Path (Split-Path -Parent $StageRuns) | Out-Null
	Copy-Item -Recurse -Force -Path $RunDir -Destination $StageRuns
	$LogStage = Join-Path $Staging "logs"
	New-Item -ItemType Directory -Force -Path $LogStage | Out-Null
	Move-Item -Force -Path (Join-Path $StageRuns "*.log") -Destination $LogStage
	$Sums = Get-ChildItem -Path $StageRuns -Recurse -File | Where-Object { $_.Name -ne "SHA256SUMS.txt" } | Sort-Object FullName | ForEach-Object {
		"{0}  {1}" -f (Get-Sha $_.FullName), $_.FullName.Substring($StageRuns.Length + 1).Replace("\", "/")
	}
	$Sums | Set-Content -Path (Join-Path $StageRuns "SHA256SUMS.txt") -Encoding ascii
	Write-Host "rival: staged $($Sums.Count) files under $StageRuns (SHA256SUMS.txt)"
	Remove-Item -Recurse -Force $RunDir
	$Ok = $CaptureOk -and $BudgetOk
}
finally {
	Remove-Scratch
}

if ($Ok) {
	Write-Host "rival: ok (staging: $Staging)"
	exit 0
}
Write-Host "rival: failed"
exit 1
