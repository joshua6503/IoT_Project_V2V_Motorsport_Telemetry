# node-simulator

Fake nodes (default `rw612-a` and `esp32-b`) that publish schema-valid telemetry, retained
status with LWT, and random alerts. Lets the dashboard and integration tests run without hardware.

```powershell
python -m venv .venv
.venv\Scripts\Activate.ps1
pip install -r tools/node-simulator/requirements.txt

python tools/node-simulator/simulator.py --help
# credentials: env vars (preferred) or hidden prompt; never put the password in a file
$env:V2V_BROKER = "10.42.0.1:1883"      # pit mode; in home mode use RaspPi5.local
$env:V2V_MQTT_USER = "dashboard"        # any user the broker knows
python tools/node-simulator/simulator.py                     # asks the password (hidden)
```

| Option | Env var | Default | Meaning |
|---|---|---|---|
| `--broker` | `V2V_BROKER` | `localhost:1883` | `host[:port]` |
| `--username` | `V2V_MQTT_USER` | none | MQTT user (anonymous if unset) |
| `--password` | `V2V_MQTT_PASSWORD` | prompt | hidden prompt when a user is set |
| `--nodes` | | `rw612-a,esp32-b` | comma-separated node ids |
| `--node-id` | | | single node (overrides `--nodes`) |
| `--rate` | | `5` | telemetry Hz per node |
| `--alert-prob` | | `0.02` | alert probability per sample per node |
| `--count` | | `0` | stop after N samples per node (0 = forever) |
| `--seed` | | none | reproducible values |

Each node has its own connection, LWT (`offline`, retained) and retained `online` on connect.
Distance is `null` (and `sensor_ok` bit0 cleared) ~5 % of samples.
