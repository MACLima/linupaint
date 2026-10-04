@echo off
rem Local development build on Windows (MSVC + Qt from aqtinstall). Linux builds run in CI.
setlocal
set VS=C:\Program Files\Microsoft Visual Studio\18\Professional
if not defined QT_DIR set QT_DIR=C:\Portable\Qt\6.8.3\msvc2022_64
if not defined BUILD_TYPE set BUILD_TYPE=Release
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set PATH=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%QT_DIR%\bin;%PATH%
cd /d "%~dp0.."
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_PREFIX_PATH=%QT_DIR% %* || exit /b 1
cmake --build build || exit /b 1
ctest --test-dir build --output-on-failure || exit /b 1
