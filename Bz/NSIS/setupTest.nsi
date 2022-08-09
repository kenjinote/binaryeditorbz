; Script generated by the HM NIS Edit Script Wizard.

!include Library.nsh
!include x64.nsh

RequestExecutionLevel admin

; HM NIS Edit Wizard helper defines
!define PRODUCT_NAME "BzEditor"
!define PRODUCT_VERSION "0.0.0.1"
!define PRODUCT_PUBLISHER "tamachan"
!define PRODUCT_WEB_SITE "https://github.com/devil-tamachan/binaryeditorbz"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\${PRODUCT_NAME}"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"


;--------------------------------
;Language Selection Dialog Settings

  ;Remember the installer language
  !define MUI_LANGDLL_REGISTRY_ROOT "HKCU" 
  !define MUI_LANGDLL_REGISTRY_KEY "Software\c.mos\BZ\MUI2" 
  !define MUI_LANGDLL_REGISTRY_VALUENAME "Installer Language"


; MUI 1.67 compatible ------
!include "MUI.nsh"

; MUI Settings
!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\classic-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\classic-uninstall.ico"

; Welcome page
!insertmacro MUI_PAGE_WELCOME
; License page
;!insertmacro MUI_PAGE_LICENSE "${NSISDIR}\License.txt"
; Directory page
!insertmacro MUI_PAGE_DIRECTORY
; Instfiles page
!insertmacro MUI_PAGE_INSTFILES
; Finish page
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Language files
  !insertmacro MUI_LANGUAGE "English" ;first language is the default language
  !insertmacro MUI_LANGUAGE "French"
  !insertmacro MUI_LANGUAGE "German"
  !insertmacro MUI_LANGUAGE "Spanish"
  !insertmacro MUI_LANGUAGE "SpanishInternational"
  !insertmacro MUI_LANGUAGE "SimpChinese"
  !insertmacro MUI_LANGUAGE "TradChinese"
  !insertmacro MUI_LANGUAGE "Japanese"
  !insertmacro MUI_LANGUAGE "Korean"
  !insertmacro MUI_LANGUAGE "Italian"
  !insertmacro MUI_LANGUAGE "Dutch"
  !insertmacro MUI_LANGUAGE "Danish"
  !insertmacro MUI_LANGUAGE "Swedish"
  !insertmacro MUI_LANGUAGE "Norwegian"
  !insertmacro MUI_LANGUAGE "NorwegianNynorsk"
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
  !insertmacro MUI_LANGUAGE "Irish"
  !insertmacro MUI_LANGUAGE "Uzbek"
  !insertmacro MUI_LANGUAGE "Galician"
  !insertmacro MUI_LANGUAGE "Afrikaans"
  !insertmacro MUI_LANGUAGE "Catalan"
  !insertmacro MUI_LANGUAGE "Esperanto"

; MUI end ------

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
; OutFile "${PRODUCT_NAME}Test-${PRODUCT_VERSION}.exe"
!ifdef DngrDir
 InstallDir ${DngrDir}
!else ifdef DngrUnDir
 InstallDir ${DngrUnDir}
!else
 InstallDir "$PROGRAMFILES\BzEditor"
!endif
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show

Section "MainSection" SEC01
  SetOutPath "$INSTDIR"
  SetOverwrite on
  CreateDirectory "$SMPROGRAMS\BzEditor"
;  CreateDirectory "$INSTDIR\htm"
  File "..\build\Release\Bz.exe"
  File "..\build64\Release\Bz64.exe"
  CopyFiles /SILENT $INSTDIR\Bz.exe $INSTDIR\BzAdmin.exe
  CopyFiles /SILENT $INSTDIR\Bz64.exe $INSTDIR\Bz64Admin.exe
  File "..\Bz.exe.manifest"
  File "..\Bz64.exe.manifest"
  File "..\BzAdmin.exe.manifest"
  File "..\Bz64Admin.exe.manifest"
;  File "..\chm\Bz.chm"
;  File "..\index.htm"
  File "data\Bz.txt"
  File "..\..\SPECIALTHANKS.txt"
  File "..\build\Release\Bzres_us.dll"
  File "..\build64\Release\Bzres_us64.dll"
;  SetOutPath "$INSTDIR\htm"
;  File "..\htm\basic.htm"
;  File "..\htm\bmpview.htm"
;  File "..\htm\filemap.htm"
;  File "..\htm\findbox.htm"
;  File "..\htm\high.htm"
;  File "..\htm\inspector.htm"
;  File "..\htm\intro.htm"
;  File "..\htm\license.htm"
;  File "..\htm\list.htm"
;  File "..\htm\splitview.htm"
;  File "..\htm\tips.htm"
;  File "..\htm\zlibAnalyzer.htm"
  SetOutPath "$APPDATA\BzEditor"
  SetOverwrite off
  File "data\Bz.def"
  SetOverwrite on
  SetOutPath "$INSTDIR"

SectionEnd

Section -AdditionalIcons
  CreateDirectory "$SMPROGRAMS\BzEditor"
${If} ${RunningX64}
  CreateShortCut "$SMPROGRAMS\BzEditor\Bz.lnk" "$INSTDIR\Bz64.exe"
  CreateShortCut "$SMPROGRAMS\BzEditor\Bz (Admin).lnk" "$INSTDIR\Bz64Admin.exe"
  CreateShortCut "$SENDTO\Bz.lnk" "$INSTDIR\Bz64.exe"
  CreateShortCut "$SENDTO\Bz (Admin).lnk" "$INSTDIR\Bz64Admin.exe"
${Else}
  CreateShortCut "$SMPROGRAMS\BzEditor\Bz.lnk" "$INSTDIR\Bz.exe"
  CreateShortCut "$SMPROGRAMS\BzEditor\Bz (Admin).lnk" "$INSTDIR\BzAdmin.exe"
  CreateShortCut "$SENDTO\Bz.lnk" "$INSTDIR\Bz.exe"
  CreateShortCut "$SENDTO\Bz (Admin).lnk" "$INSTDIR\BzAdmin.exe"
${Endif}
  CreateShortCut "$SMPROGRAMS\BzEditor\Uninstall.lnk" "$INSTDIR\uninst.exe"
SectionEnd

Section -Post
  WriteUninstaller "$INSTDIR\uninst.exe"
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\Bz.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\Bz.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
SectionEnd


Function un.onUninstSuccess
  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK "$(MUI_UNTEXT_FINISH_SUBTITLE)"
FunctionEnd


Function .onInit
  SetRegView 32
  StrCpy $INSTDIR "$PROGRAMFILES\BzEditor"

  !insertmacro MUI_LANGDLL_DISPLAY

;  ReadRegStr $R0 HKLM \
;  "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" \
;  "UninstallString"
;  StrCmp $R0 "" done
 
;  MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION \
;  "${PRODUCT_NAME}�͊��ɃC���X�g�[������Ă��܂��B$\n$\nOK�{�^�����N���b�N����ƑO�̃o�[�W�������ɍ폜���܂��BCancel�{�^�����N���b�N����Ƃ��̂܂܏㏑�����܂��B" \
;  IDOK uninst
;  Abort
  
;Run the uninstaller
;uninst:
;  ClearErrors
;  ExecWait '$R0 _?=$INSTDIR' ;Do not copy the uninstaller to a temp file
 
;  IfErrors no_remove_uninstaller
    ;You can either use Delete /REBOOTOK in the uninstaller or add some code
    ;here to remove the uninstaller. Use a registry key to check
    ;whether the user has chosen to uninstall. If you are using an uninstaller
    ;components page, make sure all sections are uninstalled.
;  no_remove_uninstaller:
  
done:
FunctionEnd

Function un.onInit

  !insertmacro MUI_UNGETLANGUAGE

;  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "$(^Name)�����S�ɍ폜���܂�����낵���ł����H" IDYES +2
;  Abort

  SetRegView 32
  StrCpy $INSTDIR "$PROGRAMFILES\BzEditor"
FunctionEnd

Section Uninstall
  Delete "$INSTDIR\uninst.exe"
  Delete "$INSTDIR\Bz.exe"
  Delete "$INSTDIR\Bz64.exe"
  Delete "$INSTDIR\BzAdmin.exe"
  Delete "$INSTDIR\Bz64Admin.exe"
  
  Delete "$INSTDIR\Bz.exe.manifest"
  Delete "$INSTDIR\Bz64.exe.manifest"
  Delete "$INSTDIR\BzAdmin.exe.manifest"
  Delete "$INSTDIR\Bz64Admin.exe.manifest"
  
;  Delete "$APPDATA\BzEditor\Bz.def"
  Delete "$INSTDIR\Bz.chm"
  Delete "$INSTDIR\index.htm"
  Delete "$INSTDIR\htm\basic.htm"
  Delete "$INSTDIR\htm\bmpview.htm"
  Delete "$INSTDIR\htm\filemap.htm"
  Delete "$INSTDIR\htm\findbox.htm"
  Delete "$INSTDIR\htm\high.htm"
  Delete "$INSTDIR\htm\inspector.htm"
  Delete "$INSTDIR\htm\intro.htm"
  Delete "$INSTDIR\htm\license.htm"
  Delete "$INSTDIR\htm\list.htm"
  Delete "$INSTDIR\htm\splitview.htm"
  Delete "$INSTDIR\htm\tips.htm"
  Delete "$INSTDIR\htm\zlibAnalyzer.htm"
  Delete "$INSTDIR\Bz.txt"
  Delete "$INSTDIR\SPECIALTHANKS.txt"
  Delete "$INSTDIR\Bzres_us.dll"
  Delete "$INSTDIR\Bzres_us64.dll"

  Delete "$SMPROGRAMS\BzEditor\Uninstall.lnk"
  Delete "$SENDTO\Bz.lnk"
  Delete "$SENDTO\Bz (Admin).lnk"

  RMDir "$INSTDIR\htm"
  RMDir "$SMPROGRAMS\BzEditor"
  RMDir "$INSTDIR"

  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  SetAutoClose true
SectionEnd