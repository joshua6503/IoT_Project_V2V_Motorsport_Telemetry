#!/usr/bin/env bash
# pi-mode.sh — switch the Raspberry Pi 5 between "home" (Wi-Fi client) and "pit" (access point
# V2V-PIT). Run with sudo. Installed by setup_pi.sh as /usr/local/sbin/pi-mode.
#
#   pi-mode home      client of the home Wi-Fi (hotspot as fallback)
#   pi-mode pit       access point "V2V-PIT" (2.4 GHz, WPA2), Pi = 10.42.0.1
#   pi-mode confirm   accept the pending switch (cancels the automatic rollback)
#   pi-mode status    mode, IPs, pending rollback, AP clients
#   pi-mode rollback  (also called by the timer) go back to the last confirmed mode
#
# A switch is a TRIAL: it is detached from your SSH session and reverted after 5 minutes unless
# you run "pi-mode confirm" through the new network. Only "confirm" makes it persistent
# (autoconnect flags), so a reboot during the trial also returns to the last confirmed mode.
# The Wi-Fi passphrase of V2V-PIT is asked once (hidden) when the profile is created.
set -euo pipefail

AP_NAME="V2V-PIT"
AP_SSID="V2V-PIT"
AP_CHANNEL=6
AP_ADDR="10.42.0.1/24"
STATE_DIR="/var/lib/v2v-mode"
ROLLBACK_SECS=300
SELF="$(readlink -f "$0")"
KEYFILE="/etc/NetworkManager/system-connections/${AP_NAME}.nmconnection"

log() { echo "[pi-mode] $*"; logger -t pi-mode -- "$*" 2>/dev/null || true; }
die() { echo "[pi-mode] ERROR: $*" >&2; exit 1; }
need_root() { [ "$(id -u)" -eq 0 ] || die "run with sudo"; }

mkdir -p "$STATE_DIR/backups" 2>/dev/null || true

confirmed_mode() { cat "$STATE_DIR/mode" 2>/dev/null || echo home; }
pending_mode() { cat "$STATE_DIR/pending" 2>/dev/null || true; }
timer_active() { systemctl is-active --quiet v2v-mode-rollback.timer; }

backup_nm() {
  local f="$STATE_DIR/backups/nm-$(date +%F-%H%M%S).tgz"
  tar czf "$f" --ignore-failed-read /etc/netplan /etc/NetworkManager/system-connections 2>/dev/null || true
  chmod 600 "$f"
  log "NetworkManager/netplan profiles backed up to $f"
  # keep the last 10
  ls -1t "$STATE_DIR"/backups/nm-*.tgz 2>/dev/null | tail -n +11 | xargs -r rm -f
}

ensure_country() {
  local c
  c="$(iw reg get 2>/dev/null | awk '/^country/ {print $2; exit}' | tr -d ':')"
  if [ -z "$c" ] || [ "$c" = "00" ]; then
    echo "[pi-mode] WARNING: Wi-Fi regulatory country not set (got '${c:-none}'). The AP may not start." >&2
    echo "          Fix:  sudo raspi-config nonint do_wifi_country MX" >&2
  fi
}

ensure_ap_profile() {
  if nmcli -g NAME connection show | grep -qx "$AP_NAME"; then return; fi
  log "profile $AP_NAME does not exist: creating it"
  local p1 p2
  while true; do
    read -rsp "Passphrase for the $AP_SSID network (8-63 chars, hidden): " p1; echo
    read -rsp "Repeat it: " p2; echo
    [ "$p1" = "$p2" ] || { echo "They do not match, try again."; continue; }
    [ ${#p1} -ge 8 ] && [ ${#p1} -le 63 ] || { echo "Must be 8-63 characters."; continue; }
    break
  done
  local uuid; uuid="$(cat /proc/sys/kernel/random/uuid)"
  # Written with shell builtins only: the passphrase never appears in argv or logs.
  ( umask 077
    cat > "$KEYFILE" <<EOF
[connection]
id=${AP_NAME}
uuid=${uuid}
type=wifi
interface-name=wlan0
autoconnect=false
autoconnect-priority=100

[wifi]
mode=ap
ssid=${AP_SSID}
band=bg
channel=${AP_CHANNEL}

[wifi-security]
key-mgmt=wpa-psk
proto=rsn;
pairwise=ccmp;
group=ccmp;
psk=${p1}

[ipv4]
method=shared
address1=${AP_ADDR}

[ipv6]
method=disabled
EOF
  )
  chmod 600 "$KEYFILE"; chown root:root "$KEYFILE"
  unset p1 p2
  nmcli connection reload
}

apply_mode() { # runs detached (systemd-run); $1 = home|pit
  case "$1" in
    pit)
      nmcli connection up "$AP_NAME" ifname wlan0
      ;;
    home)
      nmcli connection down "$AP_NAME" 2>/dev/null || true
      sleep 2
      nmcli device connect wlan0 || true
      ;;
  esac
}

switch() {
  need_root
  local target="$1" cur; cur="$(confirmed_mode)"
  if [ -n "$(pending_mode)" ] && timer_active; then
    die "there is already a pending switch to '$(pending_mode)': run 'pi-mode confirm' or wait for the rollback"
  fi
  [ "$target" = "pit" ] && { ensure_country; ensure_ap_profile; }
  backup_nm
  echo "$cur" > "$STATE_DIR/prev"
  echo "$target" > "$STATE_DIR/pending"
  systemctl stop v2v-mode-rollback.timer v2v-mode-rollback.service 2>/dev/null || true
  systemd-run --quiet --unit=v2v-mode-rollback --on-active="${ROLLBACK_SECS}s" "$SELF" rollback
  systemd-run --quiet --unit="v2v-mode-switch-$(date +%s)" --on-active=3s "$SELF" _apply "$target"
  cat <<EOF
[pi-mode] Switching to '$target' in 3 s (detached from this session). Your SSH session will drop.
          Previous confirmed mode: $cur
          Rollback to '$cur' in $((ROLLBACK_SECS/60)) minutes unless you confirm.
          Reconnect through the NEW network and run:   sudo pi-mode confirm
EOF
}

confirm() {
  need_root
  local target; target="$(pending_mode)"
  [ -n "$target" ] || { echo "[pi-mode] nothing pending (confirmed mode: $(confirmed_mode))"; return 0; }
  systemctl stop v2v-mode-rollback.timer v2v-mode-rollback.service 2>/dev/null || true
  if [ "$target" = "pit" ]; then
    nmcli connection modify "$AP_NAME" connection.autoconnect yes connection.autoconnect-priority 100
  else
    nmcli connection modify "$AP_NAME" connection.autoconnect no 2>/dev/null || true
  fi
  echo "$target" > "$STATE_DIR/mode"
  rm -f "$STATE_DIR/pending"
  log "confirmed: '$target' is now the mode the Pi boots in"
}

rollback() {
  need_root
  local target; target="$(pending_mode)"
  [ -n "$target" ] || { log "rollback: nothing pending"; return 0; }
  local prev; prev="$(cat "$STATE_DIR/prev" 2>/dev/null || echo home)"
  log "ROLLBACK: '$target' was not confirmed, returning to '$prev'"
  rm -f "$STATE_DIR/pending"
  apply_mode "$prev"
}

status() {
  echo "confirmed mode (boots in): $(confirmed_mode)"
  local pend; pend="$(pending_mode)"
  if [ -n "$pend" ]; then
    if timer_active; then
      echo "PENDING switch to '$pend' — run 'sudo pi-mode confirm'. Rollback in:"
      systemctl list-timers v2v-mode-rollback.timer --no-pager 2>/dev/null | sed -n '2p' | awk '{print "   "$1, $2, $3, "(" $NF ")"}' || true
    else
      echo "(stale pending '$pend': timer gone, e.g. after a reboot)"
    fi
  fi
  local active; active="$(nmcli -g GENERAL.CONNECTION device show wlan0 2>/dev/null || true)"
  if [ "$active" = "$AP_NAME" ]; then echo "wlan0 now: ACCESS POINT $AP_SSID (pit)"
  else echo "wlan0 now: client of '${active:-none}' (home)"; fi
  echo "addresses:"
  ip -4 -br addr show | grep -v '^lo' | sed 's/^/   /'
  echo "mDNS name: $(hostname).local"
  if [ "$active" = "$AP_NAME" ]; then
    echo "AP clients:"
    local n=0 mac
    if command -v iw >/dev/null; then
      for mac in $(iw dev wlan0 station dump 2>/dev/null | awk '/^Station/ {print $2}'); do
        n=$((n+1))
        local ip host
        ip="$(awk -v m="$mac" 'tolower($2)==tolower(m) {print $3}' /var/lib/NetworkManager/dnsmasq-wlan0.leases 2>/dev/null | head -1)"
        host="$(awk -v m="$mac" 'tolower($2)==tolower(m) {print $4}' /var/lib/NetworkManager/dnsmasq-wlan0.leases 2>/dev/null | head -1)"
        echo "   $mac  ${ip:-?}  ${host:-}"
      done
    fi
    [ "$n" -gt 0 ] || echo "   (none)"
  fi
}

case "${1:-}" in
  home|pit) switch "$1" ;;
  confirm)  confirm ;;
  rollback) rollback ;;
  status)   status ;;
  _apply)   need_root; apply_mode "${2:?}" ;;
  *) sed -n '2,13p' "$0"; exit 2 ;;
esac
