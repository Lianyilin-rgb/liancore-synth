; =============================================================================
; LianCore V3 - NSIS Installer Script
; Copyright (c) 2024-2026 ¡¨“„¡ÿ. All rights reserved.
; Installs: VST3, AAX, AI Models, Presets, Wavetables, Manuals, Tutorials
; =============================================================================

!include "MUI2.nsh"
!include "FileFunc.nsh"

Name "LianCore Synthesizer V3"
OutFile "LianCore_Setup_V3.exe"
InstallDir "$PROGRAMFILES64\Common Files\VST3"
RequestExecutionLevel admin

!define VERSION "3.0.0"
!define PRODUCT_NAME "LianCore"
!define MANUFACTURER "¡¨“„¡ÿ"
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

; =============================================================================
; Installer Sections
; =============================================================================

Section "VST3 Plugin (Required)" SecVST3
    SectionIn RO
    SetOutPath "$INSTDIR"
    File /r "stage\Common Files\VST3\*.vst3"

    WriteRegStr HKLM "Software\LianCore" "InstallDir" "$INSTDIR"
    WriteRegStr HKLM "Software\LianCore" "Version" "${VERSION}"

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

    WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

Section "AAX Plugin (Pro Tools)" SecAAX
    SetOutPath "$PROGRAMFILES\Common Files\Avid\Audio\Plug-Ins"
    File /r "stage\Common Files\AAX\LianCore.aaxplugin"
    WriteRegStr HKLM "Software\LianCore" "AAXInstalled" "1"
SectionEnd

Section "AI Models" SecModels
    SetOutPath "${SHARED_DIR}\Models"
    !if /FileExists "..\models\audio_encoder.onnx"
        File /r "..\models\*.onnx"
    !endif
    !if /FileExists "..\models\config.json"
        File /r "..\models\*.json"
    !endif
SectionEnd

Section "Factory Presets" SecPresets
    SetOutPath "${SHARED_DIR}\Presets"
    !if /FileExists "..\data\preset_library.db"
        File "..\data\preset_library.db"
    !endif
    !if /FileExists "..\data\factory_presets.db"
        File "..\data\factory_presets.db"
    !endif
SectionEnd

Section "Wavetable Library" SecWavetables
    SetOutPath "${SHARED_DIR}\Wavetables"
    !if /FileExists "..\wavetables\Analog_Saw.npy"
        File /r "..\wavetables\*.npy"
    !else
        !if /FileExists "stage\Common Files\LianCore\Wavetables\Analog_Saw.npy"
            File /r "stage\Common Files\LianCore\Wavetables\*.npy"
        !endif
    !endif
SectionEnd

Section "Quick Start Guide" SecQuickStart
    SetOutPath "${SHARED_DIR}\Docs"
    !if /FileExists "..\docs\quick-start.md"
        File "..\docs\quick-start.md"
    !endif
    !if /FileExists "..\docs\user-manual.md"
        File "..\docs\user-manual.md"
    !endif
SectionEnd

Section "User Manual (zh-CN)" SecManualCN
    SetOutPath "${SHARED_DIR}\Docs\User Manual"
    !if /FileExists "..\docs\manuals\zh-CN\LianCoreV3_User_Manual_zh-CN.pdf"
        File "..\docs\manuals\zh-CN\LianCoreV3_User_Manual_zh-CN.pdf"
    !endif
SectionEnd

Section "User Manual (zh-TW)" SecManualTW
    SetOutPath "${SHARED_DIR}\Docs\User Manual"
    !if /FileExists "..\docs\manuals\zh-TW\LianCoreV3_User_Manual_zh-TW.pdf"
        File "..\docs\manuals\zh-TW\LianCoreV3_User_Manual_zh-TW.pdf"
    !endif
SectionEnd

Section "User Manual (English)" SecManualEN
    SetOutPath "${SHARED_DIR}\Docs\User Manual"
    !if /FileExists "..\docs\manuals\en\LianCoreV3_User_Manual_en.pdf"
        File "..\docs\manuals\en\LianCoreV3_User_Manual_en.pdf"
    !endif
SectionEnd

Section "Tutorial (zh-CN)" SecTutorialCN
    SetOutPath "${SHARED_DIR}\Docs\Tutorial"
    !if /FileExists "..\docs\manuals\zh-CN\LianCoreV3_Tutorial_zh-CN.pdf"
        File "..\docs\manuals\zh-CN\LianCoreV3_Tutorial_zh-CN.pdf"
    !endif
SectionEnd

Section "Tutorial (zh-TW)" SecTutorialTW
    SetOutPath "${SHARED_DIR}\Docs\Tutorial"
    !if /FileExists "..\docs\manuals\zh-TW\LianCoreV3_Tutorial_zh-TW.pdf"
        File "..\docs\manuals\zh-TW\LianCoreV3_Tutorial_zh-TW.pdf"
    !endif
SectionEnd

Section "Tutorial (English)" SecTutorialEN
    SetOutPath "${SHARED_DIR}\Docs\Tutorial"
    !if /FileExists "..\docs\manuals\en\LianCoreV3_Tutorial_en.pdf"
        File "..\docs\manuals\en\LianCoreV3_Tutorial_en.pdf"
    !endif
SectionEnd

Section "VC++ Runtime Check" SecRuntime
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

; ---- Start Menu ----
Section "Start Menu Shortcuts" SecStartMenu
    CreateDirectory "$SMPROGRAMS\LianCore"
    CreateShortCut "$SMPROGRAMS\LianCore\LianCore Website.lnk" "https://liancore.audio"
    CreateShortCut "$SMPROGRAMS\LianCore\Uninstall LianCore.lnk" "$INSTDIR\uninstall.exe"
    !if /FileExists "..\docs\manuals\zh-CN\LianCoreV3_User_Manual_zh-CN.pdf"
        CreateShortCut "$SMPROGRAMS\LianCore\User Manual (zh-CN).lnk" "${SHARED_DIR}\Docs\User Manual\LianCoreV3_User_Manual_zh-CN.pdf"
    !endif
    !if /FileExists "..\docs\manuals\en\LianCoreV3_User_Manual_en.pdf"
        CreateShortCut "$SMPROGRAMS\LianCore\User Manual (English).lnk" "${SHARED_DIR}\Docs\User Manual\LianCoreV3_User_Manual_en.pdf"
    !endif
    !if /FileExists "..\docs\manuals\zh-CN\LianCoreV3_Tutorial_zh-CN.pdf"
        CreateShortCut "$SMPROGRAMS\LianCore\Tutorial (zh-CN).lnk" "${SHARED_DIR}\Docs\Tutorial\LianCoreV3_Tutorial_zh-CN.pdf"
    !endif
SectionEnd

; =============================================================================
; Uninstaller
; =============================================================================
Section "Uninstall"
    ; Remove VST3
    RMDir /r "$INSTDIR\LianCore.vst3"
    Delete "$INSTDIR\uninstall.exe"

    ; Remove AAX
    RMDir /r "$PROGRAMFILES\Common Files\Avid\Audio\Plug-Ins\LianCore.aaxplugin"

    ; Remove shared data
    RMDir /r "${SHARED_DIR}"

    ; Remove Start Menu
    RMDir /r "$SMPROGRAMS\LianCore"

    ; Remove registry
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LianCore"
    DeleteRegKey HKLM "Software\LianCore"
SectionEnd

; =============================================================================
; Component Descriptions
; =============================================================================
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecVST3} "LianCore VST3 synthesizer plugin (required)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecAAX} "AAX plugin for Avid Pro Tools"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecModels} "AI model files for intelligent sound generation"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecPresets} "Factory preset library with 100k+ presets"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecWavetables} "Wavetable library with 90+ waveform types"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecQuickStart} "Quick start guide and user manual (Markdown)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecManualCN} "User manual - Simplified Chinese (PDF)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecManualTW} "User manual - Traditional Chinese (PDF)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecManualEN} "User manual - English (PDF)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecTutorialCN} "Detailed tutorial - Simplified Chinese (PDF)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecTutorialTW} "Detailed tutorial - Traditional Chinese (PDF)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecTutorialEN} "Detailed tutorial - English (PDF)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecRuntime} "Microsoft Visual C++ Runtime (required)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecStartMenu} "Start menu shortcuts and quick links"
!insertmacro MUI_FUNCTION_DESCRIPTION_END