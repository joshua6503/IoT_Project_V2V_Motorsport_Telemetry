# Evidencia F1 — dashboard Node-RED (2026-11-23)

Capturas con **datos simulados** (`tools/node-simulator`, dos nodos) contra el Mosquitto nativo de la laptop
(sin usuario). No son datos de los nodos reales ni del broker de la Pi.

| Archivo | Muestra |
|---|---|
| `dashboard-gauges-charts.jpg` | tarjetas En línea, indicadores de freno/temperatura/distancia y gráficas de 5 min |
| `dashboard-alerts.jpg` | registro de alertas (hora, nodo, tipo, severidad) |

## Dashboard Python de MP2 (DEC-015)

Captura con **datos simulados** (`tools/node-simulator`, nodos `rw612-a` y `esp32-b`) contra el Mosquitto nativo de la
laptop (sin usuario). No son nodos reales ni el broker de la Pi.

| Archivo | Muestra |
|---|---|
| `dashboard-python-sim.png` | tarjetas de los dos nodos, panel de comandos con el reenvío de frenado brusco activo y el registro: alerta crítica recibida (rojo), comandos enviados (azul), duración inválida rechazada y `hard_brake` reenviado solo a `esp32-b` |

El simulador registró cada comando recibido (alerta, `clear`, `todos`, reenvío con `source_node`).

### Contra el broker de la Pi (192.168.0.239:1883)

| Archivo | Muestra |
|---|---|
| `dashboard-python-pi.png` | dashboard conectado como `dashboard` al Mosquitto de la Pi, con los nodos reales `rw612-a` y `esp32-b` (telemetría real) y los simulados `sim-01` y `sim-02` (`tools/node-simulator`), reenvío de frenado brusco activo: cada `hard_brake` se reenvía a los demás nodos (filas azules «cmd alert hard_brake (de …)») |

Los nodos simulados responden a los comandos en su log. Las placas reales reciben los comandos pero **aún no los atienden**
(su firmware no maneja `cmd`). El `--` en temperatura y distancia de `esp32-b` es lo que llega en su telemetría en ese momento
(sin valor en esos campos), no un fallo del dashboard.

El banner rojo de alerta crítica y los estados «Fuera de línea» / «sin eco» se probaron en vivo en la misma sesión
(capturas no guardadas). Pendiente: captura con los dos nodos reales y el broker de la Pi (ARQUITECTURA §9 b).
