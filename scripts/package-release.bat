@echo off
rem Packages already-built Windows, web and Android outputs into dist\Miru-<version>\.
rem   scripts\package-release.bat 1.0.0
rem Run build-windows.bat, build-web.bat and build-android.bat first.
setlocal
set VER=%~1
if "%VER%"=="" set VER=1.0.0
cd /d "%~dp0.."
set OUT=dist\Miru-%VER%
if exist "%OUT%" rmdir /s /q "%OUT%"
mkdir "%OUT%" || exit /b 1

rem Windows portable: unzip anywhere and run Miru.exe (no installer, no admin rights).
if not exist build\windows\deploy\Miru.exe (echo Missing Windows build & exit /b 1)
set STAGE=%OUT%\Miru-%VER%-windows-portable
xcopy /e /i /q /y build\windows\deploy "%STAGE%" >nul || exit /b 1
(
echo Miru %VER% - portable for Windows 10/11 ^(64-bit^)
echo.
echo 1. Unzip this folder anywhere ^(Desktop, USB stick ...^).
echo 2. Double-click Miru.exe. Nothing is installed.
echo.
echo Boards are saved wherever you choose. To remove Miru, delete the folder.
) > "%STAGE%\README.txt"
rem Miru's own style builds on Basic: drop the other Qt styles (smaller, and
rem keeps file paths short enough to unzip into deep folders).
for %%s in (FluentWinUI3 Fusion Imagine Material Universal Windows) do (
    if exist "%STAGE%\qml\QtQuick\Controls\%%s" rmdir /s /q "%STAGE%\qml\QtQuick\Controls\%%s"
    del /q "%STAGE%\Qt6QuickControls2%%s*.dll" 2>nul
)
del /q "%STAGE%\Qt6QuickControls2FluentWinUI3StyleImpl.dll" "%STAGE%\Qt6QuickControls2WindowsStyleImpl.dll" 2>nul
rem Ship the Visual C++ runtime DLLs next to the exe (app-local), so the zip runs on
rem a PC that never installed vc_redist. Drop the installer itself.
set CRT=
for /d %%d in ("%ProgramFiles%\Microsoft Visual Studio\*") do for /d %%e in ("%%d\*") do for /d %%v in ("%%e\VC\Redist\MSVC\*") do for /d %%c in ("%%v\x64\Microsoft.VC14*.CRT") do set "CRT=%%c"
if "%CRT%"=="" (echo Visual C++ runtime DLLs not found & exit /b 1)
copy /y "%CRT%\*.dll" "%STAGE%\" >nul || exit /b 1
del /q "%STAGE%\vc_redist.x64.exe" 2>nul
if exist "%STAGE%.zip" del /q "%STAGE%.zip"
tar -a -c -f "%STAGE%.zip" -C "%OUT%" "Miru-%VER%-windows-portable" || exit /b 1
rmdir /s /q "%STAGE%"

rem Web: static files for any web server.
if exist build\web\Miru.wasm (
    mkdir "%OUT%\web"
    for %%f in (index.html Miru.js Miru.wasm qtloader.js miru-icon.svg) do copy /y build\web\%%f "%OUT%\web\" >nul || exit /b 1
    tar -a -c -f "%OUT%\Miru-%VER%-web.zip" -C "%OUT%\web" . || exit /b 1
    rmdir /s /q "%OUT%\web"
) else echo Skipped web: build\web\Miru.wasm not found

rem Android APK (debug-signed, arm64).
if exist build\android\Miru.apk (
    copy /y build\android\Miru.apk "%OUT%\Miru-%VER%-android-arm64.apk" >nul
) else echo Skipped Android: build\android\Miru.apk not found

echo.
echo Release files in %OUT%:
dir /b "%OUT%"
