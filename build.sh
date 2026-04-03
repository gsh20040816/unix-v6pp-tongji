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
  OOS_MAKE_JOBS (or OOS_WIN_MAKE_JOBS)
EOF
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ssh_win="$script_dir/tools/ssh-win.sh"
build_cache_dir="$script_dir/.build-cache"
fingerprint_file="$build_cache_dir/build-inputs.sha1"

compute_build_fingerprint() {
  local source_root="$script_dir/src"

  LC_ALL=C find "$source_root" -type f \
    \( -name '*.h' -o -name '*.inc' -o -name '[Mm]akefile' \) \
    ! -path "$source_root/boot/kernel_size.inc" -print0 \
    | LC_ALL=C sort -z \
    | xargs -0 sha1sum \
    | sha1sum \
    | awk '{print $1}'
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

if [[ ! -f "$ssh_win" ]]; then
  echo "ssh helper not found: $ssh_win" >&2
  exit 1
fi

if (($# > 0)) && [[ "$1" == "-h" || "$1" == "--help" ]]; then
  usage
  exit 0
fi

remote_repo="${OOS_WIN_REPO:-Z:\\UNIX V6++V1\\oos}"
remote_tools="${remote_repo}\\tools"
default_remote_command="cd /d ${remote_tools} && call oosvars_mingw.bat && call all.bat"
remote_command="${OOS_WIN_BUILD_COMMAND:-$default_remote_command}"
user_provided_command=false
env_provided_command=false
make_jobs="${OOS_WIN_MAKE_JOBS:-${OOS_MAKE_JOBS:-}}"
current_fingerprint=""

if [[ -n "$make_jobs" ]]; then
  if ! [[ "$make_jobs" =~ ^[1-9][0-9]*$ ]]; then
    echo "invalid make jobs value: $make_jobs (must be positive integer)" >&2
    exit 1
  fi
fi

if [[ -n "${OOS_WIN_BUILD_COMMAND:-}" ]]; then
  env_provided_command=true
fi

if (($# > 0)); then
  remote_command="$*"
  user_provided_command=true
fi

if [[ "$user_provided_command" == false && "$env_provided_command" == false ]]; then
  current_fingerprint="$(compute_build_fingerprint)"
  previous_fingerprint=""
  if [[ -f "$fingerprint_file" ]]; then
    previous_fingerprint="$(cat "$fingerprint_file")"
  fi

  if [[ "$current_fingerprint" != "$previous_fingerprint" ]]; then
    remote_command="cd /d ${remote_tools} && call oosvars_mingw.bat && call clean.bat && call all.bat"
    echo "Detected header/makefile changes, forcing clean rebuild."
  fi
fi

if [[ -n "$make_jobs" ]]; then
  remote_command="set OOS_MAKE_JOBS=${make_jobs} && ${remote_command}"
fi

ensure_fs_edit_tools

echo "Building on Windows host ${OOS_WIN_HOST:-win}"
if bash "$ssh_win" "cmd.exe /c \"$remote_command\""; then
  if [[ "$user_provided_command" == false && "$env_provided_command" == false ]]; then
    mkdir -p "$build_cache_dir"
    post_fingerprint="$(compute_build_fingerprint)"
    printf '%s\n' "$post_fingerprint" > "$fingerprint_file"
  fi
  exit 0
else
  build_status=$?
  exit $build_status
fi
