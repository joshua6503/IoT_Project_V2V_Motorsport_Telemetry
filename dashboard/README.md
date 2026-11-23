# dashboard — Node-RED (en la laptop)

Dashboard en vivo de los dos nodos (`rw612-a`, `esp32-b`) con **Node-RED + `@flowfuse/node-red-dashboard`**
(Dashboard 2.0; el `node-red-dashboard` antiguo está deprecado). Corre en la laptop de Joshua (Windows),
no en la Pi. El broker es el Mosquitto de la Pi (ver [`gateway/pi/README.md`](../gateway/pi/README.md)).

## Arranque

Requiere Node.js LTS (`winget install OpenJS.NodeJS.LTS`).

```powershell
.\dashboard\start-dashboard.ps1 -Broker RaspPi5.local      # modo home / hotspot
.\dashboard\start-dashboard.ps1 -Broker 10.42.0.1          # modo pit (con la laptop en V2V-PIT)
.\dashboard\start-dashboard.ps1 -Broker localhost -NoAuth  # Mosquitto de la laptop, sin usuario (desarrollo)
```

- Dashboard: <http://localhost:1880/dashboard> · Editor: <http://localhost:1880/> (solo accesible desde la laptop).
- La primera vez instala Node-RED y el dashboard en `%USERPROFILE%\.node-red-v2v` (**fuera del repo**) y copia
  `flows.json` desde el repo. `-ResetFlows` vuelve a copiarlo (pierde cambios hechos en el editor).
- Usuario/contraseña MQTT (usuario `dashboard`): se piden **una vez** con entrada oculta y quedan en el almacén
  de credenciales cifrado de Node-RED (`flows_cred.json`), nunca en `flows.json`. `-AskCredentials` para cambiarlos.
- La dirección del broker llega por las variables `V2V_BROKER_HOST` / `V2V_BROKER_PORT` (las pone el script).

## Qué muestra (etiquetas en español)

- Una tarjeta por nodo: **En línea / Fuera de línea** (del `status` retenido) y «último mensaje hace N s»
  (ámbar si está en línea pero sin datos en 5 s).
- Indicadores: Freno %, Temperatura °C, Distancia cm (**«sin eco»** cuando llega `null`; lo mismo «sin dato» en temperatura).
- Tres gráficas de los últimos 5 minutos con los dos nodos.
- Registro de alertas (hora, nodo, tipo, severidad; sin duplicados por `node_id`+`seq`) y **banner rojo** mientras haya
  una alerta crítica activa (10 s desde la última; constante `BANNER_SEG` en el nodo «Registro de alertas»).
- Mensajes mal formados (JSON inválido, `node_id` distinto del tópico, campos fuera de rango) se **ignoran** sin romper
  el flujo; el nodo «Validar mensaje» muestra el contador y avisa en el log como máximo cada 10 s.
- Otros `node_id` (p. ej. `sim-01`) no se muestran: el tablero cubre `rw612-a` y `esp32-b`.

## Internet de la laptop mientras está en V2V-PIT

V2V-PIT no tiene salida a internet. Usa el tethering USB del iPhone y fija las métricas de ruta
(comandos de verificación y de reversa en el checklist de `gateway/pi/README.md`).

## Flujo de trabajo con `flows.json`

`dashboard/node-red/flows.json` es la copia versionada. Si editas en Node-RED: ☰ → Export → *All flows* →
sobrescribe ese archivo (comprueba que no contenga credenciales; el export normal no las incluye).
Commit: `feat(dashboard): ...`.

## Evidencia

Capturas con datos simulados (laptop, Mosquitto nativo): [`docs/04-testing/evidence/mp2-2026-11-23/`](../docs/04-testing/evidence/mp2-2026-11-23/README.md)
([indicadores y gráficas](../docs/04-testing/evidence/mp2-2026-11-23/dashboard-gauges-charts.jpg), [alertas](../docs/04-testing/evidence/mp2-2026-11-23/dashboard-alerts.jpg)).

## Pendiente (M4)

Historial largo con InfluxDB + Grafana.
