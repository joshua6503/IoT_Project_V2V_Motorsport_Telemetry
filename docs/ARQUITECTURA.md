# ARQUITECTURA — V2V Motorsport Telemetry

Documento vivo y único del proyecto. Se actualiza al cerrar cada fase.
Método: desarrollo por fases (chat = arquitecto/revisor, Claude Code = implementador,
Joshua y equipo = deciden, prueban en banco y hacen lo físico).
Lo anterior (capas L0–L7, STATUS, workflow) queda archivado en `docs/archive/` como referencia.

## 1. Objetivo

Cada vehículo es un nodo que mide su telemetría, la publica por **Wi-Fi/MQTT** a un
dashboard local y comparte eventos críticos (frenado fuerte, obstáculo, sobretemperatura)
con otros vehículos por **BLE (V2V)**, donde el receptor activa un actuador.
**Meta de noviembre 2026:** los dos nodos (FRDM-RW612 y ESP32) funcionando, publicando
al gateway en la Raspberry Pi, con dashboard en vivo.

## 2. Principios (no se negocian)

1. El camino de seguridad es local/BLE; MQTT es informativo. Ninguna alerta depende del broker.
2. Contratos primero: un cambio de protocolo se hace en `interfaces/` y luego en código.
3. Lógica pura en `firmware/common/` con prueba unitaria; `fsl_*.h` solo en `board/`.
4. No se inventan pines ni datos del chip: dato sin cita = supuesto, y se pregunta al rw612-tutor.
5. Nunca se suben al repo el SDK de NXP ni secretos (`config.h`, credenciales Wi-Fi).
6. Cada fase deja algo que funciona en banco. Lo que depende de algo externo no bloquea: se
   construye con respaldo (simulador, datos falsos) y se activa después.

## 3. Componentes

| Componente | Corre en | Estado (11-nov) |
|---|---|---|
| Nodo A — `firmware/rw612-node` | FRDM-RW612, MCUXpresso, superloop bare-metal | MP1 + filtro FIR funcionando en banco; **sin red** |
| Nodo B — `firmware/esp32-node` | ESP32 clásico, Arduino IDE | Esqueleto (solo imprime versión) |
| Gateway — Mosquitto + Node-RED | **Raspberry Pi 5** con Raspberry Pi OS (antes laptop con Docker) | `docker-compose.yml` existe para laptop; en la laptop de Joshua corre un Mosquitto nativo de Windows (puerto 1883), no Docker; Pi sin configurar |
| Dashboard — `dashboard/node-red/flows.json` | Node-RED en la Pi | Vacío |
| Simulador — `tools/node-simulator` | Cualquier PC | Listo (publica telemetría válida) |
| Contratos — `interfaces/` | — | v0.1 (MQTT, JSON Schema, GATT, `v2v_protocol.h`) |
| Pruebas — `tests/` | PC / CI | Unitarias pasan (`alert_logic`, `fir_filter`); integración pasa completa en local, incluida la prueba con broker (Mosquitto nativo de la laptop) |

## 4. Integraciones

| Enlace | Contrato | Estado |
|---|---|---|
| Nodo → broker (telemetría, alerta espejo, status/LWT) | `interfaces/mqtt-topics.md`, `payload.schema.json` | Contrato listo; falta `brake_pct`/`tps_pct` en el schema |
| Nodo ↔ nodo (V2V) | `interfaces/ble-gatt.md`, `v2v_protocol.h` | Contrato listo; nada implementado |
| Broker → dashboard | Node-RED | Pendiente |
| Red de la demo | La Pi 5 como access point de 2.4 GHz (SSID propio, IP fija del broker) | Recomendado, por confirmar; independiente de la red escolar y del celular |

## 5. Seguridad y secretos

- Credenciales Wi-Fi: ESP32 en `config.h` (ignorado por git, plantilla `config.example.h`);
  RW612 en un header local ignorado equivalente (se define en la fase del RW612).
- Mosquitto en red local de demo; sin exponer nada a internet.
- Las contraseñas de la Pi las pone Joshua; nunca en el chat ni en el repo.

## 6. Decisiones vigentes

| ID | Decisión |
|---|---|
| ADR-0001 | V2V por BLE (Thread/802.15.4 queda como futuro) |
| ADR-0002, 0004, 0005 | ESP32/Arduino como nodo B; monorepo con contratos primero; firmware en capas app/services/drivers/board (Accepted 2026-11-11) |
| ADR-0003 | Mosquitto local (**Proposed**; se reescribe en F1 al pasar el gateway a la Raspberry Pi 5) |
| DEC-001 | Base RW612 = `hello_world`, superloop (FreeRTOS se pospuso; **la Wi-Fi lo va a requerir** (supuesto, confirmar con rw612-tutor), ver F3) |
| DEC-002 | Sensores MP1: HC-SR04, DHT11, pot de freno, pot de TPS (opcional), LEDs, buzzer |
| DEC-003 / 004 | Arbitraje de severidad y patrones de actuador |
| DEC-005..008 | Mapa de pines MP1, periodos de muestreo, timebase CTIMER1, criterios de falla del HC-SR04 |
| DEC-009..011 | Filtro FIR de freno (N=9, fc=2 Hz, fs=20 Hz), muestreo sin deriva, divisores reales |

Detalle completo de cada una: `docs/archive/layers/STATUS.md`.

## 7. Líneas de trabajo

**Línea A — Proyecto principal (IoT, V2V Motorsport Telemetry).** Equipo de 4. Es el plan de fases de abajo.

**Línea B — PDS (filtro FIR).** Solo Joshua y Jair. Fue la adaptación para Procesamiento
Digital de Señales y está **congelada**: el código vive en el repo detrás de
`APP_ENABLE_BRAKE_FIR`, activo por defecto (=1) y alimenta `alert_evaluate()`; con 0 el
comportamiento es idéntico a MP1. Solo le queda HIL-20 y el reporte
si la materia lo pide. No genera fases nuevas en la línea A.

## 8. Fases (línea A)

Fecha objetivo: **25-nov-2026** (supuesta; la fecha exacta de entrega aún no se conoce).

| Fase | Qué deja funcionando | Fechas | Estado |
|---|---|---|---|
| F0 | Reestructura del repo a fases + verificación del estado real (qué compila, qué pasa, qué falta) | 11–12 nov | en curso |
| F1 | Raspberry Pi como gateway: Mosquitto + Node-RED con dashboard básico, alimentado por el simulador; schema con `brake_pct`/`tps_pct` | 12–14 nov | pendiente |
| F2 | ESP32 publicando telemetría real + status/LWT en el dashboard | 14–17 nov | pendiente |
| F3 | RW612 con Wi-Fi + MQTT publicando su telemetría y alertas, partiendo de un ejemplo con coexistencia Wi-Fi + BLE (migración a FreeRTOS) | 14–20 nov | pendiente |
| F4 | V2V por BLE (obligatorio para la meta): alerta del RW612 → actuador en el ESP32, con Wi-Fi activa al mismo tiempo | 20–24 nov | pendiente |
| F5 | Demo integrada, evidencia y entregables de IoT | 24–25 nov | pendiente |

F2 y F3 corren en paralelo (personas distintas). El V2V por BLE es parte de la meta,
así que el lado BLE del ESP32 puede adelantarse durante F2/F3 con un emisor de prueba.

## 9. Pendientes de Joshua / equipo

- Confirmar la red: Pi 5 como access point (recomendado).
- Confirmar la fecha real de entrega (se trabaja con el 25-nov).
- Asignar quién lleva cada fase (sugerido: Joshua RW612, otro integrante ESP32, otro Pi/dashboard).
- Preguntas al rw612-tutor para F3/F4: ejemplo base Wi-Fi + MQTT + BLE, coexistencia, memoria.
- Actualizar las instrucciones del Project de claude.ai al método por fases.
