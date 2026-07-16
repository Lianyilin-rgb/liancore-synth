# LianCore V3 - CA代码签名脚本 (P6-4)
# 支持自签名证书和CA颁发证书双重模式
# 用法: powershell -ExecutionPolicy Bypass -File sign_ca.ps1 [-Mode Auto|SelfSigned|CA|Cloud]
#
# 模式说明:
#   Auto      - 自动检测证书类型 (默认)
#   SelfSigned - 使用自签名证书 (开发/测试)
#   CA        - 使用CA颁发证书 (生产发布)
#   Cloud     - 使用Azure Key Vault云签名 (CI/CD)
# =============================================================================

param(
    [ValidateSet("Auto", "SelfSigned", "CA", "Cloud")]
    [string]$Mode = "Auto",

    # 自签名证书参数
    [string]$SelfSignedCertPath = ".\release\LianCoreCert.pfx",
    [string]$SelfSignedCertPassword = "LianCore2024!",

    # CA证书参数
    [string]$CACertPath = ".\release\LianCore_CA.pfx",
    [string]$CACertPassword = "",
    [string]$CACertStore = "CurrentUser",

    # Azure Key Vault参数
    [string]$KeyVaultName = "",
    [string]$KeyVaultCertName = "LianCore-CodeSigning",

    # 通用参数
    [string]$TimestampUrl = "http://timestamp.digicert.com",
    [string]$VST3Path = "build_vst3\LianCore_artefacts\Release\VST3\LianCore.vst3",
    [string]$InstallerPath = "release\stage\LianCore_Installer.exe",
    [switch]$VerifyOnly
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# Windows SDK signtool路径搜索
$SDK_PATHS = @(
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64",
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64",
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22000.0\x64",
    "C:\Program Files (x86)\Windows Kits\10\bin\10.0.19041.0\x64"
)

function Write-Step { param($msg) Write-Host "[签名] $msg" -ForegroundColor Cyan }
function Write-OK { param($msg) Write-Host "  OK: $msg" -ForegroundColor Green }
function Write-Warn { param($msg) Write-Host "  WARN: $msg" -ForegroundColor Yellow }
function Write-Err { param($msg) Write-Host "  ERR: $msg" -ForegroundColor Red }

# =============================================================================
# 阶段1: 查找signtool
# =============================================================================
function Find-SignTool {
    foreach ($sdkPath in $SDK_PATHS) {
        $tool = Join-Path $sdkPath "signtool.exe"
        if (Test-Path $tool) {
            Write-OK "找到 signtool: $tool"
            return $tool
        }
    }
    Write-Err "signtool.exe 未找到，请安装 Windows SDK"
    return $null
}

# =============================================================================
# 阶段2: 检测证书类型
# =============================================================================
function Detect-CertType {
    param([string]$CertPath, [string]$CertPassword)

    if (-NOT (Test-Path $CertPath)) { return "NotFound" }

    # 尝试加载证书并检查颁发者
    try {
        $cert = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2(
            $CertPath, $CertPassword,
            [System.Security.Cryptography.X509Certificates.X509KeyStorageFlags]::Exportable
        )

        $issuer = $cert.Issuer
        $subject = $cert.Subject

        Write-OK "证书主题: $subject"
        Write-OK "证书颁发者: $issuer"

        # 自签名证书: 颁发者 == 主题
        if ($issuer -eq $subject) {
            return "SelfSigned"
        }

        # EV证书检测: 通常包含特定OID
        $evOid = "1.3.6.1.4.1.311.60.2.1.1"
        foreach ($ext in $cert.Extensions) {
            if ($ext.Oid.Value -eq $evOid) {
                return "EV"
            }
        }

        return "CA"
    }
    catch {
        Write-Err "证书加载失败: $_"
        return "Invalid"
    }
}

# =============================================================================
# 阶段3: 验证证书有效性
# =============================================================================
function Test-Certificate {
    param([string]$CertPath, [string]$CertPassword)

    if (-NOT (Test-Path $CertPath)) {
        Write-Err "证书文件不存在: $CertPath"
        return $false
    }

    try {
        $cert = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2(
            $CertPath, $CertPassword,
            [System.Security.Cryptography.X509Certificates.X509KeyStorageFlags]::Exportable
        )

        $now = Get-Date
        if ($cert.NotBefore -gt $now) {
            Write-Err "证书尚未生效 (NotBefore: $($cert.NotBefore))"
            return $false
        }
        if ($cert.NotAfter -lt $now) {
            Write-Err "证书已过期 (NotAfter: $($cert.NotAfter))"
            return $false
        }

        $daysLeft = ($cert.NotAfter - $now).Days
        Write-OK "证书有效期: $($cert.NotBefore) ~ $($cert.NotAfter) (剩余 $daysLeft 天)"

        if ($daysLeft -lt 30) {
            Write-Warn "证书即将过期，请尽快续期!"
        }

        return $true
    }
    catch {
        Write-Err "证书验证失败: $_"
        return $false
    }
}

# =============================================================================
# 阶段4: 签名文件
# =============================================================================
function Sign-File {
    param(
        [string]$SignTool,
        [string]$FilePath,
        [string]$CertPath,
        [string]$CertPassword,
        [string]$TimestampUrl,
        [string]$CertType,
        [string]$CertStore = "CurrentUser"
    )

    if (-NOT (Test-Path $FilePath)) {
        Write-Err "文件不存在: $FilePath"
        return $false
    }

    Write-Step "签名: $(Split-Path $FilePath -Leaf)"

    $signArgs = @(
        "sign"
        "/fd", "SHA256"
        "/td", "SHA256"
        "/tr", $TimestampUrl
        "/v"
    )

    switch ($CertType) {
        "SelfSigned" {
            $signArgs += "/f"
            $signArgs += (Join-Path $ScriptDir $CertPath)
            $signArgs += "/p"
            $signArgs += $CertPassword
        }
        "CA" {
            $signArgs += "/f"
            $signArgs += (Join-Path $ScriptDir $CertPath)
            if ($CertPassword) {
                $signArgs += "/p"
                $signArgs += $CertPassword
            }
        }
        "EV" {
            $signArgs += "/f"
            $signArgs += (Join-Path $ScriptDir $CertPath)
            if ($CertPassword) {
                $signArgs += "/p"
                $signArgs += $CertPassword
            }
            # EV证书使用RFC 3161时间戳
            $signArgs += "/td", "SHA256"
            $signArgs += "/tr", "http://timestamp.digicert.com?alg=sha256"
            $signArgs += "/fd", "SHA256"
        }
        "Cloud" {
            # Azure Key Vault 签名
            $signArgs += "/a"  # 使用存储中的最佳证书
            $signArgs += "/s", "My"
        }
    }

    $fullPath = Join-Path $ScriptDir $FilePath
    Write-Host "  执行: signtool $($signArgs -join ' ') `"$fullPath`"" -ForegroundColor DarkGray

    & $SignTool $signArgs $fullPath 2>&1

    if ($LASTEXITCODE -eq 0) {
        Write-OK "签名成功"
        return $true
    }
    else {
        Write-Err "签名失败 (退出码: $LASTEXITCODE)"
        return $false
    }
}

# =============================================================================
# 阶段5: 验证签名
# =============================================================================
function Verify-Signature {
    param([string]$SignTool, [string]$FilePath)

    if (-NOT (Test-Path $FilePath)) {
        Write-Err "文件不存在: $FilePath"
        return $false
    }

    Write-Step "验证签名: $(Split-Path $FilePath -Leaf)"
    $fullPath = Join-Path $ScriptDir $FilePath

    & $SignTool verify /v /pa $fullPath 2>&1

    if ($LASTEXITCODE -eq 0) {
        Write-OK "签名验证通过"
        return $true
    }
    else {
        Write-Err "签名验证失败"
        return $false
    }
}

# =============================================================================
# 阶段6: 获取签名详细信息
# =============================================================================
function Get-SignatureInfo {
    param([string]$FilePath)

    if (-NOT (Test-Path $FilePath)) { return }

    try {
        $fullPath = Join-Path $ScriptDir $FilePath
        $signature = Get-AuthenticodeSignature -FilePath $fullPath

        Write-Step "数字签名信息: $(Split-Path $FilePath -Leaf)"
        Write-Host "  状态: $($signature.Status)"
        Write-Host "  签名者: $($signature.SignerCertificate.Subject)"
        Write-Host "  颁发者: $($signature.SignerCertificate.Issuer)"
        Write-Host "  序列号: $($signature.SignerCertificate.SerialNumber)"
        Write-Host "  有效期: $($signature.SignerCertificate.NotBefore) ~ $($signature.SignerCertificate.NotAfter)"
        Write-Host "  时间戳: $($signature.TimeStamperCertificate.Subject)"

        return $signature
    }
    catch {
        Write-Err "获取签名信息失败: $_"
    }
}

# =============================================================================
# 主流程
# =============================================================================
function Main {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Magenta
    Write-Host "  LianCore V3 - CA代码签名工具 (P6-4)" -ForegroundColor Magenta
    Write-Host "========================================" -ForegroundColor Magenta
    Write-Host ""

    $signTool = Find-SignTool
    if (-NOT $signTool) { exit 1 }

    # 确定签名模式
    if ($Mode -eq "Auto") {
        # 优先检测CA证书
        if (Test-Path (Join-Path $ScriptDir $CACertPath)) {
            $certType = Detect-CertType $CACertPath $CACertPassword
            if ($certType -eq "CA" -or $certType -eq "EV") {
                $Mode = $certType
                Write-OK "自动检测到 $certType 证书"
            }
            else {
                $Mode = "SelfSigned"
                Write-Warn "CA证书无效，回退到自签名模式"
            }
        }
        else {
            $Mode = "SelfSigned"
            Write-Warn "未找到CA证书，使用自签名模式"
        }
    }

    Write-Step "签名模式: $Mode"

    # 确定证书路径和密码
    $useCertPath = $SelfSignedCertPath
    $useCertPassword = $SelfSignedCertPassword

    if ($Mode -eq "CA" -or $Mode -eq "EV") {
        $useCertPath = $CACertPath
        $useCertPassword = $CACertPassword

        # 验证CA证书
        if (-NOT (Test-Certificate $useCertPath $useCertPassword)) {
            Write-Err "CA证书验证失败，中止签名"
            exit 1
        }
    }
    elseif ($Mode -eq "SelfSigned") {
        if (-NOT (Test-Path (Join-Path $ScriptDir $useCertPath))) {
            Write-Err "自签名证书不存在: $useCertPath"
            Write-Warn "请先生成自签名证书:"
            Write-Host "  New-SelfSignedCertificate -Type CodeSigningCert -Subject `"CN=LianCore`" -KeyUsage DigitalSignature -CertStoreLocation Cert:\CurrentUser\My"
            Write-Host "  Export-PfxCertificate -Cert (Get-ChildItem Cert:\CurrentUser\My | Where-Object {`$_.Subject -like '*LianCore*'}) -FilePath $useCertPath -Password (ConvertTo-SecureString -String '$useCertPassword' -Force -AsPlainText)"
            exit 1
        }
    }

    # 收集要签名的文件
    $filesToSign = @()

    # VST3插件
    $vst3FullPath = Join-Path $ScriptDir $VST3Path
    if (Test-Path $vst3FullPath) {
        $filesToSign += $VST3Path
    }

    # 安装程序 (如果存在)
    $installerFullPath = Join-Path $ScriptDir $InstallerPath
    if (Test-Path $installerFullPath) {
        $filesToSign += $InstallerPath
    }

    if ($filesToSign.Count -eq 0) {
        Write-Err "未找到任何需要签名的文件"
        Write-Warn "请先构建VST3: cmake --build build_vst3 --config Release --target LianCore_VST3"
        exit 1
    }

    Write-Step "待签名文件: $($filesToSign.Count) 个"
    foreach ($f in $filesToSign) {
        Write-Host "  - $f" -ForegroundColor DarkGray
    }

    if ($VerifyOnly) {
        # 仅验证模式
        foreach ($f in $filesToSign) {
            Get-SignatureInfo $f
            Verify-Signature $signTool $f
        }
        exit 0
    }

    # 执行签名
    $allSigned = $true
    foreach ($f in $filesToSign) {
        $result = Sign-File -SignTool $signTool -FilePath $f `
            -CertPath $useCertPath -CertPassword $useCertPassword `
            -TimestampUrl $TimestampUrl -CertType $Mode `
            -CertStore $CACertStore
        if (-NOT $result) { $allSigned = $false }
    }

    # 验证所有签名
    Write-Host ""
    Write-Step "签名验证"
    foreach ($f in $filesToSign) {
        Get-SignatureInfo $f
        Verify-Signature $signTool $f
    }

    Write-Host ""
    if ($allSigned) {
        Write-Host "========================================" -ForegroundColor Green
        Write-Host "  所有文件签名完成!" -ForegroundColor Green
        Write-Host "========================================" -ForegroundColor Green
    }
    else {
        Write-Host "========================================" -ForegroundColor Red
        Write-Host "  部分文件签名失败，请检查日志" -ForegroundColor Red
        Write-Host "========================================" -ForegroundColor Red
        exit 1
    }
}

# =============================================================================
# 入口
# =============================================================================
try {
    Main
}
catch {
    Write-Err "脚本执行失败: $_"
    Write-Host $_.ScriptStackTrace -ForegroundColor DarkGray
    exit 1
}