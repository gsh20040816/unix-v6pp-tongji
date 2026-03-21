#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  bash runhost.sh [user@host] [remote-command...]
  bash runhost.sh --shell [user@host]
  bash runhost.sh --host user@host [remote-command...]

Defaults:
  host           gsh@10.200.65.1
  share root     ~/shared
  remote command cd "$HOME/shared/UNIX V6++V1/oos/targets/UNIXV6++" && export BXSHARE=/usr/share/bochs && bochs-gdb -q -f bochsrc_nodebug.bxrc
  key path       ~/.ssh/oos_host_ed25519

Environment overrides:
  OOS_HOST
  OOS_HOST_COMMAND
  OOS_HOST_KEY
  OOS_HOST_SHARE_ROOT
  OOS_HOST_BXSHARE
EOF
}

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
workspace_name="$(basename "$(dirname "$script_dir")")"
repo_name="$(basename "$script_dir")"

remote="${OOS_HOST:-gsh@10.200.65.1}"
key_path="${OOS_HOST_KEY:-$HOME/.ssh/oos_host_ed25519}"
remote_share_root="${OOS_HOST_SHARE_ROOT:-\$HOME/shared}"
remote_bxshare="${OOS_HOST_BXSHARE:-/usr/share/bochs}"
remote_repo="${remote_share_root%/}/${workspace_name}/$repo_name"
remote_command="${OOS_HOST_COMMAND:-cd \"${remote_repo}/targets/UNIXV6++\" && export BXSHARE=\"${remote_bxshare}\" && exec bochs-gdb -q -f bochsrc_nodebug.bxrc}"
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

if (($# > 0)) && [[ "$1" == *"@"* || "$1" == *.* ]]; then
  remote="$1"
  shift
fi

if (($# > 0)); then
  remote_command="$*"
fi

ssh_dir="$(dirname "$key_path")"
pubkey_path="${key_path}.pub"

mkdir -p "$ssh_dir"
chmod 700 "$ssh_dir"

if [[ ! -f "$key_path" || ! -f "$pubkey_path" ]]; then
  echo "Generating SSH key pair: $key_path"
  ssh-keygen -t ed25519 -f "$key_path" -N "" -C "oos-host@$(hostname)"
fi

can_login() {
  ssh \
    -i "$key_path" \
    -o BatchMode=yes \
    -o ConnectTimeout=5 \
    -o StrictHostKeyChecking=accept-new \
    -o IdentitiesOnly=yes \
    "$remote" true >/dev/null 2>&1
}

install_key() {
  local pubkey
  pubkey="$(tr -d '\r\n' < "$pubkey_path")"

  echo "Installing public key on $remote"
  echo "A password prompt from the host is expected on first setup."

  ssh \
    -o ConnectTimeout=5 \
    -o StrictHostKeyChecking=accept-new \
    "$remote" \
    "mkdir -p ~/.ssh && chmod 700 ~/.ssh && touch ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys && (grep -qxF '$pubkey' ~/.ssh/authorized_keys || printf '%s\n' '$pubkey' >> ~/.ssh/authorized_keys)"
}

if ! can_login; then
  install_key
fi

ssh_base=(
  ssh
  -i "$key_path"
  -o StrictHostKeyChecking=accept-new
  -o IdentitiesOnly=yes
)

ssh_term="${TERM:-xterm-256color}"

if ((interactive_shell)); then
  exec env TERM="$ssh_term" "${ssh_base[@]}" -t "$remote"
fi

echo "Connecting to $remote"
exec env TERM="$ssh_term" "${ssh_base[@]}" -tt "$remote" "$remote_command"
