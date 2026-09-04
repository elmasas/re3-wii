@echo off
setlocal

set "MSYS_BASH="
if exist "C:\devkitPro\msys2\usr\bin\bash.exe" set "MSYS_BASH=C:\devkitPro\msys2\usr\bin\bash.exe"
if not defined MSYS_BASH if exist "C:\msys64\usr\bin\bash.exe" set "MSYS_BASH=C:\msys64\usr\bin\bash.exe"

if not defined MSYS_BASH (
    echo devkitPro not found. Install it from https://devkitpro.org/wiki/Getting_Started
    pause
    exit /b 1
)

set "REPO_DIR=%~dp0"
"%MSYS_BASH%" -lc "cd \"$(cygpath -u \"$REPO_DIR\")\" && ./build-wii.sh %*"
set "CODE=%errorlevel%"

pause
exit /b %CODE%
