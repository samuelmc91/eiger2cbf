;eiger2cbf MSYS2 stand-alone installer
;  (C) Copyright 2020 Herbert J. Bernstein

!pragma warning error all

;--------------------------------
;Include Modern UI

  !include "MUI2.nsh"

;--------------------------------
;General

  ;Properly display all languages (Installer will not work on Windows 95, 98 or ME!)
  Unicode true

  ;Name and file
  Name "eiger2cbf_Windows_10"
  OutFile "eiger2cbf_Windows_10.exe"

  ;Default installation folder
  InstallDir "$PROGRAMFILES\eiger2cbf"
  
  ;Get installation folder from registry if available
  InstallDirRegKey HKCU "Software\eiger2cbf" ""

  ;Request application privileges for Windows Vista
  RequestExecutionLevel admin

;--------------------------------
;Interface Settings

  !define MUI_ABORTWARNING

  ;Show all languages, despite user's codepage
  !define MUI_LANGDLL_ALLLANGUAGES

;--------------------------------
;Language Selection Dialog Settings

  ;Remember the installer language
  !define MUI_LANGDLL_REGISTRY_ROOT "HKCU" 
  !define MUI_LANGDLL_REGISTRY_KEY "Software\eiger2cbf" 
  !define MUI_LANGDLL_REGISTRY_VALUENAME "Installer Language"

;--------------------------------
;Pages

  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_LICENSE ".\doc\LICENSES.txt"
  !insertmacro MUI_PAGE_COMPONENTS
  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES
  !insertmacro MUI_PAGE_FINISH
  
  !insertmacro MUI_UNPAGE_WELCOME
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_LICENSE ".\doc\LICENSES.txt"
  !insertmacro MUI_UNPAGE_COMPONENTS
  !insertmacro MUI_UNPAGE_DIRECTORY
  !insertmacro MUI_UNPAGE_INSTFILES
  !insertmacro MUI_UNPAGE_FINISH

;--------------------------------
;Languages

  !insertmacro MUI_LANGUAGE "English" ; The first language is the default language
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
  !insertmacro MUI_LANGUAGE "Asturian"
  !insertmacro MUI_LANGUAGE "Basque"
  !insertmacro MUI_LANGUAGE "Pashto"
  !insertmacro MUI_LANGUAGE "ScotsGaelic"
  !insertmacro MUI_LANGUAGE "Georgian"
  !insertmacro MUI_LANGUAGE "Vietnamese"
  !insertmacro MUI_LANGUAGE "Welsh"
  !insertmacro MUI_LANGUAGE "Armenian"
  !insertmacro MUI_LANGUAGE "Corsican"
  !insertmacro MUI_LANGUAGE "Tatar"
  !insertmacro MUI_LANGUAGE "Hindi"

;--------------------------------
;Reserve Files
  
  ;If you are using solid compression, files that are required before
  ;the actual installation should be stored first in the data block,
  ;because this will make your installer start faster.
  
  !insertmacro MUI_RESERVEFILE_LANGDLL


;--------------------------------
;Installer Sections

Section "Install Section" SecInstall

  SetOutPath "$INSTDIR"
  
  File  "README.md"

  SetOutPath "$INSTDIR\doc"

  File  "doc\gpl.txt"
  File  "doc\lgpl.txt"
  File  "doc\LICENSE.CBFlib"
  File  "doc\LICENSE.HDF5"

  SetOutPath "C:\WINDOWS"

  File  "bin\eiger2cbf.bat"

  SetOutPath "$INSTDIR\bin"

  File  "bin\eiger2cbf.bat"
  File  "bin\eiger2cbf.exe"
  File  "bin\libgcc_s_dw2-1.dll"
  File  "bin\libhdf5-0.dll"
  File  "bin\libhdf5_cpp-0.dll"
  File  "bin\libhdf5_f90cstub-0.dll"
  File  "bin\libhdf5_fortran-0.dll"
  File  "bin\libhdf5_hl-0.dll"
  File  "bin\libhdf5_hl_cpp-0.dll"
  File  "bin\libhdf5_hl_f90cstub-0.dll"
  File  "bin\libhdf5_hl_fortran-0.dll"
  File  "bin\libhdf5_tools-0.dll"
  File  "bin\libpcre-1.dll"
  File  "bin\libpcrecpp-0.dll"
  File  "bin\libpcreposix-0.dll"
  File  "bin\libszip.dll"
  File  "bin\libwinpthread-1.dll"
  File  "bin\libzip.dll"
  File  "bin\zlib1.dll"
  
  ;Store installation folder
  WriteRegStr HKCU "Software\eiger2cbf" "" $INSTDIR
  
  ;Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; include for some of the windows messages defines

  !include "winmessages.nsh"

  ; HKLM (all users) vs HKCU (current user) defines

  !define env_hklm 'HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"'
  !define env_hkcu 'HKCU "Environment"'

  ; set variable for local machine

  WriteRegExpandStr ${env_hklm} "EIGER2CBFPATH" "$INSTDIR\bin"

  ; and current user

  WriteRegExpandStr ${env_hkcu} "EIGER2CBFPATH" "$INSTDIR\bin"

  ; make sure windows knows about the change

  SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000



SectionEnd

;--------------------------------
;Installer Functions

Function .onInit

  !insertmacro MUI_LANGDLL_DISPLAY

FunctionEnd

;--------------------------------
;Descriptions

  ;USE A LANGUAGE STRING IF YOU WANT YOUR DESCRIPTIONS TO BE LANGAUGE SPECIFIC

  ;Assign descriptions to sections
  !insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecInstall} "Install section."
  !insertmacro MUI_FUNCTION_DESCRIPTION_END

 
;--------------------------------
;Uninstaller Section

Section "Uninstall"
  
  ; delete variable

  DeleteRegValue ${env_hklm} "EIGER2CBFPATH"
  DeleteRegValue ${env_hkcu} "EIGER2CBFPATH"

  ; make sure windows knows about the change

  SendMessage ${HWND_BROADCAST} ${WM_WININICHANGE} 0 "STR:Environment" /TIMEOUT=5000

  Delete  "$INSTDIR\README.md"

  ;OutPath "$INSTDIR\doc"

  Delete  "$INSTDIR\doc\gpl.txt"
  Delete  "$INSTDIR\doc\lgpl.txt"
  Delete  "$INSTDIR\doc\LICENSE.CBFlib"
  Delete  "$INSTDIR\doc\LICENSE.HDF5"
  RMDir   "$INSTDIR\doc"

  ;OutPath "C:\WINDOWS"

  Delete   "C:\WINDOWS\eiger2cbf.bat"

  ;OutPath "$INSTDIR\bin"

  Delete  "$INSTDIR\bin\eiger2cbf.bat"
  Delete  "$INSTDIR\bin\eiger2cbf.exe"
  Delete  "$INSTDIR\bin\libgcc_s_dw2-1.dll"
  Delete  "$INSTDIR\bin\libhdf5-0.dll"
  Delete  "$INSTDIR\bin\libhdf5_cpp-0.dll"
  Delete  "$INSTDIR\bin\libhdf5_f90cstub-0.dll"
  Delete  "$INSTDIR\bin\libhdf5_fortran-0.dll"
  Delete  "$INSTDIR\bin\libhdf5_hl-0.dll"
  Delete  "$INSTDIR\bin\libhdf5_hl_cpp-0.dll"
  Delete  "$INSTDIR\bin\libhdf5_hl_f90cstub-0.dll"
  Delete  "$INSTDIR\bin\libhdf5_hl_fortran-0.dll"
  Delete  "$INSTDIR\bin\libhdf5_tools-0.dll"
  Delete  "$INSTDIR\bin\libpcre-1.dll"
  Delete  "$INSTDIR\bin\libpcrecpp-0.dll"
  Delete  "$INSTDIR\bin\libpcreposix-0.dll"
  Delete  "$INSTDIR\bin\libszip.dll"
  Delete  "$INSTDIR\bin\libwinpthread-1.dll"
  Delete  "$INSTDIR\bin\libzip.dll"
  Delete  "$INSTDIR\bin\zlib1.dll"
  RMDir   "$INSTDIR\bin"
 
  Delete "$INSTDIR\Uninstall.exe"

  RMDir "$INSTDIR"

  DeleteRegKey /ifempty HKCU "Software\eiger2cbf"

SectionEnd

;--------------------------------
;Uninstaller Functions

Function un.onInit

  !insertmacro MUI_UNGETLANGUAGE
  
FunctionEnd