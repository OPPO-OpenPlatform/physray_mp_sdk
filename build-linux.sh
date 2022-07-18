#!/bin/bash
set -e
sdkroot="$(cd $(dirname "${BASH_SOURCE[0]}");pwd)"
mkdir -p ${sdkroot}/build
cmake -B ${sdkroot}/build ${sdkroot}
cmake --build ${sdkroot}/build -j