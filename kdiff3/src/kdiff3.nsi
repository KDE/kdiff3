;NSIS Modern User Interface version 1.63
;MultiLanguage Example Script
;Written by Joost Verburg
!define MUI_PRODUCT "KDiff3" ;Define your own software name here
!include "version.nsi"
!ifndef MUI_VERSION
!define MUI_VERSION "" ;Define your own software version here
!endif
!ifndef QTDIR
!define QTDIR "f:\qt\3.1.2"
!endif
!ifndef WINDOWS_DIR
!define WINDOWS_DIR "c:\windows"
!endif
!include "MUI.nsh"



;--------------------------------
;Configuration
    InstallDir "$PROGRAMFILES\KDiff3"
    InstallDirRegKey HKEY_LOCAL_MACHINE "SOFTWARE\${MUI_PRODUCT}" ""
    ;DirShow show ; (make this hide to not let the user change it)
    ;DirText "Select the directory to install ${MUI_PRODUCT} in:"


  ;General
  OutFile "KDiff3Setup_${MUI_VERSION}.exe"
  setCompressor bzip2

  
  
  ;Remember the installer language
  !define MUI_LANGDLL_REGISTRY_ROOT "HKCU" 
  !define MUI_LANGDLL_REGISTRY_KEY "Software\${MUI_PRODUCT}" 
  !define MUI_LANGDLL_REGISTRY_VALUENAME "Installer Language"
  ;Remember the Start Menu Folder
  !define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKCU" 
  !define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\${MUI_PRODUCT}" 
  !define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "${MUI_PRODUCT}"
  !define TEMP $R0

;--------------------------------
;Modern UI Configuration

  !define MUI_LICENSEPAGE
  !define MUI_COMPONENTSPAGE
  !define MUI_DIRECTORYPAGE
  !define MUI_STARTMENUPAGE
  
  !define MUI_ABORTWARNING
  
  !define MUI_UNINSTALLER
  !define MUI_UNCONFIRMPAGE
  !define MUI_HEADERBITMAP "kdiff3.bmp"
;--------------------------------
;Languages

  !insertmacro MUI_LANGUAGE "English"
  !insertmacro MUI_LANGUAGE "French"
  !insertmacro MUI_LANGUAGE "German"
  !insertmacro MUI_LANGUAGE "Spanish"
  !insertmacro MUI_LANGUAGE "SimpChinese"
  !insertmacro MUI_LANGUAGE "TradChinese"    
  !insertmacro MUI_LANGUAGE "Japanese"
  !insertmacro MUI_LANGUAGE "Korean"
  !insertmacro MUI_LANGUAGE "Italian"
  !insertmacro MUI_LANGUAGE "Dutch"
  !insertmacro MUI_LANGUAGE "Danish"
  !insertmacro MUI_LANGUAGE "Greek"
  !insertmacro MUI_LANGUAGE "Russian"
  !insertmacro MUI_LANGUAGE "PortugueseBR"
  !insertmacro MUI_LANGUAGE "Polish"
  !insertmacro MUI_LANGUAGE "Ukrainian"
  !insertmacro MUI_LANGUAGE "Czech"
  !insertmacro MUI_LANGUAGE "Slovak"
  !insertmacro MUI_LANGUAGE "Croatian"
  !insertmacro MUI_LANGUAGE "Bulgarian"
  !insertmacro MUI_LANGUAGE "Hungarian"
  !insertmacro MUI_LANGUAGE "Thai"
  !insertmacro MUI_LANGUAGE "Romanian"
  !insertmacro MUI_LANGUAGE "Macedonian"
  !insertmacro MUI_LANGUAGE "Turkish"
  
;--------------------------------
;Language Strings
    
  ;Descriptions
  LangString DESC_SecCopyUI ${LANG_ENGLISH} "Software: English description"
  LangString DESC_SecCopyUI ${LANG_FRENCH} "Software: French description"
  LangString DESC_SecCopyUI ${LANG_GERMAN} "Software: German description"
  LangString DESC_SecCopyUI ${LANG_SPANISH} "Software: Spanish description"
  LangString DESC_SecCopyUI ${LANG_SIMPCHINESE} "Software: Simplified Chinese description"
  LangString DESC_SecCopyUI ${LANG_TRADCHINESE} "Software: Traditional Chinese description"
  LangString DESC_SecCopyUI ${LANG_JAPANESE} "Software: Japanese description"
  LangString DESC_SecCopyUI ${LANG_KOREAN} "Software: Korean description"
  LangString DESC_SecCopyUI ${LANG_ITALIAN} "Software: Italian description"
  LangString DESC_SecCopyUI ${LANG_DUTCH} "Software: Dutch description"
  LangString DESC_SecCopyUI ${LANG_DANISH} "Software: Danish description"
  LangString DESC_SecCopyUI ${LANG_GREEK} "Software: Greek description"
  LangString DESC_SecCopyUI ${LANG_RUSSIAN} "Software: Russian description"
  LangString DESC_SecCopyUI ${LANG_PORTUGUESEBR} "Software: Portuguese (Brasil) description"
  LangString DESC_SecCopyUI ${LANG_POLISH} "Software: Polish description"
  LangString DESC_SecCopyUI ${LANG_UKRAINIAN} "Software: Ukrainian description"
  LangString DESC_SecCopyUI ${LANG_CZECH} "Software: Czechian description"
  LangString DESC_SecCopyUI ${LANG_SLOVAK} "Software: Slovakian description"
  LangString DESC_SecCopyUI ${LANG_CROATIAN} "Software: Slovakian description"
  LangString DESC_SecCopyUI ${LANG_BULGARIAN} "Software: Bulgarian description"
  LangString DESC_SecCopyUI ${LANG_HUNGARIAN} "Software: Hungarian description"
  LangString DESC_SecCopyUI ${LANG_THAI} "Software: Thai description"
  LangString DESC_SecCopyUI ${LANG_ROMANIAN} "Software: Romanian description"
  LangString DESC_SecCopyUI ${LANG_MACEDONIAN} "Software: Macedonian description"
  LangString DESC_SecCopyUI ${LANG_TURKISH} "Software: Turkish description"
  
;--------------------------------
;Data
  
  LicenseData /LANG=${LANG_ENGLISH} "..\COPYING"
  LicenseData /LANG=${LANG_FRENCH} "..\COPYING"
  LicenseData /LANG=${LANG_GERMAN} "..\COPYING"
  LicenseData /LANG=${LANG_SPANISH} "..\COPYING"
  LicenseData /LANG=${LANG_SIMPCHINESE} "..\COPYING"
  LicenseData /LANG=${LANG_TRADCHINESE} "..\COPYING"
  LicenseData /LANG=${LANG_JAPANESE} "..\COPYING"
  LicenseData /LANG=${LANG_KOREAN} "..\COPYING"
  LicenseData /LANG=${LANG_ITALIAN} "..\COPYING"
  LicenseData /LANG=${LANG_DUTCH} "..\COPYING"
  LicenseData /LANG=${LANG_DANISH} "..\COPYING"
  LicenseData /LANG=${LANG_GREEK} "..\COPYING"
  LicenseData /LANG=${LANG_RUSSIAN} "..\COPYING"
  LicenseData /LANG=${LANG_PORTUGUESEBR} "..\COPYING"
  LicenseData /LANG=${LANG_POLISH} "..\COPYING"
  LicenseData /LANG=${LANG_UKRAINIAN} "..\COPYING"
  LicenseData /LANG=${LANG_CZECH} "..\COPYING"
  LicenseData /LANG=${LANG_SLOVAK} "..\COPYING"
  LicenseData /LANG=${LANG_CROATIAN} "..\COPYING"
  LicenseData /LANG=${LANG_BULGARIAN} "..\COPYING"
  LicenseData /LANG=${LANG_HUNGARIAN} "..\COPYING"
  LicenseData /LANG=${LANG_THAI} "..\COPYING"
  LicenseData /LANG=${LANG_ROMANIAN} "..\COPYING"
  LicenseData /LANG=${LANG_MACEDONIAN} "..\COPYING"
  LicenseData /LANG=${LANG_TURKISH} "..\COPYING"

;--------------------------------
;Reserve Files
  
  ;Things that need to be extracted on first (keep these lines before any File command!)
  ;Only useful for BZIP2 compression
  !insertmacro MUI_RESERVEFILE_LANGDLL
  
;--------------------------------
;Installer Sections
  
Section "Software" SecCopyUI 2
SectionIn 2 RO

    SetOutPath "$INSTDIR"
    ; add files / whatever that need to be installed here.
    WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\${MUI_PRODUCT}" "" "$INSTDIR"
    WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\${MUI_PRODUCT}" "DisplayName" "${MUI_PRODUCT} (remove only)"
    WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\${MUI_PRODUCT}" "UninstallString" '"$INSTDIR\uninst.exe"'
  DetailPrint "Writing files"
    File "kdiff3.exe"
    File "..\binaries\windows\diff.exe"
    File "..\COPYING"
    File "${WINDOWS_DIR}\system32\msvcp70.dll"
    File "${WINDOWS_DIR}\system32\msvcr70.dll"
    File "${QTDIR}\lib\qt-mt*.dll"
    ; File "*.qm"

  !insertmacro MUI_STARTMENU_WRITE_BEGIN
    
    DetailPrint "Creating shortcuts"
    CreateDirectory "$SMPROGRAMS\${MUI_STARTMENUPAGE_VARIABLE}"
    CreateShortCut "$SMPROGRAMS\${MUI_STARTMENUPAGE_VARIABLE}\KDiff3.lnk" "$INSTDIR\kdiff3.exe"
    CreateShortCut "$SMPROGRAMS\${MUI_STARTMENUPAGE_VARIABLE}\Uninstal.lnk" "$INSTDIR\uninst.exe"
  
  !insertmacro MUI_STARTMENU_WRITE_END
  
    
    ; write out uninstaller
    WriteUninstaller "$INSTDIR\uninst.exe"
    
    
  ;Store install folder
  WriteRegStr HKCU "Software\${MUI_PRODUCT}" "" $INSTDIR
    CreateShortCut "$QUICKLAUNCH\KDiff3.lnk" "$INSTDIR\kdiff3.exe"
     
SectionEnd
 
Section "Documentation"

    DetailPrint "Writing the documentation"
    SetOutPath "$INSTDIR"
    File /r tmp\kdiff3.sourceforge.net\doc
    SetOutPath "$INSTDIR"
    CreateShortCut "$SMPROGRAMS\${MUI_STARTMENUPAGE_VARIABLE}\Documentation.lnk" "$INSTDIR\doc\index.html"
    WriteRegStr HKCR "Application\kdiff3.exe\shell\open\command" "" '"$INSTDIR\kdiff3.exe" "%1"'    
SectionEnd

SubSection "Integration"
Section "WinCVS"
  DetailPrint "Integration to WinCVS"
  MessageBox  MB_OK "Please close WinCVS"
  WriteRegStr HKCU "Software\WinCvs\wincvs\CVS settings" "P_Extdiff" '$INSTDIR\kdiff3.exe'
  WriteRegBin HKCU "Software\WinCvs\wincvs\CVS settings" "P_DiffUseExtDiff" 01

SectionEnd 
Section "Explorer"
  DetailPrint "Integration to Explorer"
;  WriteRegStr HKCR "Directory\shell}\KDiff3" "" '&KDiff3'
;  WriteRegStr HKCR "Directory\shell\KDiff3\command" "" '"$INSTDIR\kdiff3.exe" "%1"'
    CreateShortCut "$SMPROGRAMS\..\..\SendTo\KDiff3.lnk" '"$INSTDIR\kdiff3.exe"'
SectionEnd 
SubSectionEnd
 
;Display the Finish header
;Insert this macro after the sections if you are not using a finish page
!insertmacro MUI_SECTIONS_FINISHHEADER

;--------------------------------
;Installer Functions

Function .onInit
;  SetOutPath $TEMP1
;  File /oname=spltmp.bmp "promotion.bmp"

; optional
; File /oname=spltmp.wav "my_splashshit.wav"

;  advsplash::show 5000 600 40 0 $TEMP1\spltmp

;  Pop $0 ; $0 has '1' if the user closed the splash screen early,
         ; '0' if everything closed normal, and '-1' if some error occured.

;  Delete $TEMP1\spltmp.bmp
;  Delete $TEMP1\spltmp.wav

  !insertmacro MUI_LANGDLL_DISPLAY

FunctionEnd

;--------------------------------
;Descriptions

!insertmacro MUI_FUNCTIONS_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecCopyUI} $(DESC_SecCopyUI)
!insertmacro MUI_FUNCTIONS_DESCRIPTION_END
 
;--------------------------------
;Uninstaller Section

Section "Uninstall"

; add delete commands to delete whatever files/registry keys/etc you installed here.
  ReadRegStr ${TEMP} "${MUI_STARTMENUPAGE_REGISTRY_ROOT}" "${MUI_STARTMENUPAGE_REGISTRY_KEY}" "${MUI_STARTMENUPAGE_REGISTRY_VALUENAME}"
  StrCmp ${TEMP} "" noshortcuts
  
    RMDir /r "$SMPROGRAMS\${TEMP}"
    
  noshortcuts:

  RMDir "$INSTDIR"
  Delete "$INSTDIR\uninst.exe"
    DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\${MUI_PRODUCT}"

  Delete "$QUICKLAUNCH\KDiff3.lnk"
    RMDir /r "$INSTDIR"
  DetailPrint "Integration to Explorer"
;  DeleteRegKey HKCR "Directory\shell\KDiff3\command"
;  DeleteRegKey HKCR "Directory\shell\KDiff3"
  Delete "$SMPROGRAMS\..\..\SendTo\KDiff3.lnk"

    !insertmacro MUI_UNFINISHHEADER

SectionEnd

;--------------------------------
;Uninstaller Functions

Function un.onInit

  ;Get language from registry
  ReadRegStr $LANGUAGE HKCU "Software\${MUI_PRODUCT}" "Installer Language"
  
FunctionEnd

