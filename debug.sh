#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  bash debug.sh
  bash debug.sh [command...]

Defaults:
  work dir        ./targets/UNIXV6++
  BXSHARE         /usr/share/bochs
  command         bochs-gdb -q -f bochsrc.bxrc

Environment overrides:
  OOS_LINUX_BXSHARE
  OOS_LINUX_DEBUG_COMMAND
EOF
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
target_dir="$script_dir/targets/UNIXV6++"
linux_bxshare="${OOS_LINUX_BXSHARE:-/usr/share/bochs}"
debug_command="${OOS_LINUX_DEBUG_COMMAND:-bochs-gdb -q -f bochsrc.bxrc}"

if (($# > 0)) && [[ "$1" == "-h" || "$1" == "--help" ]]; then
  usage
  exit 0
fi

if (($# > 0)); then
  debug_command="$*"
fi

if [[ ! -d "$target_dir" ]]; then
  echo "target directory not found: $target_dir" >&2
  exit 1
fi

cd "$target_dir"
export BXSHARE="$linux_bxshare"

echo "Starting bochs-gdb in $target_dir"
exec bash -lc "$debug_command"
