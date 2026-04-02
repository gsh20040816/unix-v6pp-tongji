#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  bash run.sh
  bash run.sh [command...]

Defaults:
  work dir        ./targets/UNIXV6++
  BXSHARE         /usr/share/bochs
  command         bochs -q -f bochsrc_nodebug.bxrc

Environment overrides:
  OOS_LINUX_BXSHARE
  OOS_LINUX_RUN_COMMAND
EOF
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
target_dir="$script_dir/targets/UNIXV6++"
linux_bxshare="${OOS_LINUX_BXSHARE:-/usr/share/bochs}"
run_command="${OOS_LINUX_RUN_COMMAND:-bochs -q -f bochsrc_nodebug.bxrc}"

if (($# > 0)) && [[ "$1" == "-h" || "$1" == "--help" ]]; then
  usage
  exit 0
fi

if (($# > 0)); then
  run_command="$*"
fi

if [[ ! -d "$target_dir" ]]; then
  echo "target directory not found: $target_dir" >&2
  exit 1
fi

cd "$target_dir"
export BXSHARE="$linux_bxshare"

echo "Starting bochs in $target_dir"
exec bash -lc "$run_command"
