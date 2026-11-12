@echo off
rem Builds an installable (debug-signed) Android APK for arm64 phones into build\android.
setlocal
if "%QT_ROOT%"=="" set QT_ROOT=C:\Qt\6.8.3
if "%ANDROID_SDK_ROOT%"=="" set ANDROID_SDK_ROOT=%LOCALAPPDATA%\Android\Sdk
if "%ANDROID_NDK_ROOT%"=="" set ANDROID_NDK_ROOT=%ANDROID_SDK_ROOT%\ndk\26.1.10909125
if "%JAVA_HOME%"=="" set JAVA_HOME=C:\Program Files\Android\Android Studio\jbr
set PATH=C:\Qt\Tools\Ninja;%JAVA_HOME%\bin;%PATH%

cd /d "%~dp0.."
call "%QT_ROOT%\android_arm64_v8a\bin\qt-cmake.bat" -S . -B build\android -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DQT_NO_PRIVATE_MODULE_WARNING=ON ^
    -DQT_HOST_PATH=%QT_ROOT%\msvc2022_64 ^
    -DANDROID_SDK_ROOT="%ANDROID_SDK_ROOT%" -DANDROID_NDK_ROOT="%ANDROID_NDK_ROOT%" || exit /b 1
cmake --build build\android --target apk || exit /b 1

rem Newest installed build-tools (zipalign + apksigner).
set BT=
for /d %%b in ("%ANDROID_SDK_ROOT%\build-tools\*") do set "BT=%%b"
if "%BT%"=="" (echo Android build-tools not found & exit /b 1)
set UNSIGNED=build\android\android-build\build\outputs\apk\release\android-build-release-unsigned.apk
set OUT=build\android\Miru.apk
rem Signing: the publisher's release key (MIRU_ANDROID_KEYSTORE, owner miru.com) when
rem provided - phones then accept every later version as an update. Without it, the
rem local Android debug key (fine for testing; it differs per PC).
if not "%MIRU_ANDROID_KEYSTORE%"=="" goto releasekey
if not exist "%USERPROFILE%\.android\debug.keystore" (
    "%JAVA_HOME%\bin\keytool" -genkeypair -keystore "%USERPROFILE%\.android\debug.keystore" -storepass android ^
        -alias androiddebugkey -keypass android -keyalg RSA -validity 10000 -dname "CN=Android Debug,O=Android,C=US" || exit /b 1
)
set "KS=%USERPROFILE%\.android\debug.keystore"
set "KS_ALIAS=androiddebugkey"
set "KS_ARGS=--ks-pass pass:android --key-pass pass:android"
echo Signing with the local debug key (set MIRU_ANDROID_KEYSTORE for release signing)
goto sign
:releasekey
if "%MIRU_ANDROID_KEY_PASS%"=="" set "MIRU_ANDROID_KEY_PASS=%MIRU_ANDROID_STORE_PASS%"
if "%MIRU_ANDROID_KEY_ALIAS%"=="" set "MIRU_ANDROID_KEY_ALIAS=miru"
set "KS=%MIRU_ANDROID_KEYSTORE%"
set "KS_ALIAS=%MIRU_ANDROID_KEY_ALIAS%"
set "KS_ARGS=--ks-pass env:MIRU_ANDROID_STORE_PASS --key-pass env:MIRU_ANDROID_KEY_PASS"
echo Signing with the release key (miru.com)
:sign
"%BT%\zipalign.exe" -f -p 4 "%UNSIGNED%" build\android\aligned.apk || exit /b 1
call "%BT%\apksigner.bat" sign --ks "%KS%" %KS_ARGS% --ks-key-alias "%KS_ALIAS%" --out "%OUT%" build\android\aligned.apk || exit /b 1
del build\android\aligned.apk
echo.
echo Built: %OUT%
echo Install on a USB-connected phone:  adb install -r %OUT%
