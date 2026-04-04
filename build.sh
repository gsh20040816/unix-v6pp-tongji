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
  remote command  cd /d Z:\UNIX V6++V1\oos\tools && call oosvars_mingw.bat && call build.bat

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

ensure_linux_fs_edit_tools() {
  local fs_root="$script_dir/tools/v6pp-fs-edit-2022"
  local build_dir="$script_dir/.build-cache/v6pp-fs-edit-2022-cmake"
  local workspace="$fs_root/workspace"
  local linux_bin="$workspace/linux-bin"
  local filescanner="$linux_bin/filescanner"
  local fsedit="$linux_bin/fsedit"

  if ! command -v cmake >/dev/null 2>&1; then
    echo "missing required command: cmake (needed to build Linux filescanner/fsedit)" >&2
    return 1
  fi

  mkdir -p "$build_dir"

  echo "Configuring Linux fs tools with CMake"
  cmake -S "$fs_root" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release

  echo "Building Linux filescanner/fsedit with CMake"
  cmake --build "$build_dir" --target filescanner fsedit --parallel

  if [[ ! -x "$filescanner" || ! -x "$fsedit" ]]; then
    echo "failed to build Linux filescanner/fsedit with CMake" >&2
    return 1
  fi
}

generate_disk_image_on_linux() {
  local workspace="$script_dir/tools/v6pp-fs-edit-2022/workspace"
  local filescanner="$workspace/linux-bin/filescanner"
  local fsedit="$workspace/linux-bin/fsedit"
  local workspace_img="$workspace/c.img"
  local final_img="$script_dir/targets/UNIXV6++/c.img"

  if [[ ! -f "$workspace/boot.bin" ]]; then
    echo "missing required file: $workspace/boot.bin" >&2
    return 1
  fi

  if [[ ! -f "$workspace/kernel.bin" ]]; then
    echo "missing required file: $workspace/kernel.bin" >&2
    return 1
  fi

  if [[ ! -d "$workspace/programs" ]]; then
    echo "missing required directory: $workspace/programs" >&2
    return 1
  fi

  echo "Generating disk image on Linux with v6pp-fs-edit-2022"
  (
    cd "$workspace"
    "$filescanner" | "$fsedit" c.img c
  )

  mkdir -p "$(dirname "$final_img")"
  cp "$workspace_img" "$final_img"
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
default_remote_command="cd /d ${remote_tools} && call oosvars_mingw.bat && call build.bat"
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
    remote_command="cd /d ${remote_tools} && call oosvars_mingw.bat && call clean.bat && call build.bat"
    echo "Detected header/makefile changes, forcing clean rebuild."
  fi
fi

if [[ -n "$make_jobs" ]]; then
  remote_command="set OOS_MAKE_JOBS=${make_jobs} && ${remote_command}"
fi

echo "Building on Windows host ${OOS_WIN_HOST:-win}"
if bash "$ssh_win" "cmd.exe /c \"$remote_command\""; then
  ensure_linux_fs_edit_tools
  generate_disk_image_on_linux

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
