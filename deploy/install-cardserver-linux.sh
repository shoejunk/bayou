#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
    echo "Run this installer as root, for example: sudo $0"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

APP_USER="${APP_USER:-bayou}"
INSTALL_PREFIX="${INSTALL_PREFIX:-/opt/bayou/cardserver}"
DATA_DIR="${DATA_DIR:-/var/lib/bayou/cardserver}"
SERVICE_NAME="${SERVICE_NAME:-bayou-cardserver}"
BUILD_DIR="${BUILD_DIR:-${REPO_ROOT}/build-linux-cardserver}"
PORT="${PORT:-55004}"

install_packages() {
    if command -v apt-get >/dev/null 2>&1; then
        apt-get update
        DEBIAN_FRONTEND=noninteractive apt-get install -y \
            build-essential ca-certificates git ninja-build pkg-config \
            python3 python3-pip libsqlite3-dev
    elif command -v dnf >/dev/null 2>&1; then
        local dnf_options=(
            --setopt=install_weak_deps=False
            --setopt=tsflags=nodocs
        )
        dnf install -y "${dnf_options[@]}" gcc gcc-c++ make
        dnf install -y "${dnf_options[@]}" git pkgconf-pkg-config
        dnf install -y "${dnf_options[@]}" ninja-build || true
        dnf install -y "${dnf_options[@]}" python3 python3-pip sqlite-devel
    else
        echo "Unsupported Linux distribution: expected apt-get or dnf."
        exit 1
    fi
}

cmake_is_new_enough() {
    command -v cmake >/dev/null 2>&1 || return 1
    local version
    version="$(cmake --version | awk 'NR==1 {print $3}')"
    [[ "$(printf '%s\n' "3.28" "${version}" | sort -V | head -n1)" == "3.28" ]]
}

ensure_cmake() {
    if cmake_is_new_enough; then
        return
    fi

    python3 -m pip install --upgrade 'cmake>=3.28,<4'
    export PATH="/usr/local/bin:${PATH}"

    if ! cmake_is_new_enough; then
        echo "CMake 3.28 or newer is required and could not be installed."
        exit 1
    fi
}

install_packages
ensure_cmake

if ! id -u "${APP_USER}" >/dev/null 2>&1; then
    useradd --system --home-dir "${DATA_DIR}" --shell /usr/sbin/nologin "${APP_USER}"
fi

mkdir -p "${INSTALL_PREFIX}" "${DATA_DIR}"
chown -R "${APP_USER}:${APP_USER}" "${DATA_DIR}"

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

sed \
    -e "s|@APP_USER@|${APP_USER}|g" \
    -e "s|@DATA_DIR@|${DATA_DIR}|g" \
    -e "s|@INSTALL_PREFIX@|${INSTALL_PREFIX}|g" \
    "${SCRIPT_DIR}/bayou-cardserver.service.in" > "/etc/systemd/system/${SERVICE_NAME}.service"

systemctl daemon-reload
systemctl enable --now "${SERVICE_NAME}"

if command -v firewall-cmd >/dev/null 2>&1 && systemctl is-active --quiet firewalld; then
    firewall-cmd --permanent --add-port="${PORT}/tcp"
    firewall-cmd --reload
fi

systemctl --no-pager --full status "${SERVICE_NAME}"
