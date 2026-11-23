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
sleep is off, and if the telemetry task makes no progress for 20 s the node restarts itself (logged).
Known open point (2026-11-23, 10 min clean test, serial closed): the node is NOT stable against the Pi
over the home router. The broker saw ~20 `offline` events and only ~1000 of ~2950 expected telemetry
messages. The board's own log shows `mqtt_client: Writing didn't complete in specified timeout:
errno=119` / `Error sending ping` while Wi-Fi still reports connected, and the telemetry task blocks
inside `esp_mqtt_client_publish()` (`sent` frozen, `drop` rising) for minutes; the stuck-task restart
only fires while MQTT is up, so it can stay muted. Boot counter and reset reason (`boots=`, `rst=` in
the serial `[st]` line, NVS) show the reboots were the node's own watchdog (`rst=SW`), not power or
panics. Suspect: Wi-Fi power save on the Pi (`802-11-wireless.powersave` is `default` on `home-wifi`);
not yet tested. Fix to validate: disable it on the Pi and make the stuck-task restart independent of
the MQTT state.

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
