#!/bin/bash
set -e
sdkroot="$(cd $(dirname "${BASH_SOURCE[0]}");pwd)"
cd ${sdkroot}/sample/src/android/hub
. compile.sh $@
