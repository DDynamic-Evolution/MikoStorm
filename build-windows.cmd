@echo off
setlocal
set "MIKO_ROOT=%~dp0"
if not defined VSINSTALLDIR (
  for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "MIKO_VS=%%i"
  if not defined MIKO_VS goto no_vs
  call :vcvars
  if errorlevel 1 exit /b 1
)
set "AUTOBUILD_VSVER=170"
set "AUTOBUILD_CONFIG_FILE=%MIKO_ROOT%autobuild.xml"
if not defined AUTOBUILD_VARIABLES_FILE set "AUTOBUILD_VARIABLES_FILE=%MIKO_ROOT%..\fs-build-variables\variables"
set "MSBUILDDISABLENODEREUSE=1"
if not defined _CL_ set "_CL_=/MP6 /Z7"
set "PATH=%ProgramFiles%\CMake\bin;%ProgramFiles%\Git\bin;%ProgramFiles%\Git\usr\bin;%ProgramFiles(x86)%\NSIS;%PATH%"
python "%MIKO_ROOT%scripts\windows_build.py" %*
exit /b %errorlevel%
:no_vs
echo Visual Studio 2022 C++ tools were not found.
exit /b 1

:vcvars
call "%MIKO_VS%\VC\Auxiliary\Build\vcvars64.bat"
exit /b %errorlevel%
