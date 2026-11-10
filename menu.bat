@echo off
setlocal
cd /d "%~dp0"
title KNG Studio

:menu
cls
echo.
echo   ==============================
echo           KNG STUDIO
echo   ==============================
echo.
echo     1  Build
echo     2  Build and run
echo     3  Run (no build)
echo     4  Build Release
echo     5  Make installer
echo     6  Clean rebuild
echo     7  Git: commit and push
echo     8  Open project folder
echo     9  Open Documents\KNG Studio
echo     0  Exit
echo.
set "choice="
set /p choice=  Pick a number:

if "%choice%"=="1" goto build
if "%choice%"=="2" goto buildrun
if "%choice%"=="3" goto run
if "%choice%"=="4" goto release
if "%choice%"=="5" goto installer
if "%choice%"=="6" goto clean
if "%choice%"=="7" goto git
if "%choice%"=="8" goto projectfolder
if "%choice%"=="9" goto docsfolder
if "%choice%"=="0" goto end
goto menu

:build
call build.bat nopause
goto done

:buildrun
call run.bat nopause
goto done

:run
set "found="
for %%F in ("build\KNGStudio_artefacts\Debug\*.exe") do (
    set "found=1"
    start "" "%%F"
)
if not defined found echo   No Debug build yet. Pick 1 to build first.
goto done

:release
echo Building Release...
cmake --build build --config Release
if errorlevel 1 (
    echo.
    echo   ===== RELEASE BUILD FAILED =====
) else (
    echo.
    echo   Release build done: build\KNGStudio_artefacts\Release
)
goto done

:installer
set "ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
    echo   Inno Setup 6 isn't installed. Get it free at jrsoftware.org, then try again.
    goto done
)
echo Building Release first...
cmake --build build --config Release
if errorlevel 1 (
    echo   ===== RELEASE BUILD FAILED =====
    goto done
)
"%ISCC%" installer.iss
if errorlevel 1 (
    echo   ===== INSTALLER FAILED =====
) else (
    echo   Installer is in the installer folder.
    start "" "installer"
)
goto done

:clean
echo.
echo   This deletes the build folder and rebuilds everything from scratch.
echo   It takes a while. Your code and projects are not touched.
set "confirm="
set /p confirm=  Type YES to continue:
if /i not "%confirm%"=="YES" goto menu
if exist build rmdir /s /q build
cmake -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo   ===== CONFIGURE FAILED =====
    goto done
)
call build.bat nopause
goto done

:git
echo.
set "msg="
set /p msg=  Commit message:
if "%msg%"=="" (
    echo   No message, nothing committed.
    goto done
)
git add .
git commit -m "%msg%"
git push
goto done

:projectfolder
start "" "%~dp0"
goto menu

:docsfolder
start "" "%USERPROFILE%\Documents\KNG Studio"
goto menu

:done
echo.
pause
goto menu

:end
endlocal
