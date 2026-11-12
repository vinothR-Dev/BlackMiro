@echo off
rem Authenticode-signs Windows files with the publisher's code-signing certificate.
rem   scripts\sign-windows.bat file1 [file2 ...]
rem Needs MIRU_SIGN_PFX (path to the .pfx) and MIRU_SIGN_PASSWORD. Without them it
rem does nothing, so local and fork builds still work (unsigned).
setlocal enabledelayedexpansion
if "%MIRU_SIGN_PFX%"=="" (echo Signing skipped: MIRU_SIGN_PFX not set & exit /b 0)
set "SIGNTOOL="
for /d %%k in ("%ProgramFiles(x86)%\Windows Kits\10\bin\10.*") do if exist "%%k\x64\signtool.exe" set "SIGNTOOL=%%k\x64\signtool.exe"
if not defined SIGNTOOL (echo signtool.exe not found - install the Windows SDK & exit /b 1)
for %%f in (%*) do (
    "!SIGNTOOL!" sign /f "%MIRU_SIGN_PFX%" /p "%MIRU_SIGN_PASSWORD%" /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 /d "Miru" "%%~f" || exit /b 1
)
echo Signed: %*
