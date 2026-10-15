# rw612-node — main node (NXP FRDM-RW612)

The MCUXpresso project is **not** in the repo yet. It is generated from the IDE
by importing an SDK example, then reorganised into the layers of
[ADR-0005](../../docs/adr/0005-layered-firmware-architecture.md).

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

Recommended path: start M1 from `peripheral_ht`, move to the coex example for M2.
Final choice: **TBD**, record it in `docs/project-notes.md` (and an ADR if it changes the architecture).

## Import steps

The IDE project is local and **not committed** (`firmware/rw612-node/V2V_IOT/` is git-ignored).
Each teammate recreates it; the settings below are the ones verified with a headless build
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

## Rules

- `#include "fsl_*.h"` only inside `board/`.
- Pins come from `hardware/pinout.md`; no pin numbers anywhere else.
- SDK/peripheral questions go to the documentation tutor (separate repo), not guesses.
