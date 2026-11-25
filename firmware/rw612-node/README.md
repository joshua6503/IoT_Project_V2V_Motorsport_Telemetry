# rw612-node — main node (NXP FRDM-RW612)

The MCUXpresso project is generated from the IDE by importing an SDK example, then organised
into the layers of [ADR-0005](../../docs/adr/0005-layered-firmware-architecture.md). Only the two
project descriptors of the MP2 project (`V2V_IOT/wifi_mqtt/.project` and `.cproject`) are committed;
every SDK file stays out of the repo (see [MP2 / F3](#mp2--f3-wi-fi--mqtt-freertos)).

## Prerequisites

| Item | Version |
|---|---|
| MCUXpresso IDE | TBD (team to standardise) |
| MCUXpresso SDK for FRDM-RW612 | **TBD** — the locally installed SDK reports 26.06.00 in its manifest; confirm everyone uses the same |
| Debug probe | on-board (MCU-Link) |

The SDK is **never** committed (`.gitignore` covers it).

## Which example to import (candidates, to confirm)

| SDK example | Why it is a candidate |
|---|---|
| `coex_examples/coex_wifi_peripheral_ht` | Wi-Fi + BLE peripheral in the same image — matches our two planes. Its readme lists RD-RW612-BGA among supported boards; confirm it imports cleanly for FRDM-RW612. |
| `edgefast_bluetooth_examples/peripheral_ht` | BLE peripheral/GATT server only — simplest base for M1. |
| `wifi_examples/wifi_mqtt` | MQTT client on lwIP + FreeRTOS — reference for the `tlm_` service (M2). |

F3 decision: the base is `wifi_examples/wifi_mqtt` (see MP2 / F3 below). The coexistence examples
are revisited in F5 (BLE next to Wi-Fi).

## Import steps

These are the **MP1** steps (`hello_world` base, superloop, `app/main_mp1.c`); that project is local and
**not committed**. For MP2 use the section below. Each teammate recreates it; the settings below are the ones verified with a headless build
(Debug and Release, 0 errors, 0 warnings, 2026-10-14).

1. MCUXpresso IDE → *Quickstart Panel* → **Import SDK example(s)…** → board `frdmrw612` →
   `demo_apps/hello_world`. Project location: **`firmware/rw612-node/V2V_IOT/hello_world`**
   (project name `V2V_IOT_hello_world`).
2. **Exclude `source/hello_world.c` from the build** (right-click → *Resource Configurations →
   Exclude from Build*, Debug and Release). The example's `board/hardware_init.c` stays:
   `board_io.c` calls its `BOARD_InitHardware()`.
3. Project → Properties → C/C++ Build → Settings → *MCU C Compiler → Includes* → *Include paths
   (-I)*, Debug and Release, add (leave *Include files (-include)* empty):
   - `${ProjDirPath}/../../app`
   - `${ProjDirPath}/../../services`
   - `${ProjDirPath}/../../drivers`
   - `${ProjDirPath}/../../board`
   - `${ProjDirPath}/../../../common/include`
   - `${ProjDirPath}/../../../../interfaces/c`
4. Add **linked source folders** (Project → New → Folder → Advanced → *Link to alternate
   location*). They are named `mp1_*` because the project already has `board/` and `drivers/`:

   | Folder | Location |
   |---|---|
   | `mp1_app` | `PROJECT_LOC/../../app` |
   | `mp1_services` | `PROJECT_LOC/../../services` |
   | `mp1_drivers` | `PROJECT_LOC/../../drivers` |
   | `mp1_board` | `PROJECT_LOC/../../board` |
   | `mp1_common` | `PROJECT_LOC/../../../common/src` |

   In each one, exclude `README.md` and `.gitkeep` from the build.
5. Copy the SDK drivers that `hello_world` does not bring from
   `C:\nxp\SDK_RW612\devices\RW612\drivers\` into the project's `drivers/` folder:
   `fsl_adc.c/.h` and `fsl_ctimer.c/.h` (both only need `fsl_common.h`, already present).
   GPIO, IO mux, clock, power, reset and the debug console are already in `hello_world`.
6. *Build* and *Debug* with the on-board MCU-Link.
7. Open the debug console (MCU-Link VCOM, 115200 8N1 as in `hello_world`). At boot it prints
   three `adc_scan pass=<n> ch0=.. ch7=..` lines 500 ms apart (turn the TPS pot to find its
   channel, OPEN-014), then one line every 200 ms:
   `t=<ms> dist=<cm> temp=<C> brake=<%> brake_raw=<counts> tps=<%> alert=<...> sev=<...> fault=<0|1>`.
   Read `brake_raw` at both pot ends to set `APP_POT_BRAKE_RAW_MIN/MAX` in `app/app_config.h`.

Switching the buzzer type (OPEN-013) is one line in `app/app_config.h`:
`#define BUZZER_KIND BUZZER_KIND_PASSIVE`.

Find the COM port from PowerShell:

```powershell
Get-CimInstance Win32_SerialPort | Select-Object DeviceID, Description
```

Open SDK questions are marked `TODO(tutor)` in `board/board_io.c`; datasheet citations
still missing are marked `TODO(datasheet)` in `drivers/` and `app/app_config.h`.

## PDS: brake FIR

Low-pass FIR on the brake-pressure pot for the Digital Signal Processing course
(REQ-F-015, REQ-F-016, DEC-009). The filter is the pure-C convolution
`firmware/common/src/fir_filter.c`; its coefficients are the generated header
`firmware/common/include/fir_brake_coeffs.h`. Both are already reached by the import
steps above (`mp1_common` linked folder and the `common/include` include path):
**no IDE change** is needed.

Switches in `app/app_config.h`:

| Switch | Default | Effect |
|---|---|---|
| `APP_ENABLE_BRAKE_FIR` | 1 | Each successful brake read goes through `fir_process()`; the filtered value feeds `tlm.brake_pct` and therefore `alert_evaluate()`. During the warm-up (first N samples, 400 ms at N = 9 / 20 Hz) the raw value is used. 0 = MP1 behaviour, filter not compiled in. |
| `APP_LOG_DSP_CSV` | 0 | 1 = print `dsp,<t_ms>,<x_pct>,<y_pct>` per brake sample and suppress the 200 ms log line. Needs `APP_ENABLE_BRAKE_FIR 1` (build error otherwise). |

With the filter on, the boot log has one extra line
`fir N=9 fs=20.0 fc=2.0 delay_ms=200` (group delay of the filter, logged on purpose),
and the 200 ms log line gains `brake_f=` (filter output) next to `brake=` (raw pot %).

Design: fs = 20 Hz (the existing 50 ms pot slot), N = 9, fc = 2 Hz, Hamming — validated on
the bench 2026-11-10 (DEC-009, evidence in
[`docs/04-testing/evidence/pds-2026-11-10/`](../../docs/04-testing/evidence/pds-2026-11-10/)).
A longer filter would be more selective but add delay to HARD_BRAKE; 9 taps (200 ms) is kept on
purpose. Faster sampling is OPEN-016.

Capture and compare spectra:

1. Set `APP_LOG_DSP_CSV 1`, rebuild, flash.
2. Close the serial terminal, then from the repo root (venv in `tools\dsp\.venv`):

   ```powershell
   python tools\dsp\analyze_capture.py --port COM5 --seconds 30
   ```

3. Set `APP_LOG_DSP_CSV 0` again for normal use.

Regenerate the coefficients (then rerun `make -C tests/unit` and rebuild):

```powershell
python tools\dsp\design_fir.py --fs 20 --fc 2 --taps 9 --window hamming --reference --plot
```

Details: [`tools/dsp/README.md`](../../tools/dsp/README.md).

## MP2 / F3: Wi-Fi + MQTT (FreeRTOS)

The node keeps the MP1 behaviour (sensors, FIR, `alert_evaluate()`, local LEDs/buzzer) and publishes to the
Mosquitto broker as node `rw612-a` (topics and payloads: [`interfaces/mqtt-topics.md`](../../interfaces/mqtt-topics.md)).
No BLE in this phase. `app/main_mp2.c` replaces `app/main_mp1.c`; `main_mp1.c` and the `hello_world` project are untouched.

| Layer | File | Role |
|---|---|---|
| app | `app/main_mp2.c`, `app/app_config_mp2.h`, `app/net_config.h` (git-ignored) | two tasks (below), build-time config, credentials |
| services | `services/tlm_mqtt.{c,h}` | Wi-Fi join (WPL), MQTT connect, LWT, reconnect, publish |
| common | `firmware/common/{include,src}/tlm_json.{h,c}` | pure-C JSON formatter, unit-tested on the host |
| drivers, board | unchanged | HC-SR04, DHT11, pots, LEDs, buzzer, `board_io` |

### Base example and what is replaced

Base: SDK `boards/frdmrw612/wifi_examples/wifi_mqtt` (lwIP MQTT client + FreeRTOS; FRDM-RW612 is in its
supported boards, `readme.md:63-65`). Its Wi-Fi join is the pattern validated in Practice 3: credentials as
`AP_SSID`/`AP_PASSWORD` macros (`wifi_mqtt.c:33-39`) and `WPL_Init` -> `WPL_Start` -> `WPL_AddNetwork` -> `WPL_Join`
(`wifi_mqtt.c:74-78,112-119`). `hardware_init.c`, clocks, FreeRTOS and lwIP configuration are the example's.
`wifi_mqtt.c` and `mqtt_freertos.c` are excluded from the build: their logic lives in `services/tlm_mqtt.c`.

### Credentials

```powershell
Copy-Item firmware\rw612-node\app\net_config.example.h firmware\rw612-node\app\net_config.h
```

Edit `net_config.h` (git-ignored): Wi-Fi SSID and passphrase (2.4 GHz), broker IPv4, port, MQTT user/password
(the Pi has no anonymous access, `interfaces/mqtt-topics.md`). Never commit it or paste it in logs. The firmware
never prints the passphrase or the MQTT password. The build stops with an `#error` if `net_config.h` is missing.

### IDE project (committed descriptors + local SDK copy)

The `.project`/`.cproject` of `V2V_IOT/wifi_mqtt` contain no absolute paths: the MP layers are linked folders
relative to the project (`PARENT-2-PROJECT_LOC`, same scheme as `hello_world`) and the settings use `${ProjName}`/`${ProjDirPath}`.
What they already include (do not redo by hand): the six `-I` paths of the MP1 steps, linked folders `mp1_app`,
`mp1_services`, `mp1_drivers`, `mp1_board`, `mp1_common`, the build exclusions (`source/wifi_mqtt.c`,
`source/mqtt_freertos.c`, `mp1_app/main_mp1.c`, `lwip/doc`) and `-DMQTT_OUTPUT_RINGBUF_SIZE=1024`
(the example's 256-byte MQTT output buffer, `middleware/lwip/src/include/lwip/apps/mqtt_opts.h:55-57`, is too small for telemetry plus a QoS 1 alert).

Steps for a new checkout (SDK 26.06.00, MCUXpresso IDE 25.6.136):

1. IDE -> *Import SDK example(s)* -> `frdmrw612` -> `wifi_examples/wifi_mqtt`, into any workspace (this creates the SDK copy).
2. Copy that project's files into the repo folder **without overwriting the committed descriptors**
   and without build output (robocopy exit codes 0-7 mean success):

   ```powershell
   robocopy "<workspace>\frdmrw612_wifi_mqtt" firmware\rw612-node\V2V_IOT\wifi_mqtt /E /XD Debug Release /XF .project .cproject
   ```

3. In `V2V_IOT\wifi_mqtt\source\mcux_config.h` change `DEBUG_CONSOLE_SYNCHRONIZATION_MODE` from `0` to `1`.
   Reason: the example uses the bare-metal console lock (`0`), but here the `tlm` task and the lwIP thread both
   print; the coexistence examples use `1` (FreeRTOS lock, `boards/frdmrw612/coex_examples/coex_wifi_peripheral_ht/mcux_config.h:23`).
4. Copy `fsl_adc.c/.h` and `fsl_ctimer.c/.h` from `C:\nxp\SDK_RW612\devices\RW612\drivers\` into `V2V_IOT\wifi_mqtt\drivers\`
   (the `wifi_mqtt` manifest does not include them).
5. Create `app\net_config.h` (above).
6. IDE -> *File -> Import -> Existing Projects into Workspace* -> select `V2V_IOT\wifi_mqtt` (do **not** copy into the workspace).

Headless build (what was run for this phase). The path **must use backslashes**: with `C:/...` the IDE fails with
"No file system is defined for scheme: C" and never exits. The IDE process also stays alive after
`Build Finished`; close it.

```powershell
& "C:\nxp\MCUXpressoIDE_25.6.136\ide\mcuxpressoidec.exe" -nosplash -application org.eclipse.cdt.managedbuilder.core.headlessbuild `
  -data "<empty scratch workspace>" -import "<repo>\firmware\rw612-node\V2V_IOT\wifi_mqtt" `
  -cleanBuild "V2V_IOT_wifi_mqtt/Debug" -no-indexer -printErrorMarkers
```

Result 2026-11-23: Debug, 0 errors, 0 warnings, 33 s; `arm-none-eabi-size`: text 482568, data 63843, bss 233012.

### Tasks and priorities

FreeRTOS: `configMAX_PRIORITIES` = 5, tick 1 kHz, time slicing off (`wifi_mqtt/FreeRTOSConfig.h:45,44,57`), heap 60 KB (`:71`).

| Task | Prio | Source of the number |
|---|---|---|
| timer service | 4 | `FreeRTOSConfig.h:92` |
| **`sense`** (ours) | **4** (`APP_SENSE_PRIO`) | first try, see below |
| Wi-Fi core, driver, wlcmgr | 3 | `middleware/wifi_nxp/wifidriver/wifi.c:146,166`, `wlcmgr/wlan.c:404` with `OSA_PRIORITY(x) = max - x` (`middleware/wifi_nxp/port/osa/osa_freertos.h:38-43`; `CONFIG_WIFI_TASK_PRIORITY_OFFSET` 0, `incl/wifi_config_default.h:73`) |
| Wi-Fi TX | 2 | `wifi.c:178` |
| lwIP `tcpip` | 2 | `lwipopts.h:71` |
| **`tlm`** (ours) | 1 (`APP_TLM_PRIO`) | lwIP `DEFAULT_THREAD_PRIO`, `lwipopts.h:103` |

- `sense`: the MP1 loop unchanged (DHT11 every 2500 ms, pot slot every 50 ms on the fixed grid resynchronised when
  more than one period late (DEC-010), HC-SR04 every 100 ms, `alert_evaluate()`, LEDs/buzzer). It blocks 1 ms between
  iterations so lower tasks run, never prints, never waits for the network. Only this task calls `board_time_*`/`board_delay_us`.
- `tlm`: joins Wi-Fi (retries every 5 s), connects MQTT, every 200 ms publishes the latest snapshot (QoS 0, 5 Hz),
  mirrors every new alert (QoS 1, retried until the client accepts it), prints the 200 ms log line.
- **Why 4 for `sense`:** the HC-SR04 echo wait (up to 30 ms, DEC-008) and the DHT11 bit timing are busy-wait measurements;
  a Wi-Fi task preempting them would corrupt the measurement. Interrupts stay enabled (ISR jitter is microseconds; the DHT11
  bit threshold is 50 us against 26-28 us / 70 us bits, `drivers/dht11.c`). The cost is that Wi-Fi tasks can be delayed
  for up to ~35 ms per 100 ms in the no-echo case. Whether Wi-Fi tolerates that is **measured, not assumed**.
- `APP_DHT11_FRAME_CRITICAL 1` (default 0) wraps only the DHT11 frame read (~5 ms) in `taskENTER_CRITICAL`; enable it only
  if the DHT11 counters below are bad.
- "New alert" = `(type, severity)` differs from the previous sample's and is not NONE (assumption): a latched `HARD_BRAKE` is published once.

### Fallback if priority 4 hurts Wi-Fi

What is measured (5 min, board and access point at rest, Wi-Fi strength normal), from the 200 ms log line and the broker:

| Metric | Where | Trigger |
|---|---|---|
| Wi-Fi link drops `wf_dc` | log | >= 1 in 5 min |
| MQTT drops `mq_dc` / reconnects `mq_rc` | log | >= 1 in 5 min |
| Telemetry rate at the broker | `mosquitto_sub -v -t v2v/rw612-a/telemetry` counted over 60 s | < 4.5 msg/s mean (target 5 Hz) |
| Publish refusals | log: `pub_err / (pub_ok + pub_err)` | > 2 % while connected |
| Ping loss laptop -> board | `ping -n 100 <board ip>` | > 5 % |

If any trigger fires with priority 4 (and the same test with the link idle and no sensor load does not):

1. **Fallback A, cap the HC-SR04 echo wait.** In `drivers/hcsr04.c` lower `HCSR04_ECHO_TIMEOUT_US` from 30000 to 12000
   (the sensor then reports "out of range" beyond ~2 m instead of ~4 m; the alert thresholds are 30 and 15 cm). Worst-case
   blocking per read drops from ~35 ms to ~17 ms. This edits a driver (behaviour-preserving below 2 m), so it is **not** applied
   by default. Repeat the measurement.
2. **Fallback B, `sense` at priority 2** (`APP_SENSE_PRIO 2` in `app_config_mp2.h`; Wi-Fi core/driver at 3 can then preempt
   the HC-SR04 wait). Accept and report the measured HC-SR04 error: read `echo_us` for ~1000 samples with a fixed target at
   30 cm (expected 1740 us, 58 us = 1 cm); the budget is +-1 cm. Report also the DHT11 counters against the priority-4 run.

DHT11 failures are logged cumulatively: `dht_ok`, `dht_cks` (checksum), `dht_to` (timeout), `dht_nr` (no response). More than
10 % non-OK reads over 5 min -> try `APP_DHT11_FRAME_CRITICAL 1` before touching priorities.

### Log line

One line every 200 ms (UART, 115200 8N1): `t dist temp brake brake_f brake_raw tps alert sev fault echo_us echo_err dht_st
dht_ok dht_cks dht_to dht_nr slot_late_max resyncs wf wf_dc mq mq_dc mq_rc pub_ok pub_err sen_ok sen_err al_drop heap rssi`
(`wf`/`mq` = link/session up now, `slot_late_max` = worst lateness of the 50 ms pot slot in ms, `heap` = free FreeRTOS heap in bytes,
`rssi` = dBm from the Wi-Fi driver, then `ip= up= link=` of the lwIP default netif). After the network starts, one `tasks(name:prio)` line lists every task's priority.
Since DEC-013 the line also carries `heap_min` (true minimum-ever free heap, `xPortGetMinimumEverFreeHeapSize`, `heap_4.c:419`) after `heap`, and
`rem= sub= cmd_rx= cmd_ok= cmd_bad= cmd_dup= cmd_qd=` before `ip=` (see [Dashboard commands](#mp2-dashboard-commands-dec-013)).
With `APP_ADC_DIAG 1` (default 0) it adds `adc_vref= adc_vbat= adc_vss=`, raw conversions of the ADC's own internal sources.
`APP_DIAG_NO_SENSE 1` (default 0) does not create the sense task, to measure the Wi-Fi link without the sensor load.

### F3 measurements (2026-11-23, iPhone hotspot, broker on the laptop)

Final configuration (`sense` prio 4, 1024-byte MQTT buffer, HC-SR04 echo cap 30 ms), 3-minute runs:
telemetry at the broker about 4.5 msg/s (target 5 Hz, to validate), `pub_err` ~31 % of publishes, ping laptop -> board 149 ms
average with ~7 % loss (laptop -> hotspot gateway: 14 ms, 0 %), RSSI -25 to -35 dBm, 0 DHT11 checksum/no-response failures,
`slot_late_max` 20 ms, 0 resyncs, free heap ~10 KB of 60 KB. Experiments: fallback A (12 ms echo cap) and B (`sense` prio 2) did not
improve the link; test C (no sense task, no telemetry) still shows ping 201 ms average with 7 of 87 lost, so the instability is not caused by
the tasks. A 4096-byte MQTT buffer was not better. Those experiments are reverted. Open items: see the F3 report.

Wi-Fi diagnosis against the Pi on the same hotspot (2026-11-23): the driver starts in IEEE power save (`wifi: power save mode 1`
in the boot log; the SDK default `CONFIG_WIFI_AUTO_POWER_SAVE 1`, `wifi_config_default.h:56-58`, applied at `wlan.c:6533-6536`).
`APP_WIFI_POWER_SAVE_OFF 1` switches it off after each join (mode 0 confirmed in the log) but in the one run made with it the board was
unreachable at layer 2, so the default is 0. Observed on the Pi: while the board retried there were no SYN-RECV entries from it, its ARP
entry was FAILED/INCOMPLETE, while the ESP32 stayed REACHABLE and the Pi-to-gateway ping had 0 % loss. When the link is good the board
publishes 5 msg/s with `pub_err=0` for about 65 s, then the path stalls for 15 s or more and Mosquitto publishes the LWT `offline`.
Once the board stayed in a state where `mqtt_client_connect` returned -4 (`ERR_RTE`) every second while the driver still reported the link up;
`tlm_mqtt_service()` only re-joins on a link-lost event, so that state is not recovered (not changed in this phase).

Hotspot vs home Wi-Fi (2026-11-23, same firmware, broker on the Pi, `sense` prio 4; home = router "Familia_CM", Pi at 192.168.0.239):

| Metric | iPhone hotspot (3-4 min runs) | Familia_CM (clean 10 min run) |
|---|---|---|
| Telemetry delivered to the broker | about 4.5 msg/s, ~91 % of the sent `seq` | 5.0 msg/s, 3082 of 3082 (`seq` 0..3081, none missing) |
| `pub_err` | ~31 % of attempts | 0 |
| `wf_dc` / `mq_dc` / `mq_rc` | 0-1 / 0-2 per run / 0-1 | 0 / 0 / 0 (an earlier 10 min run with stray processes of mine on the laptop had `mq_dc=3`, `pub_err=0`) |
| Ping laptop -> board | 149 ms avg, ~7 % lost, max 881 ms | 41 ms avg, 0 % lost (600/600), p95 112 ms, max 177 ms |
| Ping Pi -> board | 169 ms avg, 36 % lost (25 s window) | 44 ms avg, 0 % lost (600/600), p95 114 ms, max 147 ms |
| Join time | 7-10 s, sometimes 70 s to minutes | 7 s |
| `ERR_RTE` (-4) on connect | seen once | 0 |
| `[wlcm] Error: MX : Invalid country code.` | every join | not printed |
| DHT11 failures | 0 (one timeout with `sense` at prio 2) | 0 of 246 reads |
| Free heap (minimum seen) | ~10 KB | 7.6 KB |

The power-save mode is 1 (IEEE) in both. The link problems seen on the hotspot are not reproduced on the home router.
Boot prints the timebase self-test (`ctimer_hz=16000000 prescale=15 ... core_us=1000000` means CTIMER1 is 1 MHz), the ADC scan
and the Wi-Fi/MQTT progress.

### Testing against a local Mosquitto (not the installed service)

Use a private instance with a password file (same policy as the Pi: no anonymous access). Example on port 1885:

```powershell
$d = "$env:TEMP\mosq1885"; New-Item -ItemType Directory -Force $d | Out-Null
& "C:\Program Files\mosquitto\mosquitto_passwd.exe" -c "$d\passwd" rw612-a        # prompts for the password
& "C:\Program Files\mosquitto\mosquitto_passwd.exe" "$d\passwd" tester
@"
listener 1885 0.0.0.0
allow_anonymous false
password_file $d\passwd
persistence false
"@ | Set-Content "$d\mosquitto.conf"
& "C:\Program Files\mosquitto\mosquitto.exe" -c "$d\mosquitto.conf" -v
```

Subscribe: `mosquitto_sub -p 1885 -u tester -P <pw> -t "v2v/rw612-a/#" -v`. The board must reach the laptop's IP on the
same Wi-Fi network. The `mosquitto.exe` inbound firewall rules created by the installer allow any port; if the board
still cannot connect, a rule for the port is needed (see the F3 report).

## MP2: dashboard commands (DEC-013)

The node subscribes to `v2v/rw612-a/cmd` and `v2v/all/cmd` (QoS 1, not retained) and shows an alert that the dashboard
asks for, on top of the local logic. Payloads (flat JSON objects, contract in `interfaces/`):

```json
{"cmd":"alert","seq":7,"origin":"dashboard","alert_type":"manual|hard_brake|obstacle|overtemp",
 "severity":"info|warning|critical","duration_ms":3000,"source_node":"esp32-b"}
{"cmd":"clear","seq":8,"origin":"dashboard"}
```

| Rule | Behaviour |
|---|---|
| valid `alert` | drives the DEC-004 pattern of its severity for `duration_ms` (100..10000, default 3000, hard cap 10 s) |
| local vs remote | the remote alert is shown only when it is strictly more severe than the local one (or there is none); `sensor_fault` is always the local flag. `info` has no pattern of its own (green), so it never changes the outputs |
| `clear` | ends the remote alert early |
| invalid input | invalid JSON, unknown `cmd`, missing or out-of-range field, duplicate known key, payload over 256 bytes: ignored and logged (`cmd: ignored ERR_...`), never clamped |
| duplicates | (`origin`, `seq`) pairs are remembered (last 16); a repeat is ignored (`cmd: duplicate ...`), also across the two topics and for `clear` |
| publishing | a received command never publishes an alert (the local `alert` mirror is unchanged) |
| ids | `origin` and `source_node` follow `^[a-z0-9-]{1,32}$` (no underscore), like the ESP32 node |
| tolerance | unknown extra keys are **ignored** on receive (the ESP32 node does the same): nodes are tolerant on receive while the schema stays strict for what the dashboard sends |

Code: pure parser, de-duplication and merge in `firmware/common/{include,src}/alert_cmd.{h,c}` (38 host tests,
`tests/unit/test_alert_cmd.c`, no SDK); MQTT intake in `services/tlm_mqtt.c`; actuation in `services/alert_actuation.c`.
The `tlm` task logs and de-duplicates, the `sense` task applies the command through a 4-deep queue (`xQueueReceive`, timeout 0),
so the sense task never blocks and `board_time_*` stays in `sense` only.

SDK facts (paths relative to `C:\nxp\SDK_RW612`):

- Callbacks: `mqtt_set_inpub_callback()` (`middleware/lwip/src/include/lwip/apps/mqtt.h:188`) with a publish callback (once per
  message, `:163`) and a data callback (per chunk, `:151`, last chunk flagged `MQTT_DATA_FLAG_LAST`, `:136`). lwIP calls them from
  `mqtt_message_received` (`middleware/lwip/src/apps/mqtt/mqtt.c:762-775`) and sends the PUBACK itself (`:777-782`).
- The receive buffer is 128 bytes (`MQTT_VAR_HEADER_BUFFER_LEN`, `.../mqtt_opts.h:64`), so a command arrives in chunks: the data
  callback appends them into a 256-byte buffer and parses on the last one.
- Resubscribe: `mqtt_client_connect()` keeps the incoming callbacks (`mqtt.c:1315-1322`, set once in `tlm_mqtt_start`), but the broker
  forgets the subscriptions, so `mqtt_subscribe()` (`mqtt.h:195`) runs from the connection callback on **every** `MQTT_CONNECT_ACCEPTED`
  (pattern of `wifi_examples/wifi_mqtt/mqtt_freertos.c:150-170,188`) and is retried every 3 s until both SUBACKs arrive.
  In flight after a connect: the retained `online` publish plus two SUBSCRIBEs = 3 of `MQTT_REQ_MAX_IN_FLIGHT` 4 (`mqtt_opts.h:71`).

Measured on the bench 2026-11-24 (Familia_CM, Pi 192.168.0.239, 5 min, 4 commands sent mid-run): **telemetry 5.00 msg/s, 1556
messages over 311 s, `seq` 0..1555 with no gap, `pub_err=0`, no Wi-Fi or MQTT drops**; commands applied
(`rem=CRITICAL` 4 s, `WARNING` 6 s, `CRITICAL` 3 s), duplicate dropped, local alert stayed `NONE`, no alert published.
Image growth: text +7688, bss +1836 bytes versus the F3 build. Heap: about 0.6-1 KB of the FreeRTOS heap goes to the two queues
(estimate). Free heap after the join 9.1-9.7 KB; the true minimum (`heap_min`) over 8 boots was 7.7-7.9 KB on five and
5.0-5.3 KB on three: it depends on Wi-Fi join timing (assumption), and F5 (BLE) has to plan for the low value.

## MP2: per-sensor topics (one topic per variable)

MP2 asks for one MQTT topic per sensor variable. Besides the JSON `telemetry` topic (unchanged, 5 Hz, same fields and
`seq`), `rw612-a` publishes:

| Topic | Payload | QoS | Retain | Rate |
|---|---|---|---|---|
| `v2v/rw612-a/sensor/brake_pct` | `42.5` | 0 | no | 2 Hz |
| `v2v/rw612-a/sensor/temp_c` | `36.8` | 0 | no | 2 Hz |
| `v2v/rw612-a/sensor/distance_cm` | `250.0` | 0 | no | 2 Hz |

- Payload is plain decimal text with exactly one decimal (no JSON, no unit, no newline), same rounding as the telemetry
  numbers (`.5` rounds away from zero, never `-0.0`).
- The text `null` means a **sensor fault** only: the matching `sensor_ok` bit is missing (brake pot read failed, DHT11
  failed, no HC-SR04 echo at all). An HC-SR04 reading **out of range is valid** (DEC-008): it is a number and is
  published as `500.0`, never as `null`.
- Formatter: `tlm_json_value1()` in `firmware/common/{include,src}/tlm_json.{h,c}` (pure C, host tests in
  `tests/unit/test_tlm_json.c`). Service: `tlm_mqtt_publish_sensor()` in `services/tlm_mqtt.{c,h}`.
- Scheduling: the `tlm` task wakes every 50 ms (`APP_TLM_TICK_MS`). Telemetry goes out on ticks that are a multiple of 4
  (200 ms); the three sensor topics go out together on ticks = 1 mod 10 (`APP_SENSOR_TOPIC_PERIOD_MS` 500 ms). Both periods
  are an even number of ticks, so the sensor ticks (odd) never share a wake-up with a telemetry tick (even); a build
  `#error` enforces it. Each wake-up adds at most three short publishes (about 45 bytes each on the wire, 1024-byte MQTT
  ring buffer), and the sensor topics read the same latest snapshot as the telemetry.
- `APP_SENSOR_TOPICS 0` in `app/app_config_mp2.h` (or `-DAPP_SENSOR_TOPICS=0`) turns them off, for before/after comparisons.
- Log line: `sen_ok=` / `sen_err=` after `pub_err=` count the sensor-topic publishes (they are also included in `pub_ok` /
  `pub_err`).
- The contract in `interfaces/mqtt-topics.md` is updated by another session; this firmware follows the spec in the task.

Bench check (2026-11-25, V2V-PIT, broker `10.42.0.1`, 3 minutes per run, same code with `APP_SENSOR_TOPICS` 0 vs 1):

| Metric | Topics off | Topics on |
|---|---|---|
| Telemetry at the broker | 4.99 msg/s, `seq` 1155..2054, 0 gaps | 5.01 msg/s over the publishing span, `seq` 0..876, 0 gaps |
| Each sensor topic | none | 2.00 msg/s (351 messages in 175 s), all payloads match `^(-?\d+\.\d\|null)$` |
| `pub_err` / `sen_err` | 0 / 0 | 0 / 0 (`pub_ok` 1981 = 1080 sensor + 901 telemetry) |
| `wf_dc` / `mq_dc` / `mq_rc` | 0 / 0 / 0 | 0 / 0 / 0 |
| `slot_late_max`, free heap | 20 ms, 10192 B | 20 ms, 10192 B (`heap_min` 8576 in both) |
| Image (`text` / `bss`) | 490484 / 235000 | 491012 / 235000 |

A dashboard `alert` (warning, 6 s) and a `clear` sent mid-run were both applied (`cmd_rx` 2, `cmd_ok` 2, `rem=WARNING`
until the `clear`); no alert was published back. No `null` appeared in the run (no sensor fault was provoked).

## Brake range investigation (2026-11-24)

Symptom: the dashboard brake topped out at about 72.2 % (raw about 21 960 with `APP_POT_BRAKE_RAW_MIN 2850 / MAX 29320`), so
`HARD_BRAKE` never fired. **The root cause was not reproduced**; the calibration was **not changed**.

Ruled out (evidence in the session report):

- ADC set-up: MP1 and MP2 use the same `board_io.c` (unchanged since 2026-10-14) and the same `fsl_adc.c` (identical SHA-256, equal
  to the SDK); the GAU clock/reset/power sequence equals `driver_examples/adc/software_trigger/hardware_init.c:22-27`. MP2 differs from MP1
  only in the boot clock tree (`BOARD_BootClockLPR()` instead of `BOARD_BootClockRUN()`, `wifi_mqtt/source/hardware_init.c:19-21`; LPR keeps
  the 256 MHz T3 PLL that feeds the GAU, `clock_config.c:250`, and de-initialises AVPLL/TDDR, `:271-274`) and in `BOARD_InitSleepPinConfig()`
  (sleep-only pad levels, `board.c:187-206`).
- FIR and clamp: the filter has DC gain 1 (`fir_brake_coeffs.h`, sum of taps = 1) and `brake_f` equals `brake` at rest; the 72.2 % was a raw
  level, not a filtered one.
- Boot-time ADC state: 5 consecutive resets with the pot at maximum gave a boot scan of 29 258-29 294 and 29 293-29 346 after the join.
- Drift with uptime: 15 minutes of uptime at rest, then pot at maximum: `raw` 29 362-29 409, internal `vbat_s` constant at 30 104 and `vssa` 0-4.
- Current result: with this firmware the pot reaches raw about 29 380 (brake 100 %) and `HARD_BRAKE`/`critical` fires on every fast press
  (7 of 7 on the final image, each published on `v2v/rw612-a/alert`).

Observed once and unexplained: raw exactly 32767 (ADC full scale) for about 10 s with the pot being handled at 10.7 min of uptime; a
rest reading of 2243 on the previous image versus 2990 afterwards. Both point to the analog input (pot contact or wiring, or the hand on
the pot) rather than to the firmware; with no multimeter at hand the divider voltage (expected 1.00 V at J7-3 with the pot at maximum,
3.3 V at its top) was not measured. If the symptom comes back: set `APP_ADC_DIAG 1` in `app/app_config_mp2.h` and compare `brake_raw` with
`adc_vref`, `adc_vbat`, `adc_vss` (an ADC scale change moves all of them; an input-voltage problem moves only `brake_raw`), then measure J7-3.
(`kADC_TEMPP` is compiled out for the RW612, `fsl_adc.h:322-324`.)

## Rules

- `#include "fsl_*.h"` only inside `board/`.
- Pins come from `hardware/pinout.md`; no pin numbers anywhere else.
- SDK/peripheral questions go to the documentation tutor (separate repo), not guesses.
