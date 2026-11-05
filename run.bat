@echo off
setlocal
cd /d "%~dp0"

call build.bat nopause
if errorlevel 1 exit /b 1

if not exist "build\KNGStudio_artefacts\Debug\KNG Studio.exe" (
    echo Could not find KNG Studio.exe
    pause
    exit /b 1
)

echo Launching KNG Studio...
start "" "build\KNGStudio_artefacts\Debug\KNG Studio.exe"
pause
exit /b 0
