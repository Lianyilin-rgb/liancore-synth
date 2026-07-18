"""Write NSIS installer script with ASCII encoding."""
import os

NSI_CONTENT = r"""; =============================================================================
; LianCore V3 - NSIS Installer Script
; Copyright (c) 2024-2026 LianCore. All rights reserved.
; =============================================================================

!include "MUI2.nsh"
!include "FileFunc.nsh"

Name "LianCore Synthesizer V3"
OutFile "LianCore_Setup_V3.exe"
InstallDir "$PROGRAMFILES64\Common Files\VST3"
RequestExecutionLevel admin

!define VERSION "3.0.0"
!define PRODUCT_NAME "LianCore"
!define MANUFACTURER "LianCore"
!define SHARED_DIR "$PROGRAMFILES64\Common Files\LianCore"

!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "assets\LICENSE.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "VST3 Plugin" SecVST3
    SectionIn RO
    SetOutPath "$INSTDIR"
    File /r "stage\Common Files\VST3\*.vst3"
    WriteRegStr HKLM "Software\LianCore" "InstallDir" "$INSTDIR"
    WriteRegStr HKLM "Software\LianCore" "Version" "${VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LianCore" "DisplayName" "LianCore Synthesizer V3"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LianCore" "UninstallString" "$INSTDIR\uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LianCore" "DisplayVersion" "${VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LianCore" "Publisher" "${MANUFACTURER}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LianCore" "URLInfoAbout" "https://liancore.audio"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LianCore" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LianCore" "NoRepair" 1
    WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

Section "AAX Plugin for Pro Tools" SecAAX
    SetOutPath "$PROGRAMFILES\Common Files\Avid\Audio\Plug-Ins"
    File /r "stage\Common Files\AAX\LianCore.aaxplugin"
    WriteRegStr HKLM "Software\LianCore" "AAXInstalled" "1"
SectionEnd

Section "AI Models" SecModels
    SetOutPath "${SHARED_DIR}\Models"
    File /r "stage\Common Files\LianCore\Models\*"
SectionEnd

Section "Factory Presets" SecPresets
    SetOutPath "${SHARED_DIR}\Presets"
    File "stage\Common Files\LianCore\Presets\preset_library.db"
    File "stage\Common Files\LianCore\Presets\factory_presets.db"
SectionEnd

Section "Wavetable Library" SecWavetables
    SetOutPath "${SHARED_DIR}\Wavetables"
    File /r "stage\Common Files\LianCore\Wavetables\*.npy"
SectionEnd

Section "Quick Start Guide" SecQuickStart
    SetOutPath "${SHARED_DIR}\Docs"
    File "stage\Common Files\LianCore\Docs\quick-start.md"
    File "stage\Common Files\LianCore\Docs\user-manual.md"
SectionEnd

Section "User Manual - Simplified Chinese" SecManualCN
    SetOutPath "${SHARED_DIR}\Docs\User Manual"
    File "stage\Common Files\LianCore\Docs\User Manual\LianCoreV3_User_Manual_zh-CN.pdf"
SectionEnd

Section "User Manual - Traditional Chinese" SecManualTW
    SetOutPath "${SHARED_DIR}\Docs\User Manual"
    File "stage\Common Files\LianCore\Docs\User Manual\LianCoreV3_User_Manual_zh-TW.pdf"
SectionEnd

Section "User Manual - English" SecManualEN
    SetOutPath "${SHARED_DIR}\Docs\User Manual"
    File "stage\Common Files\LianCore\Docs\User Manual\LianCoreV3_User_Manual_en.pdf"
SectionEnd

Section "Tutorial - Simplified Chinese" SecTutorialCN
    SetOutPath "${SHARED_DIR}\Docs\Tutorial"
    File "stage\Common Files\LianCore\Docs\Tutorial\LianCoreV3_Tutorial_zh-CN.pdf"
SectionEnd

Section "Tutorial - Traditional Chinese" SecTutorialTW
    SetOutPath "${SHARED_DIR}\Docs\Tutorial"
    File "stage\Common Files\LianCore\Docs\Tutorial\LianCoreV3_Tutorial_zh-TW.pdf"
SectionEnd

Section "Tutorial - English" SecTutorialEN
    SetOutPath "${SHARED_DIR}\Docs\Tutorial"
    File "stage\Common Files\LianCore\Docs\Tutorial\LianCoreV3_Tutorial_en.pdf"
SectionEnd

Section "Start Menu Shortcuts" SecStartMenu
    CreateDirectory "$SMPROGRAMS\LianCore"
    CreateShortCut "$SMPROGRAMS\LianCore\LianCore Website.lnk" "https://liancore.audio"
    CreateShortCut "$SMPROGRAMS\LianCore\Uninstall LianCore.lnk" "$INSTDIR\uninstall.exe"
    CreateShortCut "$SMPROGRAMS\LianCore\User Manual (zh-CN).lnk" "${SHARED_DIR}\Docs\User Manual\LianCoreV3_User_Manual_zh-CN.pdf"
    CreateShortCut "$SMPROGRAMS\LianCore\User Manual (English).lnk" "${SHARED_DIR}\Docs\User Manual\LianCoreV3_User_Manual_en.pdf"
    CreateShortCut "$SMPROGRAMS\LianCore\Tutorial (zh-CN).lnk" "${SHARED_DIR}\Docs\Tutorial\LianCoreV3_Tutorial_zh-CN.pdf"
SectionEnd

Section "Uninstall"
    RMDir /r "$INSTDIR\LianCore.vst3"
    Delete "$INSTDIR\uninstall.exe"
    RMDir /r "$PROGRAMFILES\Common Files\Avid\Audio\Plug-Ins\LianCore.aaxplugin"
    RMDir /r "${SHARED_DIR}"
    RMDir /r "$SMPROGRAMS\LianCore"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LianCore"
    DeleteRegKey HKLM "Software\LianCore"
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecVST3} "LianCore VST3 synthesizer plugin (required)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecAAX} "AAX plugin for Avid Pro Tools"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecModels} "AI model files for intelligent sound generation"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecPresets} "Factory preset library with 100k+ presets"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecWavetables} "Wavetable library with 100 waveform types"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecQuickStart} "Quick start guide and user manual (Markdown)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecManualCN} "User manual - Simplified Chinese (PDF)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecManualTW} "User manual - Traditional Chinese (PDF)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecManualEN} "User manual - English (PDF)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecTutorialCN} "Detailed tutorial - Simplified Chinese (PDF)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecTutorialTW} "Detailed tutorial - Traditional Chinese (PDF)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecTutorialEN} "Detailed tutorial - English (PDF)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecStartMenu} "Start menu shortcuts and quick links"
!insertmacro MUI_FUNCTION_DESCRIPTION_END
"""

def main():
    project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    nsi_path = os.path.join(project_root, "release", "installer.nsi")
    with open(nsi_path, 'w', encoding='ascii') as f:
        f.write(NSI_CONTENT)
    print(f"Written: {nsi_path} ({len(NSI_CONTENT)} bytes)")

if __name__ == "__main__":
    main()