# hardware

| File | Content |
|---|---|
| [`pinout.md`](pinout.md) | pin assignment per node (all TBD) |
| [`bom.md`](bom.md) | bill of materials (models TBD) |

Rules:
- **No invented values.** A pin or a sensor model is written here only after it
  has been checked against the board schematic / user manual or the part datasheet,
  with the source cited in the table.
- Changing a pin means updating `pinout.md` in the same PR as the `board/` code
  (RW612) or `config.h` template (ESP32).

Wiring diagrams (photo or Fritzing/KiCad export) go in this folder when available.
