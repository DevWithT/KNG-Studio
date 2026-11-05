@echo off
setlocal
cd /d "%~dp0"

call build.bat
if errorlevel 1 exit /b 1

echo Launching KNG Studio...
start "" "build\KNGStudio_artefacts\Debug\KNG Studio.exe"
exit /b 0
