#!/usr/bin/env bash
set -u

AP_PROFILE="PRI_pan_tilt"
PREFERRED_HOST="10.42.0.1"
REMOTE_USER="admin"
SSH_CONNECT_TIMEOUT_SECONDS=2
REMOTE_HOSTS=(
    "10.42.0.120"
    "192.168.0.120"
    "192.168.0.122"
    "10.42.0.163"
    "10.42.0.164"
    "192.168.0.163"
    "192.168.0.164"
    "192.168.0.173"
    "192.168.0.174"
    "10.42.0.173"
    "10.42.0.174"
)

if (( $# > 1 )); then
    printf 'ERROR: usage: remote_host.sh [address]\n' >&2
    exit 1
fi

requested_host=''
if (( $# == 1 )); then
    requested_host="$1"
fi

active_wifi_profile() {
    nmcli -t -f NAME,TYPE connection show --active 2>/dev/null |
        while IFS=: read -r profile type; do
            if [[ "$type" == "wifi" ]]; then
                printf '%s\n' "$profile"
                return 0
            fi
        done
}

wifi_device() {
    nmcli -t -f DEVICE,TYPE,STATE device status 2>/dev/null |
        while IFS=: read -r device type state; do
            if [[ "$type" == "wifi" && "$state" == "connected" ]]; then
                printf '%s\n' "$device"
                return 0
            fi
        done
}

activate_access_point_client() {
    local device
    device=$(wifi_device)
    if [[ -z "$device" ]]; then
        printf '%s unavailable: no connected Wi-Fi device\n' "$AP_PROFILE" >&2
        return 1
    fi
    if ! nmcli -w 8 connection up id "$AP_PROFILE" ifname "$device" >&2; then
        printf '%s unavailable: cannot activate local Wi-Fi profile\n' "$AP_PROFILE" >&2
        return 1
    fi
    return 0
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

previous_wifi_profile=$(active_wifi_profile)
if activate_access_point_client; then
    if try_remote_host "$PREFERRED_HOST"; then
        printf '%s\n' "$PREFERRED_HOST"
        exit 0
    fi
fi

if [[ -n "$previous_wifi_profile" && "$previous_wifi_profile" != "$AP_PROFILE" ]]; then
    if ! nmcli -w 8 connection up id "$previous_wifi_profile" >&2; then
        printf '%s fail: cannot reactivate client Wi-Fi profile\n' "$previous_wifi_profile" >&2
    fi
fi

if [[ -n "$requested_host" ]] && try_remote_host "$requested_host"; then
    printf '%s\n' "$requested_host"
    exit 0
fi

for host in "${REMOTE_HOSTS[@]}"; do
    if [[ "$host" == "$requested_host" || "$host" == "$PREFERRED_HOST" ]]; then
        continue
    fi
    if try_remote_host "$host"; then
        printf '%s\n' "$host"
        exit 0
    fi
done

printf 'NULL\n'
