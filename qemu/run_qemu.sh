#!/usr/bin/env bash
set -euo pipefail

QEMU_BIN="${QEMU_BIN:-qemu-system-i386}"
QEMU_MODE="${QEMU_MODE:-curses}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
IMG_PATH="${ROOT_DIR}/build/c.img"

if [[ ! -f "${IMG_PATH}" ]]; then
  echo "qemu: image not found: ${IMG_PATH}" >&2
  exit 1
fi

DISPLAY_OPT="curses"
EXTRA_ARGS=()

case "${QEMU_MODE}" in
  curses)
    DISPLAY_OPT="curses"
    ;;
  gui)
    DISPLAY_OPT="gtk"
    ;;
  gdb-curses)
    DISPLAY_OPT="curses"
    EXTRA_ARGS=(-S -gdb tcp::1234)
    echo "Enabled gdbstub on :1234"
    ;;
  *)
    echo "qemu: unsupported QEMU_MODE: ${QEMU_MODE}" >&2
    exit 2
    ;;
esac

exec "${QEMU_BIN}" \
  -machine pc \
  -m 32M \
  -drive "file=${IMG_PATH},format=raw,if=ide,index=0,media=disk" \
  -display "${DISPLAY_OPT}" \
  -boot c \
  "${EXTRA_ARGS[@]}"
