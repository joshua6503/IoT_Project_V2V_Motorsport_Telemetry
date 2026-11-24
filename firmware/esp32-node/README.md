# esp32-node — secondary node (ESP32 classic, Arduino)

Test/support node for V2V. See [ADR-0002](../../docs/adr/0002-esp32-arduino-as-secondary-node.md).

**Phase F2 (Wi-Fi + MQTT, no BLE):** node `esp32-b` joins the Pi access point `V2V-PIT` and
publishes to Mosquitto: `v2v/esp32-b/telemetry` (QoS 0, 5 Hz target), `/alert` (QoS 1) and
`/status` (QoS 1, retained `online`, LWT `offline`). Contract: `interfaces/mqtt-topics.md`.

## Layout

```
esp32-node/
└── esp32-node/            <- Arduino sketch folder (name must match the .ino)
    ├── esp32-node.ino
    ├── config.example.h   <- template, committed
    ├── config.h           <- your copy, git-ignored (Wi-Fi/MQTT secrets)
    ├── node_net.{h,cpp}   <- Wi-Fi + MQTT (esp-mqtt), LWT, reconnect with backoff
    ├── node_io.{h,cpp}    <- brake pot, BOOT button, LEDs, buzzer
    └── src/               <- AUTO-GENERATED copies, never edit:
        ├── v2v_protocol.h          (from interfaces/c/)
        └── alert_logic.{h,c}, alert_pattern.{h,c}   (from firmware/common/)
```

**Serial port:** this board (node B) enumerates as **COM5** (CH340) on the team laptop (2026-11-23); the RW612 J-Link is COM4.

## Hardware (pins in `config.example.h`, citations in `hardware/pinout.md`)

| Function | GPIO |
|---|---|
| Brake pot wiper (ADC1_CH4) | 32 |
| LED green / yellow / red | 25 / 26 / 27 (220 Ω each to GND) |
| Buzzer (**passive**) through NPN 2N2222A | 33 → 1 kΩ → base; buzzer VIN→collector; emitter GND; flyback diode across the buzzer, cathode to +5 V |
| Manual alert | 0 (on-board BOOT button) |

Brake pot: 3.3 V → 5.6 kΩ → 50 kΩ pot → 2.7 kΩ → GND, wiper to GPIO32, no capacitor (100 nF
wiper→GND optional, not fitted); calculated wiper range 0.15–2.98 V, measured 169–3019 mV on the bench (2026-11-23). The series resistors make a shorted/open wiper detectable.
Do not hold BOOT while pressing EN/reset (it enters download mode).

Buzzer: the node B buzzer is **passive** (it only clicks with DC), so GPIO33 (DevKitC J2 pin 8, next to GPIO32 = pot wiper, keep their wires apart) is driven with LEDC PWM at
`BUZZER_TONE_HZ` (2500 Hz, 50 % duty), on only while the alert pattern or the BOOT confirmation says
"buzzer on". This resolves OPEN-013 for node B. `BUZZER_PASSIVE 0` in `config.h` switches back to plain
on/off for an active buzzer (e.g. TMB12A05).

Bench finding (2026-11-23): GPIO23 stayed silent, GPIO33 works with the same firmware, so the GPIO23
problem was the breadboard wiring/pin location, not the code. Measured on the bench: the digital
HIGH/LOW test clicks and the 2.5 kHz tone is loud on GPIO33. The buzzer stays on GPIO33.

Buzzer self-test (temporary bench aid, **off by default**): set `BUZZER_SELFTEST` in `config.h`
to run it at boot, blocking on purpose: `1` sweep 1–5 kHz in 500 Hz steps then 3 s at 2.5 kHz,
`2` continuous tone until reset (for a scope), `3` digital HIGH 2 s / LOW 2 s until reset
(LEDC detached). Modes 2 and 3 never return; reset or reflash to leave them. Keep it at `0` otherwise.

Brake calibration (`BRAKE_MV_MIN/MAX` = 170 / 3010 mV, fault window 80–3090 mV) was measured on the
bench on 2026-11-23 and is no longer an ASSUMPTION.

## Behaviour

- Brake % from the pot, 20 Hz, fed to `alert_evaluate()` (firmware/common): ≥ 70 % and ≥ 150 %/s
  raises `hard_brake` (critical, latched 2 s). Thresholds are bench defaults (ASSUMPTION, OPEN-005).
- BOOT press → `manual` / `info` alert, plus a node-local confirmation (yellow blink ~300 ms and a
  short beep, ASSUMPTION), not part of DEC-004.
- LEDs/buzzer follow `alert_pattern_for()` (DEC-004). Brake reading out of range → `sensor_ok` 0 and
  the red/yellow fault pattern.
- `sensor_ok` is the bitmask of `alert_logic.h`: `ALERT_SENSOR_BRAKE` (4) when the brake is valid, else 0.
- Telemetry `{"node_id","ts_ms","seq","brake_pct","sensor_ok"}`; alert `{"node_id","ts_ms","seq","alert_type","severity"}`
  (alert `seq` is uint16). MQTT is informative: with the link down, messages are dropped and logged, not queued.
- Serial 115200: `[wifi]`, `[mqtt]`, `[btn]`, `[alert]` events and a 1 Hz `[st]` line.

## Remote commands (DEC-013)

The node subscribes (QoS 1, again on every MQTT (re)connect) to `v2v/esp32-b/cmd` and `v2v/all/cmd`.
Commands are not retained. A command only drives the LEDs/buzzer: it never publishes an alert (no loops).

```json
{"cmd":"alert","seq":1,"origin":"dashboard","alert_type":"manual|hard_brake|obstacle|overtemp",
 "severity":"info|warning|critical","duration_ms":3000,"source_node":"rw612-a"}
{"cmd":"clear","seq":2,"origin":"dashboard"}
```

- **Validation** (`node_net.cpp`, cJSON from the core): `cmd` is `alert` or `clear`; `seq` is an integer
  0..4294967295; `origin` and `source_node` (optional, `alert` only) match the node_id pattern
  `^[a-z0-9-]{1,32}$`; `duration_ms` is optional (default 3000) and must be an integer 100..10000. Anything
  else (invalid JSON, unknown `cmd`, a field out of range, a message over 255 bytes or fragmented) is ignored
  and logged, and the whole command is dropped.
- **Dedupe:** the last 8 `(origin, seq)` pairs are remembered, so the same command arriving on both
  topics, or re-sent, is applied once.
- **Actuation** (`node_io.cpp`): `io_effective_alert()` returns the remote alert while it is active and
  strictly more severe than the local alert (or the local one is NONE), so a local alert of equal or higher
  severity wins. The result goes through the same `alert_pattern_for()` as local alerts (DEC-004). `clear`
  ends it early; otherwise it expires after `duration_ms`. `info` has no pattern in DEC-004, so the LEDs
  stay green.
- **Threads:** parsing runs in the esp-mqtt task and hands the command to `loop()` through a 4-deep queue.
  The telemetry task, its queue and the watchdog are not touched.
- **Serial log**, one `[cmd]` line per event: `subscribe`, `subscribed`, `rx <topic> <payload>`, then
  `queued` / `ignored: <reason>`, `applied ...`, `remote alert expired`. The 1 Hz `[st]` line has `remote=0|1`.

Bench (2026-11-23, Pi 192.168.0.239, 8.5 dBm): warning, critical, clear, broadcast on `v2v/all/cmd`, duplicate
seq, invalid JSON, unknown `cmd`, `duration_ms` 20000 and an uppercase `origin` all behaved as above. 5 min
with a command every 30 s: 1530 telemetry messages on the broker, 0 `seq` gaps, 300 per full minute (5 Hz),
0 `offline`, no `[wd]` restart, no alert published by the node.

Test with `mosquitto_pub -q 1 -t v2v/esp32-b/cmd -f msg.json`. On Windows PowerShell 5.1 pass the JSON
with `-f <file>`: passing it with `-m` strips the double quotes before `mosquitto_pub` sees them.

## MQTT library

**ESP-IDF `esp-mqtt` (`mqtt_client.h`), bundled with the Arduino core — no library to install.**
Pinned by the core: **esp32 by Espressif Systems 3.3.8** (`esp32:esp32`). Why not PubSubClient:
it cannot publish at QoS 1, which `alert` and `status` require. esp-mqtt gives QoS 1, retained LWT,
user/password, and runs its own task, so `esp_mqtt_client_enqueue()` never blocks `loop()`.
Reconnect: auto-reconnect is disabled and `net_loop()` retries with backoff 1 s → 30 s (doubling);
keepalive 10 s (LWT after ~15 s is an ASSUMPTION from MQTT 3.1.1, to validate).
Measured on the bench (2026-11-23, Pi on the home router): LWT `offline` arrives 16.0 s after the
last telemetry when the USB is unplugged; after `systemctl restart mosquitto` the node reconnects
about 1 s after it notices the drop (twice in a row, telemetry back at 5 Hz with no `seq` gaps).
Design notes: telemetry (QoS 0) is sent by a small task with `esp_mqtt_client_publish()`;
`esp_mqtt_client_enqueue()` for QoS 0 only delivered ~1 msg/s and was dropped. `network.timeout_ms` is
left at the default (a 3 s timeout made the node flap: an MQTT connect takes ~3 s here), Wi-Fi modem
sleep is off, and a watchdog restarts the node on a real stall (see "Wi-Fi stability findings").

### Wi-Fi stability findings (2026-11-23, home network Familia_CM)

Problem: with the default 20 dBm TX power the node lost most of its telemetry on the home Wi-Fi
(10 min clean test, serial closed: 34 % delivered, ~20 reconnects) while the RW612 on the same
network and broker delivered 100 %. Symptoms: `mqtt_client: No PING_RESP, disconnected`, `Writing
didn't complete ... errno=119` / `errno=113`, `esp_mqtt_client_publish()` blocking for 6-10 s, gaps of
5-38 s (they match lwIP's 3/6/12/24 s TCP retransmit backoff, fixed by the precompiled core).
The Wi-Fi association itself stayed up (ICMP kept answering during the stalls).

What was measured (broker-side counts from a timestamped `mosquitto_sub`, rw612-a as control):

| Test | Change | esp32-b delivered | offline events |
|---|---|---|---|
| 5 min, serial open | baseline (boot scan + diagnostics only) | 92.0 % | 3 |
| 5 min, serial open | HT20 | 97.4 % | 1 |
| 10 min, closed | HT20 | 64.4 % | 16 |
| 10 min, closed | HT20 + 802.11b/g only | 27.6 % (2 watchdog reboots) | 16 |
| 5 min, serial open | baseline | 96.6 % | 2 |
| 10 min, closed | baseline | 75.8 % | 11 |
| 6 min, closed | baseline, status line once per 60 s | 61.9 % | 8 |
| 6 min, closed | **TX power 8.5 dBm** | **100 %** | **0** |
| 10 min, closed | **TX power 8.5 dBm + new watchdog (final firmware)** | **100 % (3050/3050)** | **0** |

- **BSSID (rejected):** Familia_CM has two 2.4 GHz BSSIDs on channel 10 (`04:95:E6:DF:3A:51` at -45 dBm and
  `...3A:81` at -72 dBm in the boot scan). The node joined `...3A:51` every time and never changed; connected RSSI
  averaged -36 dBm (min -69 in one sample). Pinning would change nothing, so **no `WIFI_BSSID` option exists**;
  the boot scan only logs the BSSIDs. (If pinning is ever added it must apply only when the scanned SSID equals
  `WIFI_SSID` and the BSSID is found, so it cannot break the `V2V-PIT` access point.)
- **HT20 and 802.11b/g only (rejected):** no consistent improvement (`WIFI_FORCE_HT20`, `WIFI_FORCE_11BG`,
  both left at 0).
- **Serial / status line (rejected as a cause):** serial-open runs looked better than closed runs, but a build that
  prints the status line once per minute was equally bad when closed.
- **TX power (fix):** `WIFI_TX_POWER_DBM_X4 34` (8.5 dBm) made all the stalls disappear: 2 clean runs
  (6 and 10 min) against 6 bad ones at the default power. **The mechanism is not proven.** Candidates: current peaks of
  the 20 dBm transmitter on the laptop's bus power, or the strong signal overloading the mesh node (RSSI is -31 to -36 dBm,
  the board sits next to it). To tell them apart, repeat the 20 dBm test powered from a wall USB charger. Open point:
  8.5 dBm reduces range, so re-check it in pit mode (V2V-PIT) and at the real distance. Remove the define to go back to the default.
- Not changed (no evidence they were needed): `network.timeout_ms` (default 10 s; 3 s flapped before), keepalive (10 s,
  same as the RW612), publish path. lwIP timers and buffers are fixed by the precompiled core.

Watchdog (final): the node restarts itself, independent of the MQTT state, when (a) the telemetry queue is full and
the publish task made no progress for 20 s, or (b) Wi-Fi is up but no telemetry was accepted by the client for 60 s.
The reason is printed (`[wd] restarting node: ...`) and stored in NVS; the serial `[boot]` and `[st]` lines show
`boots`, `rst` (this boot), `prev` (previous boot) and `wd` (last watchdog reason, with the boot number).
Note: with the broker down for more than 60 s while Wi-Fi is up, rule (b) reboots the node every ~60 s (seq restarts).

Diagnostics in the 1 Hz `[st]` line: `bssid`, `ch`, `rssi`, `pubmax` (longest publish in the last second), `pubok`/`pubfail`,
`errno=<type>/<errno>` of the last MQTT transport error. The boot log lists every BSSID of the SSID (`[wifi] scan:`).

Test method: one timestamped `mosquitto_sub` for both nodes, serial port closed, counted per minute on the broker
side; the boot counter and reset reason are read once at the end (open the port without toggling DTR/RTS or it resets the board).

## Why a copied header

Arduino IDE only compiles files **inside the sketch folder** (including `src/`).
`#include "../../../interfaces/c/v2v_protocol.h"` fails. So:

- `interfaces/c/` is the source of truth.
- `tools/sync_protocol.py` copies the header into `src/` with an
  `AUTO-GENERATED COPY` banner.
- CI regenerates the copy and fails if the committed one is stale.

**Never edit `src/v2v_protocol.h` by hand.**

## Build

1. Arduino IDE → Boards Manager → install **esp32 by Espressif Systems**.
   Version 3.3.8. Board: *ESP32 Dev Module* (`esp32:esp32:esp32`). Exact dev-kit model: TBD (`hardware/bom.md`).
2. From the repo root (PowerShell):

   ```powershell
   python tools/sync_protocol.py
   Copy-Item firmware/esp32-node/esp32-node/config.example.h firmware/esp32-node/esp32-node/config.h
   ```

3. Edit `config.h` (Wi-Fi password, broker IP, MQTT user/password), open `esp32-node.ino`, Upload.
   Serial monitor at 115200.

With arduino-cli (same as CI):

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/esp32-node/esp32-node
```

Observe (replace user/password and host):

```powershell
mosquitto_sub -h <broker> -u <user> -P <pass> -t "v2v/esp32-b/#" -v
```
