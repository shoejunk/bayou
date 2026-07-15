#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${REPO_ROOT}/build-linux-servers}"
INSTALL_PREFIX="${INSTALL_PREFIX:-${REPO_ROOT}/dist/servers}"

generator_args=()
if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    if command -v ninja >/dev/null 2>&1; then
        generator_args=(-G "${CMAKE_GENERATOR:-Ninja}")
    else
        generator_args=(-G "${CMAKE_GENERATOR:-Unix Makefiles}")
    fi
fi

launcher_args=()
if command -v ccache >/dev/null 2>&1; then
    launcher_args=(
        -DCMAKE_C_COMPILER_LAUNCHER=ccache
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
    )
fi

cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" "${generator_args[@]}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBAYOU_BUILD_CLIENTS=OFF \
    -DBAYOU_BUILD_SERVERS=ON \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    "${launcher_args[@]}"

# Build sequentially because the production Oracle VM is memory constrained.
cmake --build "${BUILD_DIR}" \
    --target accounts cardserver gameserver matchmaking \
    --parallel 1
cmake --install "${BUILD_DIR}"

printf 'Installed server release to %s\n' "${INSTALL_PREFIX}"
