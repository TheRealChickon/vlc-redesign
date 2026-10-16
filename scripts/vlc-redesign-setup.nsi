; VLC Redesign per-user installer (unofficial). No admin rights, no file associations,
; never touches an installed VLC. Settings stay in the app's own portable\ folder.
; Build: makensis -DSTAGE=<extracted VLC-Redesign dir> -DOUTFILE=<setup.exe> vlc-redesign-setup.nsi
Unicode true
SetCompressor /SOLID lzma
RequestExecutionLevel user
!include "MUI2.nsh"

!define APPNAME "VLC Redesign"
!define VERSION "2026.10.15-rc1"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\VLCRedesign"

Name "${APPNAME} ${VERSION} (unofficial)"
OutFile "${OUTFILE}"
InstallDir "$LOCALAPPDATA\Programs\${APPNAME}"
InstallDirRegKey HKCU "${UNINSTKEY}" "InstallLocation"
ShowInstDetails nevershow
BrandingText "Unofficial build - not affiliated with VideoLAN or Juxtopposed"

!define MUI_ICON "${STAGE}\vlc.ico"
!define MUI_UNICON "${STAGE}\vlc.ico"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TEXT "This installs ${APPNAME} ${VERSION}, an unofficial build of VLC with the redesign by Juxtopposed.$\r$\n$\r$\nIt installs for your account only, needs no administrator rights and does not change an existing VLC installation or your file associations."
!define MUI_FINISHPAGE_RUN "$INSTDIR\vlc.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Open ${APPNAME}"
!define MUI_FINISHPAGE_SHOWREADME ""
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Create a desktop shortcut"
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION CreateDesktopShortcut
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${STAGE}\COPYING.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function CreateDesktopShortcut
  CreateShortCut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\vlc.exe" "" "$INSTDIR\vlc.ico"
FunctionEnd

Section "Install"
  SetOutPath "$INSTDIR"
  File /r "${STAGE}\*.*"
  ; keep VLC in portable mode so its settings never mix with an installed VLC
  CreateDirectory "$INSTDIR\portable"
  CreateDirectory "$SMPROGRAMS\${APPNAME}"
  CreateShortCut "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk" "$INSTDIR\vlc.exe" "" "$INSTDIR\vlc.ico"
  CreateShortCut "$SMPROGRAMS\${APPNAME}\Uninstall ${APPNAME}.lnk" "$INSTDIR\Uninstall.exe"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "${UNINSTKEY}" "DisplayName" "${APPNAME} (unofficial)"
  WriteRegStr HKCU "${UNINSTKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${UNINSTKEY}" "Publisher" "TheRealChickon (unofficial VLC build)"
  WriteRegStr HKCU "${UNINSTKEY}" "DisplayIcon" "$INSTDIR\vlc.ico"
  WriteRegStr HKCU "${UNINSTKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKCU "${UNINSTKEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegStr HKCU "${UNINSTKEY}" "URLInfoAbout" "https://github.com/TheRealChickon/vlc-redesign"
  WriteRegDWORD HKCU "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTKEY}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  ; removes the app files, its shortcuts and its own uninstall key; the portable\ folder
  ; (settings and library) is kept so reinstalling restores them
  Delete "$DESKTOP\${APPNAME}.lnk"
  RMDir /r "$SMPROGRAMS\${APPNAME}"
  FindFirst $0 $1 "$INSTDIR\*.*"
  loop:
    StrCmp $1 "" done
    StrCmp $1 "." next
    StrCmp $1 ".." next
    StrCmp $1 "portable" next
    IfFileExists "$INSTDIR\$1\*.*" 0 +3
      RMDir /r "$INSTDIR\$1"
      Goto next
    Delete "$INSTDIR\$1"
  next:
    FindNext $0 $1
    Goto loop
  done:
  FindClose $0
  RMDir "$INSTDIR\portable"  ; only removed if it is empty
  RMDir "$INSTDIR"
  DeleteRegKey HKCU "${UNINSTKEY}"
SectionEnd
