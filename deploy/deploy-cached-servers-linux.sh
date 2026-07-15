#!/usr/bin/env bash
set -euo pipefail

# This script owns and force-cleans only its marker-protected cache checkout.
# Build and dependency caches remain outside that checkout for fast repeat runs.
umask 077

REPOSITORY_URL="https://github.com/shoejunk/bayou.git"
DEPLOY_COMMIT="${DEPLOY_COMMIT:-}"

if [[ ! "${DEPLOY_COMMIT}" =~ ^[0-9a-f]{40}$ ]]; then
    echo "DEPLOY_COMMIT must be the full 40-character lowercase commit at origin/main."
    exit 1
fi

for command_name in git cmake sudo openssl getent id cut install; do
    if ! command -v "${command_name}" >/dev/null 2>&1; then
        echo "Required command is unavailable: ${command_name}"
        exit 1
    fi
done

USER_HOME="$(getent passwd "$(id -u)" | cut -d: -f6)"
if [[ -z "${USER_HOME}" ]]; then
    echo "Could not determine the current user's home directory."
    exit 1
fi

CACHE_ROOT="${BAYOU_BUILD_CACHE_ROOT:-${USER_HOME}/.cache/bayou-server-build}"
install -d -m 0700 "${CACHE_ROOT}"
CACHE_ROOT="$(cd "${CACHE_ROOT}" && pwd -P)"
if [[ ! -O "${CACHE_ROOT}" ]]; then
    echo "Build cache must be owned by the deployment user: ${CACHE_ROOT}"
    exit 1
fi
chmod 0700 "${CACHE_ROOT}"

SOURCE_DIR="${CACHE_ROOT}/source"
BUILD_DIR="${CACHE_ROOT}/build"
CPM_CACHE_DIR="${CACHE_ROOT}/cpm"
CCACHE_DIR="${CACHE_ROOT}/ccache"
ARTIFACT_ROOT="${CACHE_ROOT}/artifacts"
ARTIFACT_DIR="${ARTIFACT_ROOT}/${DEPLOY_COMMIT}"
MARKER_FILE="${CACHE_ROOT}/.bayou-server-build-cache-v1"
EXPECTED_MARKER="bayou-server-build-cache-v1:${REPOSITORY_URL}"

for managed_path in \
    "${SOURCE_DIR}" "${BUILD_DIR}" "${CPM_CACHE_DIR}" \
    "${CCACHE_DIR}" "${ARTIFACT_ROOT}" "${ARTIFACT_DIR}"; do
    if [[ -L "${managed_path}" ]]; then
        echo "Refusing symlinked managed cache path: ${managed_path}"
        exit 1
    fi
done

if [[ -d "${SOURCE_DIR}/.git" ]]; then
    if [[ ! -f "${MARKER_FILE}" ]] ||
        [[ "$(<"${MARKER_FILE}")" != "${EXPECTED_MARKER}" ]]; then
        echo "Refusing to clean an unmarked source directory: ${SOURCE_DIR}"
        exit 1
    fi
else
    if [[ -e "${SOURCE_DIR}" ]]; then
        echo "Refusing to replace a non-repository path: ${SOURCE_DIR}"
        exit 1
    fi
    git -c core.hooksPath=/dev/null clone --no-checkout --origin origin \
        "${REPOSITORY_URL}" "${SOURCE_DIR}"
    printf '%s\n' "${EXPECTED_MARKER}" > "${MARKER_FILE}"
    chmod 0600 "${MARKER_FILE}"
fi

git_command=(git -c core.hooksPath=/dev/null -C "${SOURCE_DIR}")
actual_origin="$("${git_command[@]}" remote get-url origin)"
if [[ "${actual_origin}" != "${REPOSITORY_URL}" ]]; then
    echo "Refusing unexpected origin URL: ${actual_origin}"
    exit 1
fi

"${git_command[@]}" fetch --prune --no-tags origin \
    '+refs/heads/main:refs/remotes/origin/main'
fetched_commit="$("${git_command[@]}" rev-parse --verify \
    'refs/remotes/origin/main^{commit}')"
if [[ "${DEPLOY_COMMIT}" != "${fetched_commit}" ]]; then
    echo "Requested commit is not the current origin/main tip."
    echo "Requested: ${DEPLOY_COMMIT}"
    echo "Fetched:   ${fetched_commit}"
    exit 1
fi

# The marker and exact origin check above make destructive cleanup safe here:
# SOURCE_DIR is a dedicated deployment checkout, never a developer workspace.
"${git_command[@]}" checkout --detach --force "${DEPLOY_COMMIT}"
"${git_command[@]}" clean -ffdqx

install -d -m 0700 \
    "${BUILD_DIR}" "${CPM_CACHE_DIR}" "${CCACHE_DIR}" "${ARTIFACT_ROOT}"

artifact_is_complete=true
for executable in accounts cardserver gameserver matchmaking; do
    if [[ ! -x "${ARTIFACT_DIR}/bin/${executable}" ]]; then
        artifact_is_complete=false
    fi
done
if [[ ! -f "${ARTIFACT_DIR}/.bayou-commit" ]] ||
    [[ "$(<"${ARTIFACT_DIR}/.bayou-commit" 2>/dev/null || true)" != "${DEPLOY_COMMIT}" ]]; then
    artifact_is_complete=false
fi

if [[ "${artifact_is_complete}" != true ]]; then
    if [[ -e "${ARTIFACT_DIR}" ]]; then
        echo "Refusing incomplete cached artifact directory: ${ARTIFACT_DIR}"
        echo "Remove that one directory manually after inspecting it, then retry."
        exit 1
    fi

    staging_dir="${ARTIFACT_ROOT}/.staging-${DEPLOY_COMMIT}-$$"
    install -d -m 0700 "${staging_dir}"
    cleanup_staging()
    {
        case "${staging_dir}" in
            "${ARTIFACT_ROOT}"/.staging-*) rm -rf -- "${staging_dir}" ;;
            *) echo "Refusing unsafe staging cleanup path: ${staging_dir}" >&2 ;;
        esac
    }
    trap cleanup_staging EXIT

    export CPM_SOURCE_CACHE="${CPM_CACHE_DIR}"
    export CCACHE_DIR
    BUILD_DIR="${BUILD_DIR}" INSTALL_PREFIX="${staging_dir}" \
        bash "${SOURCE_DIR}/deploy/build-servers-linux.sh"

    for executable in accounts cardserver gameserver matchmaking; do
        if [[ ! -x "${staging_dir}/bin/${executable}" ]]; then
            echo "Build did not produce ${executable}."
            exit 1
        fi
    done
    printf '%s\n' "${DEPLOY_COMMIT}" > "${staging_dir}/.bayou-commit"
    mv "${staging_dir}" "${ARTIFACT_DIR}"
    trap - EXIT
else
    echo "Reusing verified cached artifacts for ${DEPLOY_COMMIT}."
fi

if [[ "${BAYOU_BUILD_ONLY:-0}" == 1 ]]; then
    printf 'Prepared server release at %s\n' "${ARTIFACT_DIR}"
    exit 0
fi

for variable in TLS_CERT_FILE TLS_CHAIN_FILE TLS_KEY_FILE TLS_CA_FILE TLS_SERVER_NAME; do
    if [[ -z "${!variable:-}" ]]; then
        echo "${variable} is required unless BAYOU_BUILD_ONLY=1."
        exit 1
    fi
done
for variable in TLS_CERT_FILE TLS_CHAIN_FILE TLS_KEY_FILE TLS_CA_FILE; do
    if [[ "${!variable}" != /* ]]; then
        echo "${variable} must be an absolute path."
        exit 1
    fi
done

VERSION="${VERSION:-${DEPLOY_COMMIT:0:7}-$(date -u +%Y%m%d%H%M%S)}"
SOURCE_PREFIX="${ARTIFACT_DIR}"
export VERSION SOURCE_PREFIX
export TLS_CERT_FILE TLS_CHAIN_FILE TLS_KEY_FILE TLS_CA_FILE TLS_SERVER_NAME

cd "${SOURCE_DIR}"
sudo --preserve-env=VERSION,SOURCE_PREFIX,TLS_CERT_FILE,TLS_CHAIN_FILE,TLS_KEY_FILE,TLS_CA_FILE,TLS_SERVER_NAME \
    bash deploy/install-servers-linux.sh
