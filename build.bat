@echo off
setlocal
cd /d "%~dp0"
if not exist build mkdir build
if not exist release mkdir release

where g++ >nul 2>nul
if %errorlevel%==0 (
  echo [MinGW] Dang bien dich...
  windres res\app.rc -O coff -o build\app.o || goto :fail
  g++ -std=c++17 -O2 -s -static -mwindows src\main.cpp build\app.o -o release\dstsync.exe -lshell32 -lole32 -luuid || goto :fail
  goto :ok
)

where cl >nul 2>nul
if %errorlevel%==0 (
  echo [MSVC] Dang bien dich...
  rc /nologo /fo build\app.res res\app.rc || goto :fail
  cl /nologo /std:c++17 /EHsc /O2 /MT /utf-8 /DUNICODE /D_UNICODE src\main.cpp build\app.res /Fe:release\dstsync.exe /Fo:build\ /link /SUBSYSTEM:WINDOWS shell32.lib ole32.lib user32.lib gdi32.lib advapi32.lib || goto :fail
  goto :ok
)

echo Khong tim thay g++ (MinGW-w64) hoac cl (Visual Studio). Hay cai mot trong hai.
goto :fail

:ok
copy /y get-git.ps1 release\ >nul
copy /y get-git.bat release\ >nul
copy /y README.md release\ >nul
echo.
echo Xong: release\dstsync.exe
exit /b 0

:fail
echo.
echo BIEN DICH THAT BAI.
exit /b 1
