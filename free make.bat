@ECHO OFF

set SRC_DRIVE=C:
set SRC_PATH=22\bus
set DDK_PATH=D:\WINDDK\2600.1106

call %DDK_PATH%\bin\setenv.bat %DDK_PATH% wxp free

%SRC_DRIVE%
cd   %SRC_DRIVE%\%SRC_PATH%

nmake

pause
