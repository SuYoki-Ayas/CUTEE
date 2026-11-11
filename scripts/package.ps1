<#
.SYNOPSIS
    Builds a Release binary of CUTEE and packages it with all Qt runtime
    dependencies into dist\.

.DESCRIPTION
    Uses MSYS2's cmake/ninja with the MSVC toolset (same combination as the
    local-msvc CMake preset), then runs windeployqt so the output folder is
    self-contained and can be zipped and shipped.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\package.ps1
#>

[CmdletBinding()]
param(
    [string]$QtDir = 'D:\Qt\6.11.3\msvc2022_64',
    [string]$MsvcDir = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.44.35207',
    [string]$SdkDir = 'C:\Program Files (x86)\Windows Kits\10',
    [string]$SdkVersion = '10.0.28000.0',
    [string]$NinjaDir = 'D:\MSYS2\ucrt64\bin',
    [string]$BuildDir = 'build-release'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    foreach ($tool in @("$QtDir\bin\windeployqt.exe", "$MsvcDir\bin\HostX64\x64\cl.exe", "$NinjaDir\ninja.exe")) {
        if (-not (Test-Path $tool)) { throw "Missing tool: $tool" }
    }

    $versionMatch = Select-String -Path "$root\CMakeLists.txt" -Pattern 'project\(CUTEE VERSION ([0-9.]+)'
    $version = if ($versionMatch) { $versionMatch.Matches[0].Groups[1].Value } else { $null }
    if (-not $version) { throw 'Could not read the project version from CMakeLists.txt' }
    Write-Host "CUTEE $version" -ForegroundColor Cyan

    $env:PATH = "$MsvcDir\bin\HostX64\x64;$SdkDir\bin\$SdkVersion\x64;$NinjaDir;$QtDir\bin;$env:PATH"
    $env:INCLUDE = "$MsvcDir\include;$SdkDir\Include\$SdkVersion\ucrt;$SdkDir\Include\$SdkVersion\shared;$SdkDir\Include\$SdkVersion\um"
    $env:LIB = "$MsvcDir\lib\x64;$SdkDir\Lib\$SdkVersion\ucrt\x64;$SdkDir\Lib\$SdkVersion\um\x64"

    cmake -S $root -B "$root\$BuildDir" -G Ninja `
        -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_CXX_COMPILER="$($MsvcDir -replace '\\','/')/bin/HostX64/x64/cl.exe" `
        -DCMAKE_MAKE_PROGRAM="$($NinjaDir -replace '\\','/')/ninja.exe" `
        -DCMAKE_PREFIX_PATH="$($QtDir -replace '\\','/')" `
        -DCUTEE_BUILD_TESTS=OFF
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }

    cmake --build "$root\$BuildDir"
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }

    $stage = Join-Path $root "dist\CUTEE-$version-win64"
    if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
    New-Item -ItemType Directory -Path $stage -Force | Out-Null
    Copy-Item "$root\$BuildDir\CUTEE.exe" $stage

    # The trend chart's QML is embedded in the C++ source, so windeployqt cannot
    # discover the QtGraphs import by scanning the executable. Hand it a scratch
    # QML directory containing the same imports to pull in the right QML modules.
    $qmlScan = Join-Path $root "$BuildDir\qml-scan"
    New-Item -ItemType Directory -Path $qmlScan -Force | Out-Null
    @'
import QtQuick
import QtGraphs

GraphsView {
    LineSeries { }
    ScatterSeries { }
}
'@ | Set-Content -Path (Join-Path $qmlScan 'Scan.qml') -Encoding UTF8

    & "$QtDir\bin\windeployqt.exe" --release --no-system-d3d-compiler --no-opengl-sw `
        --qmldir $qmlScan (Join-Path $stage 'CUTEE.exe')
    if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed' }

    # windeployqt ships the merged qt_zh_CN.qm, but the app loads the module
    # specific catalog through QTranslator, so ship that one as well.
    $qtbaseZh = Join-Path $QtDir 'translations\qtbase_zh_CN.qm'
    if (Test-Path $qtbaseZh) {
        Copy-Item $qtbaseZh (Join-Path $stage 'translations') -Force
    }

    foreach ($required in @('Qt6Core.dll', 'Qt6Widgets.dll', 'Qt6Graphs.dll', 'Qt6Quick.dll',
                           'platforms\qwindows.dll', 'qml\QtGraphs', 'translations\qtbase_zh_CN.qm')) {
        if (-not (Test-Path (Join-Path $stage $required))) {
            throw "The deployed bundle is missing $required"
        }
    }

    $zip = Join-Path $root "dist\CUTEE-$version-win64.zip"
    if (Test-Path $zip) { Remove-Item -Force $zip }
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip
    Write-Host "Packaged: $zip" -ForegroundColor Green
}
finally {
    Pop-Location
}
