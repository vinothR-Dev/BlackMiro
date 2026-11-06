@echo off
rem Builds the WebAssembly version into build\web (open BlackMiro.html via a web server).
setlocal
if "%QT_ROOT%"=="" set QT_ROOT=C:\Qt\6.11.1
if "%EMSDK%"=="" set EMSDK=C:\emsdk

call "%EMSDK%\emsdk_env.bat" >nul 2>&1
set PATH=C:\Qt\Tools\Ninja;%PATH%

cd /d "%~dp0.."
call "%QT_ROOT%\wasm_singlethread\bin\qt-cmake.bat" -S . -B build\web -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DQT_NO_PRIVATE_MODULE_WARNING=ON ^
    -DQT_HOST_PATH=%QT_ROOT%\msvc2022_64 || exit /b 1
cmake --build build\web || exit /b 1
echo.
echo Built: build\web\BlackMiro.html
echo Serve it with:  python -m http.server 8080 --directory build\web
