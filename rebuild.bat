@echo off
call "C:\Users\svenb\AppData\Local\Microsoft\Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64
set "AUTOBUILD_CONFIG_FILE=D:\src\MikoStorm\my_autobuild.xml"
set "SCCACHE_MAX_JOBS=8"
cd /d D:\src\MikoStorm
build.bat --no-espeak --package --clean -- --Release 2>&1
