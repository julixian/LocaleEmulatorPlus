@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "ROOT=%~dp0"
set "ROOT=%ROOT:~0,-1%"

if not defined CONFIGURATION set "CONFIGURATION=Release"
if not defined CORE_DIR set "CORE_DIR=%ROOT%\..\LocaleEmulatorPlus-Core"

if not defined MSBUILD (
  set "VSWHERE=!ProgramFiles(x86)!\Microsoft Visual Studio\Installer\vswhere.exe"
  if exist "!VSWHERE!" (
    for /f "usebackq delims=" %%I in (`"!VSWHERE!" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
      set "MSBUILD=%%I"
      goto :found_msbuild
    )
  )
)

:found_msbuild
if not defined MSBUILD if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
if not defined MSBUILD if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
if not defined MSBUILD set "MSBUILD=msbuild.exe"

echo LocaleEmulatorPlus GUI build
echo   Root:        "%ROOT%"
echo   MSBuild:     "%MSBUILD%"
echo   Config:      "%CONFIGURATION%"
echo   Core output: "%CORE_DIR%"
echo.

if not exist "%ROOT%\LEPInstaller\Resources" mkdir "%ROOT%\LEPInstaller\Resources"

call :build_project "LEPCommonLibrary\LEPCommonLibrary.csproj" AnyCPU || exit /b 1
call :build_project "LEPContextMenuHandler\LEPContextMenuHandler.csproj" AnyCPU || exit /b 1
call :build_project "LEPGUI\LEPGUI.csproj" AnyCPU || exit /b 1
call :build_project "LEPUpdater\LEPUpdater.csproj" AnyCPU || exit /b 1

rem LEPProc has x86/x64 outputs, but LEPCommonLibrary is AnyCPU.  When the
rem project is built outside the solution, old MSBuild passes the child
rem platform to the reference path, so provide the expected folders.
call :prepare_common_library x86 || exit /b 1
call :bp_norefs "LEPProc\LEPProc.csproj" x86 || exit /b 1
call :prepare_common_library x64 || exit /b 1
call :bp_norefs "LEPProc\LEPProc.csproj" x64 || exit /b 1

call :build_project "LEPInstaller\LEPInstaller.csproj" AnyCPU || exit /b 1

call :copy_core || exit /b 1

echo.
echo Build succeeded.
echo   Output: "%ROOT%\Build\%CONFIGURATION%"
exit /b 0

:build_project
echo.
echo [build] %~1 ^(%~2^)
"%MSBUILD%" /nr:false "%ROOT%\%~1" /t:Build /p:Configuration=%CONFIGURATION% /p:Platform=%~2 /p:SolutionDir=%ROOT%\ || exit /b 1
exit /b 0

:bp_norefs
echo.
echo [build] %~1 ^(%~2, references prebuilt^)
"%MSBUILD%" /nr:false "%ROOT%\%~1" /t:Build /p:Configuration=%CONFIGURATION% /p:Platform=%~2 /p:BuildProjectReferences=false /p:SolutionDir=%ROOT%\ || exit /b 1
exit /b 0

:prepare_common_library
set "ARCH=%~1"
set "SRC=%ROOT%\LEPCommonLibrary\bin\%CONFIGURATION%"
set "DST=%ROOT%\LEPCommonLibrary\bin\%ARCH%\%CONFIGURATION%"
if not exist "%SRC%\LEPCommonLibrary.dll" (
  echo Missing "%SRC%\LEPCommonLibrary.dll".
  exit /b 1
)
if not exist "%DST%" mkdir "%DST%"
copy /Y "%SRC%\LEPCommonLibrary.dll" "%DST%\" >nul || exit /b 1
if exist "%SRC%\LEPCommonLibrary.pdb" copy /Y "%SRC%\LEPCommonLibrary.pdb" "%DST%\" >nul
exit /b 0

:copy_core
set "OUT=%ROOT%\Build\%CONFIGURATION%"

if exist "%CORE_DIR%\out\x86\LoaderDll_x86.dll" copy /Y "%CORE_DIR%\out\x86\LoaderDll_x86.dll" "%OUT%\" >nul || exit /b 1
if exist "%CORE_DIR%\out\x86\LocaleEmulatorPlus_x86.dll" copy /Y "%CORE_DIR%\out\x86\LocaleEmulatorPlus_x86.dll" "%OUT%\" >nul || exit /b 1
if exist "%CORE_DIR%\out\x64\LoaderDll_x64.dll" copy /Y "%CORE_DIR%\out\x64\LoaderDll_x64.dll" "%OUT%\" >nul || exit /b 1
if exist "%CORE_DIR%\out\x64\LocaleEmulatorPlus_x64.dll" copy /Y "%CORE_DIR%\out\x64\LocaleEmulatorPlus_x64.dll" "%OUT%\" >nul || exit /b 1

exit /b 0
