#!/bin/sh

readonly PROJECT_ROOT="/home/admin/pt"
readonly RPI_CLIENT_INTERFACE="wlan1"
readonly RPI_IW="/usr/sbin/iw"

enable_rpi_client_network() {
    if [ ! -r /proc/device-tree/model ] || ! tr -d '\000' </proc/device-tree/model | grep -q 'Raspberry Pi'; then
        return 0
    fi

    if ! ip link show "$RPI_CLIENT_INTERFACE" >/dev/null 2>&1; then
        rpi_ap_interface=
        while IFS=: read -r rpi_device rpi_type rpi_state rpi_connection; do
            if [ "$rpi_type" != "wifi" ] || [ "$rpi_state" != "connected" ]; then
                continue
            fi
            rpi_mode=$(sudo -n nmcli -g 802-11-wireless.mode connection show "$rpi_connection" 2>/dev/null || true)
            if [ "$rpi_mode" = "ap" ]; then
                rpi_ap_interface=$rpi_device
                break
            fi
        done <<RPI_DEVICES
$(nmcli -t -f DEVICE,TYPE,STATE,CONNECTION device status)
RPI_DEVICES

        if [ -z "$rpi_ap_interface" ]; then
            printf '[enable_rpi_client_network()] ERROR: No active AP interface is available for creating %s.\n' "$RPI_CLIENT_INTERFACE"
            return 1
        fi
        printf '[lib] RPI: creating client Wi-Fi interface %s from %s\n' "$RPI_CLIENT_INTERFACE" "$rpi_ap_interface"
        if ! sudo -n "$RPI_IW" dev "$rpi_ap_interface" interface add "$RPI_CLIENT_INTERFACE" type managed; then
            printf '[enable_rpi_client_network()] ERROR: Failed to create client Wi-Fi interface %s.\n' "$RPI_CLIENT_INTERFACE"
            return 1
        fi
        sleep 1
    fi

    while IFS=: read -r network_profile_name network_profile_type network_profile_device; do
        if [ "$network_profile_type" != "802-11-wireless" ] || [ "$network_profile_device" != "$RPI_CLIENT_INTERFACE" ]; then
            continue
        fi
        network_profile_mode=$(sudo -n nmcli -g 802-11-wireless.mode connection show "$network_profile_name" 2>/dev/null || true)
        if [ "$network_profile_mode" = "infrastructure" ]; then
            printf '[lib] RPI: client Wi-Fi profile is already active: %s\n' "$network_profile_name"
            return 0
        fi
    done <<ACTIVE_PROFILES
$(nmcli -t -f NAME,TYPE,DEVICE connection show --active)
ACTIVE_PROFILES

    network_profile_found=false
    while IFS=: read -r network_profile_name network_profile_type; do
        if [ "$network_profile_type" != "802-11-wireless" ]; then
            continue
        fi
        network_profile_mode=$(sudo -n nmcli -g 802-11-wireless.mode connection show "$network_profile_name" 2>/dev/null || true)
        if [ "$network_profile_mode" != "infrastructure" ]; then
            continue
        fi
        network_profile_found=true
        printf '[lib] RPI: activating client Wi-Fi profile on %s: %s\n' "$RPI_CLIENT_INTERFACE" "$network_profile_name"
        if sudo -n nmcli connection up id "$network_profile_name" ifname "$RPI_CLIENT_INTERFACE"; then
            printf '[lib] RPI: client Wi-Fi profile is active: %s\n' "$network_profile_name"
            return 0
        fi
        printf '[lib] RPI: profile activation failed, trying next: %s\n' "$network_profile_name"
    done <<NETWORK_PROFILES
$(nmcli -t -f NAME,TYPE connection show)
NETWORK_PROFILES

    if [ "$network_profile_found" != true ]; then
        printf '[enable_rpi_client_network()] ERROR: No client Wi-Fi profiles are configured.\n'
    else
        printf '[enable_rpi_client_network()] ERROR: No configured client Wi-Fi profile could be activated.\n'
    fi
    return 1
}
