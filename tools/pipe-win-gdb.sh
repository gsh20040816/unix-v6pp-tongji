#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# cpptools may invoke pipeProgram again as: `pipe-win-gdb.sh "kill -5 <pid>"`.
# In that case the request is local and should not be forwarded to Windows.
if (($# == 1)) && [[ "$1" == kill\ * ]]; then
  exec bash -lc "$1"
fi

exec python3 "$script_dir/pipe_win_gdb_proxy.py" "$@"
