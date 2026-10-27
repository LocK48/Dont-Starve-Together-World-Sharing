@echo off
setlocal
cd /d "%~dp0"
call build.bat || exit /b 1
echo Dang tai va dong goi PortableGit vao release\git ...
powershell -NoProfile -ExecutionPolicy Bypass -File "release\get-git.ps1" || exit /b 1
if exist dstsync-release.zip del dstsync-release.zip
powershell -NoProfile -Command "Compress-Archive -Path 'release\*' -DestinationPath 'dstsync-release.zip'"
echo.
echo Xong: dstsync-release.zip  (gui file nay cho ban be, giai nen la dung duoc)
