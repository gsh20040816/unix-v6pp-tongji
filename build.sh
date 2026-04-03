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
  remote command  cd /d Z:\UNIX V6++V1\oos\tools && call oosvars_mingw.bat && set OOS_MAKE_JOBS=<n> && set MAKEFLAGS=-j<n> && call all.bat

Environment overrides:
  OOS_WIN_HOST
  OOS_WIN_REPO
  OOS_WIN_BUILD_COMMAND
  OOS_MAKE_JOBS
EOF
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ssh_win="$script_dir/tools/ssh-win.sh"

hash_workspace_headers() {
  if ! command -v sha1sum >/dev/null 2>&1; then
    return 1
  fi

  find "$script_dir/src" -type f \
    \( -name '*.h' -o -name '*.inc' -o -name 'Makefile' -o -name 'makefile' -o -name 'Makefile.inc' \) \
    -print0 \
    | sort -z \
    | xargs -0 sha1sum \
    | sha1sum \
    | awk '{print $1}'
}

resolve_make_jobs() {
  local jobs="${OOS_MAKE_JOBS:-}"

  if [[ -z "$jobs" ]]; then
    if command -v nproc >/dev/null 2>&1; then
      jobs="$(nproc)"
    else
      jobs=4
    fi
  fi

  if [[ ! "$jobs" =~ ^[0-9]+$ ]] || [[ "$jobs" -lt 1 ]]; then
    echo "invalid OOS_MAKE_JOBS: $jobs" >&2
    return 1
  fi

  printf '%s' "$jobs"
}

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
make_jobs="$(resolve_make_jobs)"
default_base_command="cd /d ${remote_tools} && call oosvars_mingw.bat && set OOS_MAKE_JOBS=${make_jobs} && set MAKEFLAGS=-j${make_jobs}"

header_hash_file="$script_dir/.build-cache/header-fingerprint.sha1"
mkdir -p "$(dirname "$header_hash_file")"

current_header_hash=""
if current_header_hash="$(hash_workspace_headers)"; then
  previous_header_hash=""
  if [[ -f "$header_hash_file" ]]; then
    previous_header_hash="$(<"$header_hash_file")"
  fi

  if [[ "$current_header_hash" != "$previous_header_hash" ]]; then
    needs_clean=1
    echo "Detected header/makefile changes; forcing full rebuild for ABI safety."
  else
    needs_clean=0
  fi
else
  needs_clean=0
  echo "warning: sha1sum unavailable, skip header fingerprint check" >&2
fi

default_build_command="${default_base_command} && call all.bat"
if [[ "${needs_clean}" -eq 1 ]]; then
  default_build_command="${default_base_command} && call clean.bat && call all.bat"
fi

remote_command="${OOS_WIN_BUILD_COMMAND:-$default_build_command}"

if (($# > 0)); then
  remote_command="$*"
fi

ensure_fs_edit_tools

echo "Building on Windows host ${OOS_WIN_HOST:-win}"
"$ssh_win" "cmd.exe /c \"$remote_command\""

if [[ "$#" -eq 0 ]] && [[ -z "${OOS_WIN_BUILD_COMMAND:-}" ]] && [[ -n "$current_header_hash" ]]; then
  printf '%s\n' "$current_header_hash" > "$header_hash_file"
fi
