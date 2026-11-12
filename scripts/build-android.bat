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

rem Release APKs come out unsigned; sign with the local Android debug key so it
rem installs on a phone. (For Play Store, sign with your own release keystore.)
rem Newest installed build-tools (zipalign + apksigner).
set BT=
for /d %%b in ("%ANDROID_SDK_ROOT%\build-tools\*") do set "BT=%%b"
if "%BT%"=="" (echo Android build-tools not found & exit /b 1)
set UNSIGNED=build\android\android-build\build\outputs\apk\release\android-build-release-unsigned.apk
set OUT=build\android\Miru.apk
if not exist "%USERPROFILE%\.android\debug.keystore" (
    "%JAVA_HOME%\bin\keytool" -genkeypair -keystore "%USERPROFILE%\.android\debug.keystore" -storepass android ^
        -alias androiddebugkey -keypass android -keyalg RSA -validity 10000 -dname "CN=Android Debug,O=Android,C=US" || exit /b 1
)
"%BT%\zipalign.exe" -f -p 4 "%UNSIGNED%" build\android\aligned.apk || exit /b 1
call "%BT%\apksigner.bat" sign --ks "%USERPROFILE%\.android\debug.keystore" --ks-pass pass:android ^
    --ks-key-alias androiddebugkey --key-pass pass:android --out "%OUT%" build\android\aligned.apk || exit /b 1
del build\android\aligned.apk
echo.
echo Built: %OUT%
echo Install on a USB-connected phone:  adb install -r %OUT%
