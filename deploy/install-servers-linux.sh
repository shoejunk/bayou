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
GAME_SERVER_CONFIG_FILE="${GAME_SERVER_CONFIG_FILE:-/etc/bayou/gameserver.cfg}"
CARDS_DATABASE_PATH="${CARDS_DATABASE_PATH:-${DATA_DIR}/cards.db}"
APP_USER="${APP_USER:-bayou}"
TLS_CERT_FILE="${TLS_CERT_FILE:-}"
TLS_CHAIN_FILE="${TLS_CHAIN_FILE:-}"
TLS_KEY_FILE="${TLS_KEY_FILE:-}"
TLS_CA_FILE="${TLS_CA_FILE:-}"
TLS_SERVER_NAME="${TLS_SERVER_NAME:-}"
TLS_DIR="/etc/bayou/tls"
TLS_ENV_FILE="/etc/bayou/tls.env"
SERVICE_UNITS=(bayou-accounts bayou-cardserver bayou-gameserver bayou-matchmaking)

for executable in accounts cardserver gameserver matchmaking; do
    if [[ ! -x "${SOURCE_PREFIX}/bin/${executable}" ]]; then
        echo "Missing release binary: ${SOURCE_PREFIX}/bin/${executable}"
        exit 1
    fi
done

for variable in TLS_CERT_FILE TLS_CHAIN_FILE TLS_KEY_FILE TLS_CA_FILE TLS_SERVER_NAME; do
    if [[ -z "${!variable}" ]]; then
        echo "${variable} is required for a production TLS deployment."
        exit 1
    fi
done
for file in "${TLS_CERT_FILE}" "${TLS_CHAIN_FILE}" "${TLS_KEY_FILE}" "${TLS_CA_FILE}"; do
    if [[ ! -f "${file}" ]]; then
        echo "TLS input file does not exist: ${file}"
        exit 1
    fi
done
if [[ ! "${TLS_SERVER_NAME}" =~ ^[A-Za-z0-9.-]+$ ]]; then
    echo "TLS_SERVER_NAME must be a DNS name or IPv4 address."
    exit 1
fi
CARD_SERVER_HOST="${CARD_SERVER_HOST:-${TLS_SERVER_NAME}}"
if ! command -v openssl >/dev/null 2>&1; then
    echo "OpenSSL is required to validate production certificates before deployment."
    exit 1
fi

# Fail before changing the active release if the certificate is expired,
# untrusted by the supplied CA, does not match the private key, or does not
# cover the identity that clients and internal services will verify.
openssl x509 -in "${TLS_CERT_FILE}" -noout -checkend 86400
openssl verify -CAfile "${TLS_CA_FILE}" -untrusted "${TLS_CHAIN_FILE}" "${TLS_CERT_FILE}"
certificate_key="$({ openssl x509 -in "${TLS_CERT_FILE}" -pubkey -noout |
    openssl pkey -pubin -outform DER; } | sha256sum | awk '{print $1}')"
private_key="$({ openssl pkey -in "${TLS_KEY_FILE}" -pubout -outform DER; } |
    sha256sum | awk '{print $1}')"
if [[ "${certificate_key}" != "${private_key}" ]]; then
    echo "TLS certificate and private key do not match."
    exit 1
fi
if [[ "${TLS_SERVER_NAME}" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    openssl x509 -in "${TLS_CERT_FILE}" -noout -checkip "${TLS_SERVER_NAME}"
else
    openssl x509 -in "${TLS_CERT_FILE}" -noout -checkhost "${TLS_SERVER_NAME}"
fi

if ! id -u "${APP_USER}" >/dev/null 2>&1; then
    useradd --system --home-dir "${DATA_DIR}" --shell /usr/sbin/nologin "${APP_USER}"
fi

install -d -m 0755 "${RELEASE_DIR}/bin"
install -d -m 0700 -o "${APP_USER}" -g "${APP_USER}" "${DATA_DIR}"
for executable in accounts cardserver gameserver matchmaking; do
    install -m 0755 "${SOURCE_PREFIX}/bin/${executable}" "${RELEASE_DIR}/bin/${executable}"
done

if [[ ! -f "${CARDS_DATABASE_PATH}" ]]; then
    echo "Missing authoritative cards database: ${CARDS_DATABASE_PATH}"
    echo "Provision the shared database before installing the server release."
    exit 1
fi

for database in accounts.db; do
    if [[ ! -f "${DATA_DIR}/${database}" && -f "${REPO_ROOT}/${database}" ]]; then
        install -m 0600 -o "${APP_USER}" -g "${APP_USER}" \
            "${REPO_ROOT}/${database}" "${DATA_DIR}/${database}"
    fi
done
chown -R "${APP_USER}:${APP_USER}" "${DATA_DIR}"

install -d -m 0750 -o root -g "${APP_USER}" "$(dirname "${GAME_SERVER_CONFIG_FILE}")"
if [[ ! -f "${GAME_SERVER_CONFIG_FILE}" ]]; then
    printf '# Authoritative card database served by the card server.\ncard_server=%s:55004\n' \
        "${CARD_SERVER_HOST}" > "${GAME_SERVER_CONFIG_FILE}"
    chown root:"${APP_USER}" "${GAME_SERVER_CONFIG_FILE}"
    chmod 0640 "${GAME_SERVER_CONFIG_FILE}"
fi

install -d -m 0750 -o root -g "${APP_USER}" "${TLS_DIR}"
{
    cat "${TLS_CERT_FILE}"
    cat "${TLS_CHAIN_FILE}"
} > "${TLS_DIR}/server-cert.pem"
chown root:root "${TLS_DIR}/server-cert.pem"
chmod 0644 "${TLS_DIR}/server-cert.pem"
install -m 0640 -o root -g "${APP_USER}" "${TLS_KEY_FILE}" "${TLS_DIR}/server-key.pem"
install -m 0644 -o root -g root "${TLS_CA_FILE}" "${TLS_DIR}/ca.pem"
{
    printf 'BAYOU_TLS_CERT_FILE=%s\n' "${TLS_DIR}/server-cert.pem"
    printf 'BAYOU_TLS_KEY_FILE=%s\n' "${TLS_DIR}/server-key.pem"
    printf 'BAYOU_TLS_CA_FILE=%s\n' "${TLS_DIR}/ca.pem"
    printf 'BAYOU_TLS_SERVER_NAME=%s\n' "${TLS_SERVER_NAME}"
} > "${TLS_ENV_FILE}"
chown root:"${APP_USER}" "${TLS_ENV_FILE}"
chmod 0640 "${TLS_ENV_FILE}"

for unit in accounts cardserver gameserver matchmaking; do
    install -m 0644 "${SCRIPT_DIR}/bayou-${unit}.service" \
        "/etc/systemd/system/bayou-${unit}.service"
done
install -m 0644 "${SCRIPT_DIR}/bayou-payments.service" \
    /etc/systemd/system/bayou-payments.service

previous_release="$(readlink -f /opt/bayou/current 2>/dev/null || true)"
rollback_release()
{
    echo "TLS deployment health check failed; restoring ${previous_release:-the previous service state}."
    if [[ -n "${previous_release}" && -d "${previous_release}" ]]; then
        ln -sfn "${previous_release}" /opt/bayou/current
        systemctl daemon-reload
        systemctl restart "${SERVICE_UNITS[@]}" || true
    else
        systemctl stop "${SERVICE_UNITS[@]}" || true
    fi
}

ln -sfn "${RELEASE_DIR}" /opt/bayou/current
systemctl daemon-reload
systemctl enable "${SERVICE_UNITS[@]}"
if ! systemctl restart "${SERVICE_UNITS[@]}"; then
    rollback_release
    exit 1
fi

sleep 3
for service in "${SERVICE_UNITS[@]}"; do
    if ! systemctl is-active --quiet "${service}"; then
        systemctl --no-pager --full status "${service}" || true
        rollback_release
        exit 1
    fi
done

for port in 55000 55001 55002 55004; do
    if ! timeout 10 openssl s_client \
        -connect "127.0.0.1:${port}" \
        -servername "${TLS_SERVER_NAME}" \
        -CAfile "${TLS_DIR}/ca.pem" \
        -verify_return_error </dev/null >/dev/null 2>&1; then
        echo "TLS health check failed on port ${port}."
        rollback_release
        exit 1
    fi
done

if command -v firewall-cmd >/dev/null 2>&1 && systemctl is-active --quiet firewalld; then
    firewall-cmd --permanent --add-port=55000-55002/tcp
    firewall-cmd --permanent --add-port=55004/tcp
    firewall-cmd --permanent --add-port=56000-65535/tcp
    firewall-cmd --reload
fi

systemctl --no-pager --full status "${SERVICE_UNITS[@]}"
