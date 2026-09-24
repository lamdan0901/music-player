@echo off
:: Build MusicPlayer2 from the command line without opening Visual Studio.
:: Usage: build.bat [Release|Debug] [x64|Win32]   (default: Release x64)
setlocal
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"
set "PLATFORM=%~2"
if "%PLATFORM%"=="" set "PLATFORM=x64"

:: Pre-build event calls print_compile_time.bat from the project dir; make sure cmd searches it.
set NoDefaultCurrentDirectoryInExePath=

for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -prerelease -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
if not defined MSBUILD (
    echo MSBuild not found. Install Visual Studio with the C++ desktop workload.
    exit /b 1
)

"%MSBUILD%" "%~dp0MusicPlayer2.sln" /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% /m /v:minimal /nologo
if errorlevel 1 exit /b 1

:: The repo ships bass.dll/bass_fx.dll only in the Debug output folders; copy them next to other builds.
set "OUTDIR=%~dp0%PLATFORM%\"
if /i "%PLATFORM%"=="Win32" set "OUTDIR=%~dp0"
if /i not "%CONFIG%"=="Debug" xcopy /d /y /q "%OUTDIR%Debug\bass*.dll" "%OUTDIR%%CONFIG%\" >nul

:: Release exe loads skins/language/etc. from its own folder; only 2 built-in UIs show without them.
for %%d in (skins language Encoder Plugins) do xcopy /e /i /d /y /q "%~dp0MusicPlayer2\%%d" "%OUTDIR%%CONFIG%\%%d\" >nul
