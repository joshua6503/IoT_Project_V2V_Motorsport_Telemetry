#!/usr/bin/env bash
# setup_pi.sh — one-time (idempotent) gateway setup on the Raspberry Pi 5. Run with sudo,
# in HOME mode (needs internet for apt):
#
#   sudo ./setup_pi.sh                    install/refresh everything, ask for missing MQTT users
#   sudo ./setup_pi.sh --reset-user NAME  change the password of one MQTT user
#
# MQTT passwords are typed by you at the mosquitto_passwd prompt (hidden); they are never stored
# in this repo or passed on a command line. Users: rw612-a, esp32-b, dashboard.
set -euo pipefail

[ "$(id -u)" -eq 0 ] || { echo "run with sudo" >&2; exit 1; }
HERE="$(cd "$(dirname "$0")" && pwd)"
PASSWD_FILE=/etc/mosquitto/passwd
CONF=/etc/mosquitto/conf.d/v2v.conf
USERS=(rw612-a esp32-b dashboard)

RESET=""
if [ "${1:-}" = "--reset-user" ]; then RESET="${2:?usage: --reset-user NAME}"; fi

echo "== 1/6 packages"
if ! dpkg -s mosquitto mosquitto-clients >/dev/null 2>&1; then
  apt-get update
  DEBIAN_FRONTEND=noninteractive apt-get install -y mosquitto mosquitto-clients
else
  echo "mosquitto and mosquitto-clients already installed"
fi

echo "== 2/6 broker configuration"
install -d /etc/mosquitto/conf.d
{
  echo "# Managed by gateway/pi/setup_pi.sh. Authenticated MQTT for the V2V nodes."
  echo "listener 1883 0.0.0.0"
  echo "allow_anonymous false"
  echo "password_file $PASSWD_FILE"
  # Debian's main mosquitto.conf already enables persistence; add it only if missing.
  grep -qE '^\s*persistence\s+true' /etc/mosquitto/mosquitto.conf || echo "persistence true"
} > "$CONF"
chmod 644 "$CONF"

echo "== 3/6 MQTT users"
# Final owner/mode BEFORE the first mosquitto_passwd call (it warns about world-readable files).
[ -e "$PASSWD_FILE" ] || install -m 640 -o root -g mosquitto /dev/null "$PASSWD_FILE"
chown root:mosquitto "$PASSWD_FILE"; chmod 640 "$PASSWD_FILE"
for u in "${USERS[@]}"; do
  if [ "$RESET" = "$u" ] || ! grep -q "^${u}:" "$PASSWD_FILE"; then
    echo "-- password for MQTT user '$u' (hidden prompt, twice):"
    mosquitto_passwd "$PASSWD_FILE" "$u"
  else
    echo "user '$u' already exists (use --reset-user $u to change it)"
  fi
done
if [ -n "$RESET" ] && ! printf '%s\n' "${USERS[@]}" | grep -qx "$RESET"; then
  echo "unknown user '$RESET' (valid: ${USERS[*]})" >&2; exit 1
fi
chown root:mosquitto "$PASSWD_FILE"; chmod 640 "$PASSWD_FILE"

echo "== 4/6 helper scripts"
install -m 755 "$HERE/pi-mode.sh" /usr/local/sbin/pi-mode
install -m 755 "$HERE/verify_pi.sh" /usr/local/bin/verify-pi
echo "installed: /usr/local/sbin/pi-mode  /usr/local/bin/verify-pi"

echo "== 5/6 services"
systemctl enable mosquitto avahi-daemon NetworkManager >/dev/null
systemctl restart mosquitto
sleep 1
systemctl is-active --quiet mosquitto || { echo "mosquitto failed:"; journalctl -u mosquitto -n 20 --no-pager; exit 1; }

echo "== 6/6 Wi-Fi regulatory country (needed for the access point)"
c="$(iw reg get 2>/dev/null | awk '/^country/ {print $2; exit}' | tr -d ':')"
if [ -z "$c" ] || [ "$c" = "00" ]; then
  echo "WARNING: country not set. Run:  sudo raspi-config nonint do_wifi_country MX"
else
  echo "country: $c"
fi

echo
echo "Done. Next: verify-pi   (then 'sudo pi-mode pit' when you are ready for pit mode)"
