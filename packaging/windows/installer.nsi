; PhotoTransfer Windows Installer
; NSIS Script

!include "MUI2.nsh"
!include "FileFunc.nsh"

; General
Name "Photo Transfer"
OutFile "PhotoTransfer-1.0.0-Windows-Setup.exe"
InstallDir "$PROGRAMFILES64\PhotoTransfer"
InstallDirRegKey HKLM "Software\PhotoTransfer" "InstallDir"
RequestExecutionLevel admin

; Version info
VIProductVersion "1.0.0.0"
VIAddVersionKey "ProductName" "Photo Transfer"
VIAddVersionKey "CompanyName" "PhotoTransfer"
VIAddVersionKey "LegalCopyright" "Copyright 2026"
VIAddVersionKey "FileDescription" "Photo Transfer Installer"
VIAddVersionKey "FileVersion" "1.0.0"
VIAddVersionKey "ProductVersion" "1.0.0"

; Interface Settings
!define MUI_ABORTWARNING
!define MUI_ICON "icon.ico"
!define MUI_UNICON "icon.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "welcome.bmp"

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Language
!insertmacro MUI_LANGUAGE "English"

; Installer Section
Section "Install"
    SetOutPath "$INSTDIR"
    
    ; Main files
    File "photo_transfer.exe"
    File "photo_transfer_gui.exe"
    
    ; Qt DLLs (from windeployqt)
    File "*.dll"
    
    ; Qt plugins
    SetOutPath "$INSTDIR\platforms"
    File "platforms\*.dll"
    
    SetOutPath "$INSTDIR\styles"
    File "styles\*.dll"
    
    SetOutPath "$INSTDIR\imageformats"
    File "imageformats\*.dll"
    
    SetOutPath "$INSTDIR"
    
    ; Create shortcuts
    CreateDirectory "$SMPROGRAMS\Photo Transfer"
    CreateShortcut "$SMPROGRAMS\Photo Transfer\Photo Transfer.lnk" "$INSTDIR\photo_transfer_gui.exe"
    CreateShortcut "$SMPROGRAMS\Photo Transfer\Photo Transfer (CLI).lnk" "$INSTDIR\photo_transfer.exe"
    CreateShortcut "$SMPROGRAMS\Photo Transfer\Uninstall.lnk" "$INSTDIR\uninstall.exe"
    CreateShortcut "$DESKTOP\Photo Transfer.lnk" "$INSTDIR\photo_transfer_gui.exe"
    
    ; Registry entries
    WriteRegStr HKLM "Software\PhotoTransfer" "InstallDir" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PhotoTransfer" \
        "DisplayName" "Photo Transfer"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PhotoTransfer" \
        "UninstallString" "$INSTDIR\uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PhotoTransfer" \
        "DisplayIcon" "$INSTDIR\photo_transfer_gui.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PhotoTransfer" \
        "Publisher" "PhotoTransfer"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PhotoTransfer" \
        "DisplayVersion" "1.0.0"
    
    ; Calculate and write install size
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PhotoTransfer" \
        "EstimatedSize" "$0"
    
    ; Create uninstaller
    WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

; Uninstaller Section
Section "Uninstall"
    ; Remove files
    Delete "$INSTDIR\*.exe"
    Delete "$INSTDIR\*.dll"
    RMDir /r "$INSTDIR\platforms"
    RMDir /r "$INSTDIR\styles"
    RMDir /r "$INSTDIR\imageformats"
    
    ; Remove shortcuts
    Delete "$SMPROGRAMS\Photo Transfer\*.lnk"
    RMDir "$SMPROGRAMS\Photo Transfer"
    Delete "$DESKTOP\Photo Transfer.lnk"
    
    ; Remove registry entries
    DeleteRegKey HKLM "Software\PhotoTransfer"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\PhotoTransfer"
    
    ; Remove install directory
    RMDir "$INSTDIR"
SectionEnd
