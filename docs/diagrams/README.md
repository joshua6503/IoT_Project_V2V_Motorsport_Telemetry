# Diagramas MP2

| Archivo | Qué muestra |
|---|---|
| `rw612_conexiones.png/.svg` | Cableado del nodo A (FRDM-RW612), valores de `hardware/pinout.md` (DEC-005, DEC-011) |
| `esp32_conexiones.png/.svg` | Cableado del nodo B (ESP32 DevKitC): pot de freno en GPIO32, LEDs 25/26/27, buzzer pasivo en GPIO33 |
| `sistema_mp2.png/.svg` | Arquitectura MP2: nodos → Wi-Fi → Raspberry Pi 5 (Mosquitto) → dashboard Node-RED; BLE V2V como fase F5 |
| `flujo_mqtt.png/.svg` | Topics `v2v/{node_id}/telemetry|alert|status` con QoS, retain y LWT |

Voltajes de los divisores: calculados, no medidos (salvo la calibración del freno del ESP32, medida en banco).

## Regenerar (PowerShell, desde la raíz del repo)

```powershell
python -m venv tools/diagrams/.venv
tools/diagrams/.venv/Scripts/pip install -r tools/diagrams/requirements.txt
Set-Location tools/diagrams
foreach ($s in "rw612.py","esp32.py","sistema.py","flujo.py") { .venv/Scripts/python $s }
Move-Item *.png, *.svg ../../docs/diagrams/ -Force
Set-Location ../..
```
