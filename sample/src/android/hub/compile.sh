#!/bin/bash
set -e
echo "Launch Gradle to build the Android executable. If the build stucks or fail due to incompatibility"
echo "with existing Daemon, try './gradlew --stop' to stop any existing Gradle Daemons."

build_debug=1
build_release=

if [ "-r" = "$1" ] || [ "--release" = "$1" ]; then
    build_debug=
    build_release=1
    shift
elif [ "-a" = "$1" ] || [ "--all" = "$1" ]; then
    build_debug=1
    build_release=1
    shift
fi

if [ $build_debug   ]; then ./gradlew packageDebug $@; fi
if [ $build_release ]; then ./gradlew packageRelease $@; fi
