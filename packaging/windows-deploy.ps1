param(
    [string]$ExePath = "",
    [string]$QtRoot = "C:\Qt\6.12.0\msvc2022_64"
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot

if (-not $ExePath) {
    $candidates = Get-ChildItem -Path (Join-Path $projectRoot "build") -Recurse -Filter "appSecureVault.exe" -ErrorAction SilentlyContinue
    if (-not $candidates) {
        throw "appSecureVault.exe not found. Build the project first."
    }
    $ExePath = $candidates[0].FullName
}

$windeploy = Join-Path $QtRoot "bin\windeployqt.exe"
if (-not (Test-Path $windeploy)) {
    throw "windeployqt not found at $windeploy"
}

& $windeploy --qmldir $projectRoot --release $ExePath
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed"
}

Write-Host "Deployed Qt runtime next to $ExePath"
Write-Host "Copy that folder to ship a Windows build. libsodium is linked in if built static."
