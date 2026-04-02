#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  bash tools/ssh-win.sh [remote-command...]
  bash tools/ssh-win.sh --shell
  bash tools/ssh-win.sh --host user@host [remote-command...]

Defaults:
  host  win

Environment overrides:
  OOS_WIN_HOST
EOF
}

remote="${OOS_WIN_HOST:-win}"
interactive_shell=0

while (($# > 0)); do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    --shell)
      interactive_shell=1
      shift
      ;;
    --host)
      if (($# < 2)); then
        echo "--host requires a user@host argument" >&2
        exit 1
      fi
      remote="$2"
      shift 2
      ;;
    *)
      break
      ;;
  esac
done

ssh_base=(
  ssh
  -o StrictHostKeyChecking=accept-new
  "$remote"
)

if ((interactive_shell)); then
  exec "${ssh_base[@]}"
fi

exec "${ssh_base[@]}" "$@"
