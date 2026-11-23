@echo off
rem Runs the PowerShell installer without changing the system execution policy.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-EndlessSkyAccess.ps1" %*
echo.
pause
