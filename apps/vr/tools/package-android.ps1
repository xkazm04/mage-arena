# Package an arm64 ASTC Android APK and check the hand-tracking manifest markers.
# Success follows the markers, not the UAT exit code.
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\install-xr.ps1"

$VrRoot = Split-Path -Parent $PSScriptRoot
$RepoRoot = Split-Path -Parent $VrRoot
$Project = Join-Path $VrRoot 'Game\MageArenaVR.uproject'
$ArchiveDir = Join-Path $VrRoot 'Game\Saved\Android'
$LogDir = Join-Path $RepoRoot 'runs\T05'
$LogPath = Join-Path $LogDir 'package-android.log'
$Uat = 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat'

if (-not (Test-Path -LiteralPath (Join-Path $VrRoot 'Game\Plugins\MetaXR\OculusXR.uplugin'))) {
    throw 'Meta XR is not installed. Run apps/vr/tools/install-xr.ps1 first.'
}

$toolchain = Set-MageAndroidToolchain
Write-Host ('SDK=' + $toolchain.Sdk)
Write-Host ('NDK=' + $toolchain.Ndk)
Write-Host ('NDK_REVISION=' + $toolchain.NdkRevision)
Write-Host ('JAVA=' + $toolchain.Java)
Write-Host ('JAVA_MAJOR=' + $toolchain.JavaVersion)

New-Item -ItemType Directory -Force -Path $ArchiveDir, $LogDir | Out-Null
if (Test-Path -LiteralPath $LogPath) { Remove-Item -LiteralPath $LogPath -Force }

$engineRoot = 'C:\Program Files\Epic Games\UE_5.8'
$androidReceipt = Join-Path $engineRoot 'Engine\Binaries\Android\UnrealGame.target'
$cxaModule = Join-Path $engineRoot 'Engine\Source\ThirdParty\cxademangle'
if (-not (Test-Path -LiteralPath $androidReceipt) -or -not (Test-Path -LiteralPath $cxaModule)) {
    Write-Host 'OWNER_ACTION: This UE 5.8 install has no Android engine component. Epic Games Launcher -> Unreal Engine -> Library -> 5.8 -> drop-down next to Launch -> Options -> enable Android -> Apply.'
    Write-Host "OWNER_ACTION: Confirm $androidReceipt and $cxaModule exist, then re-run this script. Those files live in the engine install, which this task does not modify."
    Write-Host 'OWNER_ACTION: Until that component is installed, UBT fails in Core.Build.cs looking for cxademangle. Stock OpenXR packaging hits the same missing engine module.'
}

$uatArgs = @(
    'BuildCookRun'
    "-project=$Project"
    '-noP4'
    '-platform=Android'
    '-clientconfig=Development'
    '-cook'
    '-build'
    '-stage'
    '-pak'
    '-archive'
    "-archivedirectory=$ArchiveDir"
    '-cookflavor=ASTC'
    '-prereqs'
    '-unattended'
    '-utf8output'
)

Write-Host ('UAT=' + ($uatArgs -join ' '))
$cmdPath = Join-Path $LogDir 'package-android.cmd'
@(
    '@echo off'
    ('set "ANDROID_HOME={0}"' -f $toolchain.Sdk)
    ('set "NDKROOT={0}"' -f $toolchain.Ndk)
    ('set "JAVA_HOME={0}"' -f $toolchain.Java)
    ('call "{0}" {1}' -f $Uat, ($uatArgs -join ' '))
    'exit /b %ERRORLEVEL%'
) | Set-Content -LiteralPath $cmdPath -Encoding ASCII
$started = Get-Date
$ErrorActionPreference = 'Continue'
& cmd.exe /c "`"$cmdPath`" > `"$LogPath`" 2>&1"
$uatExit = $LASTEXITCODE
$ErrorActionPreference = 'Stop'
if (Test-Path -LiteralPath $LogPath) {
    Write-Host '--- UAT log tail ---'
    Get-Content -LiteralPath $LogPath -Tail 20 | ForEach-Object { Write-Host $_ }
}
$elapsed = [math]::Round(((Get-Date) - $started).TotalSeconds, 1)
Write-Host "UAT_EXIT=$uatExit"
Write-Host "PACKAGE_SECONDS=$elapsed"

function Find-Tool([string]$SdkRoot, [string]$ExeName) {
    $buildTools = Join-Path $SdkRoot 'build-tools'
    if (-not (Test-Path -LiteralPath $buildTools)) { return $null }
    $dirs = Get-ChildItem -LiteralPath $buildTools -Directory | Sort-Object Name -Descending
    foreach ($dir in $dirs) {
        $candidate = Join-Path $dir.FullName $ExeName
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    return $null
}

$apkRoots = @(
    $ArchiveDir
    (Join-Path $VrRoot 'Game\Saved\StagedBuilds')
    (Join-Path $VrRoot 'Game\Binaries\Android')
)
$apks = @()
foreach ($root in $apkRoots) {
    if (Test-Path -LiteralPath $root) {
        $apks += @(Get-ChildItem -LiteralPath $root -Recurse -Filter '*.apk' -ErrorAction SilentlyContinue)
    }
}
# Only APKs written by this run count; an older APK must never pass the marker check.
$apks = @($apks | Where-Object { $_.LastWriteTime -ge $started })
$arm = @($apks | Where-Object { $_.Name -match 'arm64' } | Sort-Object LastWriteTime -Descending)
if ($arm.Count -eq 0) { $arm = @($apks | Sort-Object LastWriteTime -Descending) }
if ($arm.Count -eq 0) {
    Write-Host 'APK_MISSING=1'
    Write-Host 'MARKER_RESULT=FAIL'
    exit 1
}
$apk = $arm[0]
Write-Host ('APK=' + $apk.FullName)
Write-Host ('APK_BYTES=' + $apk.Length)

$aapt = Find-Tool $toolchain.Sdk 'aapt.exe'
$aapt2 = Find-Tool $toolchain.Sdk 'aapt2.exe'
if (-not $aapt -and -not $aapt2) { throw 'Neither aapt.exe nor aapt2.exe is in the Android build-tools.' }

$markerText = ''
$ErrorActionPreference = 'Continue'
if ($aapt) {
    Write-Host "AAPT=$aapt"
    $badging = & $aapt dump badging $apk.FullName 2>&1 | Out-String
    $permissions = & $aapt dump permissions $apk.FullName 2>&1 | Out-String
    $xml = & $aapt dump xmltree $apk.FullName AndroidManifest.xml 2>&1 | Out-String
    $markerText = $badging + "`n" + $permissions + "`n" + $xml
}
if ($aapt2) {
    Write-Host "AAPT2=$aapt2"
    $badging2 = & $aapt2 dump badging $apk.FullName 2>&1 | Out-String
    $permissions2 = & $aapt2 dump permissions $apk.FullName 2>&1 | Out-String
    $markerText += "`n" + $badging2 + "`n" + $permissions2
}
$ErrorActionPreference = 'Stop'

$hasArm = $markerText -match 'arm64-v8a'
$hasPermission = $markerText -match 'com\.oculus\.permission\.HAND_TRACKING'
$hasFeature = $markerText -match 'oculus\.software\.handtracking'
# Meta refuses an upload below 34 and the immersive cap is 34, so the badging must say exactly 34.
$hasTargetSdk = $markerText -match "targetSdkVersion:'34'"
Write-Host ("MARKER_ARM64=" + $(if ($hasArm) { 'yes' } else { 'no' }))
Write-Host ("MARKER_HAND_PERMISSION=" + $(if ($hasPermission) { 'yes' } else { 'no' }))
Write-Host ("MARKER_HAND_FEATURE=" + $(if ($hasFeature) { 'yes' } else { 'no' }))
Write-Host ("MARKER_TARGET_SDK_34=" + $(if ($hasTargetSdk) { 'yes' } else { 'no' }))

$markerLog = Join-Path $LogDir 'apk-markers.txt'
@(
    "APK=$($apk.FullName)"
    "APK_BYTES=$($apk.Length)"
    "UAT_EXIT=$uatExit"
    "PACKAGE_SECONDS=$elapsed"
    "MARKER_ARM64=$(if ($hasArm) { 'yes' } else { 'no' })"
    "MARKER_HAND_PERMISSION=$(if ($hasPermission) { 'yes' } else { 'no' })"
    "MARKER_HAND_FEATURE=$(if ($hasFeature) { 'yes' } else { 'no' })"
    "MARKER_TARGET_SDK_34=$(if ($hasTargetSdk) { 'yes' } else { 'no' })"
    ''
    $markerText
) | Set-Content -LiteralPath $markerLog -Encoding UTF8

if ($hasArm -and $hasPermission -and $hasFeature -and $hasTargetSdk) {
    Write-Host 'MARKER_RESULT=PASS'
    exit 0
}
Write-Host 'MARKER_RESULT=FAIL'
exit 1
