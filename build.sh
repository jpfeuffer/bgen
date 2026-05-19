#!/bin/bash
set -euo pipefail

cmake ${CMAKE_ARGS} -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${PREFIX}"

cmake --build build --parallel "${CPU_COUNT}"
cmake --install build
