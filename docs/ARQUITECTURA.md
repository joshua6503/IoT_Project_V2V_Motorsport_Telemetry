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

| Componente | Corre en | Estado (23-nov; Gateway y nodos: 25-nov) |
|---|---|---|
| Nodo A — `firmware/rw612-node` | FRDM-RW612, MCUXpresso, **FreeRTOS + lwIP** (DEC-012), base `wifi_examples/wifi_mqtt` | `rw612-a` publica directo a la Pi por Wi-Fi/MQTT. **Estable en Wi-Fi de casa Familia_CM** (3082/3082 msgs en 10 min, 0 pérdidas); **inestable en el hotspot del iPhone**. **En pit (V2V-PIT, 25-nov): 925/925 msgs en 3 min (4.97 msg/s), 0 pérdidas, 0 `offline`**; contadores propios tras 12 min y un reinicio de la Pi: `pub_err` 3 de 2455, un corte de Wi-Fi/MQTT (el del reinicio), RSSI -42 dBm |
| Nodo B — `firmware/esp32-node` | ESP32 clásico, Arduino IDE | `esp32-b` publica a la Pi, pero **pierde mensajes en el Wi-Fi de casa** (prueba limpia de 10 min: ~34 % de la telemetría, envíos TCP detenidos con errno 119, 20 reconexiones). Buzzer pasivo en GPIO33; calibración de freno medida. **En pit (V2V-PIT, 25-nov, TX 8.5 dBm): 925/925 msgs en 3 min (4.97 msg/s), 0 pérdidas, 0 `offline`** (prueba corta; falta repetir la de 10 min) |
| Gateway — Mosquitto | **Raspberry Pi 5**: hostname `RaspPi5`, Raspberry Pi OS (Debian 13 trixie, aarch64), usuario `joshua`, SSH con llave | Scripts listos en `gateway/pi/` (`setup_pi.sh`, `pi-mode.sh`, `verify_pi.sh`); **Pi 5 con Mosquitto con usuarios; modo pit probado el 25-nov y confirmado: la Pi arranca en pit** (AP V2V-PIT, broker 10.42.0.1:1883; tras `sudo reboot` el AP y el broker volvieron solos en ~70 s y los dos nodos se reconectaron sin intervención). En home queda en Familia_CM, 192.168.0.239 (DHCP). Dos modos porque su radio Wi-Fi no puede ser cliente y AP a la vez: **home** (cliente del Wi-Fi de casa, hotspot del iPhone de respaldo; mantenimiento) y **pit** (AP «V2V-PIT», 2.4 GHz, WPA2; trabajo con nodos y demo). Mosquitto con usuario/contraseña |
| Dashboard MP2 — `dashboard/python/dashboard.py` | Python (Tkinter + paho-mqtt) en la **laptop** de Joshua (Windows) | DEC-015: dashboard de MP2. Muestra los dos nodos (en línea, último mensaje, freno, temperatura, distancia), registro de alertas, panel de comandos y reenvío de frenado brusco (DEC-014). Probado con el simulador |
| Dashboard NXP — `dashboard/node-red/flows.json` | Node-RED en la laptop | Se conserva para la presentación en NXP, sin cambios (DEC-015). Flows y `start-dashboard.ps1`; probado con el simulador (falta captura con nodos reales) |
| Simulador — `tools/node-simulator` | Cualquier PC | Publica dos nodos (rw612-a, esp32-b) con status/LWT y alertas; registra los comandos `cmd` que recibe (valida, deduplica, no actúa); broker y credenciales por argumentos o variables de entorno |
| Contratos — `interfaces/` | — | v0.3: comandos `cmd` congelados (DEC-013, cierra OPEN-010). v0.2 (telemetría: solo `node_id`, `ts_ms`, `seq` obligatorios; `brake_pct`, `tps_pct`, `sensor_ok` agregados) |
| Pruebas — `tests/` | PC / CI | Unitarias pasan (`alert_logic`, `fir_filter`); integración pasa completa en local, incluida la prueba con broker (Mosquitto nativo de la laptop) |

## 4. Integraciones

| Enlace | Contrato | Estado |
|---|---|---|
| Nodo → broker (telemetría, alerta espejo, status/LWT) | `interfaces/mqtt-topics.md`, `payload.schema.json` | Contrato v0.2 listo (F1). `rw612-a` (estable en casa) y `esp32-b` (con pérdidas) publican directo a la Pi por Wi-Fi |
| Nodo ↔ nodo (V2V) | `interfaces/ble-gatt.md`, `v2v_protocol.h` | Contrato listo; nada implementado |
| Broker → dashboard | Dashboard Python de MP2 (DEC-015) o Node-RED (NXP) en la laptop, usuario MQTT `dashboard` | Funcionando; probado con el simulador, pendiente con nodos reales |
| Dashboard → nodos (comandos) | `v2v/{node_id}/cmd` y `v2v/all/cmd` (DEC-013), QoS 1, sin retain; `interfaces/mqtt-topics.md` | Contrato congelado y dashboard listo. **Firmware pendiente** en ambos nodos (otras sesiones); hoy solo responde el simulador |
| Red de la demo | La Pi en modo **pit**: AP «V2V-PIT» (2.4 GHz, WPA2); broker en `10.42.0.1:1883` (dirección fija del AP); los nodos siempre apuntan a V2V-PIT. En modo **home** la Pi es cliente de la red de casa (nombre `RaspPi5.local`; esas IP cambian con la red) | Implementado en `gateway/pi/pi-mode.sh`; detalle en `gateway/pi/README.md`. **Probado el 25-nov:** laptop en V2V-PIT con internet por Ethernet, ambos nodos publican a 10.42.0.1 |

## 5. Seguridad y secretos

- Credenciales Wi-Fi: ESP32 en `config.h` (ignorado por git, plantilla `config.example.h`);
  RW612 en un header local ignorado equivalente (se define en la fase del RW612).
- Mosquitto con usuario/contraseña (`rw612-a`, `esp32-b`, `dashboard`; `allow_anonymous false`), sin TLS, solo en la red local/AP de la demo; sin exponer nada a internet.
- Las contraseñas (Wi-Fi de casa, V2V-PIT, MQTT) las escribe Joshua con entrada oculta; las credenciales MQTT de Node-RED quedan en su almacén de credenciales, no en `flows.json`.
- Las contraseñas de la Pi las pone Joshua; nunca en el chat ni en el repo.

## 6. Decisiones vigentes

| ID | Decisión |
|---|---|
| ADR-0001 | V2V por BLE (Thread/802.15.4 queda como futuro) |
| ADR-0002, 0004, 0005 | ESP32/Arduino como nodo B; monorepo con contratos primero; firmware en capas app/services/drivers/board (Accepted 2026-11-11) |
| ADR-0003 | Gateway = Raspberry Pi 5 + Mosquitto con usuario/contraseña; dos modos de red (home/pit) (**Accepted** 2026-11-23) |
| D1 (23-nov) | MP2 = ambos nodos (rw612-a, esp32-b) publican directo por Wi-Fi/MQTT al broker. El V2V por BLE pasa a la fase siguiente (F5) |
| D2 (23-nov) | La Pi tiene modos **home** (cliente Wi-Fi) y **pit** (AP «V2V-PIT»); los nodos siempre se configuran para V2V-PIT. Dashboard Node-RED en la laptop |
| DEC-001 | Base RW612 = `hello_world`, superloop bare-metal. **Reemplazada por DEC-012 para MP2** |
| DEC-002 | Sensores MP1: HC-SR04, DHT11, pot de freno, pot de TPS (opcional), LEDs, buzzer |
| DEC-003 / 004 | Arbitraje de severidad y patrones de actuador |
| DEC-005..008 | Mapa de pines MP1, periodos de muestreo, timebase CTIMER1, criterios de falla del HC-SR04 |
| DEC-009..011 | Filtro FIR de freno (N=9, fc=2 Hz, fs=20 Hz), muestreo sin deriva, divisores reales |
| DEC-012 (23-nov) | MP2: el firmware del RW612 pasa del superloop bare-metal (DEC-001) a **FreeRTOS + lwIP**, base `wifi_examples/wifi_mqtt` |
| DEC-013 (23-nov) | Contrato de comandos (cierra OPEN-010): tópico `v2v/{node_id}/cmd` para un nodo y `v2v/all/cmd` para todos (`all` es `node_id` reservado); QoS 1, sin retain. Payload JSON `{"cmd":"alert","seq","origin":"dashboard","alert_type","severity","duration_ms" 100..10000 (3000 por defecto),"source_node" opcional}` y `{"cmd":"clear","seq","origin"}`. El nodo, ante un `alert` válido, activa el patrón de esa severidad durante `duration_ms` (tope 10 s); una alerta local de mayor severidad gana; `clear` la termina antes; `cmd` desconocido, JSON inválido o campos fuera de rango se ignoran y se registran; deduplica por `(origin, seq)`. Un comando recibido nunca hace que el nodo publique una alerta (sin lazos) |
| DEC-014 (23-nov) | Reenvío por el dashboard (no por los nodos): con el interruptor «Reenviar frenado brusco» activo, si el nodo X publica `hard_brake`, el dashboard envía `cmd alert` (`hard_brake`, `critical`, `source_node=X`) a los demás nodos conocidos. Es actuación remota IoT por el broker, no el camino de seguridad; el V2V directo sigue siendo BLE (F5) |
| DEC-015 (23-nov) | Dashboards: MP2 usa un dashboard simple en Python (`dashboard/python/`); el de Node-RED se queda en `dashboard/node-red/` para la presentación en NXP |
| OPEN-013 (nodo B) | Resuelto para el nodo B: buzzer **pasivo en GPIO33** |
| Calibración freno ESP32 | Medida: 170–3010 mV; falla en 80/3090 |

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
| F0 Reestructura | Repo a fases + verificación del estado real | 11–23 nov | ✅ 23-nov |
| F1 Pi 5 (broker + modos casa/pits) + dashboard Node-RED en laptop + schema | Broker con usuarios en la Pi, modos home/pit con rollback, dashboard en vivo, schema v0.2 | 23–24 nov | ✅ cerrada el 25-nov: modo pit probado y confirmado (la Pi arranca en pit); la captura del dashboard con nodos reales sigue en §9b |
| F2 ESP32 publicando por Wi-Fi/MQTT | | 23–24 nov | en curso (inestabilidad del ESP32 en el Wi-Fi de casa, ver §9; en pit 100 % en 3 min el 25-nov, falta la prueba de 10 min) |
| F3 RW612 publicando por Wi-Fi/MQTT (base: ejemplo wifi del SDK / práctica 3) | | 23–25 nov | ✅ hecha (rw612-a) |
| F4 Demo MP2 + diagramas de conexión | Dashboard MP2 en Python con comandos (DEC-013..015) | 24–25 nov | en curso (diagramas, demo; dashboard Python y contrato `cmd` listos, firmware de `cmd` pendiente) |
| F5 V2V por BLE en ambos nodos (Wi-Fi + BLE a la vez en la RW612) | | después de MP2 | pendiente |
| F6 Roadmap mesh (Thread/802.15.4) para la presentación en NXP | | después de MP2 | pendiente |

F2 y F3 corren en paralelo (personas distintas). MP2 se califica con los dos nodos publicando
directo al broker por Wi-Fi; el BLE V2V va después (F5).

## 9. Pendientes de Joshua / equipo

### Siguiente sesión (en este orden)

a. **El ESP32 pierde mensajes en el Wi-Fi de casa.** El RW612 en la misma red y con el mismo broker
   va perfecto, así que la causa está del lado del ESP32. Hipótesis, en orden:
   1. Familia_CM tiene dos BSSID de 2.4 GHz (señal 95 % y 72 %, repetidor/mesh) → registrar
      BSSID/canal/RSSI conectados y fijar el BSSID más fuerte.
   2. `esp_mqtt_client_publish` se bloquea con el socket detenido → revisar enqueue/outbox y
      timeouts de red.
   3. Arreglar el watchdog de telemetría para que no dependa del estado de MQTT.
   4. Solo después, el ahorro de energía del Wi-Fi en el perfil de la Pi.

   Repetir la prueba limpia de 10 min.
b. Captura del dashboard con los dos nodos reales en la Pi → `docs/04-testing/evidence/`.
c. Diagramas de conexión, si no están hechos. Deben mostrar el **buzzer en GPIO33** (ESP32). El prompt S4 está en el
   documento del Project de claude.ai `claude/MP2_PROMPTS.md`.
d. ✅ 25-nov: prueba del modo pit hecha (internet de la laptop por Ethernet en vez del tethering; la Pi arranca en pit, comprobado con `sudo reboot`). Opcional pendiente: la prueba de rollback (D del checklist).
e. Cerrar MP2: (la IP fija ya no hace falta: la Pi arranca en pit); borrar
   `/etc/sudoers.d/v2v-tests` en la Pi; borrar `C:/Users/joshu/pi-sub.opts`; actualizar §8.

### Abiertos

- RW612 no se recupera de un ERR_RTE sostenido.
- Heap mínimo de FreeRTOS en el RW612: 7.6 KB. F5 (BLE) necesitará más; `coex_wifi_edgefast` es el
  único ejemplo de coexistencia para FRDM-RW612.

### Generales

- Confirmar la fecha real de entrega (se trabaja con el 25-nov).
- Asignar quién lleva cada fase (sugerido: Joshua RW612, otro integrante ESP32, otro Pi/dashboard).
- Preguntas al rw612-tutor para F4/F5: coexistencia Wi-Fi + BLE, memoria.
- Actualizar las instrucciones del Project de claude.ai al método por fases.
