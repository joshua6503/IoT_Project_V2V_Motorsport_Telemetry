#!/usr/bin/env bash
# verify_pi.sh — checks the gateway on the Pi: services, mode, anonymous access rejected and an
# authenticated MQTT round trip. Installed as /usr/local/bin/verify-pi. Run it in home mode, in
# pit mode and after a reboot. No sudo needed.
#
#   verify-pi                      asks the password of user "dashboard" (hidden)
#   V2V_VERIFY_USER=<user> verify-pi    check with another MQTT user. With the ACL
#                                       (gateway/pi/acl) only "dashboard" may use v2v/verify/#;
#                                       node users will fail the round trip by design.
set -uo pipefail

USER_MQTT="${V2V_VERIFY_USER:-dashboard}"
PASS=0; FAIL=0
ok()   { echo "  [ OK ] $*"; PASS=$((PASS+1)); }
bad()  { echo "  [FAIL] $*"; FAIL=$((FAIL+1)); }

echo "== services"
for s in mosquitto NetworkManager avahi-daemon; do
  en="$(systemctl is-enabled "$s" 2>/dev/null || true)"; ac="$(systemctl is-active "$s" 2>/dev/null || true)"
  if [ "$en" = "enabled" ] && [ "$ac" = "active" ]; then ok "$s enabled + active"; else bad "$s enabled=$en active=$ac"; fi
done
if ss -ltn 2>/dev/null | awk '{print $4}' | grep -qE '(^|:)1883$'; then ok "broker listening on :1883"; else bad "nothing listens on :1883"; fi

echo "== mode"
active="$(nmcli -g GENERAL.CONNECTION device show wlan0 2>/dev/null || true)"
if [ "$active" = "V2V-PIT" ]; then echo "  mode: PIT (access point V2V-PIT)"; else echo "  mode: HOME (client of '${active:-none}')"; fi
echo "  confirmed boot mode: $(cat /var/lib/v2v-mode/mode 2>/dev/null || echo 'home (default)')"
ip -4 -br addr show | grep -v '^lo' | sed 's/^/  /'
if [ -f /var/lib/v2v-mode/pending ] && systemctl is-active --quiet v2v-mode-rollback.timer; then
  echo "  NOTE: switch to '$(cat /var/lib/v2v-mode/pending)' pending confirmation"
fi

echo "== MQTT"
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/anon" "$tmp/auth"; chmod 700 "$tmp" "$tmp/anon" "$tmp/auth"
if ! mosquitto_pub -h localhost -t v2v/verify/anon -m x -q 1 >/dev/null 2>&1 \
   && ! XDG_CONFIG_HOME="$tmp/anon" mosquitto_pub -h localhost -t v2v/verify/anon -m x -q 1 >/dev/null 2>&1; then
  ok "anonymous publish rejected"
else
  bad "anonymous publish was ACCEPTED (allow_anonymous must be false)"
fi

read -rsp "  password of MQTT user '$USER_MQTT': " pw; echo
# mosquitto clients read their options from $XDG_CONFIG_HOME/mosquitto_{pub,sub}: the password
# never goes in argv. The temp dir is 0700 and removed on exit.
( umask 077
  printf -- '-u %s\n-P %s\n' "$USER_MQTT" "$pw" > "$tmp/auth/mosquitto_pub"
  cp "$tmp/auth/mosquitto_pub" "$tmp/auth/mosquitto_sub" )
unset pw
export XDG_CONFIG_HOME="$tmp/auth"
stamp="verify-$(date +%s)"
mosquitto_sub -h localhost -t "v2v/verify/rt" -C 1 -W 6 > "$tmp/got" 2>"$tmp/sub.err" &
sp=$!
sleep 1
if mosquitto_pub -h localhost -t "v2v/verify/rt" -m "$stamp" -q 1 2>"$tmp/pub.err"; then
  wait "$sp" 2>/dev/null
  if grep -qx "$stamp" "$tmp/got"; then ok "authenticated pub/sub round trip as '$USER_MQTT'"; else bad "round trip: message not received"; fi
else
  kill "$sp" 2>/dev/null; bad "authenticated publish failed: $(head -c 200 "$tmp/pub.err")"
fi

echo
echo "RESULT: $PASS ok, $FAIL failed"
[ "$FAIL" -eq 0 ]
