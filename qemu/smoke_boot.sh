#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
QEMU_BIN_PATH="${QEMU_BIN:-qemu-system-i386}"

TIMEOUT_SEC="${QEMU_SMOKE_TIMEOUT_SEC:-40}"
READY_MARKER="${QEMU_SMOKE_READY_MARKER:-OOS_BOOT_SHELL_READY}"
DEBUGCON_LOG="${QEMU_DEBUGCON_LOG:-${BUILD_DIR}/qemu-smoke-debugcon.log}"
STDOUT_LOG="${BUILD_DIR}/qemu-smoke-stdout.log"
STDERR_LOG="${BUILD_DIR}/qemu-smoke-stderr.log"

mkdir -p "${BUILD_DIR}"
rm -f "${DEBUGCON_LOG}" "${STDOUT_LOG}" "${STDERR_LOG}"

if command -v "${QEMU_BIN_PATH}" >/dev/null 2>&1; then
  "${QEMU_BIN_PATH}" --version | head -n 1
fi
echo "qemu smoke config: mode=headless timeout=${TIMEOUT_SEC}s debugcon=${DEBUGCON_LOG}"

QEMU_MODE=headless \
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
  if [[ -f "${DEBUGCON_LOG}" ]] && grep -Fq "${READY_MARKER}" "${DEBUGCON_LOG}"; then
    echo "qemu smoke test passed: found marker '${READY_MARKER}'"
    cleanup
    trap - EXIT
    exit 0
  fi

  if ! kill -0 "${QEMU_PID}" 2>/dev/null; then
    echo "qemu smoke test failed: qemu exited before marker '${READY_MARKER}'" >&2
    if [[ -f "${DEBUGCON_LOG}" ]]; then
      tail -n 120 "${DEBUGCON_LOG}" >&2 || true
    fi
    if [[ -f "${STDERR_LOG}" ]]; then
      tail -n 120 "${STDERR_LOG}" >&2 || true
    fi
    exit 1
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
