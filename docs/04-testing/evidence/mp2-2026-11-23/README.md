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

El simulador registró cada comando recibido (alerta, `clear`, `todos`, reenvío con `source_node`). Pendiente: prueba contra el
broker de la Pi (requiere la contraseña de `dashboard`) y con las placas reales, cuyo firmware aún no atiende `cmd`.

El banner rojo de alerta crítica y los estados «Fuera de línea» / «sin eco» se probaron en vivo en la misma sesión
(capturas no guardadas). Pendiente: captura con los dos nodos reales y el broker de la Pi (ARQUITECTURA §9 b).
