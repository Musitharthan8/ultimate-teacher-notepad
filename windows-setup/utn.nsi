; UTN installer, based on the Xournal++ Windows distribution layout (GPLv2+).
Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
Name "Ultimate Teacher Notepad ${UTN_VERSION}"
OutFile "${OUTPUT_INSTALLER_FILE}"
InstallDir "$LOCALAPPDATA\Programs\UTN"
InstallDirRegKey HKCU "Software\UTN" "InstallLocation"
RequestExecutionLevel user
!define MUI_ABORTWARNING
!define MUI_ICON "${ICON_FILE}"
!define MUI_UNICON "${ICON_FILE}"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${LICENSE_FILE}"
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE CheckInstallFolder
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\bin\UTN.exe"
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
Function .onInit
    ${IfNot} ${RunningX64}
        MessageBox MB_ICONSTOP "UTN requires 64-bit Windows."
        Abort
    ${EndIf}
    SetRegView 64
FunctionEnd
Function CheckInstallFolder
    ${If} ${FileExists} "$INSTDIR\*.*"
    ${AndIfNot} ${FileExists} "$INSTDIR\UTN-VERSION.txt"
        MessageBox MB_ICONSTOP "Choose an empty folder or an existing UTN installation. Close UTN before upgrading."
        Abort
    ${EndIf}
FunctionEnd
Section "Ultimate Teacher Notepad"
    Call CheckInstallFolder
    SetShellVarContext current
    SetOutPath "$INSTDIR"
    File /r "${SETUP_DIR}\*"
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    WriteRegStr HKCU "Software\UTN" "InstallLocation" "$INSTDIR"
    CreateDirectory "$SMPROGRAMS\Ultimate Teacher Notepad"
    CreateShortcut "$SMPROGRAMS\Ultimate Teacher Notepad\UTN.lnk" "$INSTDIR\bin\UTN.exe"
    CreateShortcut "$SMPROGRAMS\Ultimate Teacher Notepad\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
    CreateShortcut "$DESKTOP\UTN.lnk" "$INSTDIR\bin\UTN.exe"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\UTN" "DisplayName" "Ultimate Teacher Notepad"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\UTN" "DisplayVersion" "${UTN_VERSION}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\UTN" "Publisher" "UTN contributors"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\UTN" "DisplayIcon" "$INSTDIR\bin\UTN.exe"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\UTN" "InstallLocation" "$INSTDIR"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\UTN" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\UTN" "NoModify" 1
    WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\UTN" "NoRepair" 1
    ; Register Open With without taking over Xournal++ or the default PDF reader.
    WriteRegStr HKCU "Software\Classes\UTN.Document" "" "UTN lesson"
    WriteRegStr HKCU "Software\Classes\UTN.Document\DefaultIcon" "" '$\"$INSTDIR\bin\UTN.exe$\",0'
    WriteRegStr HKCU "Software\Classes\UTN.Document\shell\open\command" "" '$\"$INSTDIR\bin\UTN.exe$\" $\"%1$\"'
    WriteRegStr HKCU "Software\Classes\.xopp\OpenWithProgids" "UTN.Document" ""
    WriteRegStr HKCU "Software\Classes\.pdf\OpenWithProgids" "UTN.Document" ""
SectionEnd
Section "Uninstall"
    SetRegView 64
    SetShellVarContext current
    DeleteRegValue HKCU "Software\Classes\.xopp\OpenWithProgids" "UTN.Document"
    DeleteRegValue HKCU "Software\Classes\.pdf\OpenWithProgids" "UTN.Document"
    DeleteRegKey HKCU "Software\Classes\UTN.Document"
    DeleteRegKey HKCU "Software\UTN"
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\UTN"
    Delete "$DESKTOP\UTN.lnk"
    Delete "$SMPROGRAMS\Ultimate Teacher Notepad\UTN.lnk"
    Delete "$SMPROGRAMS\Ultimate Teacher Notepad\Uninstall.lnk"
    RMDir "$SMPROGRAMS\Ultimate Teacher Notepad"
    RMDir /r "$INSTDIR\bin"
    RMDir /r "$INSTDIR\lib"
    RMDir /r "$INSTDIR\share"
    RMDir /r "$INSTDIR\etc"
    Delete "$INSTDIR\LICENSE.txt"
    Delete "$INSTDIR\Xournalpp-AUTHORS.txt"
    Delete "$INSTDIR\README.txt"
    Delete "$INSTDIR\UTN-VERSION.txt"
    Delete "$INSTDIR\SOURCE-COMMIT.txt"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR"
    ; Documents and user configuration are retained.
SectionEnd
