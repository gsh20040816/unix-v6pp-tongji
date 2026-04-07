#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${OOS_BUILD_DIR:-${ROOT_DIR}/build}"
QEMU_BIN_PATH="${QEMU_BIN:-qemu-system-i386}"

TIMEOUT_SEC="${QEMU_SMOKE_TIMEOUT_SEC:-40}"
READY_MARKER="${QEMU_SMOKE_READY_MARKER:-OOS_BOOT_SHELL_READY}"
READY_EXIT_CODE="${QEMU_SMOKE_READY_EXIT_CODE:-33}"
DEBUGCON_LOG="${QEMU_DEBUGCON_LOG:-${BUILD_DIR}/qemu-smoke-debugcon.log}"
STDOUT_LOG="${BUILD_DIR}/qemu-smoke-stdout.log"
STDERR_LOG="${BUILD_DIR}/qemu-smoke-stderr.log"

mkdir -p "${BUILD_DIR}"
rm -f "${DEBUGCON_LOG}" "${STDOUT_LOG}" "${STDERR_LOG}"

if command -v "${QEMU_BIN_PATH}" >/dev/null 2>&1; then
  "${QEMU_BIN_PATH}" --version | head -n 1
fi
echo "qemu smoke config: mode=headless timeout=${TIMEOUT_SEC}s ready_exit_code=${READY_EXIT_CODE} debugcon=${DEBUGCON_LOG}"

QEMU_MODE=headless \
OOS_BUILD_DIR="${BUILD_DIR}" \
QEMU_DEBUGCON_LOG="${DEBUGCON_LOG}" \
bash "${SCRIPT_DIR}/run_qemu.sh" \
  >"${STDOUT_LOG}" 2>"${STDERR_LOG}" &
QEMU_PID=$!

cleanup() {
  if kill -0 "${QEMU_PID}" 2>/dev/null; then
    kill -TERM "${QEMU_PID}" 2>/dev/null || true
    wait "${QEMU_PID}" 2>/dev/null || true
  fi
}
trap cleanup EXIT

DEADLINE=$((SECONDS + TIMEOUT_SEC))
while (( SECONDS < DEADLINE )); do
  if ! kill -0 "${QEMU_PID}" 2>/dev/null; then
    set +e
    wait "${QEMU_PID}"
    QEMU_EXIT_CODE=$?
    set -e

    if [[ "${QEMU_EXIT_CODE}" -eq "${READY_EXIT_CODE}" ]]; then
      echo "qemu smoke test passed: qemu exited with ready code ${READY_EXIT_CODE}"
      trap - EXIT
      exit 0
    fi

    if [[ -f "${DEBUGCON_LOG}" ]] && grep -Fq "${READY_MARKER}" "${DEBUGCON_LOG}"; then
      echo "qemu smoke test passed: found marker '${READY_MARKER}'"
      trap - EXIT
      exit 0
    fi

    echo "qemu smoke test failed: qemu exited before ready (exit=${QEMU_EXIT_CODE}, expected=${READY_EXIT_CODE})" >&2
    if [[ -f "${DEBUGCON_LOG}" ]]; then
      tail -n 120 "${DEBUGCON_LOG}" >&2 || true
    fi
    if [[ -f "${STDERR_LOG}" ]]; then
      tail -n 120 "${STDERR_LOG}" >&2 || true
    fi
    exit 1
  fi

  if [[ -f "${DEBUGCON_LOG}" ]] && grep -Fq "${READY_MARKER}" "${DEBUGCON_LOG}"; then
    echo "qemu smoke test passed: found marker '${READY_MARKER}'"
    cleanup
    trap - EXIT
    exit 0
  fi

  sleep 1
done

echo "qemu smoke test failed: timeout after ${TIMEOUT_SEC}s waiting for marker '${READY_MARKER}'" >&2
if [[ -f "${DEBUGCON_LOG}" ]]; then
  echo "debugcon size: $(wc -c < "${DEBUGCON_LOG}") bytes" >&2
fi
if [[ -f "${DEBUGCON_LOG}" ]]; then
  tail -n 120 "${DEBUGCON_LOG}" >&2 || true
fi
if [[ -f "${STDERR_LOG}" ]]; then
  tail -n 120 "${STDERR_LOG}" >&2 || true
fi
exit 1
