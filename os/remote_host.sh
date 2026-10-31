#!/usr/bin/env bash
set -u

AP_PROFILE="Pan-Tilt-AP"
PREFERRED_HOST="10.42.0.163"
REMOTE_USER="admin"
SSH_CONNECT_TIMEOUT_SECONDS=2
REMOTE_HOSTS=(
    "192.168.0.163"
    "192.168.0.164"
    "10.42.0.163"
    "10.42.0.164"
    "192.168.0.120"
    "192.168.0.122"
    "10.42.0.120"
    "192.168.0.173"
    "192.168.0.174"
    "10.42.0.173"
    "10.42.0.174"
)

is_access_point_active() {
    local active_profile

    while IFS= read -r active_profile; do
        if [[ "$active_profile" == "$AP_PROFILE" ]]; then
            return 0
        fi
    done < <(nmcli -t -f NAME connection show --active 2>/dev/null)

    return 1
}

try_remote_host() {
    local host="$1"

    if ssh \
        -o BatchMode=yes \
        -o ConnectTimeout="$SSH_CONNECT_TIMEOUT_SECONDS" \
        -o ConnectionAttempts=1 \
        -- "$REMOTE_USER@$host" true >/dev/null 2>&1; then
        printf '%s ok\n' "$host" >&2
        return 0
    fi

    printf '%s fail\n' "$host" >&2
    return 1
}

access_point_active=false
if is_access_point_active; then
    access_point_active=true
    if try_remote_host "$PREFERRED_HOST"; then
        printf '%s\n' "$PREFERRED_HOST"
        exit 0
    fi
fi

for host in "${REMOTE_HOSTS[@]}"; do
    if [[ "$host" == "$PREFERRED_HOST" && "$access_point_active" == true ]]; then
        continue
    fi
    if try_remote_host "$host"; then
        printf '%s\n' "$host"
        exit 0
    fi
done

printf 'NULL\n'
