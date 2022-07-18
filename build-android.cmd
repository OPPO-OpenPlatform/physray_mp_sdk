@echo off
set "sdkroot=%~dp0"
pushd "%sdkroot%\sample\src\android\hub"
compile.bat %*
popd
