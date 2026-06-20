#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
    echo "Run this installer as root, for example: sudo $0"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SOURCE_PREFIX="${SOURCE_PREFIX:-${REPO_ROOT}/dist/servers}"
VERSION="${VERSION:-$(date -u +%Y%m%d%H%M%S)}"
RELEASE_DIR="${RELEASE_DIR:-/opt/bayou/releases/${VERSION}}"
DATA_DIR="${DATA_DIR:-/var/lib/bayou/shared}"
APP_USER="${APP_USER:-bayou}"

for executable in accounts cardserver gameserver matchmaking; do
    if [[ ! -x "${SOURCE_PREFIX}/bin/${executable}" ]]; then
        echo "Missing release binary: ${SOURCE_PREFIX}/bin/${executable}"
        exit 1
    fi
done

if ! id -u "${APP_USER}" >/dev/null 2>&1; then
    useradd --system --home-dir "${DATA_DIR}" --shell /usr/sbin/nologin "${APP_USER}"
fi

install -d -m 0755 "${RELEASE_DIR}/bin" "${DATA_DIR}"
for executable in accounts cardserver gameserver matchmaking; do
    install -m 0755 "${SOURCE_PREFIX}/bin/${executable}" "${RELEASE_DIR}/bin/${executable}"
done

for database in accounts.db cards.db; do
    if [[ ! -f "${DATA_DIR}/${database}" && -f "${REPO_ROOT}/${database}" ]]; then
        install -m 0640 -o "${APP_USER}" -g "${APP_USER}" \
            "${REPO_ROOT}/${database}" "${DATA_DIR}/${database}"
    fi
done
chown -R "${APP_USER}:${APP_USER}" "${DATA_DIR}"

ln -sfn "${RELEASE_DIR}" /opt/bayou/current

for unit in accounts cardserver gameserver matchmaking; do
    install -m 0644 "${SCRIPT_DIR}/bayou-${unit}.service" \
        "/etc/systemd/system/bayou-${unit}.service"
done
install -m 0644 "${SCRIPT_DIR}/bayou-payments.service" \
    /etc/systemd/system/bayou-payments.service

systemctl daemon-reload
systemctl enable bayou-accounts bayou-cardserver bayou-gameserver bayou-matchmaking
systemctl restart bayou-accounts bayou-cardserver bayou-gameserver bayou-matchmaking

if command -v firewall-cmd >/dev/null 2>&1 && systemctl is-active --quiet firewalld; then
    firewall-cmd --permanent --add-port=55000-55002/tcp
    firewall-cmd --permanent --add-port=55004/tcp
    firewall-cmd --permanent --add-port=56000-65535/tcp
    firewall-cmd --reload
fi

systemctl --no-pager --full status \
    bayou-accounts bayou-cardserver bayou-gameserver bayou-matchmaking
