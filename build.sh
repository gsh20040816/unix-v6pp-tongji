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

ensure_fs_edit_tools() {
  local workspace="$script_dir/tools/v6pp-fs-edit-2022/workspace"
  local fs_zip="$script_dir/tools/v6pp-fs-edit-2022.zip"
  local filescanner="$workspace/filescanner.exe"
  local fsedit="$workspace/fsedit.exe"

  if [[ -f "$filescanner" && -f "$fsedit" ]]; then
    return 0
  fi

  if [[ ! -f "$fs_zip" ]]; then
    echo "missing required tools: filescanner.exe/fsedit.exe and archive not found: $fs_zip" >&2
    return 1
  fi

  if ! command -v unzip >/dev/null 2>&1; then
    echo "missing required command: unzip (needed to restore filescanner.exe/fsedit.exe)" >&2
    return 1
  fi

  mkdir -p "$workspace"
  unzip -jo "$fs_zip" \
    "v6pp-fs-edit-2022/workspace/filescanner.exe" \
    "v6pp-fs-edit-2022/workspace/fsedit.exe" \
    -d "$workspace" >/dev/null

  if [[ ! -f "$filescanner" || ! -f "$fsedit" ]]; then
    echo "failed to restore filescanner.exe/fsedit.exe from $fs_zip" >&2
    return 1
  fi
}

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

ensure_fs_edit_tools

echo "Building on Windows host ${OOS_WIN_HOST:-win}"
exec "$ssh_win" "cmd.exe /c \"$remote_command\""
