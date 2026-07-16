# LianCore V3 - 自签名证书生成脚本 (P6-4)
# 用于开发/测试环境，生产环境请使用CA颁发证书
# 用法: powershell -ExecutionPolicy Bypass -File generate_cert.ps1

param(
    [string]$OutputPath = ".\release\LianCoreCert.pfx",
    [string]$CertPassword = "LianCore2024!",
    [string]$SubjectName = "CN=LianCore Audio Synthesizer, O=LianCore, C=CN",
    [int]$ValidYears = 3
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

function Write-Step { param($msg) Write-Host "[证书] $msg" -ForegroundColor Cyan }
function Write-OK { param($msg) Write-Host "  OK: $msg" -ForegroundColor Green }

Write-Host ""
Write-Host "========================================" -ForegroundColor Magenta
Write-Host "  LianCore V3 - 自签名代码签名证书生成" -ForegroundColor Magenta
Write-Host "========================================" -ForegroundColor Magenta
Write-Host ""

Write-Step "生成代码签名证书..."

# 生成自签名证书
$cert = New-SelfSignedCertificate `
    -Type CodeSigningCert `
    -Subject $SubjectName `
    -KeyUsage DigitalSignature `
    -CertStoreLocation "Cert:\CurrentUser\My" `
    -NotAfter (Get-Date).AddYears($ValidYears) `
    -KeyAlgorithm RSA `
    -KeyLength 4096 `
    -HashAlgorithm SHA256

Write-OK "证书已生成:"
Write-Host "  主题: $($cert.Subject)"
Write-Host "  指纹: $($cert.Thumbprint)"
Write-Host "  有效期: $($cert.NotBefore) ~ $($cert.NotAfter)"

# 导出为PFX
$securePassword = ConvertTo-SecureString -String $CertPassword -Force -AsPlainText
$outputFullPath = Join-Path $ScriptDir $OutputPath

Write-Step "导出PFX证书: $OutputPath"
Export-PfxCertificate `
    -Cert $cert `
    -FilePath $outputFullPath `
    -Password $securePassword `
    -ChainOption EndEntityCertOnly

Write-OK "证书已导出到: $outputFullPath"

# 信任自签名证书 (可选)
Write-Step "将证书添加到受信任的根证书颁发机构..."
try {
    $store = New-Object System.Security.Cryptography.X509Certificates.X509Store(
        [System.Security.Cryptography.X509Certificates.StoreName]::Root,
        [System.Security.Cryptography.X509Certificates.StoreLocation]::CurrentUser
    )
    $store.Open([System.Security.Cryptography.X509Certificates.OpenFlags]::ReadWrite)
    $store.Add($cert)
    $store.Close()
    Write-OK "证书已添加到受信任的根证书颁发机构"
}
catch {
    Write-Host "  WARN: 无法添加证书到信任存储: $_" -ForegroundColor Yellow
    Write-Host "  手动信任证书: certlm.msc -> 受信任的根证书颁发机构 -> 证书 -> 导入 $outputFullPath" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "  自签名证书生成完成!" -ForegroundColor Green
Write-Host "  文件: $outputFullPath" -ForegroundColor Green
Write-Host "  密码: $CertPassword" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "CA证书申请指南:" -ForegroundColor Cyan
Write-Host "  1. 生成CSR: certreq -new request.inf request.csr" -ForegroundColor DarkGray
Write-Host "  2. 提交CSR到CA (如 DigiCert, Sectigo, GlobalSign)" -ForegroundColor DarkGray
Write-Host "  3. 导入CA证书: certreq -accept issued.cer" -ForegroundColor DarkGray
Write-Host "  4. 导出PFX: certmgr.msc -> 导出私钥" -ForegroundColor DarkGray
Write-Host "  5. 将PFX保存为: release\LianCore_CA.pfx" -ForegroundColor DarkGray