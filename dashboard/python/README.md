# Dashboard Python (MP2)

Dashboard simple de escritorio (Tkinter + paho-mqtt) para MP2: muestra los dos nodos y envía
comandos. Es el dashboard de MP2 (DEC-015); el de Node-RED sigue en `dashboard/node-red/` para la
presentación en NXP.

## Instalación

Desde la raíz del repo, en PowerShell, con el `.venv` del proyecto (sin instalar nada global):

```powershell
.venv\Scripts\Activate.ps1
pip install -r dashboard/python/requirements.txt
```

## Ejecución

```powershell
.venv\Scripts\python dashboard/python/dashboard.py
```

Pide la contraseña del usuario MQTT con entrada oculta. Nunca se guarda en un archivo.

| Opción | Variable de entorno | Por defecto |
|---|---|---|
| `--broker host[:puerto]` | `V2V_BROKER` | `192.168.0.239:1883` (la Pi en la red de casa; en modo pit usa `10.42.0.1:1883`) |
| `--username` | `V2V_MQTT_USER` | `dashboard` |
| (sin opción) | `V2V_MQTT_PASSWORD` | pregunta con entrada oculta |
| `--nodes a,b` | | `rw612-a,esp32-b` |

```powershell
# ejemplo: modo pit
.venv\Scripts\python dashboard/python/dashboard.py --broker 10.42.0.1:1883
```

## Qué muestra cada panel

- **Estado de conexión** (arriba): verde si está conectado al broker; rojo si el broker lo rechaza o se cae
  (reintenta solo).
- **Banner rojo**: «ALERTA CRÍTICA — nodo / tipo» durante 3 s después de cada alerta crítica.
- **Una tarjeta por nodo** (`rw612-a`, `esp32-b`; aparece otra si llega un nodo nuevo):
  - *En línea / Fuera de línea*, según el estado retenido del nodo (LWT).
  - *Último mensaje*: hace cuántos segundos llegó algo del nodo.
  - *Freno* (%), *Temperatura* (°C) y *Distancia* (cm; «sin eco» cuando el sensor no recibe eco).
- **Registro**: una fila por alerta recibida (hora, nodo, tipo, severidad; en rojo las críticas) y por
  cada comando enviado (en azul, nodo `→ destino`). Guarda las últimas 200.
- **Comandos**:
  - *Destino*: `rw612-a`, `esp32-b` o `todos` (publica en `v2v/all/cmd`).
  - *Severidad*: `warning` (por defecto) o `critical`.
  - *Duración (ms)*: de 100 a 10000, por defecto 3000.
  - *Enviar alerta*: comando `alert` de tipo `manual`. *Apagar alerta*: comando `clear`.
- **Reenviar frenado brusco a los demás nodos** (DEC-014, apagado al iniciar): si un nodo X publica una alerta
  `hard_brake`, el dashboard envía `alert hard_brake critical` con `source_node=X` a los demás nodos. Es actuación
  remota por el broker, **no** el camino de seguridad; el V2V directo es BLE (F5).

El contrato de los comandos está en `interfaces/mqtt-topics.md`. Los nodos reales aún no reaccionan a los
comandos (falta el firmware); el simulador (`tools/node-simulator`) sí los registra en su log.

## Prueba sin placas

```powershell
# terminal 1: nodos simulados (no pisan el estado de los reales)
.venv\Scripts\python tools/node-simulator/simulator.py --broker 192.168.0.239:1883 --username dashboard --nodes sim-01,sim-02
# terminal 2
.venv\Scripts\python dashboard/python/dashboard.py --nodes sim-01,sim-02
```
