@echo off
setlocal

set build_debug=1
set build_release=0

if /i "-r" == "%1" (
    set build_debug=0
    set build_release=1
    shift
) else if /i "-a" == "%1" (
    set build_debug=1
    set build_release=1
    shift
)

echo.
echo Launch Gradle to build the Android executable. If the build stuck or fail due to incompatibility
echo with existing Daemon, try './gradlew --stop' to stop any existing Gradle Daemons.
echo.

if "%build_debug%" == "1" call gradlew.bat packageDebug %1 %2 %3 %4 %5 %6 %7 %8 %9
if "%build_release%" == "1" call gradlew.bat packageRelease %1 %2 %3 %4 %5 %6 %7 %8 %9
