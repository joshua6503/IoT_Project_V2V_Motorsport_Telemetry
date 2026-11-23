# Gateway en la Raspberry Pi 5

Raspberry Pi 5 (`RaspPi5`, Debian 13 trixie, usuario `joshua`) con **Mosquitto** (usuario/contraseña).
Su única radio Wi-Fi no puede ser cliente y punto de acceso a la vez, así que tiene **dos modos**
(ver [ADR-0003](../../docs/adr/0003-mqtt-with-local-mosquitto.md)):

| Modo | wlan0 es… | Para qué | Internet en la Pi |
|---|---|---|---|
| `home` | cliente del Wi-Fi de casa (hotspot del iPhone como respaldo) | mantenimiento, `apt`, SSH | sí |
| `pit` | punto de acceso **V2V-PIT** (2.4 GHz, canal 6, WPA2) | trabajo con los nodos y demo | **no** |

El dashboard Node-RED corre en la **laptop** (`dashboard/`), no en la Pi.

## Datos para los nodos (ESP32 y RW612)

Los nodos se configuran **siempre para V2V-PIT**; cambiar el modo de la Pi nunca obliga a recompilar.

| Dato | Valor |
|---|---|
| SSID | `V2V-PIT` (WPA2-PSK, 2.4 GHz; la contraseña la sabe Joshua, va solo en `config.h`, ignorado por git) |
| Broker | **`10.42.0.1`** (fija en modo pit), puerto **1883**, sin TLS |
| Usuarios MQTT | `rw612-a` y `esp32-b` (cada nodo usa el suyo; contraseña en su `config.h`) |
| Tópicos | `v2v/{node_id}/telemetry`, `alert`, `status` (ver `interfaces/mqtt-topics.md`) |

Esa IP está fijada en el perfil del AP; se verificó en la Pi con `pi-mode status` (ver reporte F1).
Las IP del modo `home` cambian con la red: usa `RaspPi5.local`.

## Instalación (una sola vez, en modo `home`)

Todo lo que necesita internet (`apt`) se hace **antes** del primer cambio a `pit`. Mientras no haya
cambio de red, la Pi sigue en el hotspot del iPhone (que sí tiene internet).

1. Desde la laptop, copiar los scripts (sin sudo):
   ```powershell
   scp gateway/pi/pi-mode.sh gateway/pi/setup_pi.sh gateway/pi/verify_pi.sh gateway/pi/add_home_wifi.sh joshua@RaspPi5.local:~/v2v-gateway/
   ```
2. En la Pi (`ssh joshua@RaspPi5.local`; con el hotspot, `RaspPi5.local` o la IP que tenga):
   ```bash
   cd ~/v2v-gateway && chmod +x *.sh
   sudo ./setup_pi.sh        # instala mosquitto, pide 3 contraseñas MQTT (ocultas), instala pi-mode y verify-pi
   verify-pi                 # pide la contraseña de "dashboard"
   ```
   Cambiar una contraseña MQTT: `sudo ./setup_pi.sh --reset-user esp32-b`.

### Perfil del Wi-Fi de casa (se agrega, NO se activa)

```bash
cd ~/v2v-gateway
sudo ./add_home_wifi.sh "NOMBRE_DE_TU_WIFI"    # respalda NM/netplan, pide la contraseña oculta
nmcli connection show                          # debe listar home-wifi (autoconnect yes, prioridad 10)
nmcli -g connection.autoconnect,connection.autoconnect-priority connection show home-wifi   # yes / 10
```

Esto **no cambia la conexión actual**. NetworkManager solo usa un perfil de autoconexión cuando
`wlan0` no tiene conexión activa: la **siguiente vez que la Pi arranque** dentro del alcance de tu
red de casa, o cuando el hotspot desaparezca. Nunca abandona una conexión que funciona para irse a
otra «mejor». Con prioridad 10 (hotspot = 0), en casa gana el Wi-Fi de casa. Para probarlo sin
esperar: `sudo reboot` estando en casa (después `ssh joshua@RaspPi5.local`).
Si el perfil quedó mal: `sudo nmcli connection delete home-wifi` y repetir el script.

## Cambiar de modo

```bash
sudo pi-mode status      # modo, IPs, cambio pendiente, clientes del AP
sudo pi-mode pit         # la primera vez pide la contraseña de V2V-PIT (oculta, dos veces)
sudo pi-mode home
sudo pi-mode confirm     # DESDE LA RED NUEVA, para aceptar el cambio
```

- El cambio corre **desacoplado** de tu sesión SSH (se cae a los ~3 s; es normal).
- Si **no** haces `confirm` en 5 minutos, la Pi **regresa sola** al modo anterior (rollback).
  Un reinicio durante la prueba también regresa al último modo confirmado.
- Antes de cambiar se respalda NetworkManager/netplan en `/var/lib/v2v-mode/backups/`.
- **La Pi arranca en el último modo confirmado** (al inicio: `home`). Así un modo solo se vuelve
  permanente después de comprobar que se llega a la Pi por esa red. Para la demo: confirma `pit`
  antes de salir y la Pi levanta V2V-PIT sola, sin depender de ninguna red externa. Para volver a
  que arranque en casa: `sudo pi-mode home` y luego `sudo pi-mode confirm`. Si el AP fallara al
  arrancar, NetworkManager cae al Wi-Fi de casa/hotspot por prioridad.

## Checklist: primera prueba en modo pit (al final, con las otras sesiones en pausa)

La laptop cambia de red: **pausa las demás sesiones de Claude Code y los nodos que prueben contra
el broker de la laptop** antes de empezar. En pit la Pi **no tiene internet**.

**A. Laptop: internet por el iPhone (USB) y Wi-Fi para V2V-PIT**
1. Conecta el iPhone por USB (tethering) y verifica el adaptador:
   ```powershell
   Get-NetAdapter | ? Status -eq Up | ft Name,InterfaceDescription     # busca "Apple Mobile Device Ethernet"
   ```
2. Métricas (PowerShell **como administrador**; ajusta `<TETHER>` al nombre del adaptador):
   ```powershell
   Set-NetIPInterface -InterfaceAlias "<TETHER>" -InterfaceMetric 10
   Set-NetIPInterface -InterfaceAlias "Wi-Fi"    -InterfaceMetric 500
   ```
   (La Pi en modo AP se anuncia como puerta de enlace; sin esto el Wi-Fi podría llevarse la ruta por defecto.)

**B. Pi (SSH por el hotspot, todavía)**
3. `sudo pi-mode status` (debe decir `confirmed mode: home`) y `verify-pi` en home (referencia).
4. `sudo pi-mode pit`: la primera vez pide la contraseña de V2V-PIT (oculta, dos veces). La sesión SSH se cae.
   Empieza la cuenta de **5 minutos**.

**C. Laptop → V2V-PIT**
5. Une el Wi-Fi de la laptop a `V2V-PIT` (escribe la contraseña tú). Verifica rutas:
   ```powershell
   Get-NetRoute -DestinationPrefix 0.0.0.0/0 | sort RouteMetric | ft InterfaceAlias,NextHop,RouteMetric  # primero el tether
   Find-NetRoute -RemoteIPAddress 10.42.0.1 | select -First 1 InterfaceAlias    # Wi-Fi
   Find-NetRoute -RemoteIPAddress 8.8.8.8   | select -First 1 InterfaceAlias    # el tether
   ```
6. `ssh joshua@10.42.0.1`, luego `sudo pi-mode confirm` (**antes de 5 min**) y `sudo pi-mode status`
   (clientes del AP; anota la IP de `wlan0` = broker).
7. `verify-pi` en pit. Prueba MQTT desde la laptop contra `10.42.0.1:1883` con el simulador.

**D. Prueba de rollback (opcional pero recomendada)**
8. `sudo pi-mode home` **sin** confirmar; espera 5 min: debe regresar a pit sola (la laptop vuelve a ver `10.42.0.1`).

**E. Al terminar: revertir las métricas**
9. Une el Wi-Fi de la laptop a tu red normal y, como administrador:
   ```powershell
   Set-NetIPInterface -InterfaceAlias "Wi-Fi" -AutomaticMetric Enabled
   Set-NetIPInterface -InterfaceAlias "<TETHER>" -AutomaticMetric Enabled
   Get-NetIPInterface -AddressFamily IPv4 | ft InterfaceAlias,InterfaceMetric,AutomaticMetric
   ```
10. Si la Pi debe arrancar en casa: `sudo pi-mode home` y `sudo pi-mode confirm` (hace falta estar en una red desde la que alcances la Pi).

## Cómo entrar por SSH en cada modo

| Modo | Laptop conectada a | Comando |
|---|---|---|
| home | la misma red que la Pi (casa o hotspot) | `ssh joshua@RaspPi5.local` (o la IP que muestre `pi-mode status`) |
| pit | **V2V-PIT** | `ssh joshua@10.42.0.1` (o `RaspPi5.local`) |

Mientras la laptop está en V2V-PIT no tiene internet por ahí: usa el tethering USB del iPhone y
comprueba las métricas de ruta (ver `dashboard/README.md`).

## Verificar

```bash
verify-pi        # servicios, modo, publicación anónima rechazada, ida y vuelta MQTT autenticada
```
Córrelo en `home`, en `pit` y después de reiniciar (`sudo reboot`).

## Si la Pi no responde (recuperación)

1. **Espera 5 minutos** si acabas de cambiar de modo: el rollback regresa al modo anterior.
2. **Reinicia** (corta y reconecta la corriente): sin `confirm`, arranca en el último modo confirmado.
3. **Monitor + teclado** (micro-HDMI y USB): entra como `joshua` y corre `sudo pi-mode home`
   (después `sudo pi-mode confirm`). Con `nmcli connection show` y `nmcli device status` ves el estado.
4. Si los perfiles quedaron mal: restaura el respaldo
   `sudo tar xzf /var/lib/v2v-mode/backups/nm-<fecha>.tgz -C /` y `sudo systemctl restart NetworkManager`.
5. Último recurso: cable Ethernet a un router (eth0 arranca por DHCP) y SSH por esa IP.

## Archivos

| Archivo | Qué hace |
|---|---|
| `setup_pi.sh` | instala y configura Mosquitto (idempotente), usuarios, scripts |
| `add_home_wifi.sh` | agrega (sin activar) el perfil `home-wifi`, prioridad 10 |
| `pi-mode.sh` | `home` / `pit` / `confirm` / `status` / `rollback` (se instala como `pi-mode`) |
| `verify_pi.sh` | verificación (se instala como `verify-pi`) |

Los secretos (contraseña de casa, de V2V-PIT y de MQTT) los escribe Joshua con entrada oculta;
nunca están en el repo ni en los scripts.
