@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0QDCoop-Setup.ps1" -Action Install %*
set "QDCOOP_RESULT=%ERRORLEVEL%"
pause
exit /b %QDCOOP_RESULT%
