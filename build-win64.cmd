@echo off
set "sdkroot=%~dp0"
mkdir "%sdkroot%\build"
cmake -B "%sdkroot%\build" ""%sdkroot%"
cmake --build "%sdkroot%\build" -j