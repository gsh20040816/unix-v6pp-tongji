#!/usr/bin/env bash
set -euo pipefail

QEMU_BIN="${QEMU_BIN:-qemu-system-i386}"
QEMU_MODE="${QEMU_MODE:-curses}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${OOS_BUILD_DIR:-${ROOT_DIR}/build}"
IMG_PATH="${IMG_PATH:-${BUILD_DIR}/c.img}"
QEMU_DEBUGCON_LOG="${QEMU_DEBUGCON_LOG:-${BUILD_DIR}/qemu-debugcon.log}"

QEMU_TTY_STATE=""

restore_qemu_tty()
{
  if [[ -n "${QEMU_TTY_STATE}" ]]; then
    stty "${QEMU_TTY_STATE}"
    QEMU_TTY_STATE=""
  fi
}

prepare_stdio_tty_passthrough()
{
  if [[ "${QEMU_MODE}" != "stdio" && "${QEMU_MODE}" != "gdb-stdio" ]]; then
    return
  fi

  if [[ ! -t 0 ]]; then
    return
  fi

  QEMU_TTY_STATE="$(stty -g)"
  # Let Ctrl+C / Ctrl+\ reach the guest; keep Ctrl+] as the host-side interrupt.
  stty intr '^]'
  stty quit '^_'
  trap restore_qemu_tty EXIT
}

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
  stdio)
    DISPLAY_OPT="none"
    EXTRA_ARGS=(
      -monitor none
      -serial stdio
      -parallel none
      -debugcon "file:/dev/stderr"
    )
    ;;
  gui)
    DISPLAY_OPT="gtk"
    ;;
  headless)
    DISPLAY_OPT="none"
    mkdir -p "$(dirname "${QEMU_DEBUGCON_LOG}")"
    : > "${QEMU_DEBUGCON_LOG}"
    EXTRA_ARGS=(
      -monitor none
      -serial none
      -parallel none
      -debugcon "file:${QEMU_DEBUGCON_LOG}"
      -device "isa-debug-exit,iobase=0xf4,iosize=0x04"
    )
    ;;
  gdb-curses)
    DISPLAY_OPT="curses"
    EXTRA_ARGS=(-S -gdb tcp::1234)
    echo "Enabled gdbstub on :1234"
    ;;
  gdb-stdio)
    DISPLAY_OPT="none"
    EXTRA_ARGS=(
      -monitor none
      -serial stdio
      -parallel none
      -debugcon "file:/dev/stderr"
      -S -gdb tcp::1234
    )
    echo "Enabled gdbstub on :1234"
    ;;
  *)
    echo "qemu: unsupported QEMU_MODE: ${QEMU_MODE} (supported: curses, stdio, gui, headless, gdb-curses, gdb-stdio)" >&2
    exit 2
    ;;
esac

prepare_stdio_tty_passthrough

QEMU_CMD=(
  "${QEMU_BIN}"
  -machine pc \
  -m 32M \
  -drive "file=${IMG_PATH},format=raw,if=ide,index=0,media=disk" \
  -display "${DISPLAY_OPT}" \
  -boot c \
  "${EXTRA_ARGS[@]}"
)

if [[ -n "${QEMU_TTY_STATE}" ]]; then
  "${QEMU_CMD[@]}"
  exit $?
fi

exec "${QEMU_CMD[@]}"
