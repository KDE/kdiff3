;KDiff3-NSIS configuration
;Based on Modern User Interface example files
;Apdapted for KDiff3 by Sebastien Fricker and Joachim Eibl
;Requires nsis_v2.19

!define KDIFF3_VERSION "0.9.98"
!define DIFF_EXT32_CLSID "{9F8528E4-AB20-456E-84E5-3CE69D8720F3}"
!define DIFF_EXT64_CLSID "{34471FFB-4002-438b-8952-E4588D0C0FE9}"

!ifdef KDIFF3_64BIT
  !define BITS 64
!else
  !define BITS 32
!endif

!define SetupFileName "KDiff3-${BITS}bit-Setup_${KDIFF3_VERSION}.exe"

;--------------------------------
;Include Modern UI

  !include "MUI.nsh"
  !include "x64.nsh"

;--------------------------------
;General

  ;Name and file
  Name "KDiff3"
  OutFile ${SetupFileName}

  ;Default installation folder
  !ifdef KDIFF3_64BIT
  ;SetRegView 64
  InstallDir "$PROGRAMFILES64\KDiff3"
  !else
  InstallDir "$PROGRAMFILES\KDiff3"
  !endif
  ;Get installation folder from registry if available
  InstallDirRegKey HKCU "Software\KDiff3" ""
  
  !addplugindir ".\nsisplugins"

;--------------------------------
;Variables

  Var MUI_TEMP
  Var STARTMENU_FOLDER
  Var DIFF_EXT_CLSID
  Var DIFF_EXT_ID
  Var DIFF_EXT_DLL
  Var DIFF_EXT_OLD_DLL
  
;--------------------------------
;Interface Settings

  !define MUI_ABORTWARNING
  !define MUI_HEADERIMAGE
  !define MUI_HEADERIMAGE_BITMAP "kdiff3.bmp" ; optional

;--------------------------------
;Language Selection Dialog Settings

  ;Remember the installer language
  !define MUI_LANGDLL_REGISTRY_ROOT "HKCU" 
  !define MUI_LANGDLL_REGISTRY_KEY "Software\KDiff3" 
  !define MUI_LANGDLL_REGISTRY_VALUENAME "Installer Language"

;--------------------------------
;Pages

  ;!insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_LICENSE $(MUILicense)
  !insertmacro MUI_PAGE_COMPONENTS
  !insertmacro MUI_PAGE_DIRECTORY
  Page custom CustomPageC
  
  ;Start Menu Folder Page Configuration
  !define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKCU" 
  !define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\KDiff3" 
  !define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"
  
  !insertmacro MUI_PAGE_STARTMENU Application $STARTMENU_FOLDER
  
  !insertmacro MUI_PAGE_INSTFILES
  
  !define MUI_FINISHPAGE_RUN KDiff3.exe
  !define MUI_FINISHPAGE_RUN_NOTCHECKED
  !define MUI_FINISHPAGE_SHOWREADME README_WIN.txt
  !define MUI_FINISHPAGE_SHOWREADME_CHECKED

  !insertmacro MUI_PAGE_FINISH
  
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES

;--------------------------------
;Languages

  !insertmacro MUI_LANGUAGE "English" # first language is the default language
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
  !insertmacro MUI_LANGUAGE "Swedish"
  !insertmacro MUI_LANGUAGE "Norwegian"
  !insertmacro MUI_LANGUAGE "Finnish"
  !insertmacro MUI_LANGUAGE "Greek"
  !insertmacro MUI_LANGUAGE "Russian"
  !insertmacro MUI_LANGUAGE "Portuguese"
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
  !insertmacro MUI_LANGUAGE "Latvian"
  !insertmacro MUI_LANGUAGE "Macedonian"
  !insertmacro MUI_LANGUAGE "Estonian"
  !insertmacro MUI_LANGUAGE "Turkish"
  !insertmacro MUI_LANGUAGE "Lithuanian"
  !insertmacro MUI_LANGUAGE "Catalan"
  !insertmacro MUI_LANGUAGE "Slovenian"
  !insertmacro MUI_LANGUAGE "Serbian"
  !insertmacro MUI_LANGUAGE "SerbianLatin"
  !insertmacro MUI_LANGUAGE "Arabic"
  !insertmacro MUI_LANGUAGE "Farsi"
  !insertmacro MUI_LANGUAGE "Hebrew"
  !insertmacro MUI_LANGUAGE "Indonesian"
  !insertmacro MUI_LANGUAGE "Mongolian"
  !insertmacro MUI_LANGUAGE "Luxembourgish"
  !insertmacro MUI_LANGUAGE "Albanian"
  !insertmacro MUI_LANGUAGE "Breton"
  !insertmacro MUI_LANGUAGE "Belarusian"
  !insertmacro MUI_LANGUAGE "Icelandic"
  !insertmacro MUI_LANGUAGE "Malay"
  !insertmacro MUI_LANGUAGE "Bosnian"
  !insertmacro MUI_LANGUAGE "Kurdish"

;--------------------------------
;License Language String

  LicenseLangString MUILicense ${LANG_ENGLISH} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_FRENCH} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_GERMAN} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_SPANISH} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_SIMPCHINESE} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_TRADCHINESE} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_JAPANESE} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_KOREAN} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_ITALIAN} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_DUTCH} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_DANISH} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_SWEDISH} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_NORWEGIAN} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_FINNISH} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_GREEK} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_RUSSIAN} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_PORTUGUESE} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_PORTUGUESEBR} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_POLISH} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_UKRAINIAN} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_CZECH} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_SLOVAK} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_CROATIAN} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_BULGARIAN} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_HUNGARIAN} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_THAI} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_ROMANIAN} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_LATVIAN} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_MACEDONIAN} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_ESTONIAN} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_TURKISH} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_LITHUANIAN} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_CATALAN} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_SLOVENIAN} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_SERBIAN} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_SERBIANLATIN} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_ARABIC} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_FARSI} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_HEBREW} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_INDONESIAN} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_MONGOLIAN} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_LUXEMBOURGISH} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_ALBANIAN} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_BRETON} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_BELARUSIAN} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_ICELANDIC} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_MALAY} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_BOSNIAN} "COPYING.txt"
  LicenseLangString MUILicense ${LANG_KURDISH} "COPYING.txt"

;--------------------------------
;Reserve Files
  
  ;These files should be inserted before other files in the data block
  ;Keep these lines before any File command
  ;Only for solid compression (by default, solid compression is enabled for BZIP2 and LZMA)
  
  !insertmacro MUI_RESERVEFILE_LANGDLL
  ReserveFile "installForAllUsersPage.ini"
  !insertmacro MUI_RESERVEFILE_INSTALLOPTIONS

;--------------------------------
;Variables

  Var INSTALL_FOR_ALL_USERS
  
;--------------------------------
;Installer Sections

Section "Software" SecSoftware
SectionIn RO
  ;Read a value from an InstallOptions INI file
  !insertmacro MUI_INSTALLOPTIONS_READ $INSTALL_FOR_ALL_USERS "installForAllUsersPage.ini" "Field 2" "State"
  
  ;Set ShellVarContext: Defines if SHCTX points to HKLM or HKCU
  StrCmp $INSTALL_FOR_ALL_USERS "0" "" +3
    SetShellVarContext current
    Goto +2
    SetShellVarContext all    

  WriteRegStr HKCU "Software\KDiff3" "InstalledForAllUsers" "$INSTALL_FOR_ALL_USERS"

  ; Make the KDiff3 uninstaller visible via "System Settings: Add or Remove Programs", (Systemsteuerung/Software)
  WriteRegStr SHCTX "Software\KDiff3" "" "$INSTDIR"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\KDiff3" "DisplayName" "KDiff3 (remove only)"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\KDiff3" "UninstallString" '"$INSTDIR\Uninstall.exe"'


  SetOutPath "$INSTDIR"
  
  ;ADD YOUR OWN FILES HERE...
    DetailPrint "Writing files"
    File  "${BITS}bit\kdiff3.exe"
    File  "${BITS}bit\kdiff3.exe.manifest"
    File  "${BITS}bit\qt.conf"
    File "COPYING.txt"
    File "Readme_Win.txt"
    File "ChangeLog.txt"
    SetOutPath "$INSTDIR\bin"
    File /r "${BITS}bit\bin\*.*"
    SetOutPath "$INSTDIR"
    Delete "$INSTDIR\kdiff3-QT4.exe"
  
  ;Store installation folder
  WriteRegStr HKCU "Software\KDiff3" "" $INSTDIR
  
  ;Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
    
    ;Create shortcuts
    CreateDirectory "$SMPROGRAMS\$STARTMENU_FOLDER"
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\KDiff3.lnk" "$INSTDIR\kdiff3.exe"
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Readme.lnk" "$INSTDIR\Readme_Win.txt"
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\GPL.lnk"    "$INSTDIR\Copying.txt"
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
    CreateShortCut "$QUICKLAUNCH\KDiff3.lnk" "$INSTDIR\kdiff3.exe"     
  
  !insertmacro MUI_STARTMENU_WRITE_END

SectionEnd

Section "Documentation" SecDocumentation
    DetailPrint "Writing the documentation"
    SetOutPath "$INSTDIR"
    File /r doc
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Documentation.lnk" "$INSTDIR\doc\index.html"
SectionEnd

Section "Translations" SecTranslations
    DetailPrint "Writing the translation messages"
    SetOutPath "$INSTDIR"
    File /r translations
SectionEnd

Section "Utilities" SecUtilities
    DetailPrint "Writing the command line utilities (GNU sed, diff, diff3, etc.)"
    SetOutPath "$INSTDIR\bin"
    File /r "bin\*.*"
SectionEnd

SubSection "Integration" SecIntegration

Section "Explorer" SecIntegrationExplorer
  DetailPrint "Integration to Explorer"
;  WriteRegStr HKCR "Directory\shell\KDiff3" "" '&KDiff3'
;  WriteRegStr HKCR "Directory\shell\KDiff3\command" "" '"$INSTDIR\kdiff3.exe" "%1"'
    CreateShortCut "$SENDTO\KDiff3.lnk" '"$INSTDIR\kdiff3.exe"'
SectionEnd

Section "Diff-Ext" SecIntegrationDiffExtForKDiff3
  DetailPrint "Diff-Ext for KDiff3"
  SetOutPath "$INSTDIR"
${If} ${RunningX64}
  StrCpy $DIFF_EXT_CLSID ${DIFF_EXT64_CLSID}
  StrCpy $DIFF_EXT_DLL "diff_ext_for_kdiff3_64.dll"
  StrCpy $DIFF_EXT_OLD_DLL "diff_ext_for_kdiff3_64_old.dll"
  StrCpy $DIFF_EXT_ID "diff-ext-for-kdiff3-64"
  IfFileExists "$INSTDIR\$DIFF_EXT_OLD_DLL" 0 +2
     Delete "$INSTDIR\$DIFF_EXT_OLD_DLL"

  IfFileExists "$INSTDIR\$DIFF_EXT_DLL" 0 +2
     Rename "$INSTDIR\$DIFF_EXT_DLL" "$INSTDIR\$DIFF_EXT_OLD_DLL"
  File "64bit\diff_ext_for_kdiff3_64.dll"

  SetRegView 64

  WriteRegStr SHCTX "Software\Classes\CLSID\$DIFF_EXT_CLSID"                ""  "$DIFF_EXT_ID"
  WriteRegStr SHCTX "Software\Classes\CLSID\$DIFF_EXT_CLSID\InProcServer32" ""  "$INSTDIR\$DIFF_EXT_DLL"
  WriteRegStr SHCTX "Software\Classes\CLSID\$DIFF_EXT_CLSID\InProcServer32" "ThreadingModel" "Apartment"
  WriteRegStr SHCTX "Software\Classes\*\shellex\ContextMenuHandlers\$DIFF_EXT_ID" "" "$DIFF_EXT_CLSID"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved" "$DIFF_EXT_CLSID" "$DIFF_EXT_ID"
  WriteRegStr SHCTX "Software\Classes\Folder\shellex\ContextMenuHandlers\$DIFF_EXT_ID" "" "$DIFF_EXT_CLSID"
  WriteRegStr SHCTX "Software\Classes\Directory\shellex\ContextMenuHandlers\$DIFF_EXT_ID" "" "$DIFF_EXT_CLSID"

  SetRegView 32

${EndIf}
  StrCpy $DIFF_EXT_CLSID ${DIFF_EXT32_CLSID}
  StrCpy $DIFF_EXT_DLL "diff_ext_for_kdiff3.dll"
  StrCpy $DIFF_EXT_OLD_DLL "diff_ext_for_kdiff3_old.dll"
  StrCpy $DIFF_EXT_ID "diff-ext-for-kdiff3"

  IfFileExists "$INSTDIR\$DIFF_EXT_OLD_DLL" 0 +2
     Delete "$INSTDIR\$DIFF_EXT_OLD_DLL"
     
  IfFileExists "$INSTDIR\$DIFF_EXT_DLL" 0 +2
     Rename "$INSTDIR\$DIFF_EXT_DLL" "$INSTDIR\$DIFF_EXT_OLD_DLL"

  File "32bit\diff_ext_for_kdiff3.dll"

  SetRegView 64

  WriteRegStr HKCU  "Software\KDiff3\diff-ext" "" ""
  WriteRegStr SHCTX "Software\KDiff3\diff-ext" "InstallDir" "$INSTDIR"
  WriteRegStr SHCTX "Software\KDiff3\diff-ext" "diffcommand" "$INSTDIR\kdiff3.exe"

  SetRegView 32

  WriteRegStr SHCTX "Software\Classes\CLSID\$DIFF_EXT_CLSID"                ""  "$DIFF_EXT_ID"
  WriteRegStr SHCTX "Software\Classes\CLSID\$DIFF_EXT_CLSID\InProcServer32" ""  "$INSTDIR\$DIFF_EXT_DLL"
  WriteRegStr SHCTX "Software\Classes\CLSID\$DIFF_EXT_CLSID\InProcServer32" "ThreadingModel" "Apartment"
  WriteRegStr SHCTX "Software\Classes\*\shellex\ContextMenuHandlers\$DIFF_EXT_ID" "" "$DIFF_EXT_CLSID"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved" "$DIFF_EXT_CLSID" "$DIFF_EXT_ID"
  WriteRegStr SHCTX "Software\Classes\Folder\shellex\ContextMenuHandlers\$DIFF_EXT_ID" "" "$DIFF_EXT_CLSID"
  WriteRegStr SHCTX "Software\Classes\Directory\shellex\ContextMenuHandlers\$DIFF_EXT_ID" "" "$DIFF_EXT_CLSID"


  File "DIFF-EXT-LICENSE.txt"  
  CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Diff-Ext License.lnk"    "$INSTDIR\DIFF-EXT-LICENSE.txt"
  
SectionEnd

Section "WinCVS" SecIntegrationWinCVS
  DetailPrint "Integration to WinCVS"
  #MessageBox  MB_OK "If WinCVS is running, please close it before proceeding."
  WriteRegStr HKCU "Software\WinCvs\wincvs\CVS settings" "P_Extdiff" '$INSTDIR\kdiff3.exe'
  WriteRegBin HKCU "Software\WinCvs\wincvs\CVS settings" "P_DiffUseExtDiff" 01
SectionEnd

Section "TortoiseSVN" SecIntegrationTortoiseSVN
  DetailPrint "Integration to TortoiseSVN"
  WriteRegStr HKCU "Software\TortoiseSVN\" "Diff" '$INSTDIR\kdiff3.exe %base %mine  --L1 Base --L2 Mine'
  WriteRegStr HKCU "Software\TortoiseSVN\" "Merge" '$INSTDIR\kdiff3.exe %base %mine %theirs -o %merged --L1 Base --L2 Mine --L3 Theirs'
SectionEnd

Section /o "SVN Merge tool" SecIntegrationSubversionDiff3Cmd
  DetailPrint "Integrate diff3_cmd.bat for Subversion"
  File "diff3_cmd.bat"
  CreateDirectory '$APPDATA\Subversion'
  CopyFiles '$INSTDIR\diff3_cmd.bat' '$APPDATA\Subversion'
SectionEnd


Section /o "ClearCase" SecIntegrationClearCase
  DetailPrint "Integrate with Rational ClearCase from IBM"
  ccInstallHelper::nsisPlugin "install" "$INSTDIR\kdiff3.exe"

  ;File "ccInstHelper.exe"
  ;ExecWait '"$INSTDIR\ccInstHelper.exe" install "$INSTDIR\kdiff3.exe"'
SectionEnd

SubSectionEnd

;--------------------------------
;Installer Functions

Function .onInit

  !insertmacro MUI_LANGDLL_DISPLAY
  !insertmacro MUI_INSTALLOPTIONS_EXTRACT "installForAllUsersPage.ini"

FunctionEnd

Function CustomPageC

  !insertmacro MUI_HEADER_TEXT "$(TEXT_IO_TITLE)" "$(TEXT_IO_SUBTITLE)"
  !insertmacro MUI_INSTALLOPTIONS_DISPLAY "installForAllUsersPage.ini"

FunctionEnd


;--------------------------------
;Descriptions

  ;USE A LANGUAGE STRING IF YOU WANT YOUR DESCRIPTIONS TO BE LANGUAGE SPECIFIC

  ;Assign descriptions to sections
  !insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecSoftware} "Main program."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecDocumentation} "English documentation in HTML-format (Docs for other languages are available on the homepage.)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecTranslations}  "Translations for visible strings in many languages. Not needed for US-English."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecUtilities}  "Command Line Utilities: GNU sed, diff, diff3, etc. precompiled for Windows"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecIntegration}   "Integrate KDiff3 with certain programs. (See also the Readme for details.)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecIntegrationExplorer}  "Integrate KDiff3 with Explorer. Adds an entry for KDiff3 in the Send-To context menu."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecIntegrationDiffExtForKDiff3}  "Installs Diff-Ext by Sergey Zorin. Adds entries for KDiff3 in Explorer context menu."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecIntegrationWinCVS}  "Integrate KDiff3 with WinCVS. (Please close WinCVS before proceeding.)"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecIntegrationTortoiseSVN}  "Integrate KDiff3 with TortoiseSVN."
    !insertmacro MUI_DESCRIPTION_TEXT ${SecIntegrationSubversionDiff3Cmd}  "Install diff3_cmd.bat for Subversion merge"
    !insertmacro MUI_DESCRIPTION_TEXT ${SecIntegrationClearCase}  "Integrate KDiff3 with Rational Clearcase from IBM"
  !insertmacro MUI_FUNCTION_DESCRIPTION_END

 
;--------------------------------
;Uninstaller Section

Section "Uninstall"
  ReadRegStr $INSTALL_FOR_ALL_USERS HKCU "Software\KDiff3" "InstalledForAllUsers"
  ;Set ShellVarContext: Defines if SHCTX points to HKLM or HKCU
  StrCmp $INSTALL_FOR_ALL_USERS "0" "" +3
    SetShellVarContext current
    Goto +2
    SetShellVarContext all
	
  Delete "$INSTDIR\Uninstall.exe"
  Delete "$INSTDIR\kdiff3.exe"
  Delete "$INSTDIR\kdiff3.exe.manifest"
  Delete "$INSTDIR\qt.conf"
  Delete "$INSTDIR\COPYING.txt"
  Delete "$INSTDIR\Readme_Win.txt"
  Delete "$INSTDIR\ChangeLog.txt"
  Delete "$INSTDIR\diff_ext_for_kdiff3.dll"
  Delete "$INSTDIR\diff_ext_for_kdiff3_old.dll"
  Delete "$INSTDIR\diff_ext_for_kdiff3_64.dll"
  Delete "$INSTDIR\diff_ext_for_kdiff3_64_old.dll"
  Delete "$INSTDIR\DIFF-EXT-LICENSE.txt"

  RMDir /r "$INSTDIR\doc"
  RMDir /r "$INSTDIR\translations"
  RMDir /r "$INSTDIR\bin"
  RMDir "$INSTDIR"                   # without /r the dir is only removed if completely empty
  
  !insertmacro MUI_STARTMENU_GETFOLDER Application $MUI_TEMP
    
  Delete "$SMPROGRAMS\$MUI_TEMP\Uninstall.lnk"
  Delete "$SMPROGRAMS\$MUI_TEMP\KDiff3.lnk"
  Delete "$SMPROGRAMS\$MUI_TEMP\KDiff3-Qt4.lnk"
  Delete "$SMPROGRAMS\$MUI_TEMP\Readme.lnk"
  Delete "$SMPROGRAMS\$MUI_TEMP\GPL.lnk"
  Delete "$SMPROGRAMS\$MUI_TEMP\Diff-Ext License.lnk"

  Delete "$SMPROGRAMS\$MUI_TEMP\Documentation.lnk"
  Delete "$QUICKLAUNCH\KDiff3.lnk"
  Delete "$SENDTO\KDiff3.lnk"
  
  ;Delete empty start menu parent diretories
  StrCpy $MUI_TEMP "$SMPROGRAMS\$MUI_TEMP"
 
  startMenuDeleteLoop:
	ClearErrors
    RMDir $MUI_TEMP
    GetFullPathName $MUI_TEMP "$MUI_TEMP\.."
    
    IfErrors startMenuDeleteLoopDone
  
    StrCmp $MUI_TEMP $SMPROGRAMS startMenuDeleteLoopDone startMenuDeleteLoop
  startMenuDeleteLoopDone:
  
  DeleteRegKey HKCU  "Software\KDiff3"
  DeleteRegKey SHCTX "Software\KDiff3"
  DeleteRegKey SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\KDiff3"

  ; diff_ext_for_kdiff3
${If} ${RunningX64}
  StrCpy $DIFF_EXT_CLSID ${DIFF_EXT64_CLSID}
  StrCpy $DIFF_EXT_ID "diff-ext-for-kdiff3-64"
  SetRegView 64
  DeleteRegKey SHCTX "Software\Classes\CLSID\$DIFF_EXT_CLSID"
  DeleteRegKey SHCTX "Software\Classes\*\shellex\ContextMenuHandlers\$DIFF_EXT_ID"
  DeleteRegKey SHCTX "Software\Classes\Folder\shellex\ContextMenuHandlers\$DIFF_EXT_ID"
  DeleteRegKey SHCTX "Software\Classes\Directory\shellex\ContextMenuHandlers\$DIFF_EXT_ID"
  DeleteRegValue SHCTX "Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved" "$DIFF_EXT_CLSID"
  SetRegView 32
${EndIf}

  StrCpy $DIFF_EXT_CLSID ${DIFF_EXT32_CLSID}
  StrCpy $DIFF_EXT_ID "diff-ext-for-kdiff3"
  DeleteRegKey SHCTX "Software\Classes\CLSID\$DIFF_EXT_CLSID"
  DeleteRegKey SHCTX "Software\Classes\*\shellex\ContextMenuHandlers\$DIFF_EXT_ID"
  DeleteRegKey SHCTX "Software\Classes\Folder\shellex\ContextMenuHandlers\$DIFF_EXT_ID"
  DeleteRegKey SHCTX "Software\Classes\Directory\shellex\ContextMenuHandlers\$DIFF_EXT_ID"
  DeleteRegValue SHCTX "Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved" "$DIFF_EXT_CLSID"

  ; clearcase
  ccInstallHelper::nsisPlugin "uninstall" "$INSTDIR\kdiff3.exe"
  ;ExecWait '"$INSTDIR\ccInstHelper.exe" uninstall "$INSTDIR\kdiff3.exe"'
  ;Delete "$INSTDIR\ccInstHelper.exe"

SectionEnd

;--------------------------------
;Uninstaller Functions

Function un.onInit

  !insertmacro MUI_UNGETLANGUAGE
  
FunctionEnd
