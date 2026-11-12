@echo off
rem One-time: creates the Android release key for the publisher miru.com.
rem   scripts\make-signing-keys.bat
rem Keep miru-release.keystore and its password safe and private: every future
rem Android update must be signed with this same key. Never commit it to git.
setlocal
if "%JAVA_HOME%"=="" set JAVA_HOME=C:\Program Files\Android\Android Studio\jbr
if exist miru-release.keystore (echo miru-release.keystore already exists & exit /b 1)
"%JAVA_HOME%\bin\keytool" -genkeypair -v -keystore miru-release.keystore -alias miru ^
    -keyalg RSA -keysize 4096 -validity 10000 -dname "CN=miru.com, O=miru.com" || exit /b 1
echo.
echo Created miru-release.keystore (alias: miru). For GitHub Actions add two repository secrets:
echo   MIRU_ANDROID_KEYSTORE_B64 - the file as base64. In PowerShell:
echo       [Convert]::ToBase64String([IO.File]::ReadAllBytes("miru-release.keystore")) ^| Set-Clipboard
echo   MIRU_ANDROID_STORE_PASS   - the password you just typed
