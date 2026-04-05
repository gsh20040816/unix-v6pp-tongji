#!/usr/bin/env bash
set -euo pipefail

QEMU_BIN="${QEMU_BIN:-qemu-system-i386}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
IMG_PATH="${ROOT_DIR}/build/c.img"

if [[ ! -f "${IMG_PATH}" ]]; then
  echo "qemu: image not found: ${IMG_PATH}" >&2
  exit 1
fi

exec "${QEMU_BIN}" \
  -machine pc \
  -m 32M \
  -drive "file=${IMG_PATH},format=raw,if=ide,index=0,media=disk" \
  -display curses \
  -boot c
