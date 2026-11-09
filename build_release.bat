@echo off
echo Building KNG Studio (Release)...
cmake --build build --config Release
if errorlevel 1 (
    echo.
    echo ===== RELEASE BUILD FAILED =====
    pause
    exit /b 1
)
echo.
echo Release build done. The app is in build\KNGStudio_artefacts\Release
pause
