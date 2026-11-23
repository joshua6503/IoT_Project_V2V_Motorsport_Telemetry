#!/usr/bin/env bash
# add_home_wifi.sh — ADD (do not activate) a NetworkManager profile "home-wifi" for the home
# network, with autoconnect and priority 10 (higher than the phone hotspot, priority 0).
# Run with sudo:   sudo ./add_home_wifi.sh "HOME_SSID"
#
# The passphrase is typed with hidden input and written with shell builtins only (never in argv,
# never in a log). The current connection is NOT touched: NetworkManager only uses the new
# profile when wlan0 has no active connection (next boot in range of the home network, or when
# the hotspot disappears). It never drops a working connection to jump to a "better" one.
set -euo pipefail

[ "$(id -u)" -eq 0 ] || { echo "run with sudo" >&2; exit 1; }
SSID="${1:?usage: sudo ./add_home_wifi.sh \"HOME_SSID\"}"
NAME="home-wifi"
KEYFILE="/etc/NetworkManager/system-connections/${NAME}.nmconnection"

if nmcli -g NAME connection show | grep -qx "$NAME"; then
  echo "profile '$NAME' already exists; remove it first with: sudo nmcli connection delete $NAME" >&2
  exit 1
fi

BK="/root/nm-backup-$(date +%F-%H%M%S).tgz"
tar czf "$BK" --ignore-failed-read /etc/netplan /etc/NetworkManager/system-connections 2>/dev/null
chmod 600 "$BK"
echo "backup: $BK"

read -rsp "Passphrase for '$SSID' (hidden): " p1; echo
[ ${#p1} -ge 8 ] && [ ${#p1} -le 63 ] || { echo "The passphrase must be 8-63 characters." >&2; exit 1; }
uuid="$(cat /proc/sys/kernel/random/uuid)"
( umask 077
  cat > "$KEYFILE" <<EOF
[connection]
id=${NAME}
uuid=${uuid}
type=wifi
interface-name=wlan0
autoconnect=true
autoconnect-priority=10

[wifi]
mode=infrastructure
ssid=${SSID}

[wifi-security]
key-mgmt=wpa-psk
psk=${p1}

[ipv4]
method=auto

[ipv6]
method=auto
EOF
)
unset p1
chmod 600 "$KEYFILE"; chown root:root "$KEYFILE"
nmcli connection reload
echo
nmcli -f NAME,AUTOCONNECT,AUTOCONNECT-PRIORITY,TYPE connection show
echo
echo "Profile added, NOT activated. Current connection unchanged:"
nmcli -g GENERAL.CONNECTION device show wlan0
