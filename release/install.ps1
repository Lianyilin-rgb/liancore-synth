# LianCore V3 VST3 安装脚本 (PowerShell)
# 功能: 无需NSIS，直接安装/卸载VST3插件
# 用法: 
#   安装: powershell -ExecutionPolicy Bypass -File install.ps1
#   卸载: powershell -ExecutionPolicy Bypass -File install.ps1 -Uninstall

param([switch]$Uninstall)

$ErrorActionPreference = "Stop"
$VST3_NAME = "LianCore.vst3"
$VST3_COMMON = "$env:CommonProgramFiles\VST3"
$SOURCE_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$VST3_SOURCE = Join-Path $SOURCE_DIR "..\build_vst3\LianCore_artefacts\Release\VST3\$VST3_NAME"

function Write-Step { param($msg) Write-Host "[LianCore] $msg" -ForegroundColor Cyan }
function Write-OK { param($msg) Write-Host "  OK: $msg" -ForegroundColor Green }
function Write-ERR { param($msg) Write-Host "  ERR: $msg" -ForegroundColor Red }

if ($Uninstall) {
    Write-Step "卸载 LianCore VST3..."
    $target = Join-Path $VST3_COMMON $VST3_NAME
    if (Test-Path $target) {
        Remove-Item -Recurse -Force $target
        Write-OK "已删除: $target"
    } else {
        Write-OK "未找到安装，跳过"
    }
    Remove-ItemProperty -Path "HKLM:\SOFTWARE\LianCore" -Name "*" -ErrorAction SilentlyContinue
    Write-Step "卸载完成"
    exit 0
}

Write-Step "LianCore V3 VST3 安装程序"
Write-Step "============================"

# 检查管理员权限
if (-NOT ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-ERR "需要管理员权限。请以管理员身份运行PowerShell。"
    exit 1
}

# 检查源文件
if (-NOT (Test-Path $VST3_SOURCE)) {
    Write-ERR "找不到VST3源文件: $VST3_SOURCE"
    Write-ERR "请先构建VST3: cmake --build build_vst3 --config Release"
    exit 1
}

# 目标路径
$targetPath = Join-Path $VST3_COMMON $VST3_NAME

# 备份旧版本
if (Test-Path $targetPath) {
    Write-Step "检测到已有版本，备份中..."
    $backup = "$targetPath.bak.$(Get-Date -Format 'yyyyMMddHHmmss')"
    Rename-Item $targetPath $backup
    Write-OK "已备份到: $backup"
}

# 复制VST3包
Write-Step "安装 VST3 插件到: $targetPath"
Copy-Item -Recurse -Force $VST3_SOURCE $targetPath
Write-OK "VST3 插件安装完成"

# 写入注册表
Write-Step "写入注册表信息..."
$regPath = "HKLM:\SOFTWARE\LianCore"
New-Item -Path $regPath -Force | Out-Null
Set-ItemProperty -Path $regPath -Name "InstallPath" -Value $targetPath
Set-ItemProperty -Path $regPath -Name "Version" -Value "3.0.0"
Set-ItemProperty -Path $regPath -Name "InstallDate" -Value (Get-Date -Format "yyyy-MM-dd")
Write-OK "注册表信息已写入"

Write-Step "============================"
Write-Step "安装完成！"
Write-Step "  版本: 3.0.0"
Write-Step "  DAW需要重新扫描插件才能发现LianCore"
Write-Step "============================"