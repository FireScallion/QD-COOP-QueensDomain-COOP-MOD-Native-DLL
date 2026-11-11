@echo off
(
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0QDCoop-Setup.ps1" -Action UninstallAll %*
if errorlevel 1 (
    pause
    exit /b 1
)
pause
exit /b 0
)
