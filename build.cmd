@echo off
setlocal
cd /d "%~dp0"
if not "%~1"=="" (
    echo Usage: build.cmd
    exit /b 1
)
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo Visual Studio Build Tools with Desktop development with C++ is required.
    exit /b 1
)
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL (
    echo The MSVC C++ toolchain was not found.
    exit /b 1
)
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0prepare-assets.ps1"
if errorlevel 1 exit /b 1
if not exist bin mkdir bin
rc /nologo /fo bin\skea-clock.res skea-clock.rc
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /W4 /O2 /MT /EHsc /utf-8 /D_WIN32_WINNT=0x0A00 skea-clock.cpp bin\skea-clock.res /Fo:bin\skea-clock.obj /Fe:bin\SkeaClock.exe /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED
if errorlevel 1 exit /b 1
echo Built: bin\SkeaClock.exe
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0distribution\update-metadata.ps1" -ExePath "%~dp0bin\SkeaClock.exe"
if errorlevel 1 exit /b 1
exit /b 0
