# =============================================================================
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
    
    # VST3 插件 (目录形式)
    File /r "stage\Common Files\VST3\LianCore.vst3"
    
    # 数据文件目录
    SetOutPath "$PROGRAMFILES64\Common Files\LianCore\Models"
    File /r "stage\Common Files\LianCore\Models\*"
    
    SetOutPath "$PROGRAMFILES64\Common Files\LianCore\Presets"
    File /r "stage\Common Files\LianCore\Presets\*"
    
    SetOutPath "$PROGRAMFILES64\Common Files\LianCore\Wavetables"
    File /r "stage\Common Files\LianCore\Wavetables\*"
    
    # 文档
    SetOutPath "$PROGRAMFILES64\Common Files\LianCore\Docs"
    File /r "stage\Common Files\LianCore\Docs\*"
    
    # 创建开始菜单
    CreateDirectory "$SMPROGRAMS\LianCore"
    CreateShortCut "$SMPROGRAMS\LianCore\LianCore Website.lnk" "https://github.com/Lianyilin-rgb/liancore-synth"
    CreateShortCut "$SMPROGRAMS\LianCore\Quick Start Guide.lnk" "$PROGRAMFILES64\Common Files\LianCore\Docs\quick-start.md"
    CreateShortCut "$SMPROGRAMS\LianCore\User Manual.lnk" "$PROGRAMFILES64\Common Files\LianCore\Docs\user-manual.md"
    
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
    # 删除 VST3 (目录)
    RMDir /r "$INSTDIR\LianCore.vst3"
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
