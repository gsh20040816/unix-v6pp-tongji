#!/usr/bin/env bash
set -uo pipefail

BOCHS_BIN="${BOCHS_BIN:-bochs-gdb}"

if [[ $# -ne 1 ]]; then
  echo "usage: $0 <bochsrc-file>" >&2
  exit 2
fi

BOCHSRC_FILE="$1"

"${BOCHS_BIN}" -q -f "${BOCHSRC_FILE}"
BOCHS_RESULT=$?

if [[ ${BOCHS_RESULT} -eq 0 || ${BOCHS_RESULT} -eq 1 ]]; then
  if [[ ${BOCHS_RESULT} -eq 1 ]]; then
    echo "bochs: soft power off (exit code 1), treated as success."
  fi
  exit 0
fi

exit "${BOCHS_RESULT}"
