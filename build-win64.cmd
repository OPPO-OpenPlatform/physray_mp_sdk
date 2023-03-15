@echo off
set "sdkroot=%~dp0"
mkdir "%sdkroot%\build"
cmake -B "%sdkroot%\build" ""%sdkroot%" -G "Visual Studio 17 2022" -A x64
cmake --build "%sdkroot%\build" -j