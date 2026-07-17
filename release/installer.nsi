; =============================================================================
; LianCore V3 - NSIS Installer Script
; Copyright (c) 2024-2026 LianCore Audio. All rights reserved.
; =============================================================================

!include "MUI2.nsh"
!include "FileFunc.nsh"

Name "LianCore Synthesizer V3"
OutFile "LianCore_Setup_V3.exe"
InstallDir "$PROGRAMFILES64\Common Files\VST3"
RequestExecutionLevel admin

!define VERSION "3.0.0"
!define PRODUCT_NAME "LianCore"
!define MANUFACTURER "LianCore Audio"

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

Section "LianCore VST3" SecVST3
    SectionIn RO
    SetOutPath "$INSTDIR"
    File /r "stage\Common Files\VST3\*.vst3"
    SetOutPath "$PROGRAMFILES64\Common Files\LianCore\Models"
    File /r "stage\Common Files\LianCore\Models\*"
    SetOutPath "$PROGRAMFILES64\Common Files\LianCore\Presets"
    File /r "stage\Common Files\LianCore\Presets\*"
    SetOutPath "$PROGRAMFILES64\Common Files\LianCore\Wavetables"
    File /r "stage\Common Files\LianCore\Wavetables\*"
    SetOutPath "$PROGRAMFILES64\Common Files\LianCore\Docs"
    File /r "stage\Common Files\LianCore\Docs\*"
    CreateDirectory "$SMPROGRAMS\LianCore"
    CreateShortCut "$SMPROGRAMS\LianCore\LianCore Website.lnk" "https://liancore.audio"
    CreateShortCut "$SMPROGRAMS\LianCore\Uninstall LianCore.lnk" "$INSTDIR\uninstall.exe"
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

Section "AAX Plugin (Pro Tools)" SecAAX
    SetOutPath "$PROGRAMFILES\Common Files\Avid\Audio\Plug-Ins"
    File /r "stage\Common Files\AAX\LianCore.aaxplugin"
    WriteRegStr HKLM "Software\LianCore" "AAXInstalled" "1"
SectionEnd

Section "Uninstall"
    RMDir /r "$INSTDIR\LianCore.vst3"
    Delete "$INSTDIR\uninstall.exe"
    RMDir /r "$PROGRAMFILES64\Common Files\LianCore"
    RMDir /r "$PROGRAMFILES\Common Files\Avid\Audio\Plug-Ins\LianCore.aaxplugin"
    RMDir /r "$SMPROGRAMS\LianCore"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LianCore"
    DeleteRegKey HKLM "Software\LianCore"
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecVST3} "LianCore VST3 synthesizer plugin and factory content (required)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecAAX} "AAX plugin for Avid Pro Tools"
!insertmacro MUI_FUNCTION_DESCRIPTION_END