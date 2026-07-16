# =============================================================================
# LianCore V3 - Source Sync Script
# Syncs F:\LianCore... (Chinese path) to C:\LianCore (ASCII path) for build
# Usage: .\scripts\sync_to_ascii.ps1 [-DryRun]
# =============================================================================
param(
    [switch]$DryRun = $false
)

$ErrorActionPreference = "Stop"

$SourceRoot = "F:\LianCore软音源合成器V3版本商业正式版"
$TargetRoot = "C:\LianCore"

$SyncDirs = @("src", "tests", "models", "presets", "resources", "cmake", "release", "scripts")
$SyncFiles = @("CMakeLists.txt", "CMakePresets.json", "LICENSE", "README.md")
$ExcludeDirs = @("build", ".git", "node_modules", "__pycache__", ".vs", "out")
$ExcludeFiles = @("*.obj", "*.pdb", "*.ilk", "*.exp", "*.lib", "*.dll", "*.exe", "*.sln", "*.vcxproj", "*.vcxproj.filters", "*.vcxproj.user", "*.cmake", ".gitignore", ".gitattributes", "*.db")

function Write-Header([string]$Text) {
    Write-Host ""
    Write-Host ("=" * 70) -ForegroundColor Cyan
    Write-Host "  $Text" -ForegroundColor Cyan
    Write-Host ("=" * 70) -ForegroundColor Cyan
}

function Write-Step([string]$Text) { Write-Host "  [>>] $Text" -ForegroundColor Yellow }
function Write-OK([string]$Text) { Write-Host "  [OK] $Text" -ForegroundColor Green }
function Write-ERR([string]$Text) { Write-Host "  [!!] $Text" -ForegroundColor Red }
function Write-INFO([string]$Text) { Write-Host "  [--] $Text" -ForegroundColor Gray }

# Pre-check
Write-Header "LianCore V3 Source Sync"
Write-Host "  Source: $SourceRoot"
Write-Host "  Target: $TargetRoot"

if ($DryRun) {
    Write-Host "  *** DRY RUN MODE - no files will be modified ***" -ForegroundColor Magenta
}

if (-not (Test-Path $SourceRoot)) { Write-ERR "Source not found: $SourceRoot"; exit 1 }
if (-not (Test-Path $TargetRoot)) {
    Write-ERR "Target not found: $TargetRoot"
    Write-INFO "Creating target directory..."
    if (-not $DryRun) { New-Item -ItemType Directory -Path $TargetRoot -Force | Out-Null }
    Write-OK "Target directory created"
}

# Build robocopy args
$RobocopyArgs = @("/MIR", "/NJH", "/NJS", "/NP", "/NDL", "/R:2", "/W:2", "/MT:8")
if ($DryRun) { $RobocopyArgs += "/L" }

$ExcludeDirArgs = @(); foreach ($d in $ExcludeDirs) { $ExcludeDirArgs += "/XD"; $ExcludeDirArgs += $d }
$ExcludeFileArgs = @(); foreach ($f in $ExcludeFiles) { $ExcludeFileArgs += "/XF"; $ExcludeFileArgs += $f }

# Sync directories
Write-Header "Syncing Source Directories"
$TotalDirs = 0

foreach ($dir in $SyncDirs) {
    $srcPath = Join-Path $SourceRoot $dir
    $dstPath = Join-Path $TargetRoot $dir
    if (-not (Test-Path $srcPath)) { Write-INFO "Skip (not found): $dir"; continue }

    Write-Step "Sync: $dir"
    $args = @($srcPath, $dstPath) + $RobocopyArgs + $ExcludeDirArgs + $ExcludeFileArgs
    & robocopy @args | Out-Null

    if ($LASTEXITCODE -ge 8) { Write-ERR "Sync failed: $dir (exit: $LASTEXITCODE)" }
    else { Write-OK "Done" }
    $TotalDirs++
}

# Sync root files
Write-Header "Syncing Root Files"
foreach ($file in $SyncFiles) {
    $srcFile = Join-Path $SourceRoot $file
    if (-not (Test-Path $srcFile)) { continue }
    Write-Step "Sync: $file"
    if (-not $DryRun) { Copy-Item -Path $srcFile -Destination (Join-Path $TargetRoot $file) -Force }
    Write-OK "Done"
}

# Sync preset DB (only if newer)
Write-Header "Syncing Preset Database"
$presetDb = "preset_library_1M.db"
$srcDb = Join-Path $SourceRoot $presetDb
$dstDb = Join-Path $TargetRoot $presetDb

if (Test-Path $srcDb) {
    $shouldCopy = $true
    if (Test-Path $dstDb) {
        if ((Get-Item $srcDb).LastWriteTime -le (Get-Item $dstDb).LastWriteTime) {
            Write-INFO "Preset DB is up-to-date (skipped)"
            $shouldCopy = $false
        }
    }
    if ($shouldCopy) {
        $sizeMB = [math]::Round((Get-Item $srcDb).Length / 1MB, 2)
        Write-Step "Sync: $presetDb ($sizeMB MB)"
        if (-not $DryRun) { Copy-Item -Path $srcDb -Destination $dstDb -Force }
        Write-OK "Done"
    }
} else { Write-INFO "Preset DB not found (skipped)" }

# Sync ONNX DLLs
Write-Header "Syncing ONNX Runtime DLLs"
foreach ($dll in @("onnxruntime.dll", "onnxruntime_providers_shared.dll")) {
    $srcDll = Join-Path $SourceRoot $dll
    if (Test-Path $srcDll) {
        Write-Step "Sync: $dll"
        if (-not $DryRun) { Copy-Item -Path $srcDll -Destination (Join-Path $TargetRoot $dll) -Force }
        Write-OK "Done"
    }
}

# Summary
Write-Header "Sync Complete"
Write-Host "  Directories synced: $TotalDirs"
if ($DryRun) {
    Write-Host "  *** DRY RUN - no changes made ***" -ForegroundColor Magenta
} else {
    Write-Host "  *** Ready to build in C:\LianCore ***" -ForegroundColor Green
    Write-Host "  Build: cmake --build C:\LianCore\build --config Release --target LianCoreTests -j 8"
}
Write-Host ""