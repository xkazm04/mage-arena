# The creature cast in UE (docs/research/CREATURE-UE-IMPORT-2026-10-24.md, CREATURE-CAST-UE-2026-10-24.md): imports
# Brennic (LOD0, bake finish), the cinder hound s7101 and the mire maw (split-and-merge, seam fixed) into the scratch
# folder apps/vr/Game/Content/CreatureImportScratch through the ImportCreatureMesh commandlet
# (Tools/ImportCreatureMeshCommandlet.cpp), takes the stills (-MageArenaCreatureImportCapture, the greybox arena under the
# T56 dusk) into runs/<Run>/shots, optionally re-runs the D-G4 budget with all three in the scene (capture-budget.ps1
# -ExtraArgs -MageArenaCreatureImportScene), copies the imported .uasset files, the shots and the logs to -Staging, and
# deletes the scratch folder before it exits, success or not. The DINOv3 licence hold stands: none of the imported
# assets is ever committed.
# Success is the seventeen pngs, MAGEVR_CAPTURE_DONE and, with -Budget, capture-budget.ps1's own ok.
# Usage: capture-creature-import.ps1 [-Run creature-cast] [-Budget] [-BudgetRuns 2] [-BaselineRuns 0]
#        [-EmberIntensity <n>] [-GlowIntensity <n>] [-GlowRim <n>] [-GlowSoft <n>] [-Staging <dir>]
# -EmberIntensity overrides the chosen glow's crack intensity; the -Glow* values override the intensity, rim and soft
# candidates (CreatureImportCapture.cpp), so a candidate is tuned without a rebuild. A negative value keeps the default.
# One Unreal process at a time: every launch waits for the lane, and the script refuses to start beside Blender or
# ComfyUI (port 8188).
param(
	[ValidatePattern('^[A-Za-z0-9_-]+$')][string]$Run = "creature-cast",
	[switch]$Budget,
	[int]$BudgetRuns = 2,
	[int]$BaselineRuns = 0,
	[double]$EmberIntensity = -1,
	[double]$GlowIntensity = -1,
	[double]$GlowRim = -1,
	[double]$GlowSoft = -1,
	[string]$Staging = "C:\Users\kazda\mage-art-staging\creature-cast-ue-2026-10-24",
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
$ShotDir = Join-Path $RunDir "shots"

# The inputs and the sha256 their research docs record (BRENNIC-MESH-REWORK-2026-10-24, CREATURE-MESH-2026-10-24,
# CREATURE-CAST-UE-2026-10-24 for the maw: the s7 fit re-rig of the 17,490-tri merge and its seam-fixed texture).
$Sources = @(
	@{ Name = "Brennic"; Fbx = (Join-Path $BrennicDir "Brennic.fbx"); FbxSha = "dc0a4f38eef1b8486f72e1ec81859bcba1605be3df412c4c6c72368b203967b0";
		Texture = (Join-Path $BrennicDir "T_Brennic_BaseColor.png"); TextureSha = "024d4aaa888d94b2f257be993a96e8ed7a80176cedffaa5b0735620b031facae" },
	@{ Name = "CinderHound"; Fbx = (Join-Path $HoundDir "CinderHound.fbx"); FbxSha = "a650bc3e0ad23abc34291bd3da0dba35698598c430484c095aafc711d07e5449";
		Texture = (Join-Path $HoundDir "T_CinderHound_BaseColor.png"); TextureSha = "b36529c7f1c79e093cde253ba59087f5120e391eec9169b480a33fbb73ece08d" },
	@{ Name = "MireMaw"; Fbx = (Join-Path $MawDir "MireMaw.fbx"); FbxSha = "6498f40c8ca4efd7da32abc903f2f537bd1e155e8b4756b9fbfb255ea8bbdcfd";
		Texture = (Join-Path $MawDir "T_MireMaw_BaseColor.png"); TextureSha = "d46f288fb31ab978e3ba033d044d6ee7184fec6a88e49cf42459a9dd27ed6e1c" }
)

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

function Assert-NoNeighbours {
	$Blender = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -like "blender*" })
	if ($Blender.Count -gt 0) { throw "Blender is running (pid $($Blender[0].Id)); this lane never runs beside it" }
	$Comfy = @(Get-NetTCPConnection -LocalPort 8188 -State Listen -ErrorAction SilentlyContinue)
	if ($Comfy.Count -gt 0) { throw "something listens on port 8188 (ComfyUI); this lane never runs beside it" }
	if (-not (Wait-UnrealLane)) { throw "the Unreal lane stayed busy for 30 minutes" }
}

function Remove-Scratch {
	if (Test-Path $Scratch) {
		Remove-Item -Recurse -Force $Scratch
		Write-Host "creature-import: removed the scratch folder $Scratch"
	}
}

New-Item -ItemType Directory -Force -Path $ShotDir | Out-Null
Get-ChildItem -Path $ShotDir -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force
New-Item -ItemType Directory -Force -Path $Staging | Out-Null

foreach ($Source in $Sources) {
	foreach ($Pair in @(@($Source.Fbx, $Source.FbxSha), @($Source.Texture, $Source.TextureSha))) {
		if (-not (Test-Path $Pair[0])) { throw "missing input $($Pair[0]) (stop and report; never re-run Blender here)" }
		$Sha = (Get-FileHash -Algorithm SHA256 -Path $Pair[0]).Hash.ToLower()
		if ($Sha -ne $Pair[1]) { throw "sha256 mismatch on $($Pair[0]): $Sha, the research doc records $($Pair[1])" }
		Write-Host "creature-import: input ok $($Pair[0]) $Sha"
	}
}

$Ok = $false
try {
	Remove-Scratch
	foreach ($Source in $Sources) {
		Assert-NoNeighbours
		$Log = Join-Path $RunDir "import-$($Source.Name).log"
		if (Test-Path $Log) { Remove-Item -Force $Log }
		Write-Host "creature-import: importing $($Source.Name)"
		& $Editor $Project -run=ImportCreatureMesh "-name=$($Source.Name)" "-fbx=$($Source.Fbx)" "-texture=$($Source.Texture)" -unattended -nosplash -nop4 -stdout -FullStdOutLogOutput "-abslog=$Log" | Out-Null
		$Code = $LASTEXITCODE
		Select-String -Path $Log -Pattern "MAGEVR_CREATURE_IMPORT" | ForEach-Object { Write-Host $_.Line }
		if ($Code -ne 0 -or -not (Select-String -Path $Log -Pattern "MAGEVR_CREATURE_IMPORT done" -Quiet)) {
			throw "import of $($Source.Name) failed exit=$Code (log: $Log)"
		}
	}

	Assert-NoNeighbours
	$LogPath = Join-Path $RunDir "capture.log"
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
		"-MageArenaCreatureImportCapture",
		"-MageArenaRun=$Run"
	)
	if ($EmberIntensity -ge 0) { $ArgumentList += "-MageArenaEmberIntensity=$EmberIntensity" }
	if ($GlowIntensity -ge 0) { $ArgumentList += "-MageArenaGlowIntensity=$GlowIntensity" }
	if ($GlowRim -ge 0) { $ArgumentList += "-MageArenaGlowRim=$GlowRim" }
	if ($GlowSoft -ge 0) { $ArgumentList += "-MageArenaGlowSoft=$GlowSoft" }
	Write-Host "creature-import: starting the capture run=$Run"
	$Process = Start-Process -FilePath $Editor -ArgumentList $ArgumentList -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $RunDir "capture.stdout.log") -RedirectStandardError (Join-Path $RunDir "capture.stderr.log")
	if (-not $Process.WaitForExit(780000)) {
		Write-Host "creature-import: 13 minute timeout, stopping the editor it started"
		& taskkill.exe /T /F /PID $Process.Id | Out-Host
		Start-Sleep -Seconds 2
	}
	$ShotNames = @(
		"00-plate.png",
		"01-maw-front.png",
		"02-maw-three-quarter.png",
		"03-maw-back.png",
		"04-hound-mask-front.png",
		"05-hound-glow-base-front.png",
		"06-hound-glow-intensity-front.png",
		"07-hound-glow-rim-front.png",
		"08-hound-glow-soft-front.png",
		"09-hound-glow-chosen-front.png",
		"10-hound-mask-three-quarter.png",
		"11-hound-glow-base-three-quarter.png",
		"12-hound-glow-intensity-three-quarter.png",
		"13-hound-glow-rim-three-quarter.png",
		"14-hound-glow-soft-three-quarter.png",
		"15-hound-glow-chosen-three-quarter.png",
		"16-cast-duel.png"
	)
	$Missing = @($ShotNames | Where-Object { -not (Test-Path (Join-Path $ShotDir $_)) -or (Get-Item (Join-Path $ShotDir $_)).Length -le 0 })
	$LogText = $(if (Test-Path $LogPath) { Get-Content -Raw -Path $LogPath } else { "" })
	$Done = $LogText -match "MAGEVR_CAPTURE_DONE"
	Select-String -Path $LogPath -Pattern "MAGEVR_(CREATURE_IMPORT|SHOT|CAPTURE)" | ForEach-Object { Write-Host $_.Line }
	$CaptureOk = ($Missing.Count -eq 0) -and $Done
	Write-Host "creature-import: capture ok=$CaptureOk missing=$($Missing -join ',') done=$Done"

	$BudgetOk = $true
	if ($Budget) {
		$BudgetScript = Join-Path $ToolsDir "capture-budget.ps1"
		if ($BaselineRuns -gt 0) {
			Assert-NoNeighbours
			& $BudgetScript -Runs $BaselineRuns -Prefix "$Run-without"
			if ($LASTEXITCODE -ne 0) { $BudgetOk = $false }
		}
		Assert-NoNeighbours
		& $BudgetScript -Runs $BudgetRuns -Prefix "$Run-with" -ExtraArgs @("-MageArenaCreatureImportScene")
		if ($LASTEXITCODE -ne 0) { $BudgetOk = $false }
		$BudgetStage = Join-Path $Staging "budget"
		New-Item -ItemType Directory -Force -Path $BudgetStage | Out-Null
		Get-ChildItem -Path (Join-Path $Repo "runs\DG4") -Directory | Where-Object { $_.Name -like "$Run-*" } | ForEach-Object {
			$Target = Join-Path $BudgetStage $_.Name
			New-Item -ItemType Directory -Force -Path $Target | Out-Null
			Copy-Item -Force -Path (Join-Path $_.FullName "*.json") -Destination $Target -ErrorAction SilentlyContinue
			Copy-Item -Force -Path (Join-Path $_.FullName "capture.log") -Destination $Target -ErrorAction SilentlyContinue
		}
	}

	# The imported assets, the shots and the logs go to staging; nothing of them stays in the worktree.
	if (Test-Path $Scratch) {
		$AssetStage = Join-Path $Staging "uasset"
		if (Test-Path $AssetStage) { Remove-Item -Recurse -Force $AssetStage }
		Copy-Item -Recurse -Force -Path $Scratch -Destination $AssetStage
	}
	$ShotStage = Join-Path $Staging "shots"
	New-Item -ItemType Directory -Force -Path $ShotStage | Out-Null
	Copy-Item -Force -Path (Join-Path $ShotDir "*") -Destination $ShotStage
	$LogStage = Join-Path $Staging "logs"
	New-Item -ItemType Directory -Force -Path $LogStage | Out-Null
	Copy-Item -Force -Path (Join-Path $RunDir "*.log") -Destination $LogStage
	$Ok = $CaptureOk -and $BudgetOk
}
finally {
	Remove-Scratch
}

if ($Ok) {
	Write-Host "creature-import: ok (staging: $Staging)"
	exit 0
}
Write-Host "creature-import: failed"
exit 1
