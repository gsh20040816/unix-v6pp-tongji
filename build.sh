#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  bash build.sh
  bash build.sh [remote-cmd...]

Defaults:
  Windows host    win
  Windows repo    Z:\UNIX V6++V1\oos
  remote command  cd /d Z:\UNIX V6++V1\oos\tools && call oosvars_mingw.bat && call all.bat

Environment overrides:
  OOS_WIN_HOST
  OOS_WIN_REPO
  OOS_WIN_BUILD_COMMAND
EOF
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ssh_win="$script_dir/tools/ssh-win.sh"

if [[ ! -x "$ssh_win" ]]; then
  echo "ssh helper not found or not executable: $ssh_win" >&2
  exit 1
fi

if (($# > 0)) && [[ "$1" == "-h" || "$1" == "--help" ]]; then
  usage
  exit 0
fi

remote_repo="${OOS_WIN_REPO:-Z:\\UNIX V6++V1\\oos}"
remote_tools="${remote_repo}\\tools"
remote_command="${OOS_WIN_BUILD_COMMAND:-cd /d ${remote_tools} && call oosvars_mingw.bat && call all.bat}"

if (($# > 0)); then
  remote_command="$*"
fi

echo "Building on Windows host ${OOS_WIN_HOST:-win}"
exec "$ssh_win" "cmd.exe /c \"$remote_command\""
