@echo off
REM LianCore V3 - 构建后自动签名 (P6-4)
REM 此脚本作为CMake POST_BUILD步骤调用
REM 自动检测证书并签名VST3/安装程序

setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set SIGN_SCRIPT=%SCRIPT_DIR%release\sign_ca.ps1

echo [LianCore] 构建后签名步骤...
echo [LianCore] 签名脚本: %SIGN_SCRIPT%

if not exist "%SIGN_SCRIPT%" (
    echo [LianCore] WARN: 签名脚本不存在，跳过签名
    exit /b 0
)

REM 调用签名脚本 (Auto模式)
powershell -ExecutionPolicy Bypass -File "%SIGN_SCRIPT%" -Mode Auto

if %ERRORLEVEL% neq 0 (
    echo [LianCore] WARN: 签名失败 (非致命错误)
    REM 签名失败不阻止构建
)

exit /b 0