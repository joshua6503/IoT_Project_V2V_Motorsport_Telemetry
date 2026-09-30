# node-simulator

Fake node that publishes schema-valid telemetry (and random alerts) to the broker.
Lets the dashboard and integration tests run without hardware.

```powershell
python -m venv .venv
.venv\Scripts\Activate.ps1
pip install -r tools/node-simulator/requirements.txt

python tools/node-simulator/simulator.py --help
python tools/node-simulator/simulator.py --broker localhost:1883 --node-id sim-01 --rate 5
```

| Option | Default | Meaning |
|---|---|---|
| `--broker` | `localhost:1883` | `host[:port]` |
| `--node-id` | `sim-01` | used in topic and payload |
| `--rate` | `5` | telemetry Hz |
| `--alert-prob` | `0.02` | alert probability per sample |
| `--count` | `0` | stop after N samples (0 = forever) |
| `--seed` | none | reproducible values |

Watch the traffic:

```powershell
docker exec -it v2v-mosquitto mosquitto_sub -t "v2v/#" -v
```
