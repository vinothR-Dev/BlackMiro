@echo off
rem Builds and deploys the Windows desktop app (Release) into build\windows\deploy.
setlocal
if "%QT_DIR%"=="" set QT_DIR=C:\Qt\6.8.3\msvc2022_64
if "%VSDIR%"=="" set VSDIR=C:\Program Files\Microsoft Visual Studio\18\Community

call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set PATH=C:\Qt\Tools\Ninja;%QT_DIR%\bin;%PATH%

cd /d "%~dp0.."
cmake -S . -B build\windows -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_DIR%" || exit /b 1
cmake --build build\windows || exit /b 1

if not exist build\windows\deploy mkdir build\windows\deploy
copy /y build\windows\Miru.exe build\windows\deploy\ >nul
copy /y build\windows\MiruRelay.exe build\windows\deploy\ >nul
"%QT_DIR%\bin\windeployqt.exe" --release --qmldir qml --no-translations build\windows\deploy\Miru.exe || exit /b 1
echo.
echo Built: build\windows\deploy\Miru.exe
