#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${REPO_ROOT}/build-linux-servers}"
INSTALL_PREFIX="${INSTALL_PREFIX:-${REPO_ROOT}/dist/servers}"

if command -v ninja >/dev/null 2>&1; then
    GENERATOR="${CMAKE_GENERATOR:-Ninja}"
else
    GENERATOR="${CMAKE_GENERATOR:-Unix Makefiles}"
fi

cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" -G "${GENERATOR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBAYOU_BUILD_CLIENTS=OFF \
    -DBAYOU_BUILD_SERVERS=ON \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}"

# Build sequentially because the production Oracle VM is memory constrained.
cmake --build "${BUILD_DIR}" --target accounts --parallel 1
cmake --build "${BUILD_DIR}" --target cardserver --parallel 1
cmake --build "${BUILD_DIR}" --target gameserver --parallel 1
cmake --build "${BUILD_DIR}" --target matchmaking --parallel 1
cmake --install "${BUILD_DIR}"

echo "Installed server release to ${INSTALL_PREFIX}"
