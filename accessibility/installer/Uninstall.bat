@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Uninstall-EndlessSkyAccess.ps1" %*
echo.
pause
