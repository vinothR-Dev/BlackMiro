@echo off
rem Builds the Windows installers from a deployed app folder:
rem   scripts\build-installers.bat <version> <deploy folder> <output folder>
rem   -> BlackMiro-<version>-windows-setup.exe (Inno Setup) and BlackMiro-<version>-windows.msi (WiX v6)
setlocal
set VER=%~1
set SRC=%~f2
set OUT=%~f3
if "%OUT%"=="" (echo usage: build-installers.bat version deployDir outDir & exit /b 1)
pushd "%~dp0..\packaging"

set "ISCC="
for %%p in ("%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" "%ProgramFiles%\Inno Setup 6\ISCC.exe") do if exist "%%~p" set "ISCC=%%~p"
if not defined ISCC (echo Inno Setup 6 not found - install it from https://jrsoftware.org& exit /b 1)
"%ISCC%" /Q /DAppVer=%VER% "/DSrcDir=%SRC%" "/DOutDir=%OUT%" BlackMiro.iss || exit /b 1

where wix >nul 2>&1 || (echo WiX not found - run: dotnet tool install --global wix --version 6.0.0& exit /b 1)
wix build -arch x64 -d AppVer=%VER% -d "SrcDir=%SRC%" -o "%OUT%\BlackMiro-%VER%-windows.msi" BlackMiro.wxs || exit /b 1
del /q "%OUT%\BlackMiro-%VER%-windows.wixpdb" 2>nul
popd
echo Built installers in %OUT%
