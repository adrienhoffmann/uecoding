@echo off
REM Wrapper to call PowerShell pre-commit script from cmd.exe
set SCRIPT_DIR=%~dp0
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%pre-commit.ps1"
IF %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%
exit /b 0
