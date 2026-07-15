# LianCore V3 - 代码签名脚本
# 用法: powershell -ExecutionPolicy Bypass -File sign.ps1

param(
    $CertPath = ".\release\LianCoreCert.pfx",
    $CertPassword = "LianCore2024!",
    $TimestampUrl = "http://timestamp.digicert.com"
)

$ErrorActionPreference = "Stop"
$SDK_BIN = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64"
$signtool = Join-Path $SDK_BIN "signtool.exe"

function Write-Step { param($msg) Write-Host "[签名] $msg" -ForegroundColor Cyan }
function Write-OK { param($msg) Write-Host "  OK: $msg" -ForegroundColor Green }

if (-NOT (Test-Path $signtool)) {
    Write-Host "ERR: signtool.exe not found" -ForegroundColor Red
    exit 1
}

$files = @(
    "build_vst3\LianCore_artefacts\Release\VST3\LianCore.vst3\Contents\x86_64-win\LianCore.vst3"
)

Write-Step "LianCore V3 代码签名"
foreach ($f in $files) {
    $fullPath = Join-Path $PSScriptRoot $f
    if (Test-Path $fullPath) {
        Write-Step "签名: $f"
        & $signtool sign /fd SHA256 /f $CertPath /p $CertPassword /tr $TimestampUrl /td SHA256 $fullPath
        if ($LASTEXITCODE -eq 0) { Write-OK "签名成功" }
    }
}
Write-Step "签名完成"