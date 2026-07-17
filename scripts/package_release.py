# =============================================================================
# LianCore - Release 打包脚本
# 创建完整的安装包，包含插件、模型、预设、波表等所有资源
# =============================================================================

import os
import sys
import shutil
import json
from datetime import datetime

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUTPUT_DIR = os.path.join(PROJECT_ROOT, "release")
STAGE_DIR = os.path.join(OUTPUT_DIR, "stage")

# 安装目录结构
INSTALL_STRUCTURE = {
    "VST3": "Common Files/VST3/LianCore.vst3",
    "AAX": "Common Files/Avid/Audio/Plug-Ins/LianCore.aaxplugin",
    "Models": "Common Files/LianCore/Models",
    "Presets": "Common Files/LianCore/Presets",
    "Wavetables": "Common Files/LianCore/Wavetables",
    "Docs": "Common Files/LianCore/Docs",
}

def setup_release_directory():
    """创建 release 目录结构"""
    print("Setting up release directory structure...")
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    os.makedirs(STAGE_DIR, exist_ok=True)
    
    for dir_name, dir_path in INSTALL_STRUCTURE.items():
        full_path = os.path.join(STAGE_DIR, dir_path.rsplit("/", 1)[0])
        os.makedirs(full_path, exist_ok=True)
        print(f"  Created: {full_path}")


def copy_models():
    """复制 ONNX 模型文件"""
    print("\nCopying AI models...")
    models_dir = os.path.join(PROJECT_ROOT, "models")
    dest_dir = os.path.join(STAGE_DIR, "Common Files/LianCore/Models")
    os.makedirs(dest_dir, exist_ok=True)
    
    model_files = [
        "liancore_ai_model.onnx",
        "transformer_encoder.onnx",
        "audio_encoder.onnx",
        "param_regressor.onnx",
        "wavetable_vae_decoder.onnx",
    ]
    
    total_size = 0
    for model_file in model_files:
        src = os.path.join(models_dir, model_file)
        if os.path.exists(src):
            dst = os.path.join(dest_dir, model_file)
            shutil.copy2(src, dst)
            size = os.path.getsize(dst)
            total_size += size
            print(f"  {model_file}: {size / 1024:.1f} KB")
    
    print(f"  Total models: {total_size / 1024 / 1024:.1f} MB")


def copy_presets():
    """复制预设数据库"""
    print("\nCopying preset database...")
    dest_dir = os.path.join(STAGE_DIR, "Common Files/LianCore/Presets")
    os.makedirs(dest_dir, exist_ok=True)

    # 主预设库
    src = os.path.join(PROJECT_ROOT, "data", "preset_library.db")
    if os.path.exists(src):
        dst = os.path.join(dest_dir, "preset_library.db")
        shutil.copy2(src, dst)
        size = os.path.getsize(dst)
        print(f"  preset_library.db: {size / 1024 / 1024:.1f} MB")
    else:
        print("  WARNING: preset_library.db not found!")

    # 工厂预设
    factory_src = os.path.join(PROJECT_ROOT, "data", "factory_presets.db")
    if os.path.exists(factory_src):
        dst = os.path.join(dest_dir, "factory_presets.db")
        shutil.copy2(factory_src, dst)
        size = os.path.getsize(dst)
        print(f"  factory_presets.db: {size / 1024 / 1024:.1f} MB")
    else:
        print("  WARNING: factory_presets.db not found! Run scripts/select_factory_presets.py first")


def copy_wavetables():
    """复制出厂波表"""
    print("\nCopying factory wavetables...")
    src_dir = os.path.join(PROJECT_ROOT, "data", "factory_wavetables")
    dest_dir = os.path.join(STAGE_DIR, "Common Files/LianCore/Wavetables")
    os.makedirs(dest_dir, exist_ok=True)
    
    if os.path.exists(src_dir):
        count = 0
        total_size = 0
        for fname in os.listdir(src_dir):
            if fname.endswith(".npy"):
                src = os.path.join(src_dir, fname)
                dst = os.path.join(dest_dir, fname)
                shutil.copy2(src, dst)
                total_size += os.path.getsize(dst)
                count += 1
        print(f"  {count} wavetables: {total_size / 1024 / 1024:.1f} MB")
    else:
        print("  WARNING: factory_wavetables not found!")


def copy_plugin():
    """复制 VST3 插件"""
    print("\nCopying VST3 plugin...")
    vst3_dir = os.path.join(STAGE_DIR, "Common Files/VST3")
    os.makedirs(vst3_dir, exist_ok=True)
    
    # 检查多个可能的构建目录
    build_dirs = [
        os.path.join(PROJECT_ROOT, "build", "LianCore_artefacts", "Release", "VST3"),
        "C:/LianCoreSrc/build_vst3/LianCore_artefacts/Release/VST3",
    ]
    
    found = False
    for build_dir in build_dirs:
        if os.path.exists(build_dir):
            for fname in os.listdir(build_dir):
                if fname.endswith(".vst3"):
                    src = os.path.join(build_dir, fname)
                    dst = os.path.join(vst3_dir, fname)
                    if os.path.isdir(src):
                        if os.path.exists(dst):
                            shutil.rmtree(dst)
                        shutil.copytree(src, dst)
                    else:
                        shutil.copy2(src, dst)
                    print(f"  {fname}: {os.path.getsize(dst) / 1024 / 1024:.1f} MB" if os.path.isfile(dst) else f"  {fname}: directory")
                    found = True
            if found:
                break
    
    if not found:
        print(f"  WARNING: VST3 build not found. Checked: {build_dirs}")
        print("  Build the plugin first: cmake --build build_vst3 --config Release --target LianCore_VST3")


def copy_aax_plugin():
    """复制 AAX 插件"""
    print("\nCopying AAX plugin...")
    aax_dir = os.path.join(STAGE_DIR, "Common Files", "Avid", "Audio", "Plug-Ins")
    os.makedirs(aax_dir, exist_ok=True)
    
    # 检查多个可能的构建目录
    build_dirs = [
        os.path.join(PROJECT_ROOT, "build", "LianCore_artefacts", "Release", "AAX"),
        "C:/LianCoreSrc/build_vst3/LianCore_artefacts/Release/AAX",
    ]
    
    found = False
    for build_dir in build_dirs:
        if os.path.exists(build_dir):
            for fname in os.listdir(build_dir):
                if fname.endswith(".aaxplugin"):
                    src = os.path.join(build_dir, fname)
                    dst = os.path.join(aax_dir, fname)
                    if os.path.isdir(src):
                        if os.path.exists(dst):
                            shutil.rmtree(dst)
                        shutil.copytree(src, dst)
                    else:
                        shutil.copy2(src, dst)
                    print(f"  {fname}: directory")
                    found = True
            if found:
                break
    
    if not found:
        print(f"  WARNING: AAX build not found. Checked: {build_dirs}")
        print("  Build the plugin first: cmake --build build_vst3 --config Release --target LianCore_AAX")


def copy_docs():
    """复制文档"""
    print("\nCopying documentation...")
    dest_dir = os.path.join(STAGE_DIR, "Common Files/LianCore/Docs")
    os.makedirs(dest_dir, exist_ok=True)

    docs = {
        os.path.join(PROJECT_ROOT, "docs", "user-manual.md"): "user-manual.md",
        os.path.join(PROJECT_ROOT, "docs", "quick-start.md"): "quick-start.md",
    }

    for src, fname in docs.items():
        if os.path.exists(src):
            dst = os.path.join(dest_dir, fname)
            shutil.copy2(src, dst)
            print(f"  {fname}: {os.path.getsize(dst)} bytes")
        else:
            print(f"  WARNING: {fname} not found")


def create_install_manifest():
    """创建安装清单"""
    print("\nCreating install manifest...")
    manifest = {
        "product": "LianCore Synthesizer",
        "version": "3.0.0",
        "build_date": datetime.now().isoformat(),
        "components": [],
    }
    
    for root, dirs, files in os.walk(STAGE_DIR):
        for f in files:
            full_path = os.path.join(root, f)
            rel_path = os.path.relpath(full_path, STAGE_DIR)
            manifest["components"].append({
                "path": rel_path,
                "size": os.path.getsize(full_path),
            })
    
    manifest_path = os.path.join(STAGE_DIR, "install_manifest.json")
    with open(manifest_path, 'w') as f:
        json.dump(manifest, f, indent=2)
    
    total_size = sum(c["size"] for c in manifest["components"])
    print(f"  Total components: {len(manifest['components'])}")
    print(f"  Total size: {total_size / 1024 / 1024:.1f} MB")


def generate_nsis_script():
    """生成 NSIS 安装脚本"""
    print("\nGenerating NSIS installer script...")
    
    nsis_script = r'''# =============================================================================
# LianCore V3 - NSIS 安装脚本
# 支持: 自动检测 VC++ 运行库, VST3/AAX 安装, 数据文件, 卸载
# =============================================================================

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "WinVer.nsh"

# ---- 基本信息 ----
Name "LianCore Synthesizer V3"
OutFile "LianCore_Setup_V3.exe"
InstallDir "$PROGRAMFILES64\Common Files\VST3"
RequestExecutionLevel admin

# ---- 版本 ----
!define VERSION "3.0.0"
!define PRODUCT_NAME "LianCore"
!define MANUFACTURER "Lian Audio"

# ---- 界面设置 ----
!define MUI_ABORTWARNING
!define MUI_ICON "assets\icon.ico"
!define MUI_UNICON "assets\icon.ico"

# ---- 安装页面 ----
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "assets\LICENSE.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

# ---- 卸载页面 ----
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

# ---- 语言 ----
!insertmacro MUI_LANGUAGE "English"

# ---- 安装部分 ----
Section "LianCore VST3" SecVST3
    SetOutPath "$INSTDIR"
    
    # VST3 插件
    File /r "stage\Common Files\VST3\*.vst3"
    
    # 数据文件目录
    SetOutPath "$PROGRAMFILES64\Common Files\LianCore\Models"
    File /r "stage\Common Files\LianCore\Models\*"
    
    SetOutPath "$PROGRAMFILES64\Common Files\LianCore\Presets"
    File /r "stage\Common Files\LianCore\Presets\*"
    
    SetOutPath "$PROGRAMFILES64\Common Files\LianCore\Wavetables"
    File /r "stage\Common Files\LianCore\Wavetables\*"
    
    # 创建开始菜单
    CreateDirectory "$SMPROGRAMS\LianCore"
    CreateShortCut "$SMPROGRAMS\LianCore\LianCore Website.lnk" "https://liancore.audio"
    
    # 写入注册表
    WriteRegStr HKLM "Software\LianCore" "InstallDir" "$INSTDIR"
    WriteRegStr HKLM "Software\LianCore" "Version" "${VERSION}"
    
    # 卸载信息
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LianCore" \
        "DisplayName" "LianCore Synthesizer V3"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LianCore" \
        "UninstallString" "$INSTDIR\uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LianCore" \
        "DisplayVersion" "${VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LianCore" \
        "Publisher" "${MANUFACTURER}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LianCore" \
        "URLInfoAbout" "https://liancore.audio"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LianCore" \
        "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LianCore" \
        "NoRepair" 1
    
    # 创建卸载程序
    WriteUninstaller "$INSTDIR\uninstall.exe"
    
    # 计算安装大小
    ${GetSize} "$PROGRAMFILES64\Common Files\LianCore" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LianCore" \
        "EstimatedSize" "$0"
SectionEnd

# ---- VC++ 运行库检测 ----
Section "VC++ Runtime Check" SecRuntime
    # 检查 VC++ 2015-2022 Redistributable
    ReadRegStr $0 HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64" "Installed"
    StrCmp $0 "1" RuntimeInstalled RuntimeMissing
    
    RuntimeMissing:
        MessageBox MB_YESNO|MB_ICONQUESTION \
            "Microsoft Visual C++ 2015-2022 Redistributable is required.$\n$\nDo you want to download and install it now?" \
            IDYES DownloadRuntime IDNO SkipRuntime
    
    DownloadRuntime:
        NSISdl::download "https://aka.ms/vs/17/release/vc_redist.x64.exe" "$TEMP\vc_redist.x64.exe"
        ExecWait '"$TEMP\vc_redist.x64.exe" /quiet /norestart'
        Delete "$TEMP\vc_redist.x64.exe"
        Goto RuntimeInstalled
    
    SkipRuntime:
        MessageBox MB_OK|MB_ICONWARNING \
            "LianCore may not function correctly without VC++ Runtime.$\nPlease install it manually from microsoft.com"
    
    RuntimeInstalled:
SectionEnd

# ---- 卸载部分 ----
Section "Uninstall"
    # 删除 VST3
    Delete "$INSTDIR\LianCore.vst3"
    Delete "$INSTDIR\uninstall.exe"
    
    # 删除数据文件
    RMDir /r "$PROGRAMFILES64\Common Files\LianCore"
    
    # 删除开始菜单
    RMDir /r "$SMPROGRAMS\LianCore"
    
    # 删除注册表
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LianCore"
    DeleteRegKey HKLM "Software\LianCore"
SectionEnd

# ---- 安装描述 ----
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecVST3} "LianCore VST3 synthesizer plugin and factory content"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecRuntime} "Microsoft Visual C++ Runtime (required for VST3)"
!insertmacro MUI_FUNCTION_DESCRIPTION_END
'''
    
    script_path = os.path.join(OUTPUT_DIR, "installer.nsi")
    with open(script_path, 'w') as f:
        f.write(nsis_script)
    
    print(f"  NSIS script: {script_path}")


def main():
    print("=" * 60)
    print("LianCore V3 - Release Package Builder")
    print("=" * 60)
    
    setup_release_directory()
    copy_models()
    copy_presets()
    copy_wavetables()
    copy_plugin()
    copy_aax_plugin()
    copy_docs()
    create_install_manifest()
    generate_nsis_script()
    
    print("\n" + "=" * 60)
    print("Package staging complete!")
    print(f"Staging directory: {STAGE_DIR}")
    print(f"NSIS script: {OUTPUT_DIR}/installer.nsi")
    print("\nTo build the installer:")
    print("  1. Install NSIS (https://nsis.sourceforge.io)")
    print("  2. Build plugin: cmake --build build --config Release --target LianCore_VST3")
    print("  3. Run: python scripts/package_release.py")
    print("  4. Run: makensis release/installer.nsi")
    print("=" * 60)


if __name__ == "__main__":
    main()