#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  bash build.sh
  bash build.sh clean
  bash build.sh [build-cmd...]

Defaults:
  local command   make -C src build

Environment overrides:
  OOS_LINUX_BUILD_COMMAND
  OOS_MAKE_JOBS
EOF
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
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

refresh_linux_image_inputs() {
  local kernel_exe="$script_dir/targets/objs/kernel.exe"
  local kernel_bin="$script_dir/targets/objs/kernel.bin"
  local boot_bin="$script_dir/targets/objs/boot.bin"
  local kernel_size_inc="$script_dir/src/boot/kernel_size.inc"
  local workspace="$script_dir/tools/v6pp-fs-edit-2022/workspace"

  if ! command -v objcopy >/dev/null 2>&1; then
    echo "missing required command: objcopy (needed to export kernel.bin)" >&2
    return 1
  fi

  if ! command -v nasm >/dev/null 2>&1; then
    echo "missing required command: nasm (needed to build boot.bin)" >&2
    return 1
  fi

  if [[ ! -f "$kernel_exe" ]]; then
    echo "missing required file: $kernel_exe" >&2
    return 1
  fi

  echo "Exporting kernel.bin from latest kernel.exe"
  objcopy -O binary "$kernel_exe" "$kernel_bin"

  local kernel_size
  local kernel_sectors
  local kernel_size_line
  local existing_kernel_size_line=""
  kernel_size=$(stat -c '%s' "$kernel_bin")
  kernel_sectors=$(( (kernel_size + 511) / 512 ))
  kernel_size_line="KERNEL_SIZE equ ${kernel_sectors}"

  if [[ -f "$kernel_size_inc" ]]; then
    existing_kernel_size_line="$(tr -d '\r\n' < "$kernel_size_inc")"
  fi
  if [[ "$existing_kernel_size_line" != "$kernel_size_line" ]]; then
    printf '%s\r\n' "$kernel_size_line" > "$kernel_size_inc"
  fi

  echo "Building boot.bin with KERNEL_SIZE=$kernel_sectors"
  (
    cd "$script_dir/src/boot"
    nasm -f bin boot.s -o "$boot_bin"
  )

  mkdir -p "$workspace"
  cp "$boot_bin" "$workspace/boot.bin"
  cp "$kernel_bin" "$workspace/kernel.bin"
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

cleanup_disk_images() {
  local workspace_img="$script_dir/tools/v6pp-fs-edit-2022/workspace/c.img"
  local final_img="$script_dir/targets/UNIXV6++/c.img"

  echo "Cleaning disk images on Linux"
  rm -f "$workspace_img" "$final_img"
}

if (($# > 0)) && [[ "$1" == "-h" || "$1" == "--help" ]]; then
  usage
  exit 0
fi

local_build_command="${OOS_LINUX_BUILD_COMMAND:-make -C src build}"
user_provided_command=false
env_provided_command=false
make_jobs="${OOS_MAKE_JOBS:-}"
current_fingerprint=""

if [[ -n "$make_jobs" ]]; then
  if ! [[ "$make_jobs" =~ ^[1-9][0-9]*$ ]]; then
    echo "invalid make jobs value: $make_jobs (must be positive integer)" >&2
    exit 1
  fi
fi

if [[ -n "${OOS_LINUX_BUILD_COMMAND:-}" ]]; then
  env_provided_command=true
fi

if (($# > 0)); then
  if [[ "$1" == "clean" && $# -eq 1 ]]; then
    echo "Cleaning local build artifacts"
    (
      cd "$script_dir"
      make -C src clean || true
    )
    cleanup_disk_images
    rm -f "$fingerprint_file"
    exit 0
  fi

  local_build_command="$*"
  user_provided_command=true
fi

if [[ "$user_provided_command" == false && "$env_provided_command" == false ]]; then
  current_fingerprint="$(compute_build_fingerprint)"
  previous_fingerprint=""
  if [[ -f "$fingerprint_file" ]]; then
    previous_fingerprint="$(cat "$fingerprint_file")"
  fi

  if [[ "$current_fingerprint" != "$previous_fingerprint" ]]; then
    echo "Detected header/makefile changes, forcing clean rebuild."
    (
      cd "$script_dir"
      make -C src clean || true
    )
  fi
fi

if [[ -n "$make_jobs" && "$user_provided_command" == false ]]; then
  local_build_command="make -C src -j${make_jobs} build"
fi

should_generate_image=true
if [[ "$local_build_command" == *"clean"* && "$local_build_command" != *"build"* && "$local_build_command" != *"all"* ]]; then
  should_generate_image=false
fi

echo "Building on Linux host"
if (
  cd "$script_dir"
  bash -lc "$local_build_command"
); then
  if [[ "$should_generate_image" == true ]]; then
    refresh_linux_image_inputs
    ensure_linux_fs_edit_tools
    generate_disk_image_on_linux
  else
    cleanup_disk_images
  fi

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
