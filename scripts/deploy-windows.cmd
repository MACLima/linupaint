@echo off
rem Self-contained Windows folder for local testing (dist\LinuPaint). Run scripts\build-windows.cmd first.
setlocal
set VS=C:\Program Files\Microsoft Visual Studio\18\Professional
if not defined QT_DIR set QT_DIR=C:\Portable\Qt\6.8.3\msvc2022_64
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%~dp0.."
if exist dist\LinuPaint rmdir /s /q dist\LinuPaint
mkdir dist\LinuPaint || exit /b 1
copy /y build\src\app\linupaint.exe dist\LinuPaint\ >nul || exit /b 1
"%QT_DIR%\bin\windeployqt.exe" --release --compiler-runtime --no-system-d3d-compiler --no-opengl-sw dist\LinuPaint\linupaint.exe || exit /b 1
