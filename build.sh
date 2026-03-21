#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
tools_dir="$script_dir/tools"

if [[ ! -d "$tools_dir" ]]; then
  echo "tools directory not found: $tools_dir" >&2
  exit 1
fi

cd "$tools_dir"

# Keep Git Bash's sh.exe out of PATH so MinGW make stays in Windows cmd mode.
MSYS2_ARG_CONV_EXCL='*' cmd.exe /c "set PATH=C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem && call oosvars_mingw.bat && call all.bat"
