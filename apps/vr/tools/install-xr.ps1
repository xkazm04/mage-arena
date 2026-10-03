# Repeatable Meta XR v207 setup for this project. No credentials. Safe to re-run.
# Dot-source to load Set-MageAndroidToolchain without downloading.
$ErrorActionPreference = 'Stop'

$script:VrRoot = Split-Path -Parent $PSScriptRoot
$script:PluginsDir = Join-Path $script:VrRoot 'Game\Plugins'
$script:CacheDir = Join-Path $script:VrRoot '.cache\xr'
$script:Uproject = Join-Path $script:VrRoot 'Game\MageArenaVR.uproject'
$script:EngineRoot = 'C:\Program Files\Epic Games\UE_5.8'

function ConvertTo-SlashPath([string]$Path) {
    return ($Path -replace '\\', '/')
}

function Get-JdkMajor([string]$JavaHome) {
    $release = Join-Path $JavaHome 'release'
    if (-not (Test-Path -LiteralPath $release)) { return 0 }
    $line = Get-Content -LiteralPath $release | Where-Object { $_ -like 'JAVA_VERSION=*' } | Select-Object -First 1
    if ($line -match 'JAVA_VERSION="?(\d+)') { return [int]$Matches[1] }
    return 0
}

function Test-JdkHome([string]$JavaHome) {
    if ([string]::IsNullOrWhiteSpace($JavaHome)) { return $false }
    if (-not (Test-Path -LiteralPath (Join-Path $JavaHome 'bin\java.exe'))) { return $false }
    return (Get-JdkMajor $JavaHome) -ge 17
}

function Get-NdkRevision([string]$NdkDir) {
    $props = Join-Path $NdkDir 'source.properties'
    if (-not (Test-Path -LiteralPath $props)) { return $null }
    $line = Get-Content -LiteralPath $props | Where-Object { $_ -like 'Pkg.Revision*' } | Select-Object -First 1
    if ($line -match '=\s*(\S+)') { return $Matches[1].Trim() }
    return $null
}

function Test-NdkInEngineRange([string]$Revision) {
    # Android_SDK.json: MinVersion r27c, MaxVersion r29. rNNc is revision N.2.
    $parts = $Revision.Split('.')
    if ($parts.Count -lt 1) { return $false }
    $major = 0
    $minor = 0
    [void][int]::TryParse($parts[0], [ref]$major)
    if ($parts.Count -ge 2) { [void][int]::TryParse($parts[1], [ref]$minor) }
    if ($major -lt 27 -or $major -gt 29) { return $false }
    if ($major -eq 27 -and $minor -lt 2) { return $false }
    if ($major -eq 29 -and $minor -gt 0) { return $false }
    return $true
}

# SHA-256 of the Meta v207 archives as downloaded and verified on 2026-10-03 (route R-A, docs/XR-TOOLCHAIN.md).
$script:PinnedSha256 = @{
    'UnrealMetaXRPlugin.207.0.zip'         = 'f946093b66cc6682887ad2a5b0b12e2880b48d65a068f7dc84dc22a67aed43a8'
    'MetaXRInteractionPackage.207.0.0.zip' = '8a1dc65ca8ded1dcdc0efead8e6545f7ae19e201564332620a72258e325d6c9f'
}

function Find-AndroidSdkRoot {
    $candidates = @()
    if ($env:ANDROID_HOME) { $candidates += $env:ANDROID_HOME }
    $userHome = [Environment]::GetEnvironmentVariable('ANDROID_HOME', 'User')
    if ($userHome) { $candidates += $userHome }
    $candidates += (Join-Path $env:USERPROFILE 'scoop\apps\android-clt\current')
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath (Join-Path $candidate 'platforms'))) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    throw 'Android SDK not found. Install the command-line tools and set ANDROID_HOME.'
}

function Find-NdkRoot([string]$SdkRoot) {
    if ($env:NDKROOT -and (Test-Path -LiteralPath (Join-Path $env:NDKROOT 'toolchains\llvm'))) {
        $revision = Get-NdkRevision $env:NDKROOT
        if ($revision -and (Test-NdkInEngineRange $revision)) {
            return (Resolve-Path -LiteralPath $env:NDKROOT).Path
        }
    }
    $ndkParent = Join-Path $SdkRoot 'ndk'
    if (-not (Test-Path -LiteralPath $ndkParent)) {
        throw "No NDK under $ndkParent. UE 5.8 wants NDK r27c through r29 (pinned 27.2.12479018)."
    }
    $preferred = Join-Path $ndkParent '27.2.12479018'
    if (Test-Path -LiteralPath (Join-Path $preferred 'toolchains\llvm')) {
        return (Resolve-Path -LiteralPath $preferred).Path
    }
    $found = @()
    foreach ($dir in (Get-ChildItem -LiteralPath $ndkParent -Directory)) {
        if (-not (Test-Path -LiteralPath (Join-Path $dir.FullName 'toolchains\llvm'))) { continue }
        $revision = Get-NdkRevision $dir.FullName
        if ($revision -and (Test-NdkInEngineRange $revision)) {
            $found += $dir.FullName
        }
    }
    if ($found.Count -eq 0) {
        throw "No NDK in the r27c-r29 range under $ndkParent."
    }
    return $found[-1]
}

function Find-JdkHome {
    if (Test-JdkHome $env:JAVA_HOME) { return (Resolve-Path -LiteralPath $env:JAVA_HOME).Path }
    $userJava = [Environment]::GetEnvironmentVariable('JAVA_HOME', 'User')
    if (Test-JdkHome $userJava) { return (Resolve-Path -LiteralPath $userJava).Path }
    $candidates = @('C:\Program Files\Zulu\zulu-22')
    foreach ($parent in @('C:\Program Files\Zulu', 'C:\Program Files\Java', 'C:\Program Files\Eclipse Adoptium', 'C:\Program Files\Microsoft')) {
        if (Test-Path -LiteralPath $parent) {
            $candidates += (Get-ChildItem -LiteralPath $parent -Directory -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName })
        }
    }
    foreach ($candidate in $candidates) {
        if (Test-JdkHome $candidate) { return (Resolve-Path -LiteralPath $candidate).Path }
    }
    throw 'No JDK 17 or newer with a release file was found. Install one and set JAVA_HOME.'
}

function Set-UserEnvIfUnsetOrBroken([string]$Name, [string]$Value) {
    $current = [Environment]::GetEnvironmentVariable($Name, 'User')
    $usable = $false
    if (-not [string]::IsNullOrWhiteSpace($current)) {
        if ($Name -eq 'JAVA_HOME') { $usable = Test-JdkHome $current }
        elseif ($Name -eq 'NDKROOT') { $usable = Test-Path -LiteralPath (Join-Path $current 'toolchains\llvm') }
        else { $usable = Test-Path -LiteralPath $current }
    }
    if (-not $usable) {
        [Environment]::SetEnvironmentVariable($Name, $Value, 'User')
        Write-Host "USER_ENV_SET $Name=$Value"
    } else {
        Write-Host "USER_ENV_KEPT $Name=$current"
    }
    Set-Item -Path "Env:$Name" -Value $Value
}

function Write-AndroidSdkIni([string]$Sdk, [string]$Ndk, [string]$Java) {
    $iniDir = Join-Path $env:LOCALAPPDATA 'Unreal Engine\Engine\Config'
    $iniPath = Join-Path $iniDir 'UserEngine.ini'
    New-Item -ItemType Directory -Force -Path $iniDir | Out-Null
    $section = '[/Script/AndroidPlatformEditor.AndroidSDKSettings]'
    $block = @(
        $section
        ('SDKPath=(Path="{0}")' -f (ConvertTo-SlashPath $Sdk))
        ('NDKPath=(Path="{0}")' -f (ConvertTo-SlashPath $Ndk))
        ('JavaPath=(Path="{0}")' -f (ConvertTo-SlashPath $Java))
    ) -join "`r`n"
    if (-not (Test-Path -LiteralPath $iniPath)) {
        [System.IO.File]::WriteAllText($iniPath, $block + "`r`n")
    } else {
        $text = [System.IO.File]::ReadAllText($iniPath)
        $pattern = '(?s)\[/Script/AndroidPlatformEditor\.AndroidSDKSettings\].*?(?=\r?\n\[|\z)'
        if ([regex]::IsMatch($text, $pattern)) {
            $text = [regex]::Replace($text, $pattern, ($block + "`r`n"))
        } else {
            if (-not $text.EndsWith("`n")) { $text += "`r`n" }
            $text = $text + "`r`n" + $block + "`r`n"
        }
        [System.IO.File]::WriteAllText($iniPath, $text)
    }
    Write-Host "USER_ENGINE_INI=$iniPath"
    # The installed engine only registers Android when Engine\Binaries\Android\UnrealGame.target
    # exists. A user-config entry with no RequiredFile lets Turnkey and UBT see the platform
    # without editing the engine. It does not install the precompiled Android engine binaries.
    $platformSection = '[InstalledPlatforms]'
    $platformLine = '+InstalledPlatformConfigurations=(PlatformName="Android", Configuration="Development", PlatformType="Game", Architecture="arm64", ProjectType="Any", bCanBeDisplayed=True)'
    $text = [System.IO.File]::ReadAllText($iniPath)
    if ($text -notmatch [regex]::Escape($platformLine)) {
        if (-not $text.EndsWith("`n")) { $text += "`r`n" }
        $text += "`r`n$platformSection`r`n$platformLine`r`n"
        [System.IO.File]::WriteAllText($iniPath, $text)
        Write-Host 'USER_ENGINE_INI_ANDROID_PLATFORM=added'
    }
}

function Set-MageAndroidToolchain {
    $sdk = Find-AndroidSdkRoot
    $ndk = Find-NdkRoot $sdk
    $java = Find-JdkHome
    Set-UserEnvIfUnsetOrBroken -Name 'ANDROID_HOME' -Value $sdk
    Set-UserEnvIfUnsetOrBroken -Name 'NDKROOT' -Value $ndk
    Set-UserEnvIfUnsetOrBroken -Name 'JAVA_HOME' -Value $java
    # UBT clears ANDROID_SDK_HOME because it confuses the Gradle plugin.
    Remove-Item Env:ANDROID_SDK_HOME -ErrorAction SilentlyContinue
    Write-AndroidSdkIni -Sdk $sdk -Ndk $ndk -Java $java
    $platforms = @()
    $platformDir = Join-Path $sdk 'platforms'
    if (Test-Path -LiteralPath $platformDir) {
        $platforms = @(Get-ChildItem -LiteralPath $platformDir -Directory | ForEach-Object { $_.Name })
    }
    $buildTools = @()
    $btDir = Join-Path $sdk 'build-tools'
    if (Test-Path -LiteralPath $btDir) {
        $buildTools = @(Get-ChildItem -LiteralPath $btDir -Directory | ForEach-Object { $_.Name })
    }
    $cmake = Join-Path $sdk 'cmake\3.22.1'
    return [pscustomobject]@{
        Sdk = $sdk
        Ndk = $ndk
        NdkRevision = (Get-NdkRevision $ndk)
        Java = $java
        JavaVersion = (Get-JdkMajor $java)
        Platforms = ($platforms -join ',')
        BuildTools = ($buildTools -join ',')
        Cmake322 = (Test-Path -LiteralPath $cmake)
    }
}

function Get-LastCurlStatus([string]$HeaderText) {
    $blocks = @($HeaderText -split '(?=HTTP/)' | Where-Object { $_ -match 'HTTP/' })
    if ($blocks.Count -eq 0) { return $HeaderText }
    return $blocks[-1]
}

function Get-MetaPackage([string]$PageUrl, [string]$FilePattern) {
    New-Item -ItemType Directory -Force -Path $script:CacheDir | Out-Null
    $safeName = ($FilePattern -replace '[^A-Za-z0-9]+', '_')
    $htmlPath = Join-Path $script:CacheDir ($safeName + '.html')
    & curl.exe -fsSL -L --retry 3 --retry-delay 2 -o $htmlPath $PageUrl
    if ($LASTEXITCODE -ne 0) { throw "Could not read the official download page $PageUrl (curl $LASTEXITCODE)." }
    $html = [System.IO.File]::ReadAllText($htmlPath)
    $regex = [regex]('"is_downloadable":true,"file_name":"(?<file>[^"]+)","license_info":\{"path":"(?<licpath>[^"]+)","id":"(?<licid>[^"]+)"\},"uri":"(?<uri>[^"]+)"')
    $match = $null
    foreach ($candidate in $regex.Matches($html)) {
        if ($candidate.Groups['file'].Value -match $FilePattern) {
            $match = $candidate
            break
        }
    }
    if ($null -eq $match) {
        throw "The official page did not publish a downloadable $FilePattern package. Owner: open $PageUrl and download the UE 5.8 v207 zip by hand if Meta now requires a login."
    }
    $fileName = $match.Groups['file'].Value
    $uri = ($match.Groups['uri'].Value -replace '\\/', '/')
    $licensePath = $match.Groups['licpath'].Value
    $licenseId = $match.Groups['licid'].Value
    Write-Host "META_PAGE=$PageUrl"
    Write-Host "META_FILE=$fileName"
    Write-Host "META_LICENSE=$licensePath id=$licenseId"
    $headerText = (& curl.exe -sI -L --max-redirs 8 $uri | Out-String)
    $status = Get-LastCurlStatus $headerText
    if ($headerText -match '(?i)accounts\.meta|facebook\.com/login|oculus\.com/login|Log in to download') {
        throw "OWNER_ACTION: $PageUrl now requires a Meta login before the v207 download. Log in with the owner's Meta developer account and download $fileName. This script does not log in."
    }
    if ($status -notmatch 'HTTP/\S+\s+200') {
        throw "OWNER_ACTION: the official CDN did not return the zip (last status block follows). If this is a licence or login wall, open $PageUrl and accept the Oculus SDK licence ($licensePath id $licenseId), then re-run.`n$status"
    }
    if ($status -match '(?i)Content-Type:\s*text/html') {
        throw "OWNER_ACTION: the CDN returned HTML instead of $fileName. Open $PageUrl, accept the Oculus SDK licence if Meta asks ($licensePath id $licenseId), and re-run. This script will not click through a login."
    }
    $length = 0
    $lengthMatch = [regex]::Match($status, '(?i)Content-Length:\s*(\d+)')
    if ($lengthMatch.Success) { $length = [int64]$lengthMatch.Groups[1].Value }
    $zipPath = Join-Path $script:CacheDir $fileName
    $have = 0L
    if (Test-Path -LiteralPath $zipPath) { $have = (Get-Item -LiteralPath $zipPath).Length }
    if ($length -gt 0 -and $have -eq $length) {
        Write-Host "CACHE_HIT=$zipPath bytes=$have"
    } else {
        Write-Host "DOWNLOAD=$uri -> $zipPath"
        & curl.exe -fL --retry 3 --retry-delay 2 -o $zipPath $uri
        if ($LASTEXITCODE -ne 0) { throw "Download failed for $fileName (curl $LASTEXITCODE)." }
        $have = (Get-Item -LiteralPath $zipPath).Length
        if ($length -gt 0 -and $have -ne $length) {
            throw "Downloaded $fileName is $have bytes; the CDN advertised $length."
        }
        Write-Host "DOWNLOADED=$zipPath bytes=$have"
    }
    # Integrity: the archive must match the SHA-256 pinned when the route was chosen (2026-10-03). A size match alone
    # proves nothing about content. A new Meta release is a deliberate re-pin, never an automatic acceptance.
    $expected = $script:PinnedSha256[$fileName]
    if (-not $expected) { throw "No pinned SHA-256 for $fileName. Verify the new release and add it to PinnedSha256 deliberately." }
    $actual = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $expected) { throw "SHA-256 mismatch for $fileName : got $actual, pinned $expected. Delete the file and investigate before re-running." }
    Write-Host "SHA256_OK=$fileName"
    return [pscustomobject]@{ Zip = $zipPath; FileName = $fileName; LicensePath = $licensePath; LicenseId = $licenseId }
}

function Install-PluginZip([string]$ZipPath, [string]$FolderName, [string]$UpluginName) {
    $dest = Join-Path $script:PluginsDir $FolderName
    $uplugin = Join-Path $dest $UpluginName
    $modules = Join-Path $dest 'Binaries\Win64\UnrealEditor.modules'
    if ((Test-Path -LiteralPath $uplugin) -and (Test-Path -LiteralPath $modules)) {
        $text = [System.IO.File]::ReadAllText($uplugin)
        if ($text -match '"VersionName"\s*:\s*"1\.207\.0"' -and $text -match '"EngineVersion"\s*:\s*"5\.8\.0"') {
            Write-Host "PLUGIN_PRESENT=$dest"
            return
        }
    }
    if (Test-Path -LiteralPath $dest) {
        Remove-Item -LiteralPath $dest -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $script:PluginsDir | Out-Null
    $tar = Join-Path $env:SystemRoot 'System32\tar.exe'
    & $tar -xf $ZipPath -C $script:PluginsDir
    if ($LASTEXITCODE -ne 0) { throw "tar failed extracting $ZipPath ($LASTEXITCODE)." }
    if (-not (Test-Path -LiteralPath $uplugin)) { throw "Extract did not produce $uplugin." }
    Write-Host "PLUGIN_EXTRACTED=$dest"
}

function Enable-UprojectPlugin([string]$Name) {
    $text = [System.IO.File]::ReadAllText($script:Uproject)
    if ($text -match ('"Name"\s*:\s*"' + [regex]::Escape($Name) + '"')) {
        Write-Host "UPROJECT_ALREADY=$Name"
        return
    }
    $pluginsMatch = [regex]::Match($text, '"Plugins"\s*:\s*\[')
    if (-not $pluginsMatch.Success) { throw "Could not find the Plugins array in $script:Uproject." }
    $insertAt = $pluginsMatch.Index + $pluginsMatch.Length
    $insertion = "`r`n        { `"Name`": `"$Name`", `"Enabled`": true },"
    $updated = $text.Insert($insertAt, $insertion)
    [System.IO.File]::WriteAllText($script:Uproject, $updated)
    Write-Host "UPROJECT_ENABLED=$Name"
}

function Invoke-MageXrInstall {
    $integration = Get-MetaPackage -PageUrl 'https://developers.meta.com/horizon/downloads/package/unreal-engine-5-integration/' -FilePattern 'UnrealMetaXRPlugin\.207'
    $interaction = Get-MetaPackage -PageUrl 'https://developers.meta.com/horizon/downloads/package/meta-xr-interaction-sdk-unreal/' -FilePattern 'MetaXRInteractionPackage\.207'
    Install-PluginZip -ZipPath $integration.Zip -FolderName 'MetaXR' -UpluginName 'OculusXR.uplugin'
    Install-PluginZip -ZipPath $interaction.Zip -FolderName 'MetaXRInteraction' -UpluginName 'OculusInteraction.uplugin'
    Enable-UprojectPlugin -Name 'OculusXR'
    Enable-UprojectPlugin -Name 'OculusInteraction'
    $modulesPath = Join-Path $script:PluginsDir 'MetaXR\Binaries\Win64\UnrealEditor.modules'
    $buildId = ''
    if (Test-Path -LiteralPath $modulesPath) {
        $modules = [System.IO.File]::ReadAllText($modulesPath)
        $idMatch = [regex]::Match($modules, '"BuildId"\s*:\s*"(\d+)"')
        if ($idMatch.Success) { $buildId = $idMatch.Groups[1].Value }
    }
    $engineVersion = Join-Path $script:EngineRoot 'Engine\Build\Build.version'
    $compatible = ''
    if (Test-Path -LiteralPath $engineVersion) {
        $versionText = [System.IO.File]::ReadAllText($engineVersion)
        $compatibleMatch = [regex]::Match($versionText, '"CompatibleChangelist"\s*:\s*(\d+)')
        if ($compatibleMatch.Success) { $compatible = $compatibleMatch.Groups[1].Value }
    }
    Write-Host "PLUGIN_BUILD_ID=$buildId"
    Write-Host "ENGINE_COMPATIBLE_CHANGELIST=$compatible"
    if ($buildId -and $compatible -and $buildId -ne $compatible) {
        Write-Host "PLUGIN_BUILD_ID_MISMATCH expected=$compatible actual=$buildId"
    }
}

if ($MyInvocation.InvocationName -ne '.') {
    Invoke-MageXrInstall
    $toolchain = Set-MageAndroidToolchain
    Write-Host ('SDK=' + $toolchain.Sdk)
    Write-Host ('NDK=' + $toolchain.Ndk)
    Write-Host ('NDK_REVISION=' + $toolchain.NdkRevision)
    Write-Host ('JAVA=' + $toolchain.Java)
    Write-Host ('JAVA_MAJOR=' + $toolchain.JavaVersion)
    Write-Host ('ANDROID_PLATFORMS=' + $toolchain.Platforms)
    Write-Host ('BUILD_TOOLS=' + $toolchain.BuildTools)
    Write-Host ('CMAKE_3_22_1=' + $toolchain.Cmake322)
}
