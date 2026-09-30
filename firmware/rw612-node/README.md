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

1. MCUXpresso IDE → *Quickstart Panel* → **Import SDK example(s)…** → board `frdmrw612` → pick the example.
2. Import into this folder: **`firmware/rw612-node/`** as the project location
   (untick *Copy sources* only if you know the SDK path is identical on every PC; copying is safer).
3. Move/create our code into the layer folders:

   | Folder | Put here |
   |---|---|
   | `app/` | `main.c`, task creation, state machine |
   | `services/` | `v2v_*.c` (GATT service), `tlm_*.c` (MQTT), `alert_*.c` (actuation) |
   | `drivers/` | sensor/actuator drivers built on `board/` |
   | `board/` | `board.c/.h`, `pin_mux.c/.h`, `clock_config.c/.h`, SDK peripheral wrappers |

   The example's SDK folders (`device/`, `component/`, `middleware/`, `CMSIS/`, `startup/`,
   `utilities/`) stay as generated and are git-ignored.
   **Name clash:** the IDE puts SDK peripheral drivers (`fsl_*.c/.h`) in a `drivers/`
   folder, the same name as our layer. `.gitignore` excludes `fsl_*` by name, so only
   our own drivers are committed. Never put our files under an `fsl_` prefix.
4. Project → Properties → C/C++ Build → Settings → *MCU C Compiler → Includes*, add:
   - `${ProjDirPath}/../../interfaces/c`
   - `${ProjDirPath}/../common/include`
5. Add `firmware/common/src` as a **linked source folder**
   (Project → New → Folder → Advanced → *Link to alternate location*: `PROJECT_LOC/../common/src`).
6. Build. Commit only our files plus the `.project`/`.cproject` if the team agrees
   they are portable (TBD — check for absolute paths first).

## Rules

- `#include "fsl_*.h"` only inside `board/`.
- Pins come from `hardware/pinout.md`; no pin numbers anywhere else.
- SDK/peripheral questions go to the documentation tutor (separate repo), not guesses.
