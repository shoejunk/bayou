#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${REPO_ROOT}/build-linux-cardserver}"
INSTALL_PREFIX="${INSTALL_PREFIX:-${REPO_ROOT}/dist/cardserver}"

if command -v ninja >/dev/null 2>&1; then
    GENERATOR="${CMAKE_GENERATOR:-Ninja}"
else
    GENERATOR="${CMAKE_GENERATOR:-Unix Makefiles}"
fi

cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" -G "${GENERATOR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBAYOU_BUILD_CLIENTS=OFF \
    -DBAYOU_BUILD_SERVERS=ON \
    -DBAYOU_BUILD_ACCOUNTS=OFF \
    -DBAYOU_BUILD_MATCHMAKING=OFF \
    -DBAYOU_BUILD_GAMESERVER=OFF \
    -DBAYOU_BUILD_CARDSERVER=ON \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}"

cmake --build "${BUILD_DIR}" --target cardserver --parallel
cmake --install "${BUILD_DIR}"

echo "Installed cardserver to ${INSTALL_PREFIX}/bin/cardserver"
