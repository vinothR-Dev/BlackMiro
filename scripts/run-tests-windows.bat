@echo off
rem Builds and runs the export test suite (headless).
setlocal
if "%QT_DIR%"=="" set QT_DIR=C:\Qt\6.11.1\msvc2022_64
if "%VSDIR%"=="" set VSDIR=C:\Program Files\Microsoft Visual Studio\18\Community
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
set PATH=C:\Qt\Tools\Ninja;%QT_DIR%\bin;%PATH%
cd /d "%~dp0.."
cmake -S . -B build\windows -DQT_NO_PRIVATE_MODULE_WARNING=ON >nul || exit /b 1
cmake --build build\windows --target tst_export || exit /b 1
build\windows\tst_export.exe
