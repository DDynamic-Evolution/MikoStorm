@echo off
set "PATH=C:\Cygwin64\bin;C:\Program Files\CMake\bin;C:\Program Files\Git\cmd;C:\Users\svenb\AppData\Local\Programs\Python\Python313;C:\Users\svenb\AppData\Local\Programs\Python\Python313\Scripts;C:\Users\svenb\AppData\Local\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe;C:\Users\svenb\AppData\Local\Microsoft\WinGet\Packages\Mozilla.sccache_Microsoft.Winget.Source_8wekyb3d8bbwe\sccache-v0.16.0-x86_64-pc-windows-msvc;%PATH%"
set "AUTOBUILD_VSVER=170"
set "AUTOBUILD_VARIABLES_FILE=D:\src\fs-build-variables\variables"
set "AUTOBUILD_CONFIG_FILE=D:\src\MikoStorm\my_autobuild.xml"
set "SCCACHE_MAX_JOBS=8"
cd /d D:\src\MikoStorm
"C:\Program Files\Git\bin\bash.exe" scripts/configure_firestorm.sh --config --build --platform windows --avx2 --fmodstudio --opensim --3dstream --mcp --ninja --compiler-cache --chan Release --package --jobs 8 --no-espeak
