@echo off
setlocal
cd /d "%~dp0"

taskkill /im "KNG Studio.exe" /f >nul 2>&1

if not exist "build\KNGStudio.sln" (
    echo Configuring project...
    cmake -B build -G "Visual Studio 17 2022"
    if errorlevel 1 goto :fail
)

echo Building KNG Studio...
cmake --build build --config Debug
if errorlevel 1 goto :fail

echo.
echo Build succeeded.
exit /b 0

:fail
echo.
echo BUILD FAILED. Scroll up for the first error.
pause
exit /b 1
