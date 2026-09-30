# esp32-node — secondary node (ESP32 classic, Arduino)

Test/support node for V2V. See [ADR-0002](../../docs/adr/0002-esp32-arduino-as-secondary-node.md).

## Layout

```
esp32-node/
└── esp32-node/            <- Arduino sketch folder (name must match the .ino)
    ├── esp32-node.ino
    ├── config.example.h   <- template, committed
    ├── config.h           <- your copy, git-ignored (Wi-Fi/MQTT secrets)
    └── src/
        └── v2v_protocol.h <- AUTO-GENERATED copy of interfaces/c/v2v_protocol.h
```

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
   Board: *ESP32 Dev Module* (`esp32:esp32:esp32`). Exact dev-kit model: TBD (`hardware/bom.md`).
2. From the repo root (PowerShell):

   ```powershell
   python tools/sync_protocol.py
   Copy-Item firmware/esp32-node/esp32-node/config.example.h firmware/esp32-node/esp32-node/config.h
   ```

3. Edit `config.h`, open `esp32-node.ino`, Upload. Serial monitor at 115200.

With arduino-cli (same as CI):

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/esp32-node/esp32-node
```
