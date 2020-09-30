:: eiger2cbf.bat
:: wrapper to start eiger2cbf under Windows-10
::    (C) Herbert J. Bernstein 2020
::    License  GPL/LGPL
SETLOCAL
  IF ["%EIGER2CBFPATH%"] == [""] SET EIGER2CBFPATH="C:\eiger2cbf\bin"
  IF NOT EXIST "%EIGER2CBFPATH%"  GOTO :ERROR1
  SET PATH="%EIGER2CBFPATH%";%PATH%
  "%EIGER2CBFPATH%\eiger2cbf.exe" %1 %2 %3 %4 %5 %6 %7 %8 %9
  GOTO :END
ENDLOCAL

:ERROR1
SET msgboxTitle=eiger2cbf.bat error
SET msgboxBody=EIGER2CBFPATH not found
SET tmpmsgbox=%temp%\~tmpmsgbox.vbs
IF EXIST "%tmpmsgbox%" DEL /F /Q "%tmpmsgbox%"
ECHO msgbox "%msgboxBody%",0,"%msgboxTitle%">"%tmpmsgbox%"
WSCRIPT "%tmpmsgbox%"

:END